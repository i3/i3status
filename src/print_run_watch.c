#include <stdio.h>
#include <string.h>
#include "i3status.h"

void print_run_watch(const char *title, const char *pidfile, const char *format) {
	bool running = process_runs(pidfile);
	const char *walk;

        if (output_format == O_I3BAR)
                printf("{\"name\":\"run_watch\", \"instance\": \"%s\", ", pidfile);

	printf("%s", (running ? color("color_good") : color("color_bad")));

        if (output_format == O_I3BAR)
                printf("\"full_text\":\"");

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

        if (output_format == O_I3BAR)
                printf("\"}");
}
