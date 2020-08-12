#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <X11/XKBlib.h>
#include <X11/extensions/XKBrules.h>

#include "i3status.h"

//void print_keyboard_layout(yajl_gen json_gen, char *buffer, const char *format) {
void print_keyboard_layout(yajl_gen json_gen, char *buffer) {

    char *outwalk = buffer;
    Display *display = XOpenDisplay(NULL);
    char *layout = '\0';

    if (display) {
        XkbStateRec state;
        XkbGetState(display, XkbUseCoreKbd, &state);

        XkbRF_VarDefsRec var_defs_rec;
        XkbRF_GetNamesProp(display, NULL, &var_defs_rec);

        layout = strtok(var_defs_rec.layout, ",");
        for (int i = 0; i < state.group; i++)
            layout = strtok('\0', ",");
    }

    if (!layout)
        OUTPUT_FULL_TEXT("NULL");
    else
        OUTPUT_FULL_TEXT(layout);
}
