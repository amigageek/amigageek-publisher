#include "common.h"

#define AT_END() (*next_char == '\0')
#define CONSUME_UNTIL(CHAR) for (; (! AT_END()) && (*next_char != CHAR); ++ next_char);
#define CONSUME_UNTIL_AFTER(CHAR) while ((! AT_END()) && (*(next_char ++) != CHAR));
#define CONSUME_STRING(STR)                         \
    ((end_char - next_char >= strlen(STR)) &&       \
    (strncmp(next_char, STR, strlen(STR)) == 0) &&  \
    (! escape_next_char) &&                         \
    (next_char += strlen(STR)))

#define LT_ESCAPE "&lt;"
#define AMP_ESCAPE "&amp;"

static Status append_element(Element** new_element_p, Element** elements_p, ElementType type,
    const char* text_start, const char* text_end, const char* url_start, const char* url_end);
static Status escape_string(char** string_p);
static Status parse_blocks(Page* page, const char* next_char, const char* end_char);
static Status parse_front_matter(Page* page, const char** next_char_p, const char* end_char);
static Status parse_inlines(Element** elements_p, const char** next_char_p, const char* end_char);

Status markdown_parse_frontmatter(const char* text, Page* page) {
    TRY
    const char* next_char = text;
    const char* end_char = text + strlen(text);

    CHECK(parse_front_matter(page, &next_char, end_char));

    FINALLY RETURN;
}

Status markdown_parse_all(const char* text, Page* page) {
    TRY
    const char* next_char = text;
    const char* end_char = text + strlen(text);

    CHECK(parse_front_matter(page, &next_char, end_char));
    CHECK(parse_blocks(page, next_char, end_char));

    FINALLY RETURN;
}

static Status parse_front_matter(Page* page, const char** next_char_p, const char* end_char) {
    TRY
    const char* next_char = *next_char_p;
    bool escape_next_char = false;

    if (! CONSUME_STRING("---\n")) {
        THROW(StatusOK);
    }

    while ((! AT_END()) && (! CONSUME_STRING("---\n"))) {
        bool is_title = CONSUME_STRING("Title: ");
        bool is_date = CONSUME_STRING("Date: ");
        bool is_desc = CONSUME_STRING("Description: ");

        if (is_title || is_date || is_desc) {
            const char* value_start = next_char;
            CONSUME_UNTIL('\n');
            size_t value_len = next_char - value_start;

            if (value_len > 0) {
                char** value_str_p = (is_title ? &page->title : (is_date ? &page->date : &page->description));
                CHECK(string_clone_substr(value_str_p, value_start, value_len));
            }
        }

        CONSUME_UNTIL_AFTER('\n');
    }

    if (AT_END()) {
        THROW(StatusOK);
    }

    *next_char_p = next_char;

    if (! page->title) {
        CHECK(string_clone(&page->title, "Untitled"));
    }

    if (! page->date) {
        CHECK(string_clone(&page->date, "0000-00-00"));
    }

    sscanf(page->date, "%u-%u-%u", &page->date_year, &page->date_month, &page->date_day);

    page->date_day = MAX(1, MIN(31, page->date_day));
    page->date_month = MAX(1, MIN(12, page->date_month));

    FINALLY RETURN;
}

static Status parse_blocks(Page* page, const char* next_char, const char* end_char) {
    TRY
    typedef enum {
        TT_None,
        TT_ImageText,
        TT_ImageURL,
        TT_List,
        TT_Paragraph,
        TT_Preformatted,
    } TokenType;

    TokenType open_token = TT_None;
    Element* open_block = NULL;
    const char* text_start = NULL;
    const char* text_end = NULL;
    const char* url_start = NULL;
    bool escape_next_char = false;

    while (! AT_END()) {
        if (CONSUME_STRING("\n")) {
            if (open_token != TT_Preformatted) {
                open_token = TT_None;
            }
        } else {
            if (CONSUME_STRING("\\")) {
                escape_next_char = true;
                continue;
            }

            switch (open_token) {
            case TT_None:
            case TT_List:
                if (CONSUME_STRING("# ")) {
                    open_token = TT_None;
                    CHECK(append_element(&open_block, &page->children, ET_Header, NULL, NULL, NULL, NULL));
                    CHECK(parse_inlines(&open_block->children, &next_char, end_char));
                } else if (CONSUME_STRING("- ")) {
                    if (open_token != TT_List) {
                        open_token = TT_List;
                        CHECK(append_element(&open_block, &page->children, ET_List, NULL, NULL, NULL, NULL));
                    }

                    Element* list_item;
                    CHECK(append_element(&list_item, &open_block->children, ET_ListItem, NULL, NULL, NULL, NULL));
                    CHECK(parse_inlines(&list_item->children, &next_char, end_char));
                } else if (CONSUME_STRING("![")) {
                    open_token = TT_ImageText;
                    text_start = next_char;
                } else if (CONSUME_STRING("***\n")) {
                    open_token = TT_None;
                    CHECK(append_element(NULL, &page->children, ET_HRule, NULL, NULL, NULL, NULL));
                } else if (CONSUME_STRING("```\n")) {
                    open_token = TT_Preformatted;
                    text_start = next_char;
                } else {
                    open_token = TT_Paragraph;
                    CHECK(append_element(&open_block, &page->children, ET_Paragraph, NULL, NULL, NULL, NULL));
                    CHECK(parse_inlines(&open_block->children, &next_char, end_char));
                }
                break;
            case TT_ImageText:
                text_end = next_char;

                if (CONSUME_STRING("](")) {
                    open_token = TT_ImageURL;
                    url_start = next_char;
                } else {
                    ++ next_char;
                }
                break;
            case TT_ImageURL: {
                const char* url_end = next_char;

                if (CONSUME_STRING(")")) {
                    open_token = TT_None;
                    CHECK(append_element(&open_block, &page->children, ET_Image, text_start, text_end, url_start, url_end));
                } else {
                    ++ next_char;
                }
                break;
            }
            case TT_Paragraph:
                CHECK(parse_inlines(&open_block->children, &next_char, end_char));
                break;
            case TT_Preformatted:
                text_end = next_char;

                if (CONSUME_STRING("```\n")) {
                    open_token = TT_None;
                    CHECK(append_element(NULL, &page->children, ET_Preformatted, text_start, text_end - 1, NULL, NULL));
                } else {
                    ++ next_char;
                }
                break;
            }
        }

        escape_next_char = false;
    }

    FINALLY RETURN;
}

static Status parse_inlines(Element** elements_p, const char** next_char_p, const char* end_char) {
    TRY
    typedef enum {
        TT_None,
        TT_Bold,
        TT_LinkText,
        TT_LinkURL,
        TT_Text,
    } TokenType;

    TokenType open_token = TT_None;
    const char* next_char = *next_char_p;
    const char* token_start = next_char;
    const char* token_end = next_char;
    const char* text_start = NULL;
    const char* text_end = NULL;
    const char* url_start = NULL;
    bool escape_next_char = false;

    for (; (! AT_END()) && (! CONSUME_STRING("\n")); token_end = next_char) {
        if (CONSUME_STRING("\\")) {
            escape_next_char = true;
            continue;
        }

        switch (open_token) {
        case TT_None:
        case TT_Text: {
            bool is_link = CONSUME_STRING("[");
            bool is_bold = CONSUME_STRING("**");

            if (open_token && (is_link || is_bold)) {
                CHECK(append_element(NULL, elements_p, ET_Text, token_start, token_end, NULL, NULL));
                token_start = token_end;
            }

            if (is_link) {
                open_token = TT_LinkText;
                text_start = next_char;
            } else if (is_bold) {
                open_token = TT_Bold;
                text_start = next_char;
            } else {
                open_token = TT_Text;
                ++ next_char;
            }
            break;
        }
        case TT_Bold:
            if (CONSUME_STRING("**")) {
                CHECK(append_element(NULL, elements_p, ET_Bold, text_start, token_end, NULL, NULL));
                open_token = TT_None;
                token_start = next_char;
            } else {
                ++ next_char;
            }
            break;
        case TT_LinkText:
            if (CONSUME_STRING("](")) {
                open_token = TT_LinkURL;
                text_end = token_end;
                url_start = next_char;
            } else {
                ++ next_char;
            }
            break;
        case TT_LinkURL:
            if (CONSUME_STRING(")")) {
                CHECK(append_element(NULL, elements_p, ET_Link, text_start, text_end, url_start, token_end));
                open_token = TT_None;
                token_start = next_char;
            } else {
                ++ next_char;
            }
            break;
        }

        escape_next_char = false;
    }

    if (open_token) {
        CHECK(append_element(NULL, elements_p, ET_Text, token_start, token_end, NULL, NULL));
    }

    *next_char_p = next_char;

    FINALLY RETURN;
}

static Status append_element(Element** new_element_p, Element** elements_p, ElementType type,
    const char* text_start, const char* text_end, const char* url_start, const char* url_end)
{
    TRY
    CHECK(vector_append(elements_p, 1, NULL));
    Element* element = &(*elements_p)[vector_length(*elements_p) - 1];

    CHECK(vector_new(&element->children, sizeof(Element), 0));
    element->type = type;

    if (text_start) {
        CHECK(string_clone_substr(&element->text, text_start, text_end - text_start));

        if (element->type != ET_Preformatted) {
            CHECK(escape_string(&element->text));
        }
    }

    if (url_start) {
        CHECK(string_clone_substr(&element->url, url_start, url_end - url_start));
        CHECK(escape_string(&element->url));
    }

    if (new_element_p) {
        *new_element_p = element;
    }

    FINALLY RETURN;
}

static Status escape_string(char** string_p) {
    TRY
    char* string = *string_p;
    size_t length = string_length(string);

    for (size_t index = 0; index < length; ) {
        switch (string[index]) {
        case '\\':
            CHECK(vector_remove(string_p, index, 1));
            break;
        case '<':
            CHECK(vector_remove(string_p, index, 1));
            CHECK(vector_insert(string_p, index, strlen(LT_ESCAPE), LT_ESCAPE));
            index += strlen(LT_ESCAPE);
            break;
        case '&':
            CHECK(vector_remove(string_p, index, 1));
            CHECK(vector_insert(string_p, index, strlen(AMP_ESCAPE), AMP_ESCAPE));
            index += strlen(AMP_ESCAPE);
            break;
        default:
            ++ index;
            continue;
        }

        string = *string_p;
        length = string_length(string);
    }

    FINALLY RETURN;
}
