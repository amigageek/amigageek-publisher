#ifndef COMMON_H
#define COMMON_H

#include "AmiUtil/Application.h"

enum {
    StatusQuit = (1 << 1),
};

typedef enum {
    ET_None,
    ET_Bold,
    ET_Header,
    ET_HRule,
    ET_Image,
    ET_Link,
    ET_List,
    ET_ListItem,
    ET_Paragraph,
    ET_Preformatted,
    ET_Text,
} ElementType;

typedef struct ElementS {
    ElementType type;
    struct ElementS* children;
    char* text;
    char* url;
} Element;

typedef struct PageS {
    struct PageS* parent;
    char* markdown_path;
    char* dir_path;
    char* relative_url;
    char* title;
    char* date;
    char* description;
    Element* children;
    uint date_year;
    uint date_month;
    uint date_day;
    bool add_to_index;
} Page;

Status file_read(char** contents_p, const char* path);
Status file_write(const char* contents, const char* path);
void html_fini(void);
Status html_generate(char** page_html_p, Page* page);
Status html_generate_root(char** root_html_p, Page* pages);
Status html_init(const char* base_path);
Status markdown_parse_all(const char* file_contents, Page* page);
Status markdown_parse_frontmatter(const char* file_contents, Page* page);
Status page_build_all(const char* base_path);
Status page_build_live(const char* base_path);
void page_fini(void);
Status page_init(void);
Status rexx_get_live_path(char** path_p);
Status rss_generate(char** html_p, Page* pages);
Status string_append_indent(char** to_string_p, const char* suffix, uint indent);

#endif
