//coppied from i3:include/libi3.h

/** Helper structure for usage in format_placeholders(). */
typedef struct placeholder_t {
    /* The placeholder to be replaced, e.g., "%title". */
    char *name;
    /* The value this placeholder should be replaced with. */
    char *value;
} placeholder_t;

/**
 * Replaces occurrences of the defined placeholders in the format string.
 *
 */
char *format_placeholders(const char *format, placeholder_t *placeholders, int num);
