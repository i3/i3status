// vim:ts=4:sw=4:expandtab
#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>

#include "i3status.h"

#include <X11/XKBlib.h>
#include <X11/extensions/XKBrules.h>

static char *displayName;
static bool initialized;

static Display *display;
static XkbDescRec *kbdDescPtr;

static bool initialize_xkb_layout() {
    XkbIgnoreExtension(False);

    if (displayName == NULL)
        displayName = strdup("");

    int eventCode;
    int errorReturn;
    int major = XkbMajorVersion;
    int minor = XkbMinorVersion;
    int reasonReturn;

    display = XkbOpenDisplay(displayName, &eventCode, &errorReturn, &major, &minor, &reasonReturn);
    switch (reasonReturn) {
        case XkbOD_BadLibraryVersion:
            fprintf(stderr, "Bad XKB library version\n");
            return true;
        case XkbOD_ConnectionRefused:
            fprintf(stderr, "Connection to X server refused\n");
            return false;  // There is a point to retry
        case XkbOD_BadServerVersion:
            fprintf(stderr, "Bad X11 server version\n");
            return true;
        case XkbOD_NonXkbServer:
            fprintf(stderr, "XKB not present\n");
            return true;
        case XkbOD_Success:
            break;
    }

    kbdDescPtr = XkbAllocKeyboard();
    if (kbdDescPtr == NULL) {
        fprintf(stderr, "Failed to get keyboard description\n");
        XCloseDisplay(display);
        return false;
    }

    kbdDescPtr->dpy = display;
    return true;
}

void update_xkb_layout(char *layout) {
    XkbRF_VarDefsRec vdr;
    XkbStateRec xkbState;
    char *tmp = NULL;
    Bool bret;

    bret = XkbRF_GetNamesProp(display, &tmp, &vdr);
    if (!bret) {
        fprintf(stderr, "Failed to get names prop\n");
        return;
    }
    if (vdr.layout == NULL || vdr.layout[0] == 0) {
        return;
    }

    XkbGetState(display, XkbUseCoreKbd, &xkbState);

    tmp = strtok(vdr.layout, ",");
    for (int i = 0; i < xkbState.group; i++) {
        tmp = strtok(NULL, ",");
    }

    if (tmp != NULL) {
        char *tmpo = layout;
        for (; *tmp != 0; tmp++, tmpo++) {
            *tmpo = toupper(*tmp);
        }
        *tmpo = 0;
    }
}

void print_xkb_layout(yajl_gen json_gen, char *buffer) {
    char *outwalk = buffer;
    char layout[32] = "UNKNOWN";

    if (!initialized) {
        initialized = initialize_xkb_layout();
    }

    if (initialized) {
        update_xkb_layout(layout);
    }

    maybe_escape_markup(layout, &outwalk);
    *outwalk = '\0';
    OUTPUT_FULL_TEXT(buffer);
}
