// vim:ts=4:sw=4:expandtab
#include <config.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <locale.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>

#include "i3status.h"

#define STRING_SIZE 50

static bool local_timezone_init = false;
static const char *local_timezone = NULL;
static const char *current_timezone = NULL;

void set_timezone(const char *tz) {
    if (!local_timezone_init) {
        /* First call, initialize. */
        local_timezone = getenv("TZ");
        local_timezone_init = true;
    }
    if (tz == NULL || tz[0] == '\0') {
        /* User wants localtime. */
        tz = local_timezone;
    }
    if (tz != current_timezone) {
        if (tz) {
            setenv("TZ", tz, 1);
        } else {
            unsetenv("TZ");
        }
        current_timezone = tz;
    }
    tzset();
}

void print_time(time_ctx_t *ctx) {
    char *outwalk = ctx->buf;
    struct tm local_tm, tm;

    if (ctx->title != NULL)
        INSTANCE(ctx->title);

    set_timezone(NULL);
    localtime_r(&ctx->t, &local_tm);

    set_timezone(ctx->tz);
    localtime_r(&ctx->t, &tm);

    // When hide_if_equals_localtime is true, compare local and target time to display only if different
    time_t local_t = mktime(&local_tm);
    double diff = difftime(local_t, ctx->t);
    if (ctx->hide_if_equals_localtime && diff == 0.0) {
        goto out;
    }

    if (ctx->locale != NULL) {
        setlocale(LC_ALL, ctx->locale);
    }

    char string_time[STRING_SIZE];

    if (ctx->format_time == NULL) {
        outwalk += strftime(ctx->buf, 4096, ctx->format, &tm);
    } else {
        strftime(string_time, sizeof(string_time), ctx->format_time, &tm);
        placeholder_t placeholders[] = {
            {.name = "%time", .value = string_time}};

        const size_t num = sizeof(placeholders) / sizeof(placeholder_t);
        char *formatted = format_placeholders(ctx->format_time, &placeholders[0], num);
        OUTPUT_FORMATTED;
        free(formatted);
    }

    if (ctx->locale != NULL) {
        setlocale(LC_ALL, "");
    }

out:
    *outwalk = '\0';
    OUTPUT_FULL_TEXT(ctx->buf);
}
