/*
 Simple 68K profiler: samples PC every N instructions and aggregates per PC.
 Outputs JSON on demand and a live stream for the host.
*/

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "geo_notify.h"
#include "geo_profiler.h"

static const char *k_prof_out_path = NULL;
static const char *k_prof_json_path = NULL;

#define GEO_PROF_SAMPLING_SHIFT 0
#define PROF_TABLE_BITS 17u
#define PROF_TABLE_SIZE (1u << PROF_TABLE_BITS)
#define PROF_TABLE_MASK (PROF_TABLE_SIZE - 1u)

typedef struct ProfEntry
{
    uint32_t key;
    uint64_t samples;
    uint64_t cycles;
} ProfEntry;

typedef struct OutPair
{
    uint32_t addr;
    uint64_t samples;
    uint64_t cycles;
} OutPair;

static uint32_t g_sampling_mask = 0u;
static ProfEntry *g_table = NULL;
static uint32_t g_used = 0;
static uint32_t g_sample_accum = 0;
static int g_enabled = 0;
static int g_ever_enabled = 0;

#ifdef GEO_PROF_STREAM_VERIFY
static ProfEntry *g_stream_capture = NULL;
static ProfEntry *g_stream_capture_prev = NULL;
static uint32_t g_stream_capture_used = 0;
#endif

static uint32_t *g_entry_epochs = NULL;
static uint32_t *g_stream_delta_idxs = NULL;
static size_t g_stream_delta_count = 0;
static uint32_t g_stream_epoch = 1;
static int g_stream_enabled = 0;

static inline uint32_t
profiler_hash(uint32_t addr)
{
    uint32_t x = addr;
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x & PROF_TABLE_MASK;
}

static void
profiler_stream_epochAdvance(void)
{
    g_stream_delta_count = 0;
    g_stream_epoch++;
    if (g_stream_epoch == 0) {
        if (g_entry_epochs) {
            memset(g_entry_epochs, 0, PROF_TABLE_SIZE * sizeof(uint32_t));
        }
        g_stream_epoch = 1;
    }
}

static void
profiler_stream_markIndex(uint32_t idx)
{
    if (!g_stream_enabled || !g_stream_delta_idxs || !g_entry_epochs || idx >= PROF_TABLE_SIZE) {
        return;
    }
    if (g_entry_epochs[idx] == g_stream_epoch) {
        return;
    }
    g_entry_epochs[idx] = g_stream_epoch;
    if (g_stream_delta_count < PROF_TABLE_SIZE) {
        g_stream_delta_idxs[g_stream_delta_count++] = idx;
    }
}

#ifdef GEO_PROF_STREAM_VERIFY
static ProfEntry *
profiler_capture_lookup(ProfEntry *table, uint32_t addr, int create, uint32_t *used)
{
    if (!table) {
        return NULL;
    }
    uint32_t idx = profiler_hash(addr);
    for (uint32_t nprobe = 0; nprobe < PROF_TABLE_SIZE; ++nprobe) {
        ProfEntry *e = &table[idx];
        if (e->key == addr + 1u) {
            return e;
        }
        if (create && e->key == 0) {
            e->key = addr + 1u;
            e->samples = 0;
            e->cycles = 0;
            if (used) {
                ++*used;
            }
            return e;
        }
        idx = (idx + 1u) & PROF_TABLE_MASK;
    }
    return NULL;
}

static void
profiler_capture_reset(void)
{
    if (g_stream_capture) {
        memset(g_stream_capture, 0, PROF_TABLE_SIZE * sizeof(ProfEntry));
    }
    if (g_stream_capture_prev) {
        memset(g_stream_capture_prev, 0, PROF_TABLE_SIZE * sizeof(ProfEntry));
    }
    g_stream_capture_used = 0;
}

static void
profiler_capture_free(void)
{
    free(g_stream_capture);
    g_stream_capture = NULL;
    free(g_stream_capture_prev);
    g_stream_capture_prev = NULL;
    g_stream_capture_used = 0;
}

static void
profiler_capture_recordHits(const geo_profiler_stream_hit_t *hits, size_t count)
{
    if (!hits || count == 0 || !g_stream_capture || !g_stream_capture_prev) {
        return;
    }
    for (size_t i = 0; i < count; ++i) {
        uint32_t addr = hits[i].pc & 0x00ffffffu;
        uint64_t new_samples = hits[i].samples;
        uint64_t new_cycles = hits[i].cycles;
        ProfEntry *prev = profiler_capture_lookup(g_stream_capture_prev, addr, 1, NULL);
        uint64_t last_samples = prev ? prev->samples : 0;
        uint64_t last_cycles = prev ? prev->cycles : 0;
        uint64_t delta_samples = (new_samples >= last_samples) ? (new_samples - last_samples) : new_samples;
        uint64_t delta_cycles = (new_cycles >= last_cycles) ? (new_cycles - last_cycles) : new_cycles;
        ProfEntry *agg = profiler_capture_lookup(g_stream_capture, addr, 1, &g_stream_capture_used);
        if (agg) {
            agg->samples += delta_samples;
            agg->cycles += delta_cycles;
        }
        if (prev) {
            prev->samples = new_samples;
            prev->cycles = new_cycles;
        }
    }
}

void
geo_profiler_capture_stream_hits(const geo_profiler_stream_hit_t *hits, size_t count)
{
    profiler_capture_recordHits(hits, count);
}
#else
static void
profiler_capture_reset(void)
{
}

static void
profiler_capture_free(void)
{
}

void
geo_profiler_capture_stream_hits(const geo_profiler_stream_hit_t *hits, size_t count)
{
    (void)hits;
    (void)count;
}
#endif

static int
profiler_appendPrintf(char *buf, size_t bufsize, size_t *pos, const char *fmt, ...)
{
    if (!buf || !bufsize || !pos || *pos >= bufsize) {
        return 0;
    }
    va_list ap;
    va_start(ap, fmt);
    int res = vsnprintf(buf + *pos, bufsize - *pos, fmt, ap);
    va_end(ap);
    if (res < 0 || (size_t)res >= bufsize - *pos) {
        *pos = bufsize - 1;
        buf[*pos] = '\0';
        return 0;
    }
    *pos += (size_t)res;
    return 1;
}

static int
profiler_compareDescCount(const void *a, const void *b)
{
    const OutPair *pa = (const OutPair *)a;
    const OutPair *pb = (const OutPair *)b;
    if (pa->samples < pb->samples) {
        return 1;
    }
    if (pa->samples > pb->samples) {
        return -1;
    }
    if (pa->addr < pb->addr) {
        return -1;
    }
    if (pa->addr > pb->addr) {
        return 1;
    }
    return 0;
}

static OutPair *
profiler_collectPairsFromTable(const ProfEntry *table, uint32_t used_estimate, size_t *out_count)
{
    if (!table || !out_count) {
        return NULL;
    }
    size_t cap = used_estimate ? used_estimate : 1;
    OutPair *pairs = (OutPair *)malloc(cap * sizeof(OutPair));
    if (!pairs) {
        return NULL;
    }
    size_t count = 0;
    for (size_t i = 0; i < PROF_TABLE_SIZE; ++i) {
        if (table[i].key != 0 && (table[i].samples || table[i].cycles)) {
            if (count >= cap) {
                size_t nc = cap ? cap * 2 : 1;
                OutPair *tmp = (OutPair *)realloc(pairs, nc * sizeof(OutPair));
                if (!tmp) {
                    free(pairs);
                    return NULL;
                }
                pairs = tmp;
                cap = nc;
            }
            pairs[count].addr = table[i].key - 1u;
            pairs[count].samples = table[i].samples;
            pairs[count].cycles = table[i].cycles;
            ++count;
        }
    }
    qsort(pairs, count, sizeof(OutPair), profiler_compareDescCount);
    *out_count = count;
    return pairs;
}

static int
profiler_writeJsonFromPairs(const char *path, const OutPair *pairs, size_t npairs)
{
    if (!path) {
        return 0;
    }
    FILE *fj = fopen(path, "wb");
    if (!fj) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Profiler dump failed: cannot open %s", path);
        geo_notify(msg, 240);
        return -1;
    }
    fputc('[', fj);
    for (size_t i = 0; i < npairs; ++i) {
        if (i) {
            fputc(',', fj);
        }
        fprintf(fj,
                "{\"pc\":\"0x%06x\",\"samples\":%llu,\"cycles\":%llu}",
                (unsigned)(pairs[i].addr & 0x00ffffffu),
                (unsigned long long)pairs[i].samples,
                (unsigned long long)pairs[i].cycles);
    }
    fputc(']', fj);
    fclose(fj);
    return 1;
}

static char *
profiler_makeTestJsonPath(const char *path)
{
    if (!path || !path[0]) {
        return NULL;
    }
    const char *last_sep = strrchr(path, '/');
    size_t dir_len = last_sep ? (size_t)(last_sep - path + 1) : 0;
    const char *name = last_sep ? last_sep + 1 : path;
    const char *prefix = "test-";
    size_t len = dir_len + strlen(prefix) + strlen(name) + 1;
    char *out = (char *)malloc(len);
    if (!out) {
        return NULL;
    }
    if (dir_len) {
        memcpy(out, path, dir_len);
    }
    size_t pos = dir_len;
    memcpy(out + pos, prefix, strlen(prefix));
    pos += strlen(prefix);
    memcpy(out + pos, name, strlen(name) + 1);
    return out;
}

void
geo_profiler_init(void)
{
    if (g_table) {
        return;
    }

    const char *e_txt = getenv("GEO_PROF_TXT");
    if (e_txt && e_txt[0]) {
        k_prof_out_path = e_txt;
    }
    const char *e_json = getenv("GEO_PROF_JSON");
    if (e_json && e_json[0]) {
        k_prof_json_path = e_json;
    }

    const char *e_every = getenv("GEO_PROF_SAMPLE_EVERY");
    const char *e_shift = getenv("GEO_PROF_SAMPLING_SHIFT");
    if (e_every && e_every[0] && e_every[0] != '0') {
        g_sampling_mask = 0u;
    } else if (e_shift && e_shift[0]) {
        int sh = atoi(e_shift);
        if (sh <= 0) {
            g_sampling_mask = 0u;
        } else if (sh >= 16) {
            g_sampling_mask = 0xffffffffu;
        } else {
            g_sampling_mask = (uint32_t)((1u << sh) - 1u);
        }
    }

    g_table = (ProfEntry *)calloc(PROF_TABLE_SIZE, sizeof(ProfEntry));
    g_used = 0;
    g_sample_accum = 0;
    g_enabled = 0;

#ifdef GEO_PROF_STREAM_VERIFY
    g_stream_capture = (ProfEntry *)calloc(PROF_TABLE_SIZE, sizeof(ProfEntry));
    g_stream_capture_prev = (ProfEntry *)calloc(PROF_TABLE_SIZE, sizeof(ProfEntry));
    g_stream_capture_used = 0;
#endif

    g_entry_epochs = (uint32_t *)calloc(PROF_TABLE_SIZE, sizeof(uint32_t));
    g_stream_delta_idxs = (uint32_t *)malloc(PROF_TABLE_SIZE * sizeof(uint32_t));
    g_stream_delta_count = 0;
    g_stream_epoch = 1;
    g_stream_enabled = 0;
}

void
geo_profiler_instr_hook(unsigned pc)
{
    if (!g_enabled) {
        return;
    }
    if ((g_sample_accum++ & g_sampling_mask) != 0) {
        return;
    }

    uint32_t a = pc & 0x00ffffffu;
    uint32_t idx = profiler_hash(a);
    for (uint32_t nprobe = 0; nprobe < PROF_TABLE_SIZE; ++nprobe) {
        ProfEntry *e = &g_table[idx];
        if (e->key == 0) {
            e->key = a + 1u;
            e->samples = 1u;
            e->cycles = 0u;
            ++g_used;
            profiler_stream_markIndex(idx);
            return;
        }
        if ((e->key - 1u) == a) {
            ++e->samples;
            profiler_stream_markIndex(idx);
            return;
        }
        idx = (idx + 1u) & PROF_TABLE_MASK;
    }
}

void
geo_profiler_account(unsigned pc, unsigned cycles)
{
    if (!g_table || !g_enabled) {
        return;
    }

    uint32_t a = pc & 0x00ffffffu;
    uint32_t idx = profiler_hash(a);
    for (uint32_t nprobe = 0; nprobe < PROF_TABLE_SIZE; ++nprobe) {
        ProfEntry *e = &g_table[idx];
        if (e->key == 0) {
            e->key = a + 1u;
            e->samples = 0u;
            e->cycles = (uint64_t)cycles;
            ++g_used;
            profiler_stream_markIndex(idx);
            return;
        }
        if ((e->key - 1u) == a) {
            e->cycles += (uint64_t)cycles;
            profiler_stream_markIndex(idx);
            return;
        }
        idx = (idx + 1u) & PROF_TABLE_MASK;
    }
}

void
geo_profiler_set_enabled(int enabled)
{
    g_enabled = enabled ? 1 : 0;
    if (g_enabled) {
        g_ever_enabled = 1;
    }
}

int
geo_profiler_get_enabled(void)
{
    return g_enabled;
}

void
geo_profiler_reset(void)
{
    if (!g_table) {
        return;
    }
    memset(g_table, 0, PROF_TABLE_SIZE * sizeof(ProfEntry));
    g_used = 0;
    g_sample_accum = 0;
    if (g_entry_epochs) {
        memset(g_entry_epochs, 0, PROF_TABLE_SIZE * sizeof(uint32_t));
    }
    g_stream_delta_count = 0;
    g_stream_epoch = 1;
    g_stream_enabled = 0;
    profiler_capture_reset();
}

size_t
geo_profiler_top_lines(geo_prof_line_hit_t *out, size_t max)
{
    (void)out;
    (void)max;
    return 0;
}

void
geo_profiler_stream_enable(int enable)
{
    g_stream_enabled = enable ? 1 : 0;
    if (!g_stream_enabled) {
        profiler_stream_epochAdvance();
    }
}

size_t
geo_profiler_stream_collect(geo_profiler_stream_hit_t *out, size_t max)
{
    if (!g_table || !g_stream_delta_idxs) {
        profiler_stream_epochAdvance();
        return 0;
    }

    size_t avail = g_stream_delta_count;
    size_t take = max == 0 ? avail : (max < avail ? max : avail);
    if (out && take > 0) {
        for (size_t i = 0; i < take; ++i) {
            uint32_t idx = g_stream_delta_idxs[i];
            const ProfEntry *e = &g_table[idx];
            out[i].pc = e->key ? (e->key - 1u) : 0u;
            out[i].samples = e->samples;
            out[i].cycles = e->cycles;
        }
    }

    profiler_stream_epochAdvance();
    return take;
}

size_t
geo_profiler_stream_pending(void)
{
    return g_stream_delta_count;
}

size_t
geo_profiler_stream_format_json(char *out, size_t cap)
{
    if (!out || cap == 0 || !g_stream_enabled) {
        return 0;
    }

    size_t pending = geo_profiler_stream_pending();
    if (pending == 0) {
        return 0;
    }

    geo_profiler_stream_hit_t *hits = (geo_profiler_stream_hit_t *)calloc(pending, sizeof(geo_profiler_stream_hit_t));
    if (!hits) {
        return 0;
    }

    size_t count = geo_profiler_stream_collect(hits, 0);
    if (count == 0) {
        free(hits);
        return 0;
    }

    geo_profiler_capture_stream_hits(hits, count);

    size_t pos = 0;
    const char *enabled = geo_profiler_get_enabled() ? "enabled" : "disabled";
    if (!profiler_appendPrintf(out, cap, &pos, "{\"stream\":\"profiler\",\"enabled\":\"%s\",\"hits\":[", enabled)) {
        free(hits);
        return 0;
    }

    for (size_t i = 0; i < count; ++i) {
        if (i > 0) {
            if (pos + 1 >= cap) {
                out[cap - 1] = '\0';
                free(hits);
                return 0;
            }
            out[pos++] = ',';
        }
        if (!profiler_appendPrintf(out,
                                   cap,
                                   &pos,
                                   "{\"pc\":\"0x%06X\",\"samples\":%llu,\"cycles\":%llu}",
                                   hits[i].pc & 0x00ffffffu,
                                   (unsigned long long)hits[i].samples,
                                   (unsigned long long)hits[i].cycles)) {
            free(hits);
            return 0;
        }
    }

    if (pos + 2 >= cap) {
        out[cap - 1] = '\0';
        free(hits);
        return 0;
    }
    out[pos++] = ']';
    out[pos++] = '}';
    out[pos] = '\0';

    free(hits);
    return pos;
}

int
geo_profiler_dump(void)
{
    if (!g_ever_enabled || !g_table) {
        return 0;
    }

    size_t npairs = 0;
    OutPair *pairs = profiler_collectPairsFromTable(g_table, g_used, &npairs);
    if (!pairs) {
        goto cleanup;
    }

    const char *json_path = (k_prof_json_path && k_prof_json_path[0]) ? k_prof_json_path :
                            (k_prof_out_path && k_prof_out_path[0]) ? k_prof_out_path : NULL;
    int rc = 0;
    if (json_path) {
        int wrote = profiler_writeJsonFromPairs(json_path, pairs, npairs);
        if (wrote < 0) {
            rc = -1;
        } else if (wrote > 0) {
            rc = 1;
        }

#ifdef GEO_PROF_STREAM_VERIFY
        if (g_stream_capture_used > 0) {
            char *test_path = profiler_makeTestJsonPath(json_path);
            if (test_path) {
                size_t test_npairs = 0;
                OutPair *test_pairs = profiler_collectPairsFromTable(g_stream_capture, g_stream_capture_used, &test_npairs);
                if (test_pairs) {
                    profiler_writeJsonFromPairs(test_path, test_pairs, test_npairs);
                    free(test_pairs);
                }
                free(test_path);
            }
        }
#endif
    }

    free(pairs);

cleanup:
    free(g_table);
    g_table = NULL;
    g_used = 0;
    g_sample_accum = 0;
    g_enabled = 0;
    profiler_capture_free();
    return rc;
}

const char *
geo_profiler_dump_path(void)
{
    if (k_prof_json_path && k_prof_json_path[0]) {
        return k_prof_json_path;
    }
    if (k_prof_out_path && k_prof_out_path[0]) {
        return k_prof_out_path;
    }
    return NULL;
}
