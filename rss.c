#include "common.h"

#include <time.h>

static Status make_rss_date(char** date_str_p, Page* page);
static int page_compare(const void* page1_p, const void* page2_p);
static uint week_day(uint day, uint month, uint year);

Status rss_generate(char** html_p, Page* pages) {
    TRY
    char* date_str = NULL;

    CHECK(string_new(html_p, 0));
    CHECK(string_append_indent(html_p, "<rss xmlns:atom=\"http://www.w3.org/2005/Atom\" version=\"2.0\">\n", 0));
    CHECK(string_append_indent(html_p, "<channel>\n", 1));
    CHECK(string_append_indent(html_p, "<title>Amiga Geek</title>\n", 2));
    CHECK(string_append_indent(html_p, "<link>https://amigageek.com/</link>\n", 2));
    CHECK(string_append_indent(html_p, "<description>Antiquated adventures of a nostalgic engineer</description>\n", 2));

    qsort(pages, vector_length(pages), sizeof(Page), page_compare);

    vector_foreach(pages, Page, page) {
        if (page->parent) {
            continue;
        }

        CHECK(string_append_indent(html_p, "<item>\n", 2));
        CHECK(string_append_indent(html_p, "<title>", 3));
        CHECK(string_append(html_p, page->title));
        CHECK(string_append(html_p, "</title>\n"));
        CHECK(string_append_indent(html_p, "<link>https://amigageek.com/", 3));
        CHECK(string_append(html_p, page->relative_url));
        CHECK(string_append(html_p, "</link>\n"));
        CHECK(string_append_indent(html_p, "<pubDate>", 3));

        CHECK(make_rss_date(&date_str, page));
        CHECK(string_append(html_p, date_str));
        string_free(&date_str);

        CHECK(string_append(html_p, "</pubDate>\n"));
        CHECK(string_append_indent(html_p, "<description>", 3));

        if (page->description) {
            CHECK(string_append(html_p, page->description));
        } else {
            vector_foreach(page->children, Element, page_child) {
                if (page_child->type == ET_Paragraph) {
                    vector_foreach(page_child->children, Element, paragraph_child) {
                        CHECK(string_append(html_p, paragraph_child->text));
                    }
                    break;
                }
            }
        }

        CHECK(string_append(html_p, "</description>\n"));
        CHECK(string_append_indent(html_p, "</item>\n", 2));
    }

    CHECK(string_append_indent(html_p, "</channel>\n", 1));
    CHECK(string_append_indent(html_p, "</rss>\n", 0));

    FINALLY
    string_free(&date_str);

    RETURN;
}

static int page_compare(const void* page1_p, const void* page2_p) {
    return - strcmp(((Page*)page1_p)->date, ((Page*)page2_p)->date);
}

static Status make_rss_date(char** date_str_p, Page* page) {
    TRY
    const char* day_names[] = {"Sat", "Sun", "Mon", "Tue", "Wed", "Thu", "Fri"};
    const char* month_names[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    const char* day_name = day_names[week_day(page->date_day, page->date_month, page->date_year)];
    const char* month_name = month_names[page->date_month - 1];

    CHECK(string_printf(date_str_p, "%s, %02d %s %d 00:00:00 UTC", day_name, page->date_day, month_name, page->date_year));

    FINALLY RETURN;
}

static uint week_day(uint day, uint month, uint year) {
    // https://en.wikipedia.org/wiki/Zeller%27s_congruence
    if (month < 3) {
        month += 12;
        -- year;
    }

    uint K = year % 100;
    uint J = year / 100;

    return (day + ((13 * (month + 1)) / 5) + K + (K / 4) + (J / 4) - (2 * J)) % 7;
}
