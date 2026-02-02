#include <string.h>

#include "e9k_checkpoint.h"
#include "geo_serial.h"

static e9k_debug_checkpoint_t e9k_checkpoint_data[E9K_CHECKPOINT_COUNT];
static int e9k_checkpoint_active = -1;
static int e9k_checkpoint_enabled = 0;

void
e9k_checkpoint_reset(void)
{
    memset(e9k_checkpoint_data, 0, sizeof(e9k_checkpoint_data));
    e9k_checkpoint_active = -1;
}

void
e9k_checkpoint_setEnabled(int enabled)
{
    e9k_checkpoint_enabled = enabled ? 1 : 0;
    if (!e9k_checkpoint_enabled) {
        e9k_checkpoint_active = -1;
    }
}

int
e9k_checkpoint_isEnabled(void)
{
    return e9k_checkpoint_enabled;
}

void
e9k_checkpoint_state_save(uint8_t *st)
{
    if (!st) {
        return;
    }
    geo_serial_push8(st, (uint8_t)e9k_checkpoint_enabled);
    geo_serial_push32(st, (uint32_t)e9k_checkpoint_active);
    for (size_t i = 0; i < E9K_CHECKPOINT_COUNT; ++i) {
        geo_serial_push64(st, e9k_checkpoint_data[i].current);
        geo_serial_push64(st, e9k_checkpoint_data[i].accumulator);
        geo_serial_push64(st, e9k_checkpoint_data[i].count);
        geo_serial_push64(st, e9k_checkpoint_data[i].average);
        geo_serial_push64(st, e9k_checkpoint_data[i].minimum);
        geo_serial_push64(st, e9k_checkpoint_data[i].maximum);
    }
}

void
e9k_checkpoint_state_load(uint8_t *st)
{
    if (!st) {
        return;
    }
    e9k_checkpoint_enabled = geo_serial_pop8(st) ? 1 : 0;
    e9k_checkpoint_active = (int)geo_serial_pop32(st);
    for (size_t i = 0; i < E9K_CHECKPOINT_COUNT; ++i) {
        e9k_checkpoint_data[i].current = geo_serial_pop64(st);
        e9k_checkpoint_data[i].accumulator = geo_serial_pop64(st);
        e9k_checkpoint_data[i].count = geo_serial_pop64(st);
        e9k_checkpoint_data[i].average = geo_serial_pop64(st);
        e9k_checkpoint_data[i].minimum = geo_serial_pop64(st);
        e9k_checkpoint_data[i].maximum = geo_serial_pop64(st);
    }
    if (!e9k_checkpoint_enabled) {
        e9k_checkpoint_active = -1;
    }
    if (e9k_checkpoint_active < -1 || e9k_checkpoint_active >= (int)E9K_CHECKPOINT_COUNT) {
        e9k_checkpoint_active = -1;
    }
}

void
e9k_checkpoint_write(uint8_t index)
{
    if (!e9k_checkpoint_enabled) {
        return;
    }
    if (index >= E9K_CHECKPOINT_COUNT) {
        return;
    }
    if (e9k_checkpoint_active >= 0) {
        e9k_debug_checkpoint_t *prev = &e9k_checkpoint_data[e9k_checkpoint_active];
        uint64_t sample = prev->current;
        if (prev->count == 0) {
            prev->minimum = sample;
            prev->maximum = sample;
        } else {
            if (sample < prev->minimum) {
                prev->minimum = sample;
            }
            if (sample > prev->maximum) {
                prev->maximum = sample;
            }
        }
        prev->count += 1;
        prev->accumulator += sample;
        prev->average = prev->count ? (prev->accumulator / prev->count) : 0;
        prev->current = 0;
    }
    e9k_checkpoint_active = (int)index;
    e9k_checkpoint_data[index].current = 0;
}

void
e9k_checkpoint_tick(uint64_t ticks)
{
    if (!e9k_checkpoint_enabled) {
        return;
    }
    if (e9k_checkpoint_active < 0) {
        return;
    }
    e9k_checkpoint_data[e9k_checkpoint_active].current += ticks;
}

size_t
e9k_checkpoint_read(e9k_debug_checkpoint_t *out, size_t cap)
{
    if (!out || cap < sizeof(e9k_checkpoint_data)) {
        return 0;
    }
    memcpy(out, e9k_checkpoint_data, sizeof(e9k_checkpoint_data));
    return sizeof(e9k_checkpoint_data);
}
