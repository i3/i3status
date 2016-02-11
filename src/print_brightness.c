#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "i3status.h"

void print_brightness(yajl_gen json_gen, char *buffer, const char *pmax, const char *pcur, const char *btxt) {
    int bcur = 0;
    int bmax = 0;
    int bpercent = 0;
    char *outwalk = buffer;


    FILE *fcur = fopen(pcur, "r");
    FILE *fmax = fopen(pmax, "r");

    outwalk += sprintf(outwalk, "%s", btxt);

    if(fcur == NULL) {
        outwalk += sprintf(outwalk, "cur err");
        goto brightness_output_and_free;
    }
    if(fmax == NULL) {
        outwalk += sprintf(outwalk, "max err");
        goto brightness_output_and_free;
    }

    fscanf(fcur, "%d", &bcur);
    fscanf(fmax, "%d", &bmax);

    bpercent = bcur * 100 / bmax;

    outwalk += sprintf(outwalk, "%d%%", bpercent);

brightness_output_and_free:
    if(fcur != NULL) {
        fclose(fcur);
    }
    if(fmax != NULL) {
        fclose(fmax);
    }
    OUTPUT_FULL_TEXT(buffer);
}
