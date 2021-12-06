#include <poll.h>
#include <sndio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "i3status.h"

struct control {
    struct control *next;
    char name[32];
    unsigned int addr;
    unsigned int max;
    unsigned int value;
    unsigned int muted;
    unsigned int muteaddr;
};

static int initialized;
static struct sioctl_hdl *hdl;
static struct control *controls;
static struct pollfd *pfds;

/*
 * new control registered or control changed
 */
static void ondesc(void *unused, struct sioctl_desc *d, int val)
{
    struct control *i, **pi;

    if (d == NULL)
        return;

    /*
     * delete existing control with the same address
     */
    for (pi = &controls; (i = *pi) != NULL; pi = &i->next) {
        if (d->addr == i->addr) {
            *pi = i->next;
            free(i);
            break;
        }
    }

    /*
     * if we find an output.mute, associate it with its output.level
     */
    if (d->type == SIOCTL_SW &&
        d->group[0] == 0 &&
        strcmp(d->node0.name, "output") == 0 &&
        strcmp(d->func, "mute") == 0) {
        char name[32];
        snprintf(name, sizeof(name), "%s%d", d->node0.name, d->node0.unit);
    	for (pi = &controls; (i = *pi) != NULL; pi = &i->next) {
            if (strcmp(name, i->name) == 0) {
                i->muted = val;
                i->muteaddr = d->addr;
                break;
            }
        }
	return;
    }

    /*
     * we're interested in top-level output.level controls only
     */
    if (d->type != SIOCTL_NUM ||
        d->group[0] != 0 ||
        strcmp(d->node0.name, "output") != 0 ||
        strcmp(d->func, "level") != 0)
        return;

    i = malloc(sizeof(struct control));
    if (i == NULL) {
        fprintf(stderr, "sndio: failed to allocate control\n");
        return;
    }

    snprintf(i->name, sizeof(i->name), "%s%d", d->node0.name, d->node0.unit);
    i->addr = d->addr;
    i->max = d->maxval;
    i->value = val;
    i->next = controls;
    i->muted = 0;
    i->muteaddr = -1;
    controls = i;
}

/*
 * control value changed
 */
static void onval(void *unused, unsigned int addr, unsigned int value)
{
    struct control *c;

    for (c = controls; ; c = c->next) {
        if (c == NULL)
            return;
        if (c->addr == addr) {
            c->value = value;
            return;
        }
        if (c->muteaddr == addr) {
            c->muted = value;
            return;
        }
    }
}

static void cleanup(void)
{
    struct control *c;

    if (hdl) {
        sioctl_close(hdl);
        hdl = NULL;
    }
    if (pfds) {
        free(pfds);
        pfds = NULL;
    }
    while ((c = controls) != NULL) {
        controls = c->next;
        free(c);
    }
}

static int init(void)
{
    /* open device */
    hdl = sioctl_open(SIO_DEVANY, SIOCTL_READ, 0);
    if (hdl == NULL) {
        fprintf(stderr, "sndio: cannot open device\n");
        goto failed;
    }

    /* register call-back for control description changes */
    if (!sioctl_ondesc(hdl, ondesc, NULL)) {
        fprintf(stderr, "sndio: cannot get description\n");
        goto failed;
    }

    /* register call-back for volume changes */
    if (!sioctl_onval(hdl, onval, NULL)) {
        fprintf(stderr, "sndio: cannot get values\n");
        goto failed;
    }

    /* allocate structures for poll() syscall */
    pfds = calloc(sioctl_nfds(hdl), sizeof(struct pollfd));
    if (pfds == NULL) {
        fprintf(stderr, "sndio: cannot allocate pollfd structures\n");
        goto failed;
    }
    return 1;
failed:
    cleanup();
    return 0;
}

int volume_sndio(int *vol, int *muted)
{
    struct control *c;
    int n, v;

    if (!initialized) {
        initialized = 1;
        init();
    }
    if (hdl == NULL)
        return -1;

    /* check if controls changed */
    n = sioctl_pollfd(hdl, pfds, POLLIN);
    if (n > 0) {
        n = poll(pfds, n, 0);
        if (n > 0) {
            if (sioctl_revents(hdl, pfds) & POLLHUP) {
                fprintf(stderr, "sndio: disconnected\n");
                cleanup();
                return -1;
            }
        }
    }

    /*
     * get control value: as there may be multiple
     * channels, return the minimum
     */
    *vol = 100;
    *muted = 0;
    for (c = controls; c != NULL; c = c->next) {
        v = (c->value * 100 + c->max / 2) / c->max;
        if (v < *vol) {
            *vol = v;
            *muted = c->muted;

        }
    }

    return 0;
}
