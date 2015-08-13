// vim:ts=4:sw=4:expandtab

#include "i3status.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XKBrules.h>

const char *getKeyboardLayout() {
    Display *dpy = XkbOpenDisplay(NULL, NULL, NULL, NULL, NULL, NULL);
    XkbRF_VarDefsRec vd;
    char *tmp = NULL;
    XkbRF_GetNamesProp(dpy, &tmp, &vd);
    XCloseDisplay(dpy);
    return vd.layout;
}

void print_keyboard_layout(yajl_gen json_gen, char *buffer, const char *format) {
    char *outwalk = buffer;
    outwalk += sprintf(outwalk, format, getKeyboardLayout());
    *outwalk = '\0';
    OUTPUT_FULL_TEXT(buffer);
}
