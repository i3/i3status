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

#if defined(__APPLE__)
#include <CoreAudio/CoreAudio.h>
#endif

#include "i3status.h"
#include "queue.h"

void print_volume(yajl_gen json_gen, char *buffer, const char *fmt, const char *fmt_muted, const char *device, const char *mixer, int mixer_idx) {
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
	if ((err = snd_mixer_attach(m, device)) < 0) {
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

	snd_mixer_handle_events (m);
	snd_mixer_selem_get_playback_volume (elem, 0, &val);
	if (max != 100) {
		float avgf = ((float)val / max) * 100;
		avg = (int)avgf;
		avg = (avgf - avg < 0.5 ? avg : (avg+1));
	} else avg = (int)val;

	/* Check for mute */
	if (snd_mixer_selem_has_playback_switch(elem)) {
		if ((err = snd_mixer_selem_get_playback_switch(elem, 0, &pbval)) < 0)
			fprintf (stderr, "i3status: ALSA: playback_switch: %s\n", snd_strerror(err));
		if (!pbval)  {
			START_COLOR("color_degraded");
			fmt = fmt_muted;
		}
	}

	snd_mixer_close(m);
	snd_mixer_selem_id_free(sid);

	const char *walk = fmt;
	for (; *walk != '\0'; walk++) {
		if (*walk != '%') {
                        *(outwalk++) = *walk;
			continue;
		}
		if (BEGINS_WITH(walk+1, "%")) {
			outwalk += sprintf(outwalk, "%%");
			walk += strlen("%");
		}
		if (BEGINS_WITH(walk+1, "volume")) {
			outwalk += sprintf(outwalk, "%d%%", avg);
			walk += strlen("volume");
		}
	}
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

        if ((mixfd = open(mixerpath, O_RDWR)) < 0)
                return;

        if (mixer_idx > 0)
                free(mixerpath);

        if (ioctl(mixfd, SOUND_MIXER_READ_DEVMASK, &devmask) == -1)
                return;
        if (ioctl(mixfd, MIXER_READ(0),&vol) == -1)
                return;

        if (((vol & 0x7f) == 0) && (((vol >> 8) & 0x7f) == 0)) {
                START_COLOR("color_degraded");
                pbval = 0;
        }

        const char *walk = fmt;
        for (; *walk != '\0'; walk++) {
                if (*walk != '%') {
                        *(outwalk++) = *walk;
                        continue;
                }
                if (BEGINS_WITH(walk+1, "%")) {
                        outwalk += sprintf(outwalk, "%%");
                        walk += strlen("%");
                }
                if (BEGINS_WITH(walk+1, "volume")) {
                        outwalk += sprintf(outwalk, "%d%%", vol & 0x7f);
                        walk += strlen("volume");
                }
        }
        close(mixfd);
#endif
#if defined(__APPLE__)
	UInt32 size = 0;
	OSStatus result = noErr;
	AudioDeviceID audiodevice = 0;
	AudioObjectPropertyAddress property = {0, 0, 0};

	if (BEGINS_WITH(device, "default")) {
		size = sizeof(audiodevice);
		property.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
		property.mScope    = kAudioObjectPropertyScopeGlobal;
		property.mElement  = kAudioObjectPropertyElementMaster;
		result = AudioObjectGetPropertyData(kAudioObjectSystemObject, &property, 0, NULL, &size, &audiodevice);
		if (result) {
			fprintf(stderr, "i3status: CoreAudio: Cannot to open %s output device: %d\n", device, result);
			goto out;
		}
	}
	else {
		// Get the number of audio devices on the system.
		property.mSelector = kAudioHardwarePropertyDevices;
		property.mScope    = kAudioObjectPropertyScopeGlobal;
		property.mElement  = kAudioObjectPropertyElementMaster;
		result = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &property, 0, NULL, &size);
		if (result) {
			fprintf(stderr, "Error getting audio property data size: %d\n", result);
			goto out;
		}
		UInt32 devices = size / sizeof(AudioDeviceID);

		// Get the list of audio devices.
		AudioDeviceID* devicelist = (AudioDeviceID*)calloc(devices, sizeof(AudioDeviceID));
		result = AudioObjectGetPropertyData(kAudioObjectSystemObject, &property, 0, NULL, &size, devicelist);
		if (result) {
			fprintf(stderr, "Error getting audio device list due to: %d\n", result);
			free(devicelist);
			goto out;
		}

		// Search if the list of audio devices contanis a device with the same name as  given in the device parameter.
		CFStringRef devicename = NULL;
		size = sizeof(CFStringRef);
		property.mSelector = kAudioObjectPropertyName;
		property.mScope    = kAudioObjectPropertyScopeGlobal;
		property.mElement  = kAudioObjectPropertyElementMaster;
		for (UInt32 i = 0; i < devices; i++) {
			result = AudioObjectGetPropertyData(devicelist[i], &property, 0, NULL, &size, &devicename);
			if (result) {
				continue;
			}
			CFIndex length = CFStringGetLength(devicename);
			CFIndex maxsize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8);
			char *name = (char *)malloc(maxsize);
			if (CFStringGetCString(devicename, name, maxsize, kCFStringEncodingUTF8)) {
				if (BEGINS_WITH(name, device)) {
					audiodevice = devicelist[i];
					free(name);
					break;
				}
			}
			free(name);
		}
		free(devicelist);
	}

	if (!audiodevice) {
		fprintf(stderr, "i3status: CoreAudio: No such audio device: %s\n", device);
		goto out;
	}

	property.mSelector = kAudioDevicePropertyStreams;
	property.mScope    = kAudioObjectPropertyScopeInput;
	property.mElement  = kAudioObjectPropertyElementMaster;
	result = AudioObjectGetPropertyDataSize(audiodevice, &property, 0, NULL, &size);
	if (result) {
		fprintf(stderr, "i3status: CoreAudio: Cannot determine stream configuration: %d\n", result);
	}

	bool input = ((size / sizeof(AudioStreamID)) > 0);
	AudioObjectPropertyScope scope = (input) ? kAudioObjectPropertyScopeInput : kAudioObjectPropertyScopeOutput;

	Float32 volume = 0;
	size = sizeof(Float32);
	property.mSelector = kAudioDevicePropertyVolumeScalar;
	property.mScope    = scope;
	property.mElement  = (input) ? kAudioObjectPropertyElementMaster : 1;
	result = AudioObjectGetPropertyData(audiodevice, &property, 0, NULL, &size, &volume);
	if (result) {
		fprintf(stderr, "i3status: CoreAudio: Cannot determine device volume: %d\n", result);
		goto out;
	}

	UInt32 mute = 0;
	size = sizeof(UInt32);
	property.mSelector = kAudioDevicePropertyMute;
	property.mScope    = scope;
	property.mElement  = kAudioObjectPropertyElementMaster;
	result = AudioObjectGetPropertyData(audiodevice, &property, 0, NULL, &size, &mute);
	if (result) {
		fprintf(stderr, "i3status: CoreAudio: Cannot determine mute state: %d\n", result);
		goto out;
	}
	else if (mute) {
		START_COLOR("color_degraded");
		fmt = fmt_muted;
		pbval = 0;
	}

	const char *walk = fmt;
	for (; *walk != '\0'; walk++) {
		if (*walk != '%') {
			*(outwalk++) = *walk;
			continue;
		}
		if (BEGINS_WITH(walk+1, "%")) {
			outwalk += sprintf(outwalk, "%%");
			walk += strlen("%");
		}
		if (BEGINS_WITH(walk+1, "volume")) {
			outwalk += sprintf(outwalk, "%d%%", (int)(volume * 100));
			walk += strlen("volume");
		}
	}
#endif

out:
        *outwalk = '\0';
        if (!pbval)
		END_COLOR;
        OUTPUT_FULL_TEXT(buffer);
}
