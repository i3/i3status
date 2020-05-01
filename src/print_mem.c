// vim:ts=4:sw=4:expandtab
#include <config.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>
#include "i3status.h"

#define STRING_SIZE 10

#define BINARY_BASE 1024UL

#if defined(linux)
static const char *const iec_symbols[] = {"B", "KiB", "MiB", "GiB", "TiB"};
#define MAX_EXPONENT ((sizeof iec_symbols / sizeof *iec_symbols) - 1)
#endif

#if defined(linux)
/*
 * Prints the given amount of bytes in a human readable manner.
 *
 */
static int print_bytes_human(char *outwalk, unsigned long bytes, const char *unit, const int decimals) {
    double base = bytes;
    size_t exponent = 0;
    while (base >= BINARY_BASE && exponent < MAX_EXPONENT) {
        if (strcasecmp(unit, iec_symbols[exponent]) == 0) {
            break;
        }

        base /= BINARY_BASE;
        exponent += 1;
    }
    return sprintf(outwalk, "%.*f %s", decimals, base, iec_symbols[exponent]);
}

static int print_percentage(char *outwalk, float percent) {
    return snprintf(outwalk, STRING_SIZE, "%.1f%s", percent, pct_mark);
}
#endif

#if defined(linux)
/*
 * Convert a string to its absolute representation based on the total
 * memory of `mem_total`.
 *
 * The string can contain any percentage values, which then return a
 * the value of `mem_amount` in relation to `mem_total`.
 * Alternatively an absolute value can be given, suffixed with an iec
 * symbol.
 *
 */
static unsigned long memory_absolute(const char *mem_amount, const unsigned long mem_total) {
    char *endptr;
    unsigned long amount = strtoul(mem_amount, &endptr, 10);

    while (endptr[0] != '\0' && isspace(endptr[0]))
        endptr++;

    switch (endptr[0]) {
        case 'T':
        case 't':
            amount *= BINARY_BASE;
        case 'G':
        case 'g':
            amount *= BINARY_BASE;
        case 'M':
        case 'm':
            amount *= BINARY_BASE;
        case 'K':
        case 'k':
            amount *= BINARY_BASE;
            break;
        case '%':
            amount = mem_total * amount / 100;
            break;
    }

    return amount;
}
#endif

void print_memory(yajl_gen json_gen, char *buffer, const char *format, const char *format_degraded, const char *threshold_degraded, const char *threshold_critical, const char *memory_used_method, const char *unit, const int decimals) {
    char *outwalk = buffer;

#if defined(linux)
    const char *selected_format = format;
    const char *output_color = NULL;

    int unread_fields = 6;
    unsigned long ram_total;
    unsigned long ram_free;
    unsigned long ram_available;
    unsigned long ram_buffers;
    unsigned long ram_cached;
    unsigned long ram_shared;

    FILE *file = fopen("/proc/meminfo", "r");
    if (!file) {
        goto error;
    }
    for (char line[128]; fgets(line, sizeof line, file);) {
        if (BEGINS_WITH(line, "MemTotal:")) {
            ram_total = strtoul(line + strlen("MemTotal:"), NULL, 10);
        } else if (BEGINS_WITH(line, "MemFree:")) {
            ram_free = strtoul(line + strlen("MemFree:"), NULL, 10);
        } else if (BEGINS_WITH(line, "MemAvailable:")) {
            ram_available = strtoul(line + strlen("MemAvailable:"), NULL, 10);
        } else if (BEGINS_WITH(line, "Buffers:")) {
            ram_buffers = strtoul(line + strlen("Buffers:"), NULL, 10);
        } else if (BEGINS_WITH(line, "Cached:")) {
            ram_cached = strtoul(line + strlen("Cached:"), NULL, 10);
        } else if (BEGINS_WITH(line, "Shmem:")) {
            ram_shared = strtoul(line + strlen("Shmem:"), NULL, 10);
        } else {
            continue;
        }
        if (--unread_fields == 0) {
            break;
        }
    }
    fclose(file);

    if (unread_fields > 0) {
        goto error;
    }

    // Values are in kB, convert them to B.
    ram_total *= 1024UL;
    ram_free *= 1024UL;
    ram_available *= 1024UL;
    ram_buffers *= 1024UL;
    ram_cached *= 1024UL;
    ram_shared *= 1024UL;

    unsigned long ram_used;
    if (BEGINS_WITH(memory_used_method, "memavailable")) {
        ram_used = ram_total - ram_available;
    } else if (BEGINS_WITH(memory_used_method, "classical")) {
        ram_used = ram_total - ram_free - ram_buffers - ram_cached;
    } else {
        die("Unexpected value: memory_used_method = %s", memory_used_method);
    }

    if (threshold_degraded) {
        const unsigned long threshold = memory_absolute(threshold_degraded, ram_total);
        if (ram_available < threshold) {
            output_color = "color_degraded";
        }
    }

    if (threshold_critical) {
        const unsigned long threshold = memory_absolute(threshold_critical, ram_total);
        if (ram_available < threshold) {
            output_color = "color_bad";
        }
    }

    if (output_color) {
        START_COLOR(output_color);

        if (format_degraded)
            selected_format = format_degraded;
    }

    char string_ram_total[STRING_SIZE];
    char string_ram_used[STRING_SIZE];
    char string_ram_free[STRING_SIZE];
    char string_ram_available[STRING_SIZE];
    char string_ram_shared[STRING_SIZE];
    char string_percentage_free[STRING_SIZE];
    char string_percentage_available[STRING_SIZE];
    char string_percentage_used[STRING_SIZE];
    char string_percentage_shared[STRING_SIZE];

    print_bytes_human(string_ram_total, ram_total, unit, decimals);
    print_bytes_human(string_ram_used, ram_used, unit, decimals);
    print_bytes_human(string_ram_free, ram_free, unit, decimals);
    print_bytes_human(string_ram_available, ram_available, unit, decimals);
    print_bytes_human(string_ram_shared, ram_shared, unit, decimals);
    print_percentage(string_percentage_free, 100.0 * ram_free / ram_total);
    print_percentage(string_percentage_available, 100.0 * ram_available / ram_total);
    print_percentage(string_percentage_used, 100.0 * ram_used / ram_total);
    print_percentage(string_percentage_shared, 100.0 * ram_shared / ram_total);

    placeholder_t placeholders[] = {
        {.name = "%total", .value = string_ram_total},
        {.name = "%used", .value = string_ram_used},
        {.name = "%free", .value = string_ram_free},
        {.name = "%available", .value = string_ram_available},
        {.name = "%shared", .value = string_ram_shared},
        {.name = "%percentage_free", .value = string_percentage_free},
        {.name = "%percentage_available", .value = string_percentage_available},
        {.name = "%percentage_used", .value = string_percentage_used},
        {.name = "%percentage_shared", .value = string_percentage_shared}};

    const size_t num = sizeof(placeholders) / sizeof(placeholder_t);
    buffer = format_placeholders(selected_format, &placeholders[0], num);

    if (output_color)
        END_COLOR;

    OUTPUT_FULL_TEXT(buffer);
    free(buffer);

    return;
error:
    OUTPUT_FULL_TEXT("can't read memory");
    fputs("i3status: Cannot read system memory using /proc/meminfo\n", stderr);
#else
    OUTPUT_FULL_TEXT("");
    fputs("i3status: Memory status information is not supported on this system\n", stderr);
#endif
}
