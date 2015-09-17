// vim:ts=4:sw=4:expandtab
#include "i3status.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>

#include "i3status.h"
#include <ibus.h>

void print_ibus(yajl_gen json_gen, char *buffer, const char *format) {
    char *outwalk = buffer;
    const char *walk;

#if defined(linux)

    IBusBus *bus = ibus_bus_new();
    if (bus == NULL)
        goto error;
    if (!ibus_bus_is_connected(bus))
        goto error;

    IBusEngineDesc *engine = ibus_bus_get_global_engine(bus);
    if (engine == NULL)
        goto error;

    // print engine components here
    for (walk = format; *walk != '\0'; walk++) {
        if (*walk != '%') {
            *(outwalk++) = *walk;
            continue;
        }
        if (BEGINS_WITH(walk + 1, "name")) {
            outwalk += sprintf(outwalk, "%s", ibus_engine_desc_get_name(engine));
            walk += strlen("name");
        }
        if (BEGINS_WITH(walk + 1, "longname")) {
            outwalk += sprintf(outwalk, "%s", ibus_engine_desc_get_longname(engine));
            walk += strlen("longname");
        }
        if (BEGINS_WITH(walk + 1, "description")) {
            outwalk += sprintf(outwalk, "%s", ibus_engine_desc_get_description(engine));
            walk += strlen("description");
        }
        if (BEGINS_WITH(walk + 1, "language")) {
            outwalk += sprintf(outwalk, "%s", ibus_engine_desc_get_language(engine));
            walk += strlen("language");
        }
        if (BEGINS_WITH(walk + 1, "license")) {
            outwalk += sprintf(outwalk, "%s", ibus_engine_desc_get_license(engine));
            walk += strlen("license");
        }
        if (BEGINS_WITH(walk + 1, "author")) {
            outwalk += sprintf(outwalk, "%s", ibus_engine_desc_get_author(engine));
            walk += strlen("author");
        }
        if (BEGINS_WITH(walk + 1, "icon")) {
            outwalk += sprintf(outwalk, "%s", ibus_engine_desc_get_icon(engine));
            walk += strlen("icon");
        }
        if (BEGINS_WITH(walk + 1, "layout")) {
            outwalk += sprintf(outwalk, "%s", ibus_engine_desc_get_layout(engine));
            walk += strlen("layout");
        }
        if (BEGINS_WITH(walk + 1, "layout_variant")) {
            outwalk += sprintf(outwalk, "%s", ibus_engine_desc_get_layout_variant(engine));
            walk += strlen("layout_variant");
        }
        if (BEGINS_WITH(walk + 1, "layout_option")) {
            outwalk += sprintf(outwalk, "%s", ibus_engine_desc_get_layout_option(engine));
            walk += strlen("layout_option");
        }
        if (BEGINS_WITH(walk + 1, "rank")) {
            outwalk += sprintf(outwalk, "%i", ibus_engine_desc_get_rank(engine));
            walk += strlen("rank");
        }
        if (BEGINS_WITH(walk + 1, "hotkeys")) {
            outwalk += sprintf(outwalk, "%s", ibus_engine_desc_get_hotkeys(engine));
            walk += strlen("hotkeys");
        }
        if (BEGINS_WITH(walk + 1, "symbol")) {
            outwalk += sprintf(outwalk, "%s", ibus_engine_desc_get_symbol(engine));
            walk += strlen("symbol");
        }
        if (BEGINS_WITH(walk + 1, "setup")) {
            outwalk += sprintf(outwalk, "%s", ibus_engine_desc_get_setup(engine));
            walk += strlen("setup");
        }
        if (BEGINS_WITH(walk + 1, "version")) {
            outwalk += sprintf(outwalk, "%s", ibus_engine_desc_get_version(engine));
            walk += strlen("version");
        }
        if (BEGINS_WITH(walk + 1, "textdomain")) {
            outwalk += sprintf(outwalk, "%s", ibus_engine_desc_get_textdomain(engine));
            walk += strlen("textdomain");
        }
    }

    *outwalk = '\0';
    OUTPUT_FULL_TEXT(buffer);
    return;

error:
#endif
    OUTPUT_FULL_TEXT("cant get ibus status");
    (void)fputs("i3status: Cannot get ibus status()\n", stderr);
}
