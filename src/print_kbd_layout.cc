// vim:ts=4:sw=4:expandtab
#include <algorithm>
#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>
#include "XKeyboard.h"
#include "i3status.h"

static XKeyboard xkb;
std::string current_layout;

bool kbd_layout_updated() {
    auto new_layout = xkb.currentGroupSymbol();
    if (new_layout != current_layout) {
        current_layout = new_layout;
        return true;
    }
    return false;
}

void print_kbd_layout(yajl_gen json_gen, char *buffer, const char *format) {
    char *outwalk = buffer;

    if (current_layout.empty()) {
        kbd_layout_updated();
    }

    std::string current_layout_uc = current_layout;
    std::transform(current_layout_uc.begin(), current_layout_uc.end(), current_layout_uc.begin(),
                   [](unsigned char c) { return std::toupper(c); });

    placeholder_t placeholders[] = {
        {.name = "%l", .value = current_layout.c_str()},
        {.name = "%L", .value = current_layout_uc.c_str()}};

    const size_t num = sizeof(placeholders) / sizeof(placeholder_t);
    buffer = format_placeholders(format, &placeholders[0], num);

    OUTPUT_FULL_TEXT(buffer);
    free(buffer);
}
