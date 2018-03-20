// vim:ts=4:sw=4:expandtab
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>
#include "i3status.h"

#define BINARY_BASE UINT64_C(1024)

#define MAX_EXPONENT 4
static const char *const iec_symbols[MAX_EXPONENT + 1] = {"", "Ki", "Mi", "Gi", "Ti"};

static const char filename[] = "/proc/meminfo";

/*
 * Prints the given amount of bytes in a human readable manner.
 *
 */
static int print_bytes_human(char *outwalk, uint64_t bytes) {
    double size = bytes;
    int exponent = 0;
    int bin_base = BINARY_BASE;
    while (size >= bin_base && exponent < MAX_EXPONENT) {
        size /= bin_base;
        exponent += 1;
    }
    return sprintf(outwalk, "%.1f %sB", size, iec_symbols[exponent]);
}

/*
 * Determines whether remaining bytes are below given threshold.
 *
 */
static bool below_threshold(const long totalRam, const long usedRam, const char *threshold_type, const long low_threshold) {
    // empty is available or free, based on "use_available_memory"
    long empty = totalRam - usedRam;
    if (BEGINS_WITH(threshold_type, "percentage_")) {
        return 100.0 * empty / totalRam < low_threshold;
    } else if (strcasecmp(threshold_type, "bytes_free") == 0) {
        return empty < low_threshold;
    } else if (threshold_type[0] != '\0' && strncasecmp(threshold_type + 1, "bytes_", strlen("bytes_")) == 0) {
        uint64_t base = BINARY_BASE;
        long factor = 1;

        switch (threshold_type[0]) {
            case 'T':
            case 't':
                factor *= base;
            case 'G':
            case 'g':
                factor *= base;
            case 'M':
            case 'm':
                factor *= base;
            case 'K':
            case 'k':
                factor *= base;
                break;
            default:
                return false;
        }
        return empty < low_threshold * factor;
    }
    return false;
}

void print_memory(yajl_gen json_gen, char *buffer, const char *format, const char *degraded_format_below_threshold, const char *degraded_threshold_type, const float degraded_low_threshold, const char *critical_format_below_threshold, const char *critical_threshold_type, const float critical_low_threshold, const bool use_available_memory) {
    char *outwalk = buffer;

#if defined(linux)
    const char *selected_format = format;
    const char *walk;
    bool colorful_output = false;

    long totalRam = -1;
    long freeRam = -1;
    long availableRam = -1;
    long usedRam = -1;
    long sharedRam = -1;
    long cached = -1;
    long buffers = -1;

    FILE *file = fopen(filename, "r");
    if (!file) {
        goto error;
    }
    char line[128];
    while (fgets(line, sizeof line, file)) {
        if (BEGINS_WITH(line, "MemTotal:")) {
            totalRam = strtol(line + strlen("MemTotal:"), NULL, 10);
        }
        if (BEGINS_WITH(line, "MemFree:")) {
            freeRam = strtol(line + strlen("MemFree:"), NULL, 10);
        }
        if (BEGINS_WITH(line, "MemAvailable:")) {
            availableRam = strtol(line + strlen("MemAvailable:"), NULL, 10);
        }
        if (BEGINS_WITH(line, "Buffers:")) {
            buffers = strtol(line + strlen("Buffers:"), NULL, 10);
        }
        if (BEGINS_WITH(line, "Cached:")) {
            cached = strtol(line + strlen("Cached:"), NULL, 10);
        }
        if (BEGINS_WITH(line, "Shmem:")) {
            sharedRam = strtol(line + strlen("Shmem:"), NULL, 10);
        }
        if (totalRam != -1 && freeRam != -1 && availableRam != -1 && buffers != -1 && cached != -1 && sharedRam != -1) {
            break;
        }
    }
    fclose(file);

    if (totalRam == -1 || freeRam == -1 || availableRam == -1 || buffers == -1 || cached == -1 || sharedRam == -1) {
        goto error;
    }

    totalRam = totalRam * BINARY_BASE;
    freeRam = freeRam * BINARY_BASE;
    availableRam = availableRam * BINARY_BASE;
    buffers = buffers * BINARY_BASE;
    cached = cached * BINARY_BASE;
    sharedRam = sharedRam * BINARY_BASE;
    if (use_available_memory) {
        usedRam = totalRam - availableRam;
    } else {
        usedRam = totalRam - freeRam - buffers - cached;
    }

    if (degraded_low_threshold > 0 && below_threshold(totalRam, usedRam, degraded_threshold_type, degraded_low_threshold)) {
        if (critical_low_threshold > 0 && below_threshold(totalRam, usedRam, critical_threshold_type, critical_low_threshold)) {
            START_COLOR("color_bad");
            colorful_output = true;
            if (critical_format_below_threshold != NULL)
                selected_format = critical_format_below_threshold;
        } else {
            START_COLOR("color_degraded");
            colorful_output = true;
            if (degraded_format_below_threshold != NULL)
                selected_format = degraded_format_below_threshold;
        }
    }

    for (walk = selected_format; *walk != '\0'; walk++) {
        if (*walk != '%') {
            *(outwalk++) = *walk;
            continue;
        }
        if (BEGINS_WITH(walk + 1, "total")) {
            outwalk += print_bytes_human(outwalk, totalRam);
            walk += strlen("total");
        }

        if (BEGINS_WITH(walk + 1, "used")) {
            outwalk += print_bytes_human(outwalk, usedRam);
            walk += strlen("used");
        }

        if (BEGINS_WITH(walk + 1, "free")) {
            outwalk += print_bytes_human(outwalk, freeRam);
            walk += strlen("free");
        }

        if (BEGINS_WITH(walk + 1, "available")) {
            outwalk += print_bytes_human(outwalk, availableRam);
            walk += strlen("available");
        }

        if (BEGINS_WITH(walk + 1, "shared")) {
            outwalk += print_bytes_human(outwalk, sharedRam);
            walk += strlen("shared");
        }

        if (BEGINS_WITH(walk + 1, "percentage_free")) {
            outwalk += sprintf(outwalk, "%.01f%s", 100.0 * freeRam / totalRam, pct_mark);
            walk += strlen("percentage_free");
        }

        if (BEGINS_WITH(walk + 1, "percentage_available")) {
            outwalk += sprintf(outwalk, "%.01f%s", 100.0 * availableRam / totalRam, pct_mark);
            walk += strlen("percentage_available");
        }

        if (BEGINS_WITH(walk + 1, "percentage_used")) {
            outwalk += sprintf(outwalk, "%.01f%s", 100.0 * usedRam / totalRam, pct_mark);
            walk += strlen("percentage_used");
        }

        if (BEGINS_WITH(walk + 1, "percentage_shared")) {
            outwalk += sprintf(outwalk, "%.01f%s", 100.0 * sharedRam / totalRam, pct_mark);
            walk += strlen("percentage_shared");
        }
    }

    if (colorful_output)
        END_COLOR;

    *outwalk = '\0';
    OUTPUT_FULL_TEXT(buffer);

    return;
error:
    OUTPUT_FULL_TEXT("can't read memory");
    fputs("i3status: Cannot read system memory using /proc/meminfo\n", stderr);
#else
    fputs("i3status: Memory status information is not supported on this system\n", stderr);
#endif
}
