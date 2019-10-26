// vim:ts=4:sw=4:expandtab
#include <config.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>
#include "i3status.h"

#define BINARY_BASE UINT64_C(1024)

#define MAX_EXPONENT 4

#if defined(linux)
static const char *const iec_symbols[MAX_EXPONENT + 1] = {"", "Ki", "Mi", "Gi", "Ti"};

static const char memoryfile_linux[] = "/proc/meminfo";
#endif

#if defined(linux)
/*
 * Prints the given amount of bytes in a human readable manner.
 *
 */
static int print_bytes_human(char *outwalk, uint64_t bytes, const char *unit, const int decimals) {
    double size = bytes;
    int exponent = 0;
    int bin_base = BINARY_BASE;
    while (size >= bin_base && exponent < MAX_EXPONENT) {
        if (strcasecmp(unit, iec_symbols[exponent]) == 0) {
            break;
        }

        size /= bin_base;
        exponent += 1;
    }
    return sprintf(outwalk, "%.*f %sB", decimals, size, iec_symbols[exponent]);
}
#endif

#if defined(linux)
/*
 * Convert a string to its absolute representation based on the total
 * memory of `mem_total`.
 *
 * The string can contain any percentage values, which then return a
 * the value of `size` in relation to `mem_total`.
 * Alternatively an absolute value can be given, suffixed with an iec
 * symbol.
 *
 */
static long memory_absolute(const long mem_total, const char *size) {
    char *endptr = NULL;

    long mem_absolute = strtol(size, &endptr, 10);

    if (endptr) {
        while (endptr[0] != '\0' && isspace(endptr[0]))
            endptr++;

        switch (endptr[0]) {
            case 'T':
            case 't':
                mem_absolute *= BINARY_BASE;
            case 'G':
            case 'g':
                mem_absolute *= BINARY_BASE;
            case 'M':
            case 'm':
                mem_absolute *= BINARY_BASE;
            case 'K':
            case 'k':
                mem_absolute *= BINARY_BASE;
                break;
            case '%':
                mem_absolute = mem_total * mem_absolute / 100;
                break;
            default:
                break;
        }
    }

    return mem_absolute;
}
#endif

void print_memory(yajl_gen json_gen, char *buffer, const char *format, const char *format_degraded, const char *threshold_degraded, const char *threshold_critical, const char *memory_used_method, const char *unit, const int decimals) {
    char *outwalk = buffer;

#if defined(linux)
    const char *selected_format = format;
    const char *walk;
    const char *output_color = NULL;

    long ram_total = -1;
    long ram_free = -1;
    long ram_available = -1;
    long ram_used = -1;
    long ram_shared = -1;
    long ram_cached = -1;
    long ram_buffers = -1;

    FILE *file = fopen(memoryfile_linux, "r");
    if (!file) {
        goto error;
    }
    char line[128];
    while (fgets(line, sizeof line, file)) {
        if (BEGINS_WITH(line, "MemTotal:")) {
            ram_total = strtol(line + strlen("MemTotal:"), NULL, 10);
        }
        if (BEGINS_WITH(line, "MemFree:")) {
            ram_free = strtol(line + strlen("MemFree:"), NULL, 10);
        }
        if (BEGINS_WITH(line, "MemAvailable:")) {
            ram_available = strtol(line + strlen("MemAvailable:"), NULL, 10);
        }
        if (BEGINS_WITH(line, "Buffers:")) {
            ram_buffers = strtol(line + strlen("Buffers:"), NULL, 10);
        }
        if (BEGINS_WITH(line, "Cached:")) {
            ram_cached = strtol(line + strlen("Cached:"), NULL, 10);
        }
        if (BEGINS_WITH(line, "Shmem:")) {
            ram_shared = strtol(line + strlen("Shmem:"), NULL, 10);
        }
        if (ram_total != -1 && ram_free != -1 && ram_available != -1 && ram_buffers != -1 && ram_cached != -1 && ram_shared != -1) {
            break;
        }
    }
    fclose(file);

    if (ram_total == -1 || ram_free == -1 || ram_available == -1 || ram_buffers == -1 || ram_cached == -1 || ram_shared == -1) {
        goto error;
    }

    ram_total = ram_total * BINARY_BASE;
    ram_free = ram_free * BINARY_BASE;
    ram_available = ram_available * BINARY_BASE;
    ram_buffers = ram_buffers * BINARY_BASE;
    ram_cached = ram_cached * BINARY_BASE;
    ram_shared = ram_shared * BINARY_BASE;

    if (BEGINS_WITH(memory_used_method, "memavailable")) {
        ram_used = ram_total - ram_available;
    } else if (BEGINS_WITH(memory_used_method, "classical")) {
        ram_used = ram_total - ram_free - ram_buffers - ram_cached;
    }

    if (threshold_degraded) {
        long abs = memory_absolute(ram_total, threshold_degraded);
        if (ram_available < abs) {
            output_color = "color_degraded";
        }
    }

    if (threshold_critical) {
        long abs = memory_absolute(ram_total, threshold_critical);
        if (ram_available < abs) {
            output_color = "color_bad";
        }
    }

    if (output_color) {
        START_COLOR(output_color);

        if (format_degraded)
            selected_format = format_degraded;
    }

    for (walk = selected_format; *walk != '\0'; walk++) {
        if (*walk != '%') {
            *(outwalk++) = *walk;

        } else if (BEGINS_WITH(walk + 1, "total")) {
            outwalk += print_bytes_human(outwalk, ram_total, unit, decimals);
            walk += strlen("total");

        } else if (BEGINS_WITH(walk + 1, "used")) {
            outwalk += print_bytes_human(outwalk, ram_used, unit, decimals);
            walk += strlen("used");

        } else if (BEGINS_WITH(walk + 1, "free")) {
            outwalk += print_bytes_human(outwalk, ram_free, unit, decimals);
            walk += strlen("free");

        } else if (BEGINS_WITH(walk + 1, "available")) {
            outwalk += print_bytes_human(outwalk, ram_available, unit, decimals);
            walk += strlen("available");

        } else if (BEGINS_WITH(walk + 1, "shared")) {
            outwalk += print_bytes_human(outwalk, ram_shared, unit, decimals);
            walk += strlen("shared");

        } else if (BEGINS_WITH(walk + 1, "percentage_free")) {
            outwalk += sprintf(outwalk, "%.01f%s", 100.0 * ram_free / ram_total, pct_mark);
            walk += strlen("percentage_free");

        } else if (BEGINS_WITH(walk + 1, "percentage_available")) {
            outwalk += sprintf(outwalk, "%.01f%s", 100.0 * ram_available / ram_total, pct_mark);
            walk += strlen("percentage_available");

        } else if (BEGINS_WITH(walk + 1, "percentage_used")) {
            outwalk += sprintf(outwalk, "%.01f%s", 100.0 * ram_used / ram_total, pct_mark);
            walk += strlen("percentage_used");

        } else if (BEGINS_WITH(walk + 1, "percentage_shared")) {
            outwalk += sprintf(outwalk, "%.01f%s", 100.0 * ram_shared / ram_total, pct_mark);
            walk += strlen("percentage_shared");

        } else {
            *(outwalk++) = '%';
        }
    }

    if (output_color)
        END_COLOR;

    *outwalk = '\0';
    OUTPUT_FULL_TEXT(buffer);

    return;
error:
    OUTPUT_FULL_TEXT("can't read memory");
    fputs("i3status: Cannot read system memory using /proc/meminfo\n", stderr);
#else
    OUTPUT_FULL_TEXT("");
    fputs("i3status: Memory status information is not supported on this system\n", stderr);
#endif
}
