// vim:ts=8:expandtab
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>

#ifdef LINUX
#include <alsa/asoundlib.h>
#include <alloca.h>
#endif

#ifdef __FreeBSD__
#include <fcntl.h>
#include <unistd.h>
#include <sys/soundcard.h>
#endif

#ifdef __OpenBSD__
#include <fcntl.h>
#include <unistd.h>
#include <soundcard.h>
#endif

#include "i3status.h"
#include "queue.h"

#ifdef LINUX
struct mixer_hdl {
	char *device;
	char *mixer;
	int mixer_idx;
	snd_mixer_selem_id_t *sid;
	snd_mixer_t *m;
	snd_mixer_elem_t *elem;
	long min;
	long max;

	TAILQ_ENTRY(mixer_hdl) handles;
};

TAILQ_HEAD(handles_head, mixer_hdl) cached = TAILQ_HEAD_INITIALIZER(cached);

static void free_hdl(struct mixer_hdl *hdl) {
	free(hdl->device);
	free(hdl->mixer);
	free(hdl);
}
#endif

void print_volume(yajl_gen json_gen, char *buffer, const char *fmt, const char *device, const char *mixer, int mixer_idx) {
        char *outwalk = buffer;
	int pbval = 1;

        /* Printing volume only works with ALSA at the moment */
        if (output_format == O_I3BAR) {
                char *instance;
                asprintf(&instance, "%s.%s.%d", device, mixer, mixer_idx);
                INSTANCE(instance);
                free(instance);
        }
#ifdef LINUX
	/* Check if we already opened the mixer and get the handle
	 * from cache if so */
	bool found = false;
	int err;
	struct mixer_hdl *hdl;
	TAILQ_FOREACH(hdl, &cached, handles) {
		if (strcmp(hdl->device, device) != 0 ||
		    strcmp(hdl->mixer, mixer) != 0 ||
		    hdl->mixer_idx != mixer_idx)
			continue;
		found = true;
		break;
	}

	if (!found) {
		if ((hdl = calloc(sizeof(struct mixer_hdl), 1)) == NULL)
			return;

		if ((hdl->device = strdup(device)) == NULL) {
			free(hdl);
			return;
		}

		if ((hdl->mixer = strdup(mixer)) == NULL) {
			free(hdl->device);
			free(hdl);
			return;
		}

		hdl->mixer_idx = mixer_idx;
		snd_mixer_selem_id_malloc(&(hdl->sid));
		if (hdl->sid == NULL) {
			free_hdl(hdl);
			return;
		}

		if ((err = snd_mixer_open(&(hdl->m), 0)) < 0) {
			fprintf(stderr, "i3status: ALSA: Cannot open mixer: %s\n", snd_strerror(err));
			free_hdl(hdl);
			return;
		}

		/* Attach this mixer handle to the given device */
		if ((err = snd_mixer_attach(hdl->m, device)) < 0) {
			fprintf(stderr, "i3status: ALSA: Cannot attach mixer to device: %s\n", snd_strerror(err));
			snd_mixer_close(hdl->m);
			free_hdl(hdl);
			return;
		}

		/* Register this mixer */
		if ((err = snd_mixer_selem_register(hdl->m, NULL, NULL)) < 0) {
			fprintf(stderr, "i3status: ALSA: snd_mixer_selem_register: %s\n", snd_strerror(err));
			snd_mixer_close(hdl->m);
			free_hdl(hdl);
			return;
		}

		if ((err = snd_mixer_load(hdl->m)) < 0) {
			fprintf(stderr, "i3status: ALSA: snd_mixer_load: %s\n", snd_strerror(err));
			snd_mixer_close(hdl->m);
			free_hdl(hdl);
			return;
		}

		/* Find the given mixer */
		snd_mixer_selem_id_set_index(hdl->sid, mixer_idx);
		snd_mixer_selem_id_set_name(hdl->sid, mixer);
		if (!(hdl->elem = snd_mixer_find_selem(hdl->m, hdl->sid))) {
			fprintf(stderr, "i3status: ALSA: Cannot find mixer %s (index %i)\n",
				snd_mixer_selem_id_get_name(hdl->sid), snd_mixer_selem_id_get_index(hdl->sid));
			snd_mixer_close(hdl->m);
			free_hdl(hdl);
			return;
		}

		/* Get the volume range to convert the volume later */
		snd_mixer_selem_get_playback_volume_range(hdl->elem, &(hdl->min), &(hdl->max));
		TAILQ_INSERT_TAIL(&cached, hdl, handles);
	}

	long val;
	snd_mixer_handle_events (hdl->m);
	snd_mixer_selem_get_playback_volume (hdl->elem, 0, &val);
	int avg;
	if (hdl->max != 100) {
		float avgf = ((float)val / hdl->max) * 100;
		avg = (int)avgf;
		avg = (avgf - avg < 0.5 ? avg : (avg+1));
	} else avg = (int)val;

	/* Check for mute */
	if (snd_mixer_selem_has_playback_switch(hdl->elem)) {
		if ((err = snd_mixer_selem_get_playback_switch(hdl->elem, 0, &pbval)) < 0)
			fprintf (stderr, "i3status: ALSA: playback_switch: %s\n", snd_strerror(err));
		if (!pbval)  {
			START_COLOR("color_bad");
			avg = 0;
		}
	}

	const char *walk = fmt;
	for (; *walk != '\0'; walk++) {
		if (*walk != '%') {
                        *(outwalk++) = *walk;
			continue;
		}
		if (BEGINS_WITH(walk+1, "volume")) {
			outwalk += sprintf(outwalk, "%d%%", avg);
			walk += strlen("volume");
		}
	}
#endif
#if defined(__FreeBSD__) || defined(__OpenBSD__)
        char mixerpath[] = "/dev/mixer";
        int mixfd, vol, devmask = 0;

        if ((mixfd = open(mixerpath, O_RDWR)) < 0)
                return;
        if (ioctl(mixfd, SOUND_MIXER_READ_DEVMASK, &devmask) == -1)
                return;
        if (ioctl(mixfd, MIXER_READ(0),&vol) == -1)
                return;

        const char *walk = fmt;
        for (; *walk != '\0'; walk++) {
                if (*walk != '%') {
                        *(outwalk++) = *walk;
                        continue;
                }
                if (BEGINS_WITH(walk+1, "volume")) {
                        outwalk += sprintf(outwalk, "%d%%", vol & 0x7f);
                        walk += strlen("volume");
                }
        }
        close(mixfd);
#endif

        *outwalk = '\0';
        OUTPUT_FULL_TEXT(buffer);

        if (!pbval)
		END_COLOR;
}
