/*
 * COPYRIGHT © 2026 Enable Software Pty Ltd - All Rights Reserved
 *
 * https://github.com/alpine9000/engine9000-public
 *
 * See COPYING for license details
 */

#include <ctype.h>
#include <stdint.h>
#include <string.h>

#include "syntax_highlight_asm.h"

static int
syntax_highlight_asm_isRegisterName(const char *text, int start, int length)
{
    if (!text || start < 0 || length <= 0) {
        return 0;
    }
    if (length == 2) {
        char a = (char)tolower((unsigned char)text[start]);
        char b = text[start + 1];
        if ((a == 'd' || a == 'a') && b >= '0' && b <= '7') {
            return 1;
        }
        if (a == 'f' && b >= '0' && b <= '7') {
            return 1;
        }
    }
    if (length == 2 &&
        tolower((unsigned char)text[start]) == 's' &&
        tolower((unsigned char)text[start + 1]) == 'p') {
        return 1;
    }
    if (length == 2 &&
        tolower((unsigned char)text[start]) == 'p' &&
        tolower((unsigned char)text[start + 1]) == 'c') {
        return 1;
    }
    if (length == 2 &&
        tolower((unsigned char)text[start]) == 's' &&
        tolower((unsigned char)text[start + 1]) == 'r') {
        return 1;
    }
    if (length == 3 &&
        tolower((unsigned char)text[start]) == 'u' &&
        tolower((unsigned char)text[start + 1]) == 's' &&
        tolower((unsigned char)text[start + 2]) == 'p') {
        return 1;
    }
    if (length == 3 &&
        tolower((unsigned char)text[start]) == 'c' &&
        tolower((unsigned char)text[start + 1]) == 'c' &&
        tolower((unsigned char)text[start + 2]) == 'r') {
        return 1;
    }
    return 0;
}

static int
syntax_highlight_asm_isIdentChar(char c)
{
    if (isalnum((unsigned char)c)) {
        return 1;
    }
    if (c == '_' || c == '.' || c == '$' || c == '@' || c == '?') {
        return 1;
    }
    return 0;
}

static int
syntax_highlight_asm_isHexChar(char c)
{
    if (isdigit((unsigned char)c)) {
        return 1;
    }
    c = (char)tolower((unsigned char)c);
    return c >= 'a' && c <= 'f';
}

static int
syntax_highlight_asm_parseNumber(const char *line, int cursor, int limit, int *outLength)
{
    if (!line || cursor < 0 || cursor >= limit || !outLength) {
        return 0;
    }
    int i = cursor;
    if (line[i] == '#') {
        ++i;
    }
    if (i >= limit) {
        return 0;
    }
    if (line[i] == '$') {
        int begin = i;
        ++i;
        while (i < limit && syntax_highlight_asm_isHexChar(line[i])) {
            ++i;
        }
        if (i > begin + 1) {
            *outLength = i - cursor;
            return 1;
        }
        return 0;
    }
    if (line[i] == '0' && i + 1 < limit &&
        (line[i + 1] == 'x' || line[i + 1] == 'X')) {
        int begin = i;
        i += 2;
        while (i < limit && syntax_highlight_asm_isHexChar(line[i])) {
            ++i;
        }
        if (i > begin + 2) {
            *outLength = i - cursor;
            return 1;
        }
        return 0;
    }
    if (line[i] == '%') {
        int begin = i;
        ++i;
        while (i < limit && (line[i] == '0' || line[i] == '1')) {
            ++i;
        }
        if (i > begin + 1) {
            *outLength = i - cursor;
            return 1;
        }
        return 0;
    }
    if (isdigit((unsigned char)line[i])) {
        ++i;
        while (i < limit && isdigit((unsigned char)line[i])) {
            ++i;
        }
        *outLength = i - cursor;
        return 1;
    }
    return 0;
}

static void
syntax_highlight_asm_highlightLine(const char *line,
                                   int lineLength,
                                   int lineIndex,
                                   syntax_highlight_asm_add_span_fn addSpanFn,
                                   void *addSpanUser)
{
    if (!line || lineLength <= 0 || lineIndex < 0 || !addSpanFn) {
        return;
    }

    int cursor = 0;
    while (cursor < lineLength && isspace((unsigned char)line[cursor])) {
        ++cursor;
    }
    if (cursor >= lineLength) {
        return;
    }

    if (line[cursor] == ';') {
        (void)addSpanFn(addSpanUser, lineIndex, cursor, lineLength - cursor, syntax_highlight_kind_comment);
        return;
    }

    int start = cursor;
    while (cursor < lineLength && syntax_highlight_asm_isIdentChar(line[cursor])) {
        ++cursor;
    }
    if (cursor > start && cursor < lineLength && line[cursor] == ':') {
        (void)addSpanFn(addSpanUser, lineIndex, start, (cursor + 1) - start, syntax_highlight_kind_function);
        ++cursor;
    } else {
        cursor = start;
    }

    int opcodeSeen = 0;
    while (cursor < lineLength) {
        while (cursor < lineLength && isspace((unsigned char)line[cursor])) {
            ++cursor;
        }
        if (cursor >= lineLength) {
            break;
        }
        if (line[cursor] == ';') {
            (void)addSpanFn(addSpanUser, lineIndex, cursor, lineLength - cursor, syntax_highlight_kind_comment);
            break;
        }
        if (line[cursor] == '"' || line[cursor] == '\'') {
            char quote = line[cursor];
            int begin = cursor;
            ++cursor;
            while (cursor < lineLength) {
                if (line[cursor] == '\\' && cursor + 1 < lineLength) {
                    cursor += 2;
                    continue;
                }
                if (line[cursor] == quote) {
                    ++cursor;
                    break;
                }
                ++cursor;
            }
            (void)addSpanFn(addSpanUser, lineIndex, begin, cursor - begin, syntax_highlight_kind_string);
            continue;
        }

        int numberLength = 0;
        if (syntax_highlight_asm_parseNumber(line, cursor, lineLength, &numberLength)) {
            (void)addSpanFn(addSpanUser, lineIndex, cursor, numberLength, syntax_highlight_kind_number);
            cursor += numberLength;
            continue;
        }

        if (line[cursor] == '.') {
            int begin = cursor;
            ++cursor;
            while (cursor < lineLength && syntax_highlight_asm_isIdentChar(line[cursor])) {
                ++cursor;
            }
            if (cursor > begin + 1) {
                (void)addSpanFn(addSpanUser, lineIndex, begin, cursor - begin, syntax_highlight_kind_preproc);
                continue;
            }
            continue;
        }

        if (syntax_highlight_asm_isIdentChar(line[cursor])) {
            int begin = cursor;
            ++cursor;
            while (cursor < lineLength && syntax_highlight_asm_isIdentChar(line[cursor])) {
                ++cursor;
            }
            int length = cursor - begin;
            if (syntax_highlight_asm_isRegisterName(line, begin, length)) {
                (void)addSpanFn(addSpanUser, lineIndex, begin, length, syntax_highlight_kind_type);
                continue;
            }
            if (!opcodeSeen) {
                (void)addSpanFn(addSpanUser, lineIndex, begin, length, syntax_highlight_kind_keyword);
                opcodeSeen = 1;
            }
            continue;
        }

        ++cursor;
    }
}

int
syntax_highlight_asm_buildLineSpans(const char *line,
                                    int lineLength,
                                    int lineIndex,
                                    syntax_highlight_asm_add_span_fn addSpanFn,
                                    void *addSpanUser)
{
    if (!line || lineLength <= 0 || lineIndex < 0 || !addSpanFn) {
        return 0;
    }
    syntax_highlight_asm_highlightLine(line, lineLength, lineIndex, addSpanFn, addSpanUser);
    return 1;
}

int
syntax_highlight_asm_buildSpans(const char *text,
                                size_t textLength,
                                const size_t *lineStarts,
                                int lineCount,
                                syntax_highlight_asm_add_span_fn addSpanFn,
                                void *addSpanUser)
{
    if (!text || !lineStarts || lineCount <= 0 || !addSpanFn) {
        return 0;
    }

    for (int i = 0; i < lineCount; ++i) {
        size_t lineStart = lineStarts[i];
        size_t lineEnd = textLength;
        if (i + 1 < lineCount) {
            lineEnd = lineStarts[i + 1];
            if (lineEnd > lineStart && text[lineEnd - 1] == '\n') {
                --lineEnd;
            }
        }
        if (lineEnd > lineStart && text[lineEnd - 1] == '\r') {
            --lineEnd;
        }
        if (lineEnd <= lineStart) {
            continue;
        }
        int lineLength = (int)(lineEnd - lineStart);
        (void)syntax_highlight_asm_buildLineSpans(text + lineStart, lineLength, i, addSpanFn, addSpanUser);
    }
    return 1;
}
