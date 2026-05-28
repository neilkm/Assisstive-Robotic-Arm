#ifndef SOFTWARE_COMMON_TESTS_TEST_FORMAT_H
#define SOFTWARE_COMMON_TESTS_TEST_FORMAT_H

#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*common_test_char_writer_t)(char character, void *context);

static inline void common_test_write_char(common_test_char_writer_t writer, void *context, char character)
{
    if (writer != NULL) {
        writer(character, context);
    }
}

static inline void common_test_write_text(common_test_char_writer_t writer, void *context, const char *text)
{
    if (text == NULL) {
        return;
    }

    while (*text != '\0') {
        common_test_write_char(writer, context, *text);
        text++;
    }
}

static inline void common_test_write_newline(common_test_char_writer_t writer, void *context)
{
    common_test_write_text(writer, context, "\r\n");
}

static inline void common_test_write_separator(common_test_char_writer_t writer,
                                               void *context,
                                               char fill,
                                               int width)
{
    for (int i = 0; i < width; ++i) {
        common_test_write_char(writer, context, fill);
    }
    common_test_write_newline(writer, context);
}

static inline void common_test_write_centered_line(common_test_char_writer_t writer,
                                                   void *context,
                                                   const char *text,
                                                   int width)
{
    const int text_width = (text != NULL) ? (int)strlen(text) : 0;
    const int padding = (text_width < width) ? (width - text_width) / 2 : 0;

    common_test_write_char(writer, context, '|');
    for (int i = 1; i < padding; ++i) {
        common_test_write_char(writer, context, ' ');
    }

    common_test_write_text(writer, context, text);

    for (int i = padding + text_width; i < width - 1; ++i) {
        common_test_write_char(writer, context, ' ');
    }
    common_test_write_char(writer, context, '|');
    common_test_write_newline(writer, context);
}

static inline void common_test_write_harness_banner(common_test_char_writer_t writer,
                                                    void *context,
                                                    const char *harness_name)
{
    enum { common_test_banner_width = 72 };

    common_test_write_newline(writer, context);
    common_test_write_separator(writer, context, '=', common_test_banner_width);
    common_test_write_centered_line(writer, context, "TEST HARNESS", common_test_banner_width);
    common_test_write_centered_line(writer, context, harness_name, common_test_banner_width);
    common_test_write_separator(writer, context, '=', common_test_banner_width);
}

static inline void common_test_write_case_banner(common_test_char_writer_t writer,
                                                 void *context,
                                                 const char *test_case_name)
{
    enum { common_test_banner_width = 72 };

    common_test_write_newline(writer, context);
    common_test_write_separator(writer, context, '-', common_test_banner_width);
    common_test_write_centered_line(writer, context, "TEST CASE", common_test_banner_width);
    common_test_write_centered_line(writer, context, test_case_name, common_test_banner_width);
    common_test_write_separator(writer, context, '-', common_test_banner_width);
}

static inline void common_test_write_result(common_test_char_writer_t writer,
                                            void *context,
                                            const char *test_case_name,
                                            int passed)
{
    common_test_write_char(writer, context, '[');
    common_test_write_text(writer, context, passed ? "PASS" : "FAIL");
    common_test_write_text(writer, context, "] ");
    common_test_write_text(writer, context, test_case_name);
    common_test_write_newline(writer, context);
}

#ifdef __cplusplus
}
#endif

#endif
