#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/XKBlib.h>
#include <X11/extensions/XKBrules.h>

#include "i3status.h"

#define STRING_SIZE 20

void print_keyboard_status(yajl_gen json_gen, char *buffer, const char *format) {

  char *outwalk = buffer;
  char caps_lock = 0;
  char num_lock = 0;
  char string_caps_lock[STRING_SIZE];
  char string_num_lock[STRING_SIZE];

  Display *display = XOpenDisplay(getenv("DISPLAY"));
  XKeyboardState keyboard_state;
  XGetKeyboardControl(display, &keyboard_state);

  if (display) {
    caps_lock = keyboard_state.led_mask & 0x1;
    num_lock = keyboard_state.led_mask & 0x2;
    XCloseDisplay(display);
  }

  snprintf(string_caps_lock, STRING_SIZE, "%s", caps_lock ? "ON" : "OFF");
  snprintf(string_num_lock, STRING_SIZE, "%s", num_lock ? "ON" : "OFF");

  placeholder_t placeholders[] = {
    {.name = "%C", .value = string_caps_lock},
    {.name = "%N", .value = string_num_lock}};

  const size_t num = sizeof(placeholders) / sizeof(placeholder_t);
  buffer = format_placeholders(format, &placeholders[0], num);
  OUTPUT_FULL_TEXT(buffer);
  free(buffer);
}
