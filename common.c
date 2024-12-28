#include "common.h"

#define ASSERT_FILE(EXPR) ASSERT(EXPR, "Error accessing file %s", path)

Status file_read(char** contents_p, const char* path) {
    TRY
    FILE* file = NULL;

    ASSERT_FILE(file = fopen(path, "r"));
    ASSERT_FILE(fseek(file, 0, SEEK_END) == 0);

    int file_size;
    ASSERT_FILE((file_size = ftell(file)) >= 0);
    CHECK(string_new(contents_p, file_size));

    rewind(file);
    ASSERT_FILE(fread(*contents_p, 1, file_size, file) == file_size);

    FINALLY
    if (file) {
        fclose(file);
    }

    RETURN;
}

Status file_write(const char* contents, const char* path) {
    TRY
    FILE* file = NULL;
    size_t file_size = strlen(contents);

    ASSERT_FILE(file = fopen(path, "w"));
    ASSERT_FILE(fwrite(contents, 1, file_size, file) == file_size);

    FINALLY
    if (file) {
        fclose(file);
    }

    RETURN;
}

Status string_append_indent(char** to_string_p, const char* suffix, uint indent) {
    TRY
    for (uint i = 0; i < indent; ++ i) {
        CHECK(string_append(to_string_p, "  "));
    }

    CHECK(string_append(to_string_p, suffix));

    FINALLY RETURN;
}
