#include "common.h"

#include <dirent.h>
#include <limits.h>
#include <proto/dos.h>
#include <proto/openurl.h>
#include <sys/stat.h>
#include <sys/types.h>

static Status build_page(Page* page);
static Status find_all_pages(const char* dir_path, const char* dir_url);
static void free_elements(Element** elements);
static Status get_live_url(char** live_url_p, const char* path, const char* base_path);
static Status get_real_path(char** real_path_p, const char* path);

static struct {
    Page *pages;
} g;

Status page_init(void) {
    TRY
    CHECK(vector_new(&g.pages, sizeof(Page), 0));

    FINALLY RETURN;
}

void page_fini(void) {
    if (g.pages) {
        vector_foreach(g.pages, Page, page) {
            string_free(&page->markdown_path);
            string_free(&page->dir_path);
            string_free(&page->relative_url);
            string_free(&page->title);
            string_free(&page->date);
            string_free(&page->description);
            free_elements(&page->children);
        }

        vector_free(&g.pages);
    }
}

static void free_elements(Element** elements_p) {
    if (*elements_p) {
        vector_foreach(*elements_p, Element, element) {
            string_free(&element->text);
            string_free(&element->url);
            free_elements(&element->children);
        }

        vector_free(elements_p);
    }
}

Status page_build_all(const char* base_path) {
    TRY
    char* file_path = NULL;
    char* text = NULL;

    CHECK(find_all_pages(base_path, ""));

    vector_foreach(g.pages, Page, page) {
        CHECK(build_page(page));
    }

    CHECK(string_path_join(&file_path, base_path, "index.html"));
    CHECK(html_generate_root(&text, g.pages));
    CHECK(file_write(text, file_path));

    string_free(&text);
    string_free(&file_path);

    CHECK(string_path_join(&file_path, base_path, "index.xml"));
    CHECK(rss_generate(&text, g.pages));
    CHECK(file_write(text, file_path));

    FINALLY
    string_free(&text);
    string_free(&file_path);
    
    RETURN;
}

Status page_build_live(const char* base_path) {
    TRY
    char** live_paths = NULL;
    char* live_url = NULL;

    CHECK(rexx_get_live_paths(&live_paths));

    vector_foreach(live_paths, char*, live_path_p) {
        CHECK(vector_append(&g.pages, 1, NULL));
        Page* page = &g.pages[vector_length(g.pages) - 1];

        SWAP(page->markdown_path, *live_path_p);
        CHECK(string_path_dirpart(&page->dir_path, page->markdown_path));
        CHECK(vector_new(&page->children, sizeof(Element), 0));

        CHECK(build_page(page));

        CHECK(get_live_url(&live_url, page->dir_path, base_path));
        URL_Open(live_url, TAG_DONE);
        string_free(&live_url);
    }

    FINALLY
    string_free(&live_url);

    if (live_paths) {
        vector_foreach(live_paths, char*, live_path_p) {
            string_free(live_path_p);
        }

        vector_free(&live_paths);
    }

    RETURN;
}

static Status find_all_pages(const char* dir_path, const char* dir_url) {
    TRY
    DIR* dir = NULL;
    char* sub_path = NULL;
    char* sub_url = NULL;

    ASSERT(dir = opendir(dir_path), "%s is not a directory", dir_path);

    for (struct dirent* dir_ent; (dir_ent = readdir(dir));) {
        CHECK(string_path_join(&sub_path, dir_path, dir_ent->d_name));

        struct stat dir_stat;
        ASSERT(stat(sub_path, &dir_stat) == 0, "Cannot stat %s", dir_ent->d_name);

        if (S_ISDIR(dir_stat.st_mode)) {
            CHECK(string_printf(&sub_url, "%s%s/", dir_url, dir_ent->d_name));
            CHECK(find_all_pages(sub_path, sub_url));
            string_free(&sub_url);
        } else if (strcmp(dir_ent->d_name, "index.md") == 0) {
            CHECK(vector_append(&g.pages, 1, NULL));
            Page* page = &g.pages[vector_length(g.pages) - 1];

            SWAP(page->markdown_path, sub_path);
            CHECK(string_clone(&page->dir_path, dir_path));
            CHECK(string_clone(&page->relative_url, dir_url));
            CHECK(vector_new(&page->children, sizeof(Element), 0));

            page->add_to_index = string_startswith(dir_url, "posts/") || (string_count_substr(dir_url, "/") == 1);
        }

        string_free(&sub_path);
    }

    FINALLY
    string_free(&sub_url);
    string_free(&sub_path);

    if (dir) {
        closedir(dir);
    }

    RETURN;
}

static Status build_page(Page* page) {
    TRY
    char* text_markdown = NULL;
    char* text_html = NULL;
    char* html_path = NULL;

    CHECK(file_read(&text_markdown, page->markdown_path));
    CHECK(markdown_parse(text_markdown, page));
    CHECK(html_generate(&text_html, page));

    CHECK(string_clone_substr(&html_path, page->markdown_path, string_length(page->markdown_path) - 2));
    CHECK(string_append(&html_path, "html"));
    CHECK(file_write(text_html, html_path));

    FINALLY
    string_free(&html_path);
    string_free(&text_html);
    string_free(&text_markdown);

    RETURN;
}

static Status get_live_url(char** live_url_p, const char* dir_path, const char* base_path) {
    TRY
    char* real_dir_path = NULL;
    char* real_base_path = NULL;

    CHECK(get_real_path(&real_dir_path, dir_path));
    CHECK(get_real_path(&real_base_path, base_path));

    size_t real_base_len = string_length(real_base_path);

    ASSERT(string_length(real_dir_path) > real_base_len);
    ASSERT(strncmp(real_base_path, real_dir_path, real_base_len) == 0);

    CHECK(string_clone(live_url_p, "file://localhost/"));
    CHECK(string_path_append(live_url_p, base_path));
    CHECK(string_path_append(live_url_p, &real_dir_path[real_base_len + 1]));
    CHECK(string_path_append(live_url_p, "#end"));

    FINALLY
    string_free(&real_base_path);
    string_free(&real_dir_path);

    RETURN;
}

static Status get_real_path(char** real_path_p, const char* path) {
    TRY
    BPTR lock = 0;

    ASSERT(lock = Lock(path, ACCESS_READ));

    CHECK(string_new(real_path_p, PATH_MAX));
    ASSERT(NameFromLock(lock, *real_path_p, PATH_MAX));
    CHECK(string_truncate(real_path_p, strlen(*real_path_p)));

    FINALLY
    UnLock(lock);

    RETURN;
}
