#include "common.h"

#include <dirent.h>
#include <limits.h>
#include <proto/dos.h>
#include <proto/openurl.h>
#include <sys/stat.h>
#include <sys/types.h>

static Status build_all_pages(const char* dir_path, const char* dir_url, const char* filter_path, Page* parent);
static Status build_page(Page* page);
static void free_elements(Element** elements);
static Status get_live_url(char** live_url_p, const char* path, const char* base_path);
static Status get_real_path(char** real_path_p, const char* path);
static Status parse_frontmatter(Page* page);

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

    CHECK(build_all_pages(base_path, "", NULL, NULL));

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
    char* live_path = NULL;
    char* live_url = NULL;
    char* real_base_path = NULL;
    char* real_live_path = NULL;

    CHECK(rexx_get_live_path(&live_path));

    if (live_path) {
        CHECK(get_real_path(&real_base_path, base_path));
        CHECK(get_real_path(&real_live_path, live_path));
        CHECK(build_all_pages(real_base_path, "", real_live_path, NULL));

        Page* live_page = &vector_last(g.pages);

        CHECK(get_live_url(&live_url, live_page->dir_path, base_path));
        URL_Open(live_url, TAG_DONE);
        string_free(&live_url);
    }

    FINALLY
    string_free(&real_live_path);
    string_free(&real_base_path);
    string_free(&live_url);
    string_free(&live_path);

    RETURN;
}

static Status build_all_pages(const char* dir_path, const char* dir_url, const char* filter_path, Page* parent) {
    TRY
    DIR* dir = NULL;
    char* sub_path = NULL;
    char* sub_url = NULL;
    struct stat path_stat;
    Page* index_page = NULL;

    CHECK(string_path_join(&sub_path, dir_path, "index.md"));

    if (stat(sub_path, &path_stat) == 0) {
        CHECK(vector_append(&g.pages, 1, NULL));
        index_page = &g.pages[vector_length(g.pages) - 1];

        SWAP(index_page->markdown_path, sub_path);
        CHECK(string_clone(&index_page->dir_path, dir_path));
        CHECK(string_clone(&index_page->relative_url, dir_url));
        CHECK(vector_new(&index_page->children, sizeof(Element), 0));

        index_page->add_to_index = string_startswith(dir_url, "posts/") || (string_count_substr(dir_url, "/") == 1);
        index_page->parent = parent;

        if ((! filter_path) || (strcmp(index_page->markdown_path, filter_path) == 0)) {
            CHECK(build_page(index_page));
        } else {
            CHECK(parse_frontmatter(index_page));
        }
    }

    string_free(&sub_path);

    ASSERT(dir = opendir(dir_path), "%s is not a directory", dir_path);

    for (struct dirent* dir_ent; (dir_ent = readdir(dir));) {
        CHECK(string_path_join(&sub_path, dir_path, dir_ent->d_name));

        ASSERT(stat(sub_path, &path_stat) == 0, "Cannot stat %s", dir_ent->d_name);

        if (S_ISDIR(path_stat.st_mode)) {
            if ((! filter_path) || string_startswith(filter_path, sub_path)) {
                CHECK(string_printf(&sub_url, "%s%s/", dir_url, dir_ent->d_name));
                CHECK(build_all_pages(sub_path, sub_url, filter_path, index_page ? index_page : parent));
                string_free(&sub_url);
            }
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

static Status parse_frontmatter(Page* page) {
    TRY
    char* text_markdown = NULL;

    CHECK(file_read(&text_markdown, page->markdown_path));
    CHECK(markdown_parse_frontmatter(text_markdown, page));

    FINALLY
    string_free(&text_markdown);

    RETURN;
}

static Status build_page(Page* page) {
    TRY
    char* text_markdown = NULL;
    char* text_html = NULL;
    char* html_path = NULL;

    CHECK(file_read(&text_markdown, page->markdown_path));
    CHECK(markdown_parse_all(text_markdown, page));

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
