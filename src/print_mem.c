// vim:ts=4:sw=4:expandtab
#include <config.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include "i3status.h"

#define MAX_DECIMALS 4
#define STRING_SIZE ((sizeof "1023. TiB") + MAX_DECIMALS)

#define BINARY_BASE 1024UL

static const char *const iec_symbols[] = {"B", "KiB", "MiB", "GiB", "TiB"};
#define MAX_EXPONENT ((sizeof iec_symbols / sizeof *iec_symbols) - 1)

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
    const int prec = decimals > MAX_DECIMALS ? MAX_DECIMALS : decimals;
    return sprintf(outwalk, "%.*f %s", prec, base, iec_symbols[exponent]);
}

static int print_percentage(char *outwalk, float percent) {
    return snprintf(outwalk, STRING_SIZE, "%.1f%s", percent, pct_mark);
}

static unsigned long page2byte (unsigned pageCnt) {
    return pageCnt * getpagesize ();
}

#if defined(__linux__)
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
            /* fall-through */
        case 'G':
        case 'g':
            amount *= BINARY_BASE;
            /* fall-through */
        case 'M':
        case 'm':
            amount *= BINARY_BASE;
            /* fall-through */
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

struct print_mem_info {
    unsigned long ram_total;
    unsigned long ram_free;
    unsigned long ram_used;
    unsigned long ram_available;
    unsigned long ram_buffers;
    unsigned long ram_cached;
    unsigned long ram_shared;
    char *output_color;
};

static void get_memory_info(struct print_mem_info *minfo, memory_ctx_t *ctx) {
    char *outwalk = ctx->buf;
#if defined(__linux__)
    int unread_fields = 6;

    minfo->output_color = NULL

    FILE *file = fopen("/proc/meminfo", "r");
    if (!file) {
        goto error;
    }
    for (char line[128]; fgets(line, sizeof line, file);) {
        if (BEGINS_WITH(line, "MemTotal:")) {
            minfo->ram_total = strtoul(line + strlen("MemTotal:"), NULL, 10);
        } else if (BEGINS_WITH(line, "MemFree:")) {
            minfo->ram_free = strtoul(line + strlen("MemFree:"), NULL, 10);
        } else if (BEGINS_WITH(line, "MemAvailable:")) {
            minfo->ram_available = strtoul(line + strlen("MemAvailable:"), NULL, 10);
        } else if (BEGINS_WITH(line, "Buffers:")) {
            minfo->ram_buffers = strtoul(line + strlen("Buffers:"), NULL, 10);
        } else if (BEGINS_WITH(line, "Cached:")) {
            minfo->ram_cached = strtoul(line + strlen("Cached:"), NULL, 10);
        } else if (BEGINS_WITH(line, "Shmem:")) {
            minfo->ram_shared = strtoul(line + strlen("Shmem:"), NULL, 10);
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
    minfo->ram_total *= 1024UL;
    minfo->ram_free *= 1024UL;
    minfo->ram_available *= 1024UL;
    minfo->ram_buffers *= 1024UL;
    minfo->ram_cached *= 1024UL;
    minfo->ram_shared *= 1024UL;

    if (BEGINS_WITH(ctx->memory_used_method, "memavailable")) {
        minfo->ram_used = minfo->ram_total - minfo->ram_available;
    } else if (BEGINS_WITH(ctx->memory_used_method, "classical")) {
        minfo->ram_used = minfo->ram_total - minfo->ram_free - minfo->ram_buffers - minfo->ram_cached;
    } else {
        die("Unexpected value: memory_used_method = %s", ctx->memory_used_method);
    }

    if (ctx->threshold_degraded) {
        const unsigned long threshold = memory_absolute(ctx->threshold_degraded, minfo->ram_total);
        if (minfo->ram_available < threshold) {
            output_color = "color_degraded";
        }
    }

    if (ctx->threshold_critical) {
        const unsigned long threshold = memory_absolute(ctx->threshold_critical, minfo->ram_total);
        if (minfo->ram_available < threshold) {
            minfo->output_color = "color_bad";
        }
    }
error:
    OUTPUT_FULL_TEXT("can't read memory");
    fputs("i3status: Cannot read system memory using /proc/meminfo\n", stderr);

#elif defined(__OpenBSD__)
    int mib [] = { CTL_VM, VM_UVMEXP };
    struct uvmexp our_uvmexp;
    size_t size = sizeof (our_uvmexp);

    if (sysctl (mib, 2, &our_uvmexp, &size, NULL, 0) < 0) {
        OUTPUT_FULL_TEXT("");
        fputs("i3status: cannot get memory info!\n", stderr);
        return;
    }

    minfo->ram_total = page2byte(our_uvmexp.npages);
    minfo->ram_free  = page2byte(our_uvmexp.free + our_uvmexp.inactive);
    minfo->ram_used  = page2byte(our_uvmexp.npages - our_uvmexp.free - our_uvmexp.inactive);
    minfo->ram_available = minfo->ram_free;
    minfo->ram_buffers = 0;
    minfo->ram_cached = 0;
    minfo->ram_shared = 0;
    minfo->output_color = NULL;
#else
    OUTPUT_FULL_TEXT("");
    fputs("i3status: Memory status information is not supported on this system\n", stderr);
#endif
}

void print_formatted_memory(struct print_mem_info *minfo, memory_ctx_t *ctx) {
    char *outwalk = ctx->buf;
    const char *selected_format = ctx->format;
    char string_ram_total[STRING_SIZE];
    char string_ram_used[STRING_SIZE];
    char string_ram_free[STRING_SIZE];
    char string_ram_available[STRING_SIZE];
    char string_ram_shared[STRING_SIZE];
    char string_percentage_free[STRING_SIZE];
    char string_percentage_available[STRING_SIZE];
    char string_percentage_used[STRING_SIZE];
    char string_percentage_shared[STRING_SIZE];

    if (minfo->output_color) {
        START_COLOR(minfo->output_color);

        if (ctx->format_degraded)
            selected_format = ctx->format_degraded;
    }

    print_bytes_human(string_ram_total, minfo->ram_total, ctx->unit, ctx->decimals);
    print_bytes_human(string_ram_used, minfo->ram_used, ctx->unit, ctx->decimals);
    print_bytes_human(string_ram_free, minfo->ram_free, ctx->unit, ctx->decimals);
    print_bytes_human(string_ram_available, minfo->ram_available, ctx->unit, ctx->decimals);
    print_bytes_human(string_ram_shared, minfo->ram_shared, ctx->unit, ctx->decimals);
    print_percentage(string_percentage_free, 100.0 * minfo->ram_free / minfo->ram_total);
    print_percentage(string_percentage_available, 100.0 * minfo->ram_available / minfo->ram_total);
    print_percentage(string_percentage_used, 100.0 * minfo->ram_used / minfo->ram_total);
    print_percentage(string_percentage_shared, 100.0 * minfo->ram_shared / minfo->ram_total);

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
    char *formatted = format_placeholders(selected_format, &placeholders[0], num);
    OUTPUT_FORMATTED;
    free(formatted);

    if (minfo->output_color)
        END_COLOR;

    OUTPUT_FULL_TEXT(ctx->buf);
}

void print_memory(memory_ctx_t *ctx) {
    struct print_mem_info minfo;

    get_memory_info(&minfo, ctx);
    print_formatted_memory(&minfo, ctx);
}
