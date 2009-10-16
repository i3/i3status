#include <stdio.h>
#include <string.h>
#include "i3status.h"

void print_run_watch(const char *title, const char *pidfile, const char *format) {
	bool running = process_runs(pidfile);
	const char *walk;

	printf("%s", (running ? color("#00FF00") : color("#FF0000")));

        for (walk = format; *walk != '\0'; walk++) {
                if (*walk != '%') {
                        putchar(*walk);
                        continue;
                }

                if (strncmp(walk+1, "title", strlen("title")) == 0) {
                        printf("%s", title);
                        walk += strlen("title");
                } else if (strncmp(walk+1, "status", strlen("status")) == 0) {
                        printf("%s", (running ? "yes" : "no"));
                        walk += strlen("status");
                }
        }

	printf("%s", endcolor());
}
