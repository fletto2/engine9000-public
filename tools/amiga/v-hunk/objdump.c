/*
 * COPYRIGHT © 2026 Enable Software Pty Ltd - All Rights Reserved
 *
 * https://github.com/alpine9000/engine9000-public
 *
 * See COPYING for license details
 */


#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "doshunks.h"

#define OBJDUMP_VERSION 0
#define OBJDUMP_REVISION 1

#define OBJDUMP_HUNK_TYPE_MASK 0x0000ffffu
#define OBJDUMP_HUNK_SIZE_MASK 0x3fffffffu
#define OBJDUMP_HUNKF_ADVISORY (1u << 29)

typedef struct objdump_symbol {
    char *name;
    uint32_t offset;
} objdump_symbol_t;

typedef struct objdump_section {
    int index;
    uint32_t hunkType;
    uint32_t sizeBytes;
    uint32_t fileSizeBytes;
    uint32_t fileOffset;
    int hasFileOffset;
    char *hunkName;
    objdump_symbol_t *symbols;
    int symbolCount;
    int symbolCap;
} objdump_section_t;

typedef struct objdump_program {
    objdump_section_t *sections;
    int sectionCount;
    int sectionCap;
} objdump_program_t;

typedef struct objdump_options {
    const char *inputPath;
    int showHeaders;
    int showSymbols;
    int showDisassembly;
} objdump_options_t;

static void
objdump_printUsage(const char *argv0)
{
    printf("Usage: %s [option(s)] <file>\n", argv0 ? argv0 : "v-hunk-objdump");
    printf("Options:\n");
    printf("  -h, --section-headers   Display section headers\n");
    printf("  -t, --syms              Display the symbol table\n");
    printf("  -d                      Display a minimal disassembly view\n");
    printf("  -e <file>               Input file (objdump compatibility)\n");
    printf("  -v, --version           Display version information\n");
    printf("      --help              Display this help\n");
}

static void
objdump_printVersion(const char *argv0)
{
    printf("%s %d.%d Amiga hunk-format\n",
           argv0 ? argv0 : "v-hunk-objdump",
           OBJDUMP_VERSION,
           OBJDUMP_REVISION);
}

static int
objdump_strDuplicate(const char *src, char **outCopy)
{
    if (outCopy) {
        *outCopy = NULL;
    }
    if (!src || !outCopy) {
        return 0;
    }
    size_t len = strlen(src);
    char *copy = (char *)malloc(len + 1u);
    if (!copy) {
        return 0;
    }
    memcpy(copy, src, len + 1u);
    *outCopy = copy;
    return 1;
}

static int
objdump_readU32be(FILE *file, uint32_t *outValue)
{
    uint8_t bytes[4];
    if (!file || !outValue) {
        return 0;
    }
    if (fread(bytes, 1, sizeof(bytes), file) != sizeof(bytes)) {
        return 0;
    }
    *outValue = ((uint32_t)bytes[0] << 24) |
                ((uint32_t)bytes[1] << 16) |
                ((uint32_t)bytes[2] << 8) |
                (uint32_t)bytes[3];
    return 1;
}

static int
objdump_readU16be(FILE *file, uint16_t *outValue)
{
    uint8_t bytes[2];
    if (!file || !outValue) {
        return 0;
    }
    if (fread(bytes, 1, sizeof(bytes), file) != sizeof(bytes)) {
        return 0;
    }
    *outValue = (uint16_t)(((uint16_t)bytes[0] << 8) | (uint16_t)bytes[1]);
    return 1;
}

static int
objdump_skipBytes(FILE *file, uint32_t count)
{
    if (!file) {
        return 0;
    }
    if (count == 0u) {
        return 1;
    }
    if (fseek(file, (long)count, SEEK_CUR) == 0) {
        return 1;
    }
    return 0;
}

static int
objdump_align32(FILE *file)
{
    if (!file) {
        return 0;
    }
    long offset = ftell(file);
    if (offset < 0) {
        return 0;
    }
    long misalign = offset & 3l;
    if (misalign == 0l) {
        return 1;
    }
    long pad = 4l - misalign;
    if (fseek(file, pad, SEEK_CUR) != 0) {
        return 0;
    }
    return 1;
}

static int
objdump_skipLongs(FILE *file, uint32_t count)
{
    if (count > (UINT32_MAX / 4u)) {
        return 0;
    }
    return objdump_skipBytes(file, count * 4u);
}

static char *
objdump_readLongString(FILE *file, uint32_t longCount)
{
    if (!file) {
        return NULL;
    }
    if (longCount == 0u) {
        char *empty = (char *)malloc(1u);
        if (empty) {
            empty[0] = '\0';
        }
        return empty;
    }
    if (longCount > (UINT32_MAX / 4u)) {
        return NULL;
    }
    uint32_t byteCount = longCount * 4u;
    char *buffer = (char *)malloc((size_t)byteCount + 1u);
    if (!buffer) {
        return NULL;
    }
    if (fread(buffer, 1, byteCount, file) != byteCount) {
        free(buffer);
        return NULL;
    }
    buffer[byteCount] = '\0';
    for (uint32_t i = 0; i < byteCount; ++i) {
        if (buffer[i] == '\0') {
            break;
        }
        if ((unsigned char)buffer[i] < 0x20u) {
            buffer[i] = '?';
        }
    }
    return buffer;
}

static void
objdump_programFree(objdump_program_t *program)
{
    if (!program) {
        return;
    }
    for (int i = 0; i < program->sectionCount; ++i) {
        objdump_section_t *section = &program->sections[i];
        free(section->hunkName);
        section->hunkName = NULL;
        for (int s = 0; s < section->symbolCount; ++s) {
            free(section->symbols[s].name);
            section->symbols[s].name = NULL;
        }
        free(section->symbols);
        section->symbols = NULL;
        section->symbolCount = 0;
        section->symbolCap = 0;
    }
    free(program->sections);
    program->sections = NULL;
    program->sectionCount = 0;
    program->sectionCap = 0;
}

static int
objdump_programEnsureSections(objdump_program_t *program, int minCap)
{
    if (!program) {
        return 0;
    }
    if (program->sectionCap >= minCap) {
        return 1;
    }
    int nextCap = (program->sectionCap > 0) ? program->sectionCap : 8;
    while (nextCap < minCap) {
        nextCap *= 2;
    }
    objdump_section_t *nextSections = (objdump_section_t *)realloc(
        program->sections, (size_t)nextCap * sizeof(*nextSections));
    if (!nextSections) {
        return 0;
    }
    for (int i = program->sectionCap; i < nextCap; ++i) {
        memset(&nextSections[i], 0, sizeof(nextSections[i]));
        nextSections[i].index = i;
    }
    program->sections = nextSections;
    program->sectionCap = nextCap;
    return 1;
}

static objdump_section_t *
objdump_programAddSection(objdump_program_t *program)
{
    if (!program) {
        return NULL;
    }
    if (!objdump_programEnsureSections(program, program->sectionCount + 1)) {
        return NULL;
    }
    objdump_section_t *section = &program->sections[program->sectionCount];
    memset(section, 0, sizeof(*section));
    section->index = program->sectionCount;
    section->hunkType = 0u;
    program->sectionCount++;
    return section;
}

static objdump_section_t *
objdump_programSectionAt(objdump_program_t *program, int index)
{
    if (!program || index < 0 || index >= program->sectionCount) {
        return NULL;
    }
    return &program->sections[index];
}

static int
objdump_sectionEnsureSymbols(objdump_section_t *section, int minCap)
{
    if (!section) {
        return 0;
    }
    if (section->symbolCap >= minCap) {
        return 1;
    }
    int nextCap = (section->symbolCap > 0) ? section->symbolCap : 16;
    while (nextCap < minCap) {
        nextCap *= 2;
    }
    objdump_symbol_t *nextSymbols = (objdump_symbol_t *)realloc(
        section->symbols, (size_t)nextCap * sizeof(*nextSymbols));
    if (!nextSymbols) {
        return 0;
    }
    for (int i = section->symbolCap; i < nextCap; ++i) {
        memset(&nextSymbols[i], 0, sizeof(nextSymbols[i]));
    }
    section->symbols = nextSymbols;
    section->symbolCap = nextCap;
    return 1;
}

static int
objdump_sectionAddSymbol(objdump_section_t *section, const char *name, uint32_t offset)
{
    if (!section || !name || !name[0]) {
        return 1;
    }
    if (!objdump_sectionEnsureSymbols(section, section->symbolCount + 1)) {
        return 0;
    }
    objdump_symbol_t *symbol = &section->symbols[section->symbolCount++];
    if (!objdump_strDuplicate(name, &symbol->name)) {
        section->symbolCount--;
        return 0;
    }
    symbol->offset = offset;
    return 1;
}

static int
objdump_skipRelocLong(FILE *file)
{
    for (;;) {
        uint32_t count = 0;
        uint32_t target = 0;
        if (!objdump_readU32be(file, &count)) {
            return 0;
        }
        if (count == 0u) {
            break;
        }
        if (!objdump_readU32be(file, &target)) {
            return 0;
        }
        (void)target;
        if (!objdump_skipLongs(file, count)) {
            return 0;
        }
    }
    return 1;
}

static int
objdump_skipRelocShort(FILE *file)
{
    for (;;) {
        uint16_t count = 0;
        if (!objdump_readU16be(file, &count)) {
            return 0;
        }
        if (count == 0u) {
            break;
        }
        uint32_t bytes = ((uint32_t)count + 1u) * 2u;
        if (!objdump_skipBytes(file, bytes)) {
            return 0;
        }
    }
    return objdump_align32(file);
}

static int
objdump_skipExt(FILE *file)
{
    for (;;) {
        uint32_t extWord = 0;
        if (!objdump_readU32be(file, &extWord)) {
            return 0;
        }
        if (extWord == 0u) {
            break;
        }
        uint32_t kind = (extWord >> 24) & 0xffu;
        uint32_t nameLongs = extWord & 0x00ffffffu;
        if (!objdump_skipLongs(file, nameLongs)) {
            return 0;
        }

        if (kind == 1u || kind == 2u || kind == 3u) {
            if (!objdump_skipLongs(file, 1u)) {
                return 0;
            }
        } else {
            uint32_t refCount = 0;
            if (!objdump_readU32be(file, &refCount)) {
                return 0;
            }
            for (uint32_t i = 0; i < refCount; ++i) {
                uint32_t hunk = 0;
                uint32_t offsCount = 0;
                if (!objdump_readU32be(file, &hunk)) {
                    return 0;
                }
                if (!objdump_readU32be(file, &offsCount)) {
                    return 0;
                }
                (void)hunk;
                if (!objdump_skipLongs(file, offsCount)) {
                    return 0;
                }
            }
        }
    }
    return 1;
}

static int
objdump_setSectionTypeAndSize(objdump_section_t *section, uint32_t hunkType, uint32_t sizeWord)
{
    if (!section) {
        return 0;
    }
    uint32_t sizeBytes = (sizeWord & OBJDUMP_HUNK_SIZE_MASK) * 4u;
    section->hunkType = hunkType;
    if (section->sizeBytes == 0u || sizeBytes > section->sizeBytes) {
        section->sizeBytes = sizeBytes;
    }
    if (hunkType != HUNK_BSS) {
        section->fileSizeBytes = sizeBytes;
    }
    return 1;
}

static int
objdump_parseExecutable(FILE *file, objdump_program_t *program)
{
    uint32_t tableSize = 0;
    uint32_t firstHunk = 0;
    uint32_t lastHunk = 0;
    uint32_t nameCount = 0;

    if (!file || !program) {
        return 0;
    }

    for (;;) {
        if (!objdump_readU32be(file, &nameCount)) {
            return 0;
        }
        if (nameCount == 0u) {
            break;
        }
        if (!objdump_skipLongs(file, nameCount)) {
            return 0;
        }
    }

    if (!objdump_readU32be(file, &tableSize)) {
        return 0;
    }
    if (!objdump_readU32be(file, &firstHunk)) {
        return 0;
    }
    if (!objdump_readU32be(file, &lastHunk)) {
        return 0;
    }
    if (lastHunk < firstHunk) {
        return 0;
    }
    uint32_t segCount = (lastHunk - firstHunk) + 1u;
    if (tableSize < segCount) {
        return 0;
    }

    for (uint32_t i = 0; i < segCount; ++i) {
        if (!objdump_programAddSection(program)) {
            return 0;
        }
    }

    for (uint32_t i = 0; i < tableSize; ++i) {
        uint32_t sizeWord = 0;
        if (!objdump_readU32be(file, &sizeWord)) {
            return 0;
        }
        if (i < segCount) {
            objdump_section_t *section = objdump_programSectionAt(program, (int)i);
            if (!section) {
                return 0;
            }
            section->sizeBytes = (sizeWord & OBJDUMP_HUNK_SIZE_MASK) * 4u;
        }
    }

    for (uint32_t sectionIndex = 0; sectionIndex < segCount; ++sectionIndex) {
        objdump_section_t *section = objdump_programSectionAt(program, (int)sectionIndex);
        if (!section) {
            return 0;
        }

        for (;;) {
            uint32_t hunkWord = 0;
            if (!objdump_readU32be(file, &hunkWord)) {
                return 0;
            }
            uint32_t hunkType = hunkWord & OBJDUMP_HUNK_TYPE_MASK;

            if (hunkType == HUNK_END) {
                break;
            }

            if (hunkType == HUNK_NAME) {
                uint32_t stringLongs = 0;
                if (!objdump_readU32be(file, &stringLongs)) {
                    return 0;
                }
                free(section->hunkName);
                section->hunkName = objdump_readLongString(file, stringLongs);
                if (!section->hunkName) {
                    return 0;
                }
                continue;
            }

            if (hunkType == HUNK_CODE || hunkType == HUNK_DATA || hunkType == HUNK_BSS) {
                uint32_t sizeWord = 0;
                if (!objdump_readU32be(file, &sizeWord)) {
                    return 0;
                }
                if (!objdump_setSectionTypeAndSize(section, hunkType, sizeWord)) {
                    return 0;
                }
                if (hunkType != HUNK_BSS) {
                    long payloadOffset = ftell(file);
                    if (payloadOffset >= 0) {
                        section->fileOffset = (uint32_t)payloadOffset;
                        section->hasFileOffset = 1;
                    }
                    if (!objdump_skipBytes(file, section->fileSizeBytes)) {
                        return 0;
                    }
                }
                continue;
            }

            if (hunkType == HUNK_RELOC32 || hunkType == HUNK_RELOC16 || hunkType == HUNK_RELOC8) {
                if (!objdump_skipRelocLong(file)) {
                    return 0;
                }
                continue;
            }

            if (hunkType == HUNK_RELOC32SHORT || hunkType == HUNK_RELRELOC32 || hunkType == HUNK_DREL32) {
                if (!objdump_skipRelocShort(file)) {
                    return 0;
                }
                continue;
            }

            if (hunkType == HUNK_EXT) {
                if (!objdump_skipExt(file)) {
                    return 0;
                }
                continue;
            }

            if (hunkType == HUNK_SYMBOL) {
                for (;;) {
                    uint32_t nameLongs = 0;
                    uint32_t offset = 0;
                    if (!objdump_readU32be(file, &nameLongs)) {
                        return 0;
                    }
                    if (nameLongs == 0u) {
                        break;
                    }
                    char *symbolName = objdump_readLongString(file, nameLongs);
                    if (!symbolName) {
                        return 0;
                    }
                    if (!objdump_readU32be(file, &offset)) {
                        free(symbolName);
                        return 0;
                    }
                    if (!objdump_sectionAddSymbol(section, symbolName, offset)) {
                        free(symbolName);
                        return 0;
                    }
                    free(symbolName);
                }
                continue;
            }

            if (hunkType == HUNK_DEBUG) {
                uint32_t debugLongs = 0;
                if (!objdump_readU32be(file, &debugLongs)) {
                    return 0;
                }
                if (!objdump_skipLongs(file, debugLongs)) {
                    return 0;
                }
                continue;
            }

            if ((hunkWord & OBJDUMP_HUNKF_ADVISORY) != 0u) {
                uint32_t advisoryLongs = 0;
                if (!objdump_readU32be(file, &advisoryLongs)) {
                    return 0;
                }
                if (!objdump_skipLongs(file, advisoryLongs)) {
                    return 0;
                }
                continue;
            }

            return 0;
        }
    }

    return 1;
}

static int
objdump_parseObject(FILE *file, objdump_program_t *program)
{
    if (!file || !program) {
        return 0;
    }

    uint32_t unitNameLongs = 0;
    if (!objdump_readU32be(file, &unitNameLongs)) {
        return 0;
    }
    if (!objdump_skipLongs(file, unitNameLongs)) {
        return 0;
    }

    for (;;) {
        long mark = ftell(file);
        uint32_t testWord = 0;
        if (!objdump_readU32be(file, &testWord)) {
            break;
        }
        if (fseek(file, mark, SEEK_SET) != 0) {
            return 0;
        }

        objdump_section_t *section = objdump_programAddSection(program);
        if (!section) {
            return 0;
        }

        for (;;) {
            uint32_t hunkWord = 0;
            if (!objdump_readU32be(file, &hunkWord)) {
                return 0;
            }
            uint32_t hunkType = hunkWord & ~HUNKF_MEMTYPE;

            if (hunkType == HUNK_END) {
                break;
            }

            if (hunkType == HUNK_NAME) {
                uint32_t stringLongs = 0;
                if (!objdump_readU32be(file, &stringLongs)) {
                    return 0;
                }
                free(section->hunkName);
                section->hunkName = objdump_readLongString(file, stringLongs);
                if (!section->hunkName) {
                    return 0;
                }
                continue;
            }

            if (hunkType == HUNK_CODE || hunkType == HUNK_PPC_CODE || hunkType == HUNK_DATA || hunkType == HUNK_BSS) {
                uint32_t sizeWord = 0;
                if (!objdump_readU32be(file, &sizeWord)) {
                    return 0;
                }
                uint32_t mappedType = hunkType;
                if (mappedType == HUNK_PPC_CODE) {
                    mappedType = HUNK_CODE;
                }
                if (!objdump_setSectionTypeAndSize(section, mappedType, sizeWord)) {
                    return 0;
                }
                if (mappedType != HUNK_BSS) {
                    long payloadOffset = ftell(file);
                    if (payloadOffset >= 0) {
                        section->fileOffset = (uint32_t)payloadOffset;
                        section->hasFileOffset = 1;
                    }
                    if (!objdump_skipBytes(file, section->fileSizeBytes)) {
                        return 0;
                    }
                }
                continue;
            }

            if (hunkType == HUNK_ABSRELOC32 || hunkType == HUNK_ABSRELOC16 ||
                hunkType == HUNK_RELRELOC32 || hunkType == HUNK_RELRELOC26 ||
                hunkType == HUNK_RELRELOC16 || hunkType == HUNK_RELRELOC8 ||
                hunkType == HUNK_DREL32 || hunkType == HUNK_DREL16 || hunkType == HUNK_DREL8) {
                if (!objdump_skipRelocLong(file)) {
                    return 0;
                }
                continue;
            }

            if (hunkType == HUNK_RELOC32SHORT) {
                if (!objdump_skipRelocShort(file)) {
                    return 0;
                }
                continue;
            }

            if (hunkType == HUNK_EXT || hunkType == HUNK_SYMBOL) {
                for (;;) {
                    uint32_t extWord = 0;
                    if (!objdump_readU32be(file, &extWord)) {
                        return 0;
                    }
                    if (extWord == 0u) {
                        break;
                    }
                    uint8_t extType = (uint8_t)((extWord >> 24) & 0xffu);
                    uint32_t nameLongs = extWord & 0x00ffffffu;
                    char *symbolName = objdump_readLongString(file, nameLongs);
                    if (!symbolName) {
                        return 0;
                    }

                    if ((extType & 0x80u) != 0u) {
                        if (extType == EXT_ABSCOMMON || extType == EXT_RELCOMMON) {
                            if (!objdump_skipLongs(file, 1u)) {
                                free(symbolName);
                                return 0;
                            }
                        }
                        uint32_t nRefs = 0;
                        if (!objdump_readU32be(file, &nRefs)) {
                            free(symbolName);
                            return 0;
                        }
                        if (!objdump_skipLongs(file, nRefs)) {
                            free(symbolName);
                            return 0;
                        }
                    } else if (extType == EXT_SYMB || extType == EXT_DEF) {
                        uint32_t value = 0;
                        if (!objdump_readU32be(file, &value)) {
                            free(symbolName);
                            return 0;
                        }
                        if (!objdump_sectionAddSymbol(section, symbolName, value)) {
                            free(symbolName);
                            return 0;
                        }
                    } else if (extType == EXT_ABS) {
                        if (!objdump_skipLongs(file, 1u)) {
                            free(symbolName);
                            return 0;
                        }
                    } else {
                        free(symbolName);
                        return 0;
                    }
                    free(symbolName);
                }
                continue;
            }

            if (hunkType == HUNK_DEBUG) {
                uint32_t debugLongs = 0;
                if (!objdump_readU32be(file, &debugLongs)) {
                    return 0;
                }
                if (!objdump_skipLongs(file, debugLongs)) {
                    return 0;
                }
                continue;
            }

            return 0;
        }
    }

    return 1;
}

static int
objdump_parseInputFile(const char *path, objdump_program_t *program)
{
    FILE *file = NULL;
    uint32_t firstWord = 0;
    int ok = 0;

    if (!path || !program) {
        return 0;
    }

    file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "cannot open input file \"%s\" for reading\n", path);
        return 0;
    }

    if (!objdump_readU32be(file, &firstWord)) {
        fclose(file);
        return 0;
    }

    if (firstWord == HUNK_HEADER) {
        ok = objdump_parseExecutable(file, program);
    } else if (firstWord == HUNK_UNIT) {
        ok = objdump_parseObject(file, program);
    } else {
        fprintf(stderr, "\"%s\" is not a hunk-format file\n", path);
        ok = 0;
    }

    fclose(file);
    return ok;
}

static const char *
objdump_baseSectionName(uint32_t hunkType)
{
    if (hunkType == HUNK_CODE) {
        return ".text";
    }
    if (hunkType == HUNK_DATA) {
        return ".data";
    }
    if (hunkType == HUNK_BSS) {
        return ".bss";
    }
    return ".sec";
}

static void
objdump_makeSectionToken(const objdump_section_t *section, char *outName, size_t outCap)
{
    if (!outName || outCap == 0u) {
        return;
    }
    outName[0] = '\0';
    if (!section) {
        return;
    }
    const char *baseName = objdump_baseSectionName(section->hunkType);
    snprintf(outName, outCap, "%s.%d", baseName, section->index);
}

static int
objdump_symbolCmpByOffsetThenName(const void *a, const void *b)
{
    const objdump_symbol_t *symA = (const objdump_symbol_t *)a;
    const objdump_symbol_t *symB = (const objdump_symbol_t *)b;
    if (symA->offset < symB->offset) {
        return -1;
    }
    if (symA->offset > symB->offset) {
        return 1;
    }
    if (!symA->name && !symB->name) {
        return 0;
    }
    if (!symA->name) {
        return -1;
    }
    if (!symB->name) {
        return 1;
    }
    return strcmp(symA->name, symB->name);
}

static void
objdump_sortSectionSymbols(objdump_program_t *program)
{
    if (!program) {
        return;
    }
    for (int i = 0; i < program->sectionCount; ++i) {
        objdump_section_t *section = &program->sections[i];
        if (section->symbolCount <= 1) {
            continue;
        }
        qsort(section->symbols,
              (size_t)section->symbolCount,
              sizeof(section->symbols[0]),
              objdump_symbolCmpByOffsetThenName);
    }
}

static void
objdump_printFileBanner(const char *path)
{
    printf("\n%s:     file format amiga\n\n", path ? path : "<unknown>");
}

static void
objdump_printHeaders(const objdump_program_t *program)
{
    if (!program) {
        return;
    }
    printf("Sections:\n");
    printf("Idx Name          Size      VMA       LMA       File off  Algn\n");
    for (int i = 0; i < program->sectionCount; ++i) {
        const objdump_section_t *section = &program->sections[i];
        char sectionName[64];
        objdump_makeSectionToken(section, sectionName, sizeof(sectionName));
        uint32_t fileOffset = section->hasFileOffset ? section->fileOffset : 0u;
        printf("%3d %-13s %08x  %08x  %08x  %08x  2**2\n",
               section->index,
               sectionName,
               section->sizeBytes,
               0u,
               0u,
               fileOffset);
        if (section->hunkType == HUNK_CODE) {
            printf("                  CONTENTS, ALLOC, LOAD, RELOC, CODE\n");
        } else if (section->hunkType == HUNK_DATA) {
            printf("                  CONTENTS, ALLOC, LOAD, DATA\n");
        } else if (section->hunkType == HUNK_BSS) {
            printf("                  ALLOC\n");
        } else {
            printf("                  CONTENTS, ALLOC\n");
        }
    }
}

static void
objdump_printSymbols(const objdump_program_t *program)
{
    if (!program) {
        return;
    }
    printf("SYMBOL TABLE:\n");
    for (int i = 0; i < program->sectionCount; ++i) {
        const objdump_section_t *section = &program->sections[i];
        char sectionName[64];
        objdump_makeSectionToken(section, sectionName, sizeof(sectionName));
        for (int s = 0; s < section->symbolCount; ++s) {
            const objdump_symbol_t *symbol = &section->symbols[s];
            if (!symbol->name || !symbol->name[0]) {
                continue;
            }
            printf("%08x g       %-10s 0000 %02d %s\n",
                   symbol->offset,
                   sectionName,
                   section->index & 0xff,
                   symbol->name);
        }
    }
}

static void
objdump_printDisassembly(const objdump_program_t *program)
{
    if (!program) {
        return;
    }
    for (int i = 0; i < program->sectionCount; ++i) {
        const objdump_section_t *section = &program->sections[i];
        if (section->hunkType != HUNK_CODE) {
            continue;
        }
        char sectionName[64];
        objdump_makeSectionToken(section, sectionName, sizeof(sectionName));
        printf("\nDisassembly of section %s:\n\n", sectionName);
        for (int s = 0; s < section->symbolCount; ++s) {
            const objdump_symbol_t *symbol = &section->symbols[s];
            if (!symbol->name || !symbol->name[0]) {
                continue;
            }
            printf("%08x:\t; %s\n", symbol->offset, symbol->name);
        }
    }
}

static int
objdump_parseArgs(int argc, char **argv, objdump_options_t *outOptions)
{
    objdump_options_t options;
    memset(&options, 0, sizeof(options));

    if (!outOptions) {
        return 0;
    }

    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (!arg || !arg[0]) {
            continue;
        }

        if (strcmp(arg, "--help") == 0) {
            objdump_printUsage(argv[0]);
            return 0;
        }
        if (strcmp(arg, "-v") == 0 || strcmp(arg, "--version") == 0) {
            objdump_printVersion(argv[0]);
            return 0;
        }
        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--section-headers") == 0) {
            options.showHeaders = 1;
            continue;
        }
        if (strcmp(arg, "-t") == 0 || strcmp(arg, "--syms") == 0) {
            options.showSymbols = 1;
            continue;
        }
        if (strcmp(arg, "-d") == 0) {
            options.showDisassembly = 1;
            continue;
        }
        if (strcmp(arg, "-e") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "-e: missing file argument\n");
                return -1;
            }
            options.inputPath = argv[++i];
            continue;
        }
        if (arg[0] == '-') {
            continue;
        }
        options.inputPath = arg;
    }

    if (!options.inputPath || !options.inputPath[0]) {
        fprintf(stderr, "missing input file\n");
        return -1;
    }
    if (!options.showHeaders && !options.showSymbols && !options.showDisassembly) {
        options.showHeaders = 1;
    }

    *outOptions = options;
    return 1;
}

int
main(int argc, char **argv)
{
    objdump_options_t options;
    int parseResult = objdump_parseArgs(argc, argv, &options);
    if (parseResult <= 0) {
        return (parseResult == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    objdump_program_t program;
    memset(&program, 0, sizeof(program));
    if (!objdump_parseInputFile(options.inputPath, &program)) {
        objdump_programFree(&program);
        return EXIT_FAILURE;
    }

    objdump_sortSectionSymbols(&program);

    objdump_printFileBanner(options.inputPath);
    if (options.showHeaders) {
        objdump_printHeaders(&program);
        if (options.showSymbols || options.showDisassembly) {
            printf("\n");
        }
    }
    if (options.showSymbols) {
        objdump_printSymbols(&program);
    }
    if (options.showDisassembly) {
        objdump_printDisassembly(&program);
    }

    objdump_programFree(&program);
    return EXIT_SUCCESS;
}
