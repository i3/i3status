#include <stdio.h>
#include "i3status.h"

void print_run_watch(const char *title, const char *pidfile, const char *format) {
	bool running = process_runs(pidfile);
	printf("%s%s: %s%s",
		(running ? color("#00FF00") : color("#FF0000")),
		title,
		(running ? "yes" : "no"), endcolor());
}
