// vim:ts=4:sw=4:expandtab
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>

#ifdef LINUX
#include <alsa/asoundlib.h>
#include <alloca.h>
#endif

#if defined(__FreeBSD__) || defined(__DragonFly__)
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

static char *apply_volume_format(const char *fmt, char *outwalk, int ivolume) {
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
        if (BEGINS_WITH(walk + 1, "volume")) {
            outwalk += sprintf(outwalk, "%d%%", ivolume);
            walk += strlen("volume");
        }
    }
    return outwalk;
}

void print_volume(yajl_gen json_gen, char *buffer, const char *fmt, const char *fmt_muted, const char *device, const char *mixer, int mixer_idx) {
    char *outwalk = buffer;
    int pbval = 1;

    /* Printing volume works with ALSA and PulseAudio at the moment */
    if (output_format == O_I3BAR) {
        char *instance;
        asprintf(&instance, "%s.%s.%d", device, mixer, mixer_idx);
        INSTANCE(instance);
        free(instance);
    }

    /* Try PulseAudio first */

    /* If the device name has the format "pulse[:N]" where N is the
     * index of the PulseAudio sink then force PulseAudio, optionally
     * overriding the default sink */
    if (!strncasecmp(device, "pulse", strlen("pulse"))) {
        uint32_t sink_idx = device[5] == ':' ? (uint32_t)atoi(device + 6)
                                             : DEFAULT_SINK_INDEX;
        int ivolume = pulse_initialize() ? volume_pulseaudio(sink_idx) : 0;
        /* negative result means error, stick to 0 */
        if (ivolume < 0)
            ivolume = 0;
        outwalk = apply_volume_format(fmt, outwalk, ivolume);
        goto out;
    } else if (!strcasecmp(device, "default") && pulse_initialize()) {
        /* no device specified or "default" set */
        int ivolume = volume_pulseaudio(DEFAULT_SINK_INDEX);
        if (ivolume >= 0) {
            outwalk = apply_volume_format(fmt, outwalk, ivolume);
            goto out;
        }
        /* negative result means error, fail PulseAudio attempt */
    }
/* If some other device was specified or PulseAudio is not detected,
 * proceed to ALSA / OSS */

#ifdef LINUX
    int err;
    snd_mixer_t *m;
    snd_mixer_selem_id_t *sid;
    snd_mixer_elem_t *elem;
    long min, max, val;
    int avg;

    if ((err = snd_mixer_open(&m, 0)) < 0) {
        fprintf(stderr, "i3status: ALSA: Cannot open mixer: %s\n", snd_strerror(err));
        goto out;
    }

    /* Attach this mixer handle to the given device */
    if (!strcasecmp(device, "alsa") ? (err = snd_mixer_attach(m, "pulse")) < 0 : (err = snd_mixer_attach(m, device)) < 0) {
        fprintf(stderr, "i3status: ALSA: Cannot attach mixer to device: %s\n", snd_strerror(err));
        snd_mixer_close(m);
        goto out;
    }

    /* Register this mixer */
    if ((err = snd_mixer_selem_register(m, NULL, NULL)) < 0) {
        fprintf(stderr, "i3status: ALSA: snd_mixer_selem_register: %s\n", snd_strerror(err));
        snd_mixer_close(m);
        goto out;
    }

    if ((err = snd_mixer_load(m)) < 0) {
        fprintf(stderr, "i3status: ALSA: snd_mixer_load: %s\n", snd_strerror(err));
        snd_mixer_close(m);
        goto out;
    }

    snd_mixer_selem_id_malloc(&sid);
    if (sid == NULL) {
        snd_mixer_close(m);
        goto out;
    }

    /* Find the given mixer */
    snd_mixer_selem_id_set_index(sid, mixer_idx);
    snd_mixer_selem_id_set_name(sid, mixer);
    if (!(elem = snd_mixer_find_selem(m, sid))) {
        fprintf(stderr, "i3status: ALSA: Cannot find mixer %s (index %i)\n",
                snd_mixer_selem_id_get_name(sid), snd_mixer_selem_id_get_index(sid));
        snd_mixer_close(m);
        snd_mixer_selem_id_free(sid);
        goto out;
    }

    /* Get the volume range to convert the volume later */
    snd_mixer_selem_get_playback_volume_range(elem, &min, &max);

    snd_mixer_handle_events(m);
    snd_mixer_selem_get_playback_volume(elem, 0, &val);
    if (max != 100) {
        float avgf = ((float)val / max) * 100;
        avg = (int)avgf;
        avg = (avgf - avg < 0.5 ? avg : (avg + 1));
    } else
        avg = (int)val;

    /* Check for mute */
    if (snd_mixer_selem_has_playback_switch(elem)) {
        if ((err = snd_mixer_selem_get_playback_switch(elem, 0, &pbval)) < 0)
            fprintf(stderr, "i3status: ALSA: playback_switch: %s\n", snd_strerror(err));
        if (!pbval) {
            START_COLOR("color_degraded");
            fmt = fmt_muted;
        }
    }

    snd_mixer_close(m);
    snd_mixer_selem_id_free(sid);

    outwalk = apply_volume_format(fmt, outwalk, avg);

#endif
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
    char *mixerpath;
    char defaultmixer[] = "/dev/mixer";
    int mixfd, vol, devmask = 0;
    pbval = 1;

    if (mixer_idx > 0)
        asprintf(&mixerpath, "/dev/mixer%d", mixer_idx);
    else
        mixerpath = defaultmixer;

    if ((mixfd = open(mixerpath, O_RDWR)) < 0) {
        warn("OSS: Cannot open mixer");
        goto out;
    }

    if (mixer_idx > 0)
        free(mixerpath);

    if (ioctl(mixfd, SOUND_MIXER_READ_DEVMASK, &devmask) == -1) {
        warn("OSS: Cannot read mixer information");
        goto out;
    }
    if (ioctl(mixfd, MIXER_READ(0), &vol) == -1) {
        warn("OSS: Cannot read mixer information");
        goto out;
    }

    if (((vol & 0x7f) == 0) && (((vol >> 8) & 0x7f) == 0)) {
        START_COLOR("color_degraded");
        pbval = 0;
    }

    outwalk = apply_volume_format(fmt, outwalk, vol & 0x7f);
    close(mixfd);
#endif

out:
    *outwalk = '\0';
    if (!pbval)
        END_COLOR;
    OUTPUT_FULL_TEXT(buffer);
}
