// vim:ts=4:sw=4:expandtab
#include <config.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <signal.h>
#include <pulse/pulseaudio.h>
#include "i3status.h"
#include "queue.h"

#define APP_NAME "i3status"
#define APP_ID "org.i3wm"

typedef struct index_info_s {
    bool is_sink;
    char *name;
    uint32_t idx;
    int volume;
    char description[MAX_PULSE_DESCRIPTION_LEN];
    TAILQ_ENTRY(index_info_s)
    entries;
} index_info_t;

static pa_threaded_mainloop *main_loop = NULL;
static pa_context *context = NULL;
static pa_mainloop_api *api = NULL;
static bool context_ready = false;
static bool mainloop_thread_running = false;
static uint32_t default_sink_idx = DEFAULT_DEVICE_INDEX;
static uint32_t default_source_idx = DEFAULT_DEVICE_INDEX;
TAILQ_HEAD(tailhead, index_info_s)
cached_info =
    TAILQ_HEAD_INITIALIZER(cached_info);
static pthread_mutex_t pulse_mutex = PTHREAD_MUTEX_INITIALIZER;

static void pulseaudio_error_log(pa_context *c) {
    fprintf(stderr,
            "i3status: PulseAudio: %s\n",
            pa_strerror(pa_context_errno(c)));
}

static bool pulseaudio_free_operation(pa_context *c, pa_operation *o) {
    if (o)
        pa_operation_unref(o);
    else
        pulseaudio_error_log(c);
    /* return false if the operation failed */
    return o;
}

/*
 * save the info for the specified sink or source index
 * returning true if the value was changed
 */
static bool save_info(bool is_sink, uint32_t idx, int new_volume, const char *new_description, const char *name) {
    pthread_mutex_lock(&pulse_mutex);
    index_info_t *entry;

    /* if this is NULL, gracefully handle and replace with empty-string */
    if (!new_description) {
        new_description = "";
        fprintf(stderr, "i3status: PulseAudio: NULL new_description provided\n");
    }

    TAILQ_FOREACH(entry, &cached_info, entries) {
        if (entry->is_sink != is_sink)
            continue;

        if (name) {
            if (!entry->name || strcmp(entry->name, name)) {
                continue;
            }
        } else {
            if (entry->idx != idx) {
                continue;
            }
        }

        bool changed = false;

        if (new_volume != entry->volume) {
            entry->volume = new_volume;
            changed = true;
        }

        if (strncmp(entry->description, new_description, sizeof(entry->description))) {
            strncpy(entry->description, new_description, sizeof(entry->description) - 1);
            entry->description[sizeof(entry->description) - 1] = '\0';
            changed = true;
        }

        pthread_mutex_unlock(&pulse_mutex);
        return changed;
    }
    /* index not found, store it */
    entry = malloc(sizeof(*entry));
    TAILQ_INSERT_HEAD(&cached_info, entry, entries);
    entry->is_sink = is_sink;
    entry->idx = idx;
    entry->volume = new_volume;
    strncpy(entry->description, new_description, sizeof(entry->description) - 1);
    entry->description[sizeof(entry->description) - 1] = '\0';
    if (name) {
        entry->name = malloc(strlen(name) + 1);
        strcpy(entry->name, name);
    } else {
        entry->name = NULL;
    }
    pthread_mutex_unlock(&pulse_mutex);
    return true;
}

static void store_info_from_sink_cb(pa_context *c,
                                    const pa_sink_info *info,
                                    int eol,
                                    void *userdata) {
    if (eol < 0) {
        if (pa_context_errno(c) == PA_ERR_NOENTITY)
            return;

        pulseaudio_error_log(c);
        return;
    }

    if (eol > 0)
        return;

    int avg_vol = pa_cvolume_avg(&info->volume);
    int vol_perc = roundf((float)avg_vol * 100 / PA_VOLUME_NORM);
    int composed_volume = COMPOSE_VOLUME_MUTE(vol_perc, info->mute);

    const char *desc = (info->active_port != NULL) ? info->active_port->description : info->description;

    /* if this is the default sink or source we must try to save it twice: once with
     * DEFAULT_DEVICE_INDEX as the index, and another with its proper value
     * (using bitwise OR to avoid early-out logic) */
    if ((info->index == default_sink_idx &&
        save_info(true, DEFAULT_DEVICE_INDEX, composed_volume, desc, NULL)) |
        save_info(true, info->index, composed_volume, desc, info->name)) {
        /* if the volume, mute flag or description changed, wake the main thread */
        pthread_kill(main_thread, SIGUSR1);
    }
}

static void store_info_from_source_cb(pa_context *c,
                                      const pa_source_info *info,
                                      int eol,
                                      void *userdata) {
    if (eol < 0) {
        if (pa_context_errno(c) == PA_ERR_NOENTITY)
            return;

        pulseaudio_error_log(c);
        return;
    }

    if (eol > 0)
        return;

    int avg_vol = pa_cvolume_avg(&info->volume);
    int vol_perc = roundf((float)avg_vol * 100 / PA_VOLUME_NORM);
    int composed_volume = COMPOSE_VOLUME_MUTE(vol_perc, info->mute);

    /* if this is the default sink or source we must try to save it twice: once with
     * DEFAULT_DEVICE_INDEX as the index, and another with its proper value
     * (using bitwise OR to avoid early-out logic) */
    const char *desc = (info->active_port != NULL) ? info->active_port->description : info->description;

    if ((info->index == default_source_idx &&
        save_info(false, DEFAULT_DEVICE_INDEX, composed_volume, desc, NULL)) |
        save_info(false, info->index, composed_volume, desc, info->name)) {
        /* if the volume, mute flag or description changed, wake the main thread */
        pthread_kill(main_thread, SIGUSR1);
    }
}

static void get_device_info(pa_context *c, bool is_sink, uint32_t idx, const char *name) {
    pa_operation *o;

    if (name || idx == DEFAULT_DEVICE_INDEX) {
        if (is_sink) {
            o = pa_context_get_sink_info_by_name(
                c, name ? name : "@DEFAULT_SINK@", store_info_from_sink_cb, NULL);
        } else {
            o = pa_context_get_source_info_by_name(
                c, name ? name : "@DEFAULT_SOURCE@", store_info_from_source_cb, NULL);
        }
    } else {
        if (is_sink) {
            o = pa_context_get_sink_info_by_index(
                c, idx, store_info_from_sink_cb, NULL);
        } else {
            o = pa_context_get_source_info_by_index(
                c, idx, store_info_from_source_cb, NULL);
        }
    }
    pulseaudio_free_operation(c, o);
}

static void store_default_sink_cb(pa_context *c,
                                  const pa_sink_info *i,
                                  int eol,
                                  void *userdata) {
    if (i) {
        if (default_sink_idx != i->index) {
            /* default sink changed? */
            default_sink_idx = i->index;
            store_info_from_sink_cb(c, i, eol, userdata);
        }
    }
}

static void store_default_source_cb(pa_context *c,
                                    const pa_source_info *i,
                                    int eol,
                                    void *userdata) {
    if (i) {
        if (default_source_idx != i->index) {
            /* default source changed? */
            default_source_idx = i->index;
            store_info_from_source_cb(c, i, eol, userdata);
        }
    }
}

static void update_default_sink_source(pa_context *c) {
    pa_operation *o =  pa_context_get_sink_info_by_name(
        c,
        "@DEFAULT_SINK@",
        store_default_sink_cb,
        NULL);
    pulseaudio_free_operation(c, o);

    o =  pa_context_get_source_info_by_name(
        c,
        "@DEFAULT_SOURCE@",
        store_default_source_cb,
        NULL);

    pulseaudio_free_operation(c, o);
}

static void subscribe_cb(pa_context *c, pa_subscription_event_type_t t,
                         uint32_t idx, void *userdata) {
    if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) != PA_SUBSCRIPTION_EVENT_CHANGE)
        return;
    pa_subscription_event_type_t facility =
        t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK;
    switch (facility) {
        case PA_SUBSCRIPTION_EVENT_SERVER:
            /* server change event, see if the default sink or source changed */
            update_default_sink_source(c);
            break;
        case PA_SUBSCRIPTION_EVENT_SINK:
            get_device_info(c, true, idx, NULL);
            break;
        case PA_SUBSCRIPTION_EVENT_SOURCE:
            get_device_info(c, false, idx, NULL);
            break;
        default:
            break;
    }
}

static void context_state_callback(pa_context *c, void *userdata) {
    switch (pa_context_get_state(c)) {
        case PA_CONTEXT_UNCONNECTED:
        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
        case PA_CONTEXT_TERMINATED:
        default:
            context_ready = false;
            break;

        case PA_CONTEXT_READY: {
            pa_context_set_subscribe_callback(c, subscribe_cb, NULL);
            update_default_sink_source(c);

            pa_operation *o = pa_context_subscribe(
                c,
                PA_SUBSCRIPTION_MASK_SERVER | PA_SUBSCRIPTION_MASK_SINK | PA_SUBSCRIPTION_MASK_SOURCE,
                NULL,
                NULL);
            if (!pulseaudio_free_operation(c, o))
                break;
            context_ready = true;
        } break;

        case PA_CONTEXT_FAILED:
            /* server disconnected us, attempt to reconnect */
            context_ready = false;
            pa_context_unref(context);
            context = NULL;
            break;
    }
}

/*
 * returns the current volume in percent, which, as per PulseAudio,
 * may be > 100%
 */
int volume_pulseaudio(bool is_sink, uint32_t idx, const char *name) {
    if (!context_ready || 
        (is_sink && default_sink_idx == DEFAULT_DEVICE_INDEX) || 
        (!is_sink && default_source_idx == DEFAULT_DEVICE_INDEX))
        return -1;

    pthread_mutex_lock(&pulse_mutex);
    const index_info_t *entry;
    TAILQ_FOREACH(entry, &cached_info, entries) {
        if (entry->is_sink != is_sink)
            continue;

        if (name) {
            if (!entry->name || strcmp(entry->name, name)) {
                continue;
            }
        } else {
            if (entry->idx != idx) {
                continue;
            }
        }
        int vol = entry->volume;
        pthread_mutex_unlock(&pulse_mutex);
        return vol;
    }
    pthread_mutex_unlock(&pulse_mutex);
    /* first time requires a prime callback call because we only get updates
     * when the description or volume actually changes, but we need it to be
     * correct even if it never changes */
    pa_threaded_mainloop_lock(main_loop);
    get_device_info(context, is_sink, idx, name);
    pa_threaded_mainloop_unlock(main_loop);
    /* show 0 while we don't have this information */
    return 0;
}

bool description_pulseaudio(bool is_sink, uint32_t idx, const char *name, char buffer[MAX_PULSE_DESCRIPTION_LEN]) {
    if (!context_ready || 
        (is_sink && default_sink_idx == DEFAULT_DEVICE_INDEX) || 
        (!is_sink && default_source_idx == DEFAULT_DEVICE_INDEX))
        return -1;

    pthread_mutex_lock(&pulse_mutex);
    const index_info_t *entry;
    TAILQ_FOREACH(entry, &cached_info, entries) {
        if (entry->is_sink != is_sink)
            continue;

        if (name) {
            if (!entry->name || strcmp(entry->name, name)) {
                continue;
            }
        } else {
            if (entry->idx != idx) {
                continue;
            }
        }
        strncpy(buffer, entry->description, sizeof(entry->description) - 1);
        pthread_mutex_unlock(&pulse_mutex);
        buffer[sizeof(entry->description) - 1] = '\0';
        return true;
    }
    pthread_mutex_unlock(&pulse_mutex);
    /* first time requires a prime callback call because we only get updates
     * when the description or volume actually changes, but we need it to be
     * correct even if it never changes */
    pa_threaded_mainloop_lock(main_loop);
    get_device_info(context, is_sink, idx, name);
    pa_threaded_mainloop_unlock(main_loop);
    /* show empty string while we don't have this information */
    buffer[0] = '\0';
    return true;
}

/*
 *  detect and, if necessary, initialize the PulseAudio API
 */
bool pulse_initialize() {
    if (!main_loop) {
        main_loop = pa_threaded_mainloop_new();
        if (!main_loop)
            return false;
    }
    if (!api) {
        api = pa_threaded_mainloop_get_api(main_loop);
        if (!api)
            return false;
    }
    if (!context) {
        pa_proplist *proplist = pa_proplist_new();
        pa_proplist_sets(proplist, PA_PROP_APPLICATION_NAME, APP_NAME);
        pa_proplist_sets(proplist, PA_PROP_APPLICATION_ID, APP_ID);
        pa_proplist_sets(proplist, PA_PROP_APPLICATION_VERSION, I3STATUS_VERSION);
        context = pa_context_new_with_proplist(api, APP_NAME, proplist);
        pa_proplist_free(proplist);
        if (!context)
            return false;
        pa_context_set_state_callback(context,
                                      context_state_callback,
                                      NULL);
        if (pa_context_connect(context,
                               NULL,
                               PA_CONTEXT_NOFAIL | PA_CONTEXT_NOAUTOSPAWN,
                               NULL) < 0) {
            pulseaudio_error_log(context);
            return false;
        }
        if (!mainloop_thread_running &&
            pa_threaded_mainloop_start(main_loop) < 0) {
            pulseaudio_error_log(context);
            pa_threaded_mainloop_free(main_loop);
            main_loop = NULL;
            return false;
        }
        mainloop_thread_running = true;
    }
    return true;
}
