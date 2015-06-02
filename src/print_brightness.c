// vim:ts=4:sw=4:expandtab
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/inotify.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>

#include "i3status.h"

#define VALUE_UNSET -1
#define BUF_SIZE 33
static long max_value = VALUE_UNSET;
static int first_run = VALUE_UNSET;
static int symbol_cnt = VALUE_UNSET;
static int sizes[BUF_SIZE];

/*
 * Thread callback function, blocked until change comes.
 * Support for the conditional async build.
 */

#ifdef ASYNC
void *change_callback(void *param) {
    int fd = inotify_init();
    inotify_add_watch(fd, (const char *)param, IN_MODIFY);
    const int buf_size = sizeof(struct inotify_event);
    char buf[buf_size];
    while (true) {
        read(fd, buf, buf_size);

        pthread_mutex_lock(&i3status_sleep_mutex);
        pthread_cond_broadcast(&i3status_sleep_cond);
        pthread_mutex_unlock(&i3status_sleep_mutex);
    }
    return NULL;
}
#endif

/*
 * Thread callback function, blocked until change comes.
 *
 */
long load_max_value(const char *device_max) {
    FILE *file_max = NULL;
    if ((file_max = fopen(device_max, "r")) == NULL) {
        fprintf(stderr, "i3status: brightness: Cannot open file: %s\n", device_max);
        return VALUE_UNSET;
    }

    long value = VALUE_UNSET;
    if (fscanf(file_max, "%ld", &value) != 1 || value == VALUE_UNSET) {
        fclose(file_max);
        fprintf(stderr, "i3status: brightness: %s bad content format, expecting integer\n", device_max);
        return VALUE_UNSET;
    }
    fclose(file_max);
    return value;
}

static char *apply_brightness_format(const char *fmt, char *outwalk, long bright) {
    const char *walk = fmt;

    for (; *walk != '\0'; walk++) {
        if (*walk != '%') {
            *(outwalk++) = *walk;
            continue;
        }
        if (BEGINS_WITH(walk + 1, "%")) {
            outwalk += sprintf(outwalk, "%%");
            walk += strlen("%");
        }
        if (BEGINS_WITH(walk + 1, "brightness")) {
            outwalk += sprintf(outwalk, "%ld%%", bright);
            walk += strlen("brightness");
        }
    }
    return outwalk;
}

void print_brightness(yajl_gen json_gen, char *buffer, char *format_symbolic, char *format_numeric, char *format_type, char *device_cur, char *device_max) {
    if (first_run == VALUE_UNSET) {
#ifdef ASYNC
        pthread_t observer;
        if (pthread_create(&observer, NULL, (void *(*)(void *))change_callback, (void *)device_cur) != 0) {
            fprintf(stderr, "i3status: brightness: Cannot create an observer thread.\n");
        }
#endif
        // Suppor for multibyte characters. Save incremental size for each of them.
        // Symbols count can be variable from 2 up to BUF_SIZE
        char *fmt = format_symbolic;
        const int max_cnt = BUF_SIZE - 1, min_cnt = 2;

        for (symbol_cnt = 1; *fmt != '\0';) {
            sizes[symbol_cnt] = mblen(fmt, MB_CUR_MAX) + sizes[symbol_cnt - 1];
            fmt = format_symbolic + sizes[symbol_cnt];
            symbol_cnt++;
            if (symbol_cnt > max_cnt)
                break;
        }
        symbol_cnt--;
        if (symbol_cnt < min_cnt) {
            fprintf(stderr, "i3status: brightness: Bad format, at least %d symbols.\n", min_cnt);
            return;
        }

        first_run = 0;
    }

    // Load and store maximum value
    char *outwalk = buffer;
    if (max_value == VALUE_UNSET) {
        if ((max_value = load_max_value(device_max)) == VALUE_UNSET) {
            START_COLOR("color_bad");
            outwalk += sprintf(outwalk, "bad max file");
            END_COLOR;
            OUTPUT_FULL_TEXT(buffer);
            return;
        }
    }

    FILE *file_cur = NULL;
    if ((file_cur = fopen(device_cur, "r")) == NULL) {
        fprintf(stderr, "i3status: brightness: Cannot open file: %s\n", device_cur);
        fclose(file_cur);
    }

    // Load current value from file
    long current = VALUE_UNSET;
    if (fscanf(file_cur, "%ld", &current) != 1 || current == VALUE_UNSET) {
        fclose(file_cur);
        START_COLOR("color_bad");
        outwalk += sprintf(outwalk, "bad current file");
        END_COLOR;
        OUTPUT_FULL_TEXT(buffer);
        return;
    }
    fclose(file_cur);

    if (strcmp(format_type, "numeric") == 0) {
        current = (long)((float)100 * current / max_value);
        current = current > 100 ? 100 : current;
        current = current < 0 ? 0 : current;
        outwalk = apply_brightness_format(format_numeric, outwalk, current);
    } else {
        current = current >= max_value ? max_value - 1 : current;
        current = current < 0 ? 0 : current;
        int index = (symbol_cnt * current) / max_value + 1;

        // sizes[x] - sizes[x - 1] is sizeof multibyte char in bytes
        int base = sizes[index - 1];
        int size = sizes[index] - base;
        memcpy(outwalk, format_symbolic + base, sizeof(char) * size);
        outwalk += size;
    }

    *outwalk = '\0';
    OUTPUT_FULL_TEXT(buffer);
}
