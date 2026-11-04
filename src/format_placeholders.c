/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 */
// copied from  i3:libi3/format_placeholders.c
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "i3status.h"

#ifndef CS_STARTS_WITH
#define CS_STARTS_WITH(string, needle) (strncmp((string), (needle), strlen((needle))) == 0)
#endif

/*
 * Replaces occurrences of the defined placeholders in the format string.
 *
 */
char *format_placeholders(const char *format, placeholder_t *placeholders, int num) {
    if (format == NULL)
        return NULL;

    /* We have to first iterate over the string to see how much buffer space
     * we need to allocate. A placeholder without a value expands to nothing. */
    size_t buffer_len = strlen(format) + 1;
    for (const char *walk = format; *walk != '\0'; walk++) {
        for (int i = 0; i < num; i++) {
            if (!CS_STARTS_WITH(walk, placeholders[i].name))
                continue;

            const size_t name_len = strlen(placeholders[i].name);
            const size_t value_len = (placeholders[i].value != NULL) ? strlen(placeholders[i].value) : 0;
            buffer_len = buffer_len - name_len + value_len;
            walk += name_len - 1;
            break;
        }
    }

    /* Now we can parse the format string. The buffer lives on the heap (and
     * not on the stack) because its size depends on the user's configuration. */
    char *buffer = scalloc(buffer_len);
    char *outwalk = buffer;
    for (const char *walk = format; *walk != '\0'; walk++) {
        if (*walk != '%') {
            *(outwalk++) = *walk;
            continue;
        }

        bool matched = false;
        for (int i = 0; i < num; i++) {
            if (!CS_STARTS_WITH(walk, placeholders[i].name)) {
                continue;
            }

            matched = true;
            if (placeholders[i].value != NULL) {
                const size_t value_len = strlen(placeholders[i].value);
                memcpy(outwalk, placeholders[i].value, value_len);
                outwalk += value_len;
            }
            walk += strlen(placeholders[i].name) - 1;
            break;
        }

        if (!matched)
            *(outwalk++) = *walk;
    }

    *outwalk = '\0';
    return buffer;
}
