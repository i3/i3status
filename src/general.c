// vim:ts=8:expandtab
#include <sys/types.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/fcntl.h>

#include "i3status.h"

/*
 * Reads size bytes into the destination buffer from filename.
 *
 */
void slurp(char *filename, char *destination, int size) {
        int fd;

        if ((fd = open(filename, O_RDONLY)) == -1)
                die("Could not open \"%s\"\n", filename);

        (void)read(fd, destination, size);
        (void)close(fd);
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

        return (walk == input ? walk : walk-1);
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

        if (wmii_path != NULL)
                write_error_to_statusbar(buffer);
        else
                fprintf(stderr, "%s", buffer);
        exit(EXIT_FAILURE);
}

/*
 * This function just concats two strings in place, it should only be used
 * for concatting order to the name of a file or concatting color codes.
 * Otherwise, the buffer size would have to be increased.
 *
 */
char *order_to_str(int number, char *name) {
        static char buf[32];
        (void)snprintf(buf, sizeof(buf), "%d%s", number, name);
        return buf;
}
