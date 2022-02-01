// vim:ts=4:sw=4:expandtab
#include <stdlib.h>
#include "i3status.h"

#define STRING_SIZE 5

static int get_brightness(const char *brt, const char *max_brt) {
  FILE *brt_file = fopen(brt, "r");
  if (!brt_file)
    return -1;
  FILE *max_brt_file = fopen(max_brt, "r");
  if (!max_brt_file) {
    fclose(brt_file);
    return -2;
  }

  char value[20];
  char max_value[20];

  fgets(value, 20, brt_file);
  fgets(max_value, 20, max_brt_file);
  fclose(brt_file);
  fclose(max_brt_file);

  unsigned long actual = strtod(value, NULL);
  unsigned long max = strtod(max_value, NULL);

  return actual * 100 / max;
}

void print_brightness(brightness_ctx_t *ctx) {
  int brightness = get_brightness(ctx->brightness_path, ctx->max_brightness_path);
  const char *walk = ctx->format;
  char *outwalk = ctx->buf;

  char string_brightness[STRING_SIZE];
  snprintf(string_brightness, STRING_SIZE, "%d", brightness);
  placeholder_t placeholders[] = {
    {.name = "%brightness", .value = string_brightness }};

  const size_t num = sizeof(placeholders) / sizeof(placeholder_t);
  char *formatted = format_placeholders(walk, &placeholders[0], num);
  OUTPUT_FORMATTED;
  OUTPUT_FULL_TEXT(ctx->buf);
}
