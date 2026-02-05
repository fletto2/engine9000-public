/*
 * COPYRIGHT © 2026 Enable Software Pty Ltd - All Rights Reserved
 *
 * https://github.com/alpine9000/engine9000-public
 *
 * See COPYING for license details
 */

#pragma once

typedef enum syntax_highlight_kind {
    syntax_highlight_kind_normal = 0,
    syntax_highlight_kind_keyword = 1,
    syntax_highlight_kind_type = 2,
    syntax_highlight_kind_string = 3,
    syntax_highlight_kind_comment = 4,
    syntax_highlight_kind_number = 5,
    syntax_highlight_kind_preproc = 6,
    syntax_highlight_kind_function = 7
} syntax_highlight_kind_t;

typedef struct syntax_highlight_span {
    int startColumn;
    int length;
    syntax_highlight_kind_t kind;
} syntax_highlight_span_t;

int
syntax_highlight_getLineSpans(const char *path,
                              int lineNumber,
                              const syntax_highlight_span_t **outSpans,
                              int *outSpanCount);

int
syntax_highlight_getHoverExpr(const char *path,
                              int lineNumber,
                              int column,
                              char *outExpr,
                              int outCap);

void
syntax_highlight_shutdown(void);
