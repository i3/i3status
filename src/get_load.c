#include "i3status.h"

const char *get_load() {
	static char part[512];

/* Get load */
#ifdef LINUX
	slurp("/proc/loadavg", part, sizeof(part));
	*skip_character(part, ' ', 3) = '\0';
#else
	/* TODO: correctly check for NetBSD, check if it works the same on *BSD */
	struct loadavg load;
	size_t length = sizeof(struct loadavg);
	int mib[2] = { CTL_VM, VM_LOADAVG };
	if (sysctl(mib, 2, &load, &length, NULL, 0) < 0)
		die("Could not sysctl({ CTL_VM, VM_LOADAVG })\n");
	double scale = load.fscale;
	(void)snprintf(part, sizeof(part), "%.02f %.02f %.02f",
			(double)load.ldavg[0] / scale,
			(double)load.ldavg[1] / scale,
			(double)load.ldavg[2] / scale);
#endif

	return part;
}
