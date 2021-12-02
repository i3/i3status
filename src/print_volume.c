// vim:ts=4:sw=4:expandtab
#include <config.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <ctype.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>

#ifdef __linux__
#include <alsa/asoundlib.h>
#include <alloca.h>
#include <math.h>
#endif

#if defined(__FreeBSD__) || defined(__DragonFly__)
#include <fcntl.h>
#include <unistd.h>
#include <sys/soundcard.h>
#endif

#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <fcntl.h>
#include <unistd.h>
#include <sys/audioio.h>
#include <sys/ioctl.h>
#endif

#include "i3status.h"
#include "queue.h"

#define STRING_SIZE 10

#define ALSA_VOLUME(channel)                                                    \
    err = snd_mixer_selem_get_##channel##_dB_range(elem, &min, &max) ||         \
          snd_mixer_selem_get_##channel##_dB(elem, 0, &val);                    \
    if (err != 0 || min >= max) {                                               \
        err = snd_mixer_selem_get_##channel##_volume_range(elem, &min, &max) || \
              snd_mixer_selem_get_##channel##_volume(elem, 0, &val);            \
        force_linear = true;                                                    \
    }

#define ALSA_MUTE_SWITCH(channel)                                                        \
    if ((err = snd_mixer_selem_get_##channel##_switch(elem, 0, &pbval)) < 0)             \
        fprintf(stderr, "i3status: ALSA: " #channel "_switch: %s\n", snd_strerror(err)); \
    if (!pbval) {                                                                        \
        START_COLOR("color_degraded");                                                   \
        ctx->fmt = ctx->fmt_muted;                                                       \
    }

static char *apply_volume_format(const char *fmt, int ivolume, const char *devicename) {
    char string_volume[STRING_SIZE];

    snprintf(string_volume, STRING_SIZE, "%d%s", ivolume, pct_mark);

    placeholder_t placeholders[] = {
        {.name = "%%", .value = pct_mark},
        {.name = "%volume", .value = string_volume},
        {.name = "%devicename", .value = devicename}};

    const size_t num = sizeof(placeholders) / sizeof(placeholder_t);
    return format_placeholders(fmt, &placeholders[0], num);
}

void print_volume(volume_ctx_t *ctx) {
    char *outwalk = ctx->buf;
    int pbval = 1;

    /* Printing volume works with ALSA and PulseAudio at the moment */
    if (output_format == O_I3BAR) {
        char *instance;
        asprintf(&instance, "%s.%s.%d", ctx->device, ctx->mixer, ctx->mixer_idx);
        INSTANCE(instance);
        free(instance);
    }

#if HAS_PULSEAUDIO
    /* Try PulseAudio first */

    /* If the device name has the format "pulse[:N]" where N is the
     * index of the PulseAudio sink then force PulseAudio, optionally
     * overriding the default sink */
    if (!strncasecmp(ctx->device, "pulse", strlen("pulse"))) {
        uint32_t sink_idx = ctx->device[strlen("pulse")] == ':' ? (uint32_t)atoi(ctx->device + strlen("pulse:")) : DEFAULT_SINK_INDEX;
        const char *sink_name = ctx->device[strlen("pulse")] == ':' &&
                                        !isdigit(ctx->device[strlen("pulse:")])
                                    ? ctx->device + strlen("pulse:")
                                    : NULL;
        int cvolume = 0;
        char description[MAX_SINK_DESCRIPTION_LEN] = {'\0'};

        if (pulse_initialize()) {
            cvolume = volume_pulseaudio(sink_idx, sink_name);
            /* false result means error, stick to empty-string */
            if (!description_pulseaudio(sink_idx, sink_name, description)) {
                description[0] = '\0';
            }
        }

        int ivolume = DECOMPOSE_VOLUME(cvolume);
        bool muted = DECOMPOSE_MUTED(cvolume);
        if (muted) {
            START_COLOR("color_degraded");
            pbval = 0;
        }

        /* negative result means error, stick to 0 */
        if (ivolume < 0)
            ivolume = 0;
        char *formatted = apply_volume_format(muted ? ctx->fmt_muted : ctx->fmt,
                                              ivolume,
                                              description);
        OUTPUT_FORMATTED;
        free(formatted);
        goto out_with_format;
    } else if (!strcasecmp(ctx->device, "default") && pulse_initialize()) {
        /* no device specified or "default" set */
        char description[MAX_SINK_DESCRIPTION_LEN];
        bool success = description_pulseaudio(DEFAULT_SINK_INDEX, NULL, description);
        int cvolume = volume_pulseaudio(DEFAULT_SINK_INDEX, NULL);
        int ivolume = DECOMPOSE_VOLUME(cvolume);
        bool muted = DECOMPOSE_MUTED(cvolume);
        if (ivolume >= 0 && success) {
            if (muted) {
                START_COLOR("color_degraded");
                pbval = 0;
            }
            char *formatted = apply_volume_format(muted ? ctx->fmt_muted : ctx->fmt,
                                                  ivolume,
                                                  description);
            OUTPUT_FORMATTED;
            free(formatted);
            goto out_with_format;
        }
        /* negative result or NULL description means error, fail PulseAudio attempt */
    }
/* If some other device was specified or PulseAudio is not detected,
 * proceed to ALSA / OSS */
#endif

#ifdef __linux__
    const long MAX_LINEAR_DB_SCALE = 24;
    int err;
    snd_mixer_t *m;
    snd_mixer_selem_id_t *sid;
    snd_mixer_elem_t *elem;
    long min, max, val;
    const char *mixer_name;
    bool force_linear = false;
    int avg;

    if ((err = snd_mixer_open(&m, 0)) < 0) {
        fprintf(stderr, "i3status: ALSA: Cannot open mixer: %s\n", snd_strerror(err));
        goto out;
    }

    /* Attach this mixer handle to the given device */
    if ((err = snd_mixer_attach(m, ctx->device)) < 0) {
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
    snd_mixer_selem_id_set_index(sid, ctx->mixer_idx);
    snd_mixer_selem_id_set_name(sid, ctx->mixer);
    if (!(elem = snd_mixer_find_selem(m, sid))) {
        fprintf(stderr, "i3status: ALSA: Cannot find mixer %s (index %u)\n",
                snd_mixer_selem_id_get_name(sid), snd_mixer_selem_id_get_index(sid));
        snd_mixer_close(m);
        snd_mixer_selem_id_free(sid);
        goto out;
    }

    /* Get the volume range to convert the volume later */
    snd_mixer_handle_events(m);
    if (!strncasecmp(ctx->mixer, "capture", strlen("capture"))) {
        ALSA_VOLUME(capture)
    } else {
        ALSA_VOLUME(playback)
    }

    if (err != 0) {
        fprintf(stderr, "i3status: ALSA: Cannot get playback volume.\n");
        goto out;
    }

    mixer_name = snd_mixer_selem_get_name(elem);
    if (!mixer_name) {
        fprintf(stderr, "i3status: ALSA: NULL mixer_name.\n");
        goto out;
    }

    /* Use linear mapping for raw register values or small ranges of 24 dB */
    if (force_linear || max - min <= MAX_LINEAR_DB_SCALE * 100) {
        float avgf = ((float)(val - min) / (max - min)) * 100;
        avg = (int)avgf;
        avg = (avgf - avg < 0.5 ? avg : (avg + 1));
    } else {
        /* mapped volume to be more natural for the human ear */
        double normalized = exp10((val - max) / 6000.0);
        if (min != SND_CTL_TLV_DB_GAIN_MUTE) {
            double min_norm = exp10((min - max) / 6000.0);
            normalized = (normalized - min_norm) / (1 - min_norm);
        }
        avg = lround(normalized * 100);
    }

    /* Check for mute */
    if (snd_mixer_selem_has_playback_switch(elem)) {
        ALSA_MUTE_SWITCH(playback)
    } else if (snd_mixer_selem_has_capture_switch(elem)) {
        ALSA_MUTE_SWITCH(capture)
    }

    char *formatted = apply_volume_format(ctx->fmt, avg, mixer_name);
    OUTPUT_FORMATTED;
    free(formatted);

    snd_mixer_close(m);
    snd_mixer_selem_id_free(sid);
    goto out_with_format;

#endif
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
    char *mixerpath;
    char defaultmixer[] = "/dev/mixer";
    int mixfd, vol, devmask = 0;
    const char *devicename = "UNSUPPORTED"; /* TODO: implement support for this */
    pbval = 1;

    if (ctx->mixer_idx > 0)
        asprintf(&mixerpath, "/dev/mixer%d", ctx->mixer_idx);
    else
        mixerpath = defaultmixer;

    if ((mixfd = open(mixerpath, O_RDWR)) < 0) {
#if defined(__NetBSD__) || defined(__OpenBSD__)
        warn("audioio: Cannot open mixer");
#else
        warn("OSS: Cannot open mixer");
#endif
        goto out;
    }

    if (ctx->mixer_idx > 0)
        free(mixerpath);

#if defined(__NetBSD__) || defined(__OpenBSD__)
    int oclass_idx = -1, master_idx = -1, master_mute_idx = -1;
    int master_next = AUDIO_MIXER_LAST;
    mixer_devinfo_t devinfo, devinfo2;
    mixer_ctrl_t vinfo;

    devinfo.index = 0;
    while (ioctl(mixfd, AUDIO_MIXER_DEVINFO, &devinfo) >= 0) {
        if (devinfo.type != AUDIO_MIXER_CLASS) {
            devinfo.index++;
            continue;
        }
        if (strncmp(devinfo.label.name, AudioCoutputs, MAX_AUDIO_DEV_LEN) == 0)
            oclass_idx = devinfo.index;

        devinfo.index++;
    }

    devinfo2.index = 0;
    while (ioctl(mixfd, AUDIO_MIXER_DEVINFO, &devinfo2) >= 0) {
        if ((devinfo2.type == AUDIO_MIXER_VALUE) && (devinfo2.mixer_class == oclass_idx) && (strncmp(devinfo2.label.name, AudioNmaster, MAX_AUDIO_DEV_LEN) == 0)) {
            master_idx = devinfo2.index;
            master_next = devinfo2.next;
        }

        if ((devinfo2.type == AUDIO_MIXER_ENUM) && (devinfo2.mixer_class == oclass_idx) && (strncmp(devinfo2.label.name, AudioNmute, MAX_AUDIO_DEV_LEN) == 0))
            if (master_next == devinfo2.index)
                master_mute_idx = devinfo2.index;

        if (master_next != AUDIO_MIXER_LAST)
            master_next = devinfo2.next;
        devinfo2.index++;
    }

    if (master_idx == -1)
        goto out;

    devinfo.index = master_idx;
    if (ioctl(mixfd, AUDIO_MIXER_DEVINFO, &devinfo) == -1)
        goto out;

    vinfo.dev = master_idx;
    vinfo.type = AUDIO_MIXER_VALUE;
    vinfo.un.value.num_channels = devinfo.un.v.num_channels;
    if (ioctl(mixfd, AUDIO_MIXER_READ, &vinfo) == -1)
        goto out;

    if (AUDIO_MAX_GAIN != 100) {
        float avgf = ((float)vinfo.un.value.level[AUDIO_MIXER_LEVEL_MONO] / AUDIO_MAX_GAIN) * 100;
        vol = (int)avgf;
        vol = (avgf - vol < 0.5 ? vol : (vol + 1));
    } else {
        vol = (int)vinfo.un.value.level[AUDIO_MIXER_LEVEL_MONO];
    }

    if (master_mute_idx != -1) {
        vinfo.dev = master_mute_idx;
        vinfo.type = AUDIO_MIXER_ENUM;
        if (ioctl(mixfd, AUDIO_MIXER_READ, &vinfo) == -1)
            goto out;

        if (vinfo.un.ord) {
            START_COLOR("color_degraded");
            fmt = fmt_muted;
            pbval = 0;
        }
    }

#else
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

#endif
    ctx->buf = apply_volume_format(ctx->fmt, vol & 0x7f, devicename);
    close(mixfd);
    goto out_with_format;
#endif

out:
    if (!pbval)
        END_COLOR;
    OUTPUT_FULL_TEXT(ctx->buf);
    return;

out_with_format:
    if (!pbval)
        END_COLOR;
    OUTPUT_FULL_TEXT(ctx->buf);
}
