// vim:ts=4:sw=4:expandtab
#include <config.h>
#include <sys/types.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <glob.h>
#include <sys/fcntl.h>
#include <sys/stat.h>

#include "i3status.h"

#define exit_if_null(pointer, ...) \
    {                              \
        if (pointer == NULL)       \
            die(__VA_ARGS__);      \
    }

/*
 * Reads (size - 1) bytes into the destination buffer from filename,
 * and null-terminate it.
 *
 * On success, true is returned. Otherwise, false is returned and the content
 * of destination is left untouched.
 *
 */
bool slurp(const char *filename, char *destination, int size) {
    int fd;

    if ((fd = open(filename, O_RDONLY)) == -1)
        return false;

    /* We need one byte for the trailing 0 byte */
    int n = read(fd, destination, size - 1);
    if (n != -1)
        destination[n] = '\0';
    (void)close(fd);

    return n != -1;
}

/*
 * This function resolves ~ in pathnames.
 * It may resolve wildcards in the first part of the path, but if no match
 * or multiple matches are found, it just returns a copy of path as given.
 *
 */
char *resolve_tilde(const char *path) {
    static glob_t globbuf;
    char *head, *tail, *result = NULL;

    tail = strchr(path, '/');
    head = strndup(path, tail ? (size_t)(tail - path) : strlen(path));

    int res = glob(head, GLOB_TILDE, NULL, &globbuf);
    free(head);
    /* no match, or many wildcard matches are bad */
    if (res == GLOB_NOMATCH || globbuf.gl_pathc != 1)
        result = sstrdup(path);
    else if (res != 0) {
        die("glob() failed");
    } else {
        head = globbuf.gl_pathv[0];
        result = scalloc(strlen(head) + (tail ? strlen(tail) : 0) + 1);
        strcpy(result, head);
        if (tail) {
            strcat(result, tail);
        }
    }
    globfree(&globbuf);

    return result;
}

char *sstrdup(const char *str) {
    if (str == NULL) {
        return NULL;
    }
    char *result = strdup(str);
    exit_if_null(result, "Error: out of memory (strdup())\n");
    return result;
}

void *scalloc(size_t size) {
    void *result = calloc(size, 1);
    exit_if_null(result, "Error: out of memory (calloc(%zu))\n", size);
    return result;
}

/*
 * Skip the given character for exactly 'amount' times, returns
 * a pointer to the first non-'character' character in 'input'.
 *
 */
char *skip_character(char *input, char character, int amount) {
    char *walk;
    size_t len = strlen(input);
    int blanks = 0;

    for (walk = input; ((size_t)(walk - input) < len) && (blanks < amount); walk++)
        if (*walk == character)
            blanks++;

    return (walk == input ? walk : walk - 1);
}

/*
 * Write errormessage to statusbar and exit
 *
 */
void die(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    (void)vfprintf(stderr, fmt, ap);
    va_end(ap);

    exit(EXIT_FAILURE);
}
