// vim:ts=4:sw=4:expandtab
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>
#include "i3status.h"

#include <X11/XKBlib.h>

Display *disp = NULL;
char *layouts = NULL;

void setup_keyboard() {
    int evCode, errRet, rsnRet;
    int maj = XkbMajorVersion;
    int min = XkbMinorVersion;

    disp = XkbOpenDisplay("", &evCode, &errRet, &maj, &min, &rsnRet);
    // Names
    XkbDescPtr desc = XkbAllocKeyboard();
    XkbGetNames(disp, XkbSymbolsNameMask, desc);

    Atom symNameAtom = desc->names->symbols;
    layouts = XGetAtomName(disp, symNameAtom);
}

void print_keyboard(yajl_gen json_gen, char *buffer, const char *fmt) {
    const char *walk = fmt;
    char *outwalk = buffer;
    if (disp == NULL)
        setup_keyboard();

    // Get state
    XkbStatePtr state = calloc(1, sizeof(XkbStateRec));
    XkbGetState(disp, 0x100, state);

    // Extract current layout
    for (int i = 0; layouts[i]; i++) {
        if (layouts[i] == '+') {
            if (state->group <= 0) {
                int j = ++i;
                while (layouts[j] && layouts[j] != '+' && layouts[j] != ':') {
                    j++;
                }
                //Found
                for (; *walk != '\0'; walk++) {
                    if (*walk != '%') {
                        *(outwalk++) = *walk;
                        continue;
                    }
                    if (BEGINS_WITH(walk + 1, "map")) {
                        strncpy(outwalk, layouts + i, j - i);
                        outwalk += j - i;
                        *(outwalk++) = '\0';
                        walk += strlen("map");
                    } else {
                        *(outwalk++) = *walk;
                    }
                }
                OUTPUT_FULL_TEXT(buffer);
                break;
            } else {
                state->group--;
            }
        }
    }
}
