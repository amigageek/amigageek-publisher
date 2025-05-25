#include "common.h"

#include <datatypes/pictureclass.h>
#include <proto/datatypes.h>

#define INDENT 5

static Status generate_element(char** page_html_p, Element* element, Page* page, uint indent);
static Status generate_image_tags(char** page_html_p, char* dir_path, Element* element, uint indent);
static Status make_formatted_date(char** date_str_p, Page* page);

static struct {
    char* page_template;
} g;

Status html_init(const char* base_path) {
    TRY
    char* html_path = NULL;

    CHECK(string_path_join(&html_path, base_path, "page.html"));
    CHECK(file_read(&g.page_template, html_path));

    FINALLY
    string_free(&html_path);

    RETURN;
}

void html_fini(void) {
    string_free(&g.page_template);
}

Status html_generate(char** page_html_p, Page* page) {
    TRY
    char* date_str = NULL;
    char* title_str = NULL;
    char* body_html = NULL;

    CHECK(make_formatted_date(&date_str, page));
    CHECK(string_printf(&title_str, "%s | Amiga Geek", page->title));

    CHECK(string_new(&body_html, 0));
    CHECK(string_append_indent(&body_html, "<p class=\"heading\"><font size=\"+2\"><b>", INDENT));
    CHECK(string_append(&body_html, page->title));
    CHECK(string_append(&body_html, "</b></font></p>\n"));
    CHECK(string_append_indent(&body_html, "<p>Last updated: ", INDENT));
    CHECK(string_append(&body_html, date_str));
    CHECK(string_append(&body_html, "</p>\n"));

    CHECK(string_append_indent(&body_html, "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\">\n", INDENT));
    CHECK(string_append_indent(&body_html, "<tr>\n", INDENT + 1));
    CHECK(string_append_indent(&body_html, "<td class=\"vspace\" height=\"10\"></td>\n", INDENT + 2));
    CHECK(string_append_indent(&body_html, "</tr>\n", INDENT + 1));
    CHECK(string_append_indent(&body_html, "<tr>\n", INDENT + 1));
    CHECK(string_append_indent(&body_html, "<td class=\"hrule\" height=\"1\" bgcolor=\"#383860\"></td>\n", INDENT + 2));
    CHECK(string_append_indent(&body_html, "</tr>\n", INDENT + 1));
    CHECK(string_append_indent(&body_html, "</table>\n", INDENT));

    vector_foreach(page->children, Element, child) {
        CHECK(generate_element(&body_html, child, page, INDENT));
    }

    CHECK(string_clone(page_html_p, g.page_template));
    CHECK(string_replace_first(page_html_p, "$BODY", body_html));
    CHECK(string_replace_first(page_html_p, "$TITLE", title_str));

    FINALLY
    string_free(&body_html);
    string_free(&title_str);
    string_free(&date_str);

    RETURN;
}

Status html_generate_root(char** root_html_p, Page* pages) {
    TRY
    Page* project_pages = NULL;
    Page* post_pages = NULL;
    char* body_html = NULL;

    CHECK(vector_new(&project_pages, sizeof(Page), 0));
    CHECK(vector_new(&post_pages, sizeof(Page), 0));

    vector_foreach(pages, Page, page) {
        if (page->description) {
            size_t insert_at = 0;

            for (; insert_at < vector_length(project_pages); ++ insert_at) {
                if (strcmp(page->title, project_pages[insert_at].title) < 0) {
                    break;
                }
            }

            CHECK(vector_insert(&project_pages, insert_at, 1, page));
        } else {
            size_t insert_at = 0;

            for (; insert_at < vector_length(post_pages); ++ insert_at) {
                if (strcmp(page->date, post_pages[insert_at].date) > 0) {
                    break;
                }
            }

            CHECK(vector_insert(&post_pages, insert_at, 1, page));
        }
    }

    CHECK(string_new(&body_html, 0));
    CHECK(string_append_indent(&body_html, "<p class=\"heading\"><font size=\"+2\"><b>Projects</b></font></p>\n", INDENT));
    CHECK(string_append_indent(&body_html, "<table class=\"table\" cellspacing=\"0\" cellpadding=\"0\">\n", INDENT));

    vector_foreach(project_pages, Page, page) {
        CHECK(string_append_indent(&body_html, "<tr>\n", INDENT + 1));
        CHECK(string_append_indent(&body_html, "<td class=\"vspace\" height=\"10\"></td></tr>\n", INDENT + 2));
        CHECK(string_append_indent(&body_html, "</tr>\n", INDENT + 1));
        CHECK(string_append_indent(&body_html, "<tr>\n", INDENT + 1));
        CHECK(string_append_indent(&body_html, "<td class=\"hspace\" width=\"10\"></td>\n", INDENT + 2));
        CHECK(string_append_indent(&body_html, "<td><a href=\"", INDENT + 2));
        CHECK(string_append(&body_html, page->relative_url));
        CHECK(string_append(&body_html, "\">"));
        CHECK(string_append(&body_html, page->title));
        CHECK(string_append(&body_html, "</a></td>\n"));
        CHECK(string_append_indent(&body_html, "<td class=\"hspace\" width=\"10\"></td>\n", INDENT + 2));
        CHECK(string_append_indent(&body_html, "<td>", INDENT + 2));
        CHECK(string_append(&body_html, page->description));
        CHECK(string_append(&body_html, "</td>\n"));
        CHECK(string_append_indent(&body_html, "<td class=\"hspace\" width=\"10\"></td>\n", INDENT + 2));
        CHECK(string_append_indent(&body_html, "</tr>\n", INDENT + 1));
    }

    CHECK(string_append_indent(&body_html, "<tr>\n", INDENT + 1));
    CHECK(string_append_indent(&body_html, "<td class=\"vspace\" height=\"10\"></td></tr>\n", INDENT + 2));
    CHECK(string_append_indent(&body_html, "</tr>\n", INDENT + 1));
    CHECK(string_append_indent(&body_html, "</table>\n", INDENT));
    CHECK(string_append_indent(&body_html, "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\">\n", INDENT));
    CHECK(string_append_indent(&body_html, "<tr>\n", INDENT + 1));
    CHECK(string_append_indent(&body_html, "<td class=\"hrule\" height=\"1\" bgcolor=\"#383860\"></td>\n", INDENT + 2));
    CHECK(string_append_indent(&body_html, "</tr>\n", INDENT + 1));
    CHECK(string_append_indent(&body_html, "</table>\n", INDENT));
    CHECK(string_append_indent(&body_html, "<p class=\"heading\"><font size=\"+2\"><b>Recent posts</b></font></p>\n", INDENT));
    CHECK(string_append_indent(&body_html, "<table class=\"table\" cellspacing=\"0\" cellpadding=\"0\">\n", INDENT));

    vector_foreach(post_pages, Page, page) {
        CHECK(string_append_indent(&body_html, "<tr>\n", INDENT + 1));
        CHECK(string_append_indent(&body_html, "<td class=\"vspace\" height=\"10\"></td></tr>\n", INDENT + 2));
        CHECK(string_append_indent(&body_html, "</tr>\n", INDENT + 1));
        CHECK(string_append_indent(&body_html, "<tr>\n", INDENT + 1));
        CHECK(string_append_indent(&body_html, "<td class=\"hspace\" width=\"10\"></td>\n", INDENT + 2));
        CHECK(string_append_indent(&body_html, "<td>", INDENT + 2));
        CHECK(string_append(&body_html, page->date));
        CHECK(string_append(&body_html, "</td>\n"));
        CHECK(string_append_indent(&body_html, "<td class=\"hspace\" width=\"10\"></td>\n", INDENT + 2));
        CHECK(string_append_indent(&body_html, "<td><a href=\"", INDENT + 2));
        CHECK(string_append(&body_html, page->relative_url));
        CHECK(string_append(&body_html, "\">"));
        CHECK(string_append(&body_html, page->title));
        CHECK(string_append(&body_html, "</a></td>\n"));
        CHECK(string_append_indent(&body_html, "<td class=\"hspace\" width=\"10\"></td>\n", INDENT + 2));
        CHECK(string_append_indent(&body_html, "</tr>\n", INDENT + 1));
    }

    CHECK(string_append_indent(&body_html, "</table>\n", INDENT));

    CHECK(string_clone(root_html_p, g.page_template));
    CHECK(string_replace_first(root_html_p, "$BODY", body_html));
    CHECK(string_replace_first(root_html_p, "$TITLE", "Home | Amiga Geek"));

    FINALLY
    string_free(&body_html);
    vector_free(&post_pages);
    vector_free(&project_pages);

    RETURN;
}

static Status make_formatted_date(char** date_str_p, Page* page) {
    TRY
    const char* day_suffix = NULL;

    switch (page->date_day) {
    case 1:
    case 21:
    case 31:
        day_suffix = "st";
        break;
    case 2:
    case 22:
        day_suffix = "nd";
        break;
    case 3:
    case 23:
        day_suffix = "rd";
        break;
    default:
        day_suffix = "th";
        break;
    }

    const char* month_names[] = {"January", "February", "March", "April", "May",
        "June", "July", "August", "September", "October", "November", "December"};

    CHECK(string_printf(date_str_p, "%u%s %s %u", page->date_day,
        day_suffix, month_names[page->date_month - 1], page->date_year));

    FINALLY RETURN;
}

static Status generate_element(char** page_html_p, Element* element, Page* page, uint indent) {
    TRY
    char* anchor_name = NULL;

    switch (element->type) {
    case ET_Bold:
        CHECK(string_append(page_html_p, "<b>"));
        break;
    case ET_Header:
        ASSERT(vector_length(element->children) == 1 && element->children[0].type == ET_Text);

        CHECK(string_append_indent(page_html_p, "<p class=\"heading\" id=\"", indent));
        CHECK(string_clone(&anchor_name, element->children[0].text));
        string_tolower(anchor_name);
        CHECK(string_replace_all(&anchor_name, " ", "_"));
        CHECK(string_append(page_html_p, anchor_name));
        CHECK(string_append(page_html_p, "\"><font size=\"+2\"><b>"));
        string_free(&anchor_name);
        break;
    case ET_HRule:
        CHECK(string_append_indent(page_html_p, "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\">\n", indent));
        CHECK(string_append_indent(page_html_p, "<tr>\n", indent + 1));
        CHECK(string_append_indent(page_html_p, "<td class=\"vspace\" height=\"10\"></td>\n", indent + 2));
        CHECK(string_append_indent(page_html_p, "</tr>\n", indent + 1));
        CHECK(string_append_indent(page_html_p, "<td class=\"hrule\" height=\"1\" bgcolor=\"#383860\"></td>\n", indent + 2));
        CHECK(string_append_indent(page_html_p, "</tr>\n", indent + 1));
        CHECK(string_append_indent(page_html_p, "</table>\n", indent));
        break;
    case ET_Image: {
        CHECK(generate_image_tags(page_html_p, page->dir_path, element, indent));

        if (string_length(element->text) > 0) {
            CHECK(string_append_indent(page_html_p, "<table cellspacing=\"0\" cellpadding=\"0\">\n", indent));
            CHECK(string_append_indent(page_html_p, "<tr>\n", indent + 1));
            CHECK(string_append_indent(page_html_p, "<td class=\"vspace\" height=\"10\"></td>\n", indent + 2));
            CHECK(string_append_indent(page_html_p, "</tr>\n", indent + 1));
            CHECK(string_append_indent(page_html_p, "<tr>\n", indent + 1));
            CHECK(string_append_indent(page_html_p, "<td class=\"hspace2\" width=\"20\"></td>\n", indent + 2));
            CHECK(string_append_indent(page_html_p, "<td>\n", indent + 2));
            CHECK(string_append_indent(page_html_p, "<b>", indent + 3));
        }
        break;
    }
    case ET_Link:
        CHECK(string_append(page_html_p, "<a href=\""));
        CHECK(string_append(page_html_p, element->url));
        CHECK(string_append(page_html_p, "\">"));
        break;
    case ET_List:
        CHECK(string_append_indent(page_html_p, "<ul>\n", indent));
        break;
    case ET_ListItem:
        CHECK(string_append_indent(page_html_p, "<li>", indent));
        break;
    case ET_Paragraph:
        CHECK(string_append_indent(page_html_p, "<p>", indent));
        break;
    case ET_Preformatted:
        CHECK(string_append_indent(page_html_p, "<table cellspacing=\"0\" cellpadding=\"0\">\n", indent));
        CHECK(string_append_indent(page_html_p, "<tr>\n", indent + 1));
        CHECK(string_append_indent(page_html_p, "<td class=\"vspace\" height=\"10\"></td>\n", indent + 2));
        CHECK(string_append_indent(page_html_p, "</tr>\n", indent + 1));
        CHECK(string_append_indent(page_html_p, "</table>\n", indent));
        CHECK(string_append_indent(page_html_p, "<table cellspacing=\"0\" cellpadding=\"10\" width=\"100%\">\n", indent));
        CHECK(string_append_indent(page_html_p, "<tr>\n", indent + 1));
        CHECK(string_append_indent(page_html_p, "<td class=\"pre\" bgcolor=\"#1A1A28\">\n", indent + 2));
        CHECK(string_append_indent(page_html_p, "<pre><font size=\"2\">", indent + 3));
        break;
    default:
        break;
    }

    if (element->text) {
        CHECK(string_append(page_html_p, element->text));
    }

    vector_foreach(element->children, Element, child) {
        CHECK(generate_element(page_html_p, child, page, indent + 1));
    }

    switch (element->type) {
    case ET_Bold:
        CHECK(string_append(page_html_p, "</b>"));
        break;
    case ET_Header:
        CHECK(string_append(page_html_p, "</b></font></p>\n"));
        break;
    case ET_Image:
        if (string_length(element->text) > 0) {
            CHECK(string_append(page_html_p, "</b>\n"));
            CHECK(string_append_indent(page_html_p, "</td>\n", indent + 2));
            CHECK(string_append_indent(page_html_p, "<td class=\"hspace2\" width=\"20\"></td>\n", indent + 2));
            CHECK(string_append_indent(page_html_p, "</tr>\n", indent + 1));
            CHECK(string_append_indent(page_html_p, "</table>\n", indent));
        }
        break;
    case ET_Link:
        CHECK(string_append(page_html_p, "</a>"));
        break;
    case ET_List:
        CHECK(string_append_indent(page_html_p, "</ul>\n", indent));
        break;
    case ET_ListItem:
        CHECK(string_append(page_html_p, "</li>\n"));
        break;
    case ET_Paragraph:
        CHECK(string_append(page_html_p, "</p>\n"));
        break;
    case ET_Preformatted:
        CHECK(string_append(page_html_p, "</font></pre>\n"));
        CHECK(string_append_indent(page_html_p, "</td>\n", indent + 2));
        CHECK(string_append_indent(page_html_p, "</tr>\n", indent + 1));
        CHECK(string_append_indent(page_html_p, "</table>\n", indent));
        break;
    default:
        break;
    }

    FINALLY
    string_free(&anchor_name);

    RETURN;
}

static Status generate_image_tags(char** page_html_p, char* dir_path, Element* element, uint indent) {
    TRY
    char* half_file_name = NULL;
    char* image_path = NULL;
    char* text_html = NULL;
    Object* image_dt = NULL;
    struct BitMapHeader* image_bmh = NULL;

    CHECK(string_clone(&half_file_name, element->url));
    CHECK(string_replace_first(&half_file_name, ".", "_half."));

    CHECK(string_path_join(&image_path, dir_path, half_file_name));
    ASSERT(image_dt = NewDTObject(image_path, DTA_SourceType, DTST_FILE, DTA_GroupID, GID_PICTURE, TAG_DONE));
    ASSERT(GetDTAttrs(image_dt, PDTA_BitMapHeader, &image_bmh, TAG_DONE));

    CHECK(string_append_indent(page_html_p, "<center>\n", indent));
    CHECK(string_append_indent(page_html_p, "<div class=\"image\" style=\"content: url(", indent + 1));
    CHECK(string_append(page_html_p, element->url));
    CHECK(string_append(page_html_p, "); "));

    CHECK(string_printf(&text_html, "width: %upx; height: %upx\">\n", image_bmh->bmh_Width * 2, image_bmh->bmh_Height * 2));
    CHECK(string_append(page_html_p, text_html));
    string_free(&text_html);

    CHECK(string_append_indent(page_html_p, "<img src=\"", indent + 2))
    CHECK(string_append(page_html_p, half_file_name));
    CHECK(string_append(page_html_p, "\" "));

    CHECK(string_printf(&text_html, "width=\"%d\" height=\"%d\"", image_bmh->bmh_Width, image_bmh->bmh_Height));
    CHECK(string_append(page_html_p, text_html));
    CHECK(string_append(page_html_p, ">\n"));

    CHECK(string_append_indent(page_html_p, "</div>\n", indent + 1));
    CHECK(string_append_indent(page_html_p, "</center>\n", indent));

    FINALLY
    DisposeDTObject(image_dt);
    string_free(&text_html);
    string_free(&image_path);
    string_free(&half_file_name);

    RETURN;
}
