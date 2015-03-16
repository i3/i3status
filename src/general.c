// vim:ts=4:sw=4:expandtab
#include <sys/types.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/stat.h>

#include "i3status.h"

/*
 * Reads size bytes into the destination buffer from filename.
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

    return true;
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
    char buffer[512];
    va_list ap;
    va_start(ap, fmt);
    (void)vsnprintf(buffer, sizeof(buffer), fmt, ap);
    va_end(ap);

    fprintf(stderr, "%s", buffer);
    exit(EXIT_FAILURE);
}
