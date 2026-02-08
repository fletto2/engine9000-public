/*
 * COPYRIGHT © 2026 Enable Software Pty Ltd - All Rights Reserved
 *
 * https://github.com/alpine9000/engine9000-public
 *
 * See COPYING for license details
 */


#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include "debugger.h"

#include "file.h"

int
file_getExeDir(char *out, size_t cap)
{
    return debugger_platform_getExeDir(out, cap);
}

int
file_getAssetPath(const char *rel, char *out, size_t cap)
{
    if (!rel || !*rel || !out || cap == 0) return 0;
    char base[PATH_MAX];
    if (!file_getExeDir(base, sizeof(base))) return 0;
    size_t n = strlen(base);
    if (n >= cap) n = cap-1;
    memcpy(out, base, n);
    if (n > 0 && out[n-1] != '/' && n < cap-1) out[n++] = '/';
    size_t rl = strlen(rel);
    if (n + rl >= cap) rl = cap - 1 - n;
    memcpy(out + n, rel, rl);
    out[n+rl] = '\0';
    return 1;
}

static int
file_isExecutableFile(const char *p)
{
    return debugger_platform_isExecutableFile(p);
}

int
file_findInPath(const char *prog, char *out, size_t cap)
{
    if (!prog || !*prog || !out || cap == 0) return 0;
    const char path_sep = debugger_platform_pathListSeparator();
    // If prog contains a path separator, check directly
    if (strchr(prog, '/') || strchr(prog, '\\')) {
        if (file_isExecutableFile(prog)) {
            size_t l = strlen(prog); if (l >= cap) l = cap - 1; memcpy(out, prog, l); out[l] = '\0';
            return 1;
        }
        return 0;
    }
    const char *path = getenv("PATH");
    if (!path || !*path) return 0;
    const char *p = path; const char *seg = p;
    char buf[PATH_MAX];
    while (*p) {
        if (*p == path_sep) {
            size_t sl = (size_t)(p - seg);
            if (sl == 0) {
                // Empty segment means current directory
                if (snprintf(buf, sizeof(buf), "%s", prog) < (int)sizeof(buf) && file_isExecutableFile(buf)) {
                    size_t l = strlen(buf); if (l >= cap) l = cap - 1; memcpy(out, buf, l); out[l] = '\0'; return 1;
                }
            } else {
                if (sl >= sizeof(buf)) sl = sizeof(buf)-1;
                memcpy(buf, seg, sl); buf[sl] = '\0';
                size_t bl = strlen(buf);
                if (bl + 1 + strlen(prog) < sizeof(buf)) {
                    if (bl > 0 && buf[bl-1] != '/' && buf[bl-1] != '\\') { buf[bl++] = '/'; buf[bl] = '\0'; }
                    strncat(buf, prog, sizeof(buf) - bl - 1);
                    if (file_isExecutableFile(buf)) { size_t l = strlen(buf); if (l >= cap) l = cap - 1; memcpy(out, buf, l); out[l] = '\0'; return 1; }
                }
            }
            seg = p + 1;
        }
        p++;
    }
    // Last segment
    if (seg) {
        if (*seg == '\0') {
            if (file_isExecutableFile(prog)) { size_t l = strlen(prog); if (l >= cap) l = cap - 1; memcpy(out, prog, l); out[l] = '\0'; return 1; }
        } else {
            size_t sl = (size_t)(p - seg);
            if (sl >= sizeof(buf)) sl = sizeof(buf)-1;
            memcpy(buf, seg, sl); buf[sl] = '\0';
            size_t bl = strlen(buf);
            if (bl + 1 + strlen(prog) < sizeof(buf)) {
                if (bl > 0 && buf[bl-1] != '/') { buf[bl++] = '/'; buf[bl] = '\0'; }
                strncat(buf, prog, sizeof(buf) - bl - 1);
                if (file_isExecutableFile(buf)) { size_t l = strlen(buf); if (l >= cap) l = cap - 1; memcpy(out, buf, l); out[l] = '\0'; return 1; }
            }
        }
    }
    return 0;
}
