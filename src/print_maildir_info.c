// vim:ts=4:sw=4:expandtab
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <libgen.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>

#include "i3status.h"

#define MBNAMELEN   100

#define do_or_die(x)              if (!(x)) return false;

typedef struct exclude_t exclude_t;
struct exclude_t {
    char        name[PATH_MAX];
    exclude_t  *next;
};

typedef struct mailbox_t mailbox_t;
struct mailbox_t {
    char        path[PATH_MAX];
    size_t      count;
    mailbox_t  *next;
};

char *maildir;
mailbox_t *mailboxes;
exclude_t *ignored;

/*
 * Free ignore list.
 *
 */
static void free_ignore_list(void) {
    exclude_t *p;

    while (ignored != NULL) {
        p = ignored->next;
        free(ignored);
        ignored = p;
    }
}

/*
 * Allocate individual entry in ignore list.
 *
 */
static bool alloc_ignore_entry(char *path, size_t len) {
    exclude_t *p;

    if ((p = ignored) == NULL) {
        if ((ignored = malloc(sizeof(exclude_t))) == NULL) {
            return false;
        }
        p = ignored;
    } else {
        while (p->next) {
            p = p->next;
        }

        if ((p->next = malloc(sizeof(exclude_t))) == NULL) {
            free_ignore_list();
            return false;
        }
        p = p->next;
    }

    p->next = NULL;
    snprintf(p->name, len, "%s", path);

    return true;
}

/*
 * Inflate folder ignore list with names of directories that can't contain
 * maildirs.  Also include user-supplied ignore list.  Be careful about
 * leading and trailing whitespace.
 *
 */
static bool setup_ignore_list(char *excluded) {
    char *p;

    do_or_die(alloc_ignore_entry(".", PATH_MAX));
    do_or_die(alloc_ignore_entry("..", PATH_MAX));
    do_or_die(alloc_ignore_entry("cur", PATH_MAX));
    do_or_die(alloc_ignore_entry("tmp", PATH_MAX));

    while (*excluded == ' ') {
        excluded++;
    }

    while ((p = strchr(excluded, ' ')) != NULL) {
        do_or_die(alloc_ignore_entry(excluded, p - excluded + 1));

        while (*p == ' ') {
            p++;
        }
        if (*(excluded = p) == '\0') {
            break;
        }
    }
    do_or_die(alloc_ignore_entry(excluded, PATH_MAX));

    return true;
}

/*
 * Verify that directory isn't mentioned in ignore list.  This function
 * handles "." and ".." directories among other things.
 *
 */
static bool filter(char *name) {
    exclude_t *p;

    for (p = ignored; p; p = p->next)
        if (strcmp(p->name, name) == 0)
            return false;

    return true;
}

/*
 * Add maildir to "mailboxes" list iff it contains new mail.
 *
 */
static bool add_maildir(char *path) {
    DIR *dir;
    struct dirent *dent;
    mailbox_t *box, *p;

    do_or_die((box = malloc(sizeof(mailbox_t))) != NULL);

    if ((dir = opendir(path)) == NULL) {
        free(box);
        return false;
    }

    box->count = 0;
    while ((dent = readdir(dir)) != NULL) {
        if (dent->d_type == DT_REG) {
            (box->count)++;
        }
    }
    (void)closedir(dir);

    if (box->count == 0) {
        free(box);
        return true;
    }

    snprintf(box->path, PATH_MAX, "%s", dirname(path + strlen(maildir) + 1));
    box->next = NULL;

    if ((p = mailboxes) == NULL) {
        mailboxes = box;
        return true;
    }

    while (p->next != NULL) {
        p = p->next;
    }

    p->next = box;
    return true;
}

/*
 * Free "mailboxes" list.
 *
 */
static void free_mailboxes(void) {
    mailbox_t *p;

    while (mailboxes != NULL) {
        p = mailboxes->next;
        free(mailboxes);
        mailboxes = p;
    }
}

/*
 * Recursively search for directories containing "new" subdirectory.
 *
 */
static bool find_maildirs(const char *path) {
    DIR *dir;
    struct dirent *dent;
    char npath[PATH_MAX];

    do_or_die(dir = opendir(path))

    while ((dent = readdir(dir)) != NULL) {
        snprintf(npath, PATH_MAX, "%s/%s", path, dent->d_name);

        if (dent->d_type != DT_DIR) {
            continue;
        }

        if (!strcmp(dent->d_name, "new")) {
            do_or_die(add_maildir(npath));
            continue;
        }

        if (filter(dent->d_name)) {
            do_or_die(find_maildirs(npath));
        }
    }

    (void)closedir(dir);

    return true;
}

/*
 * Recursively search for maildir++s and report those containing new mail.
 * Colors follow the rule "no news is a good news".  Should catch changes in
 * maildir structure on fly.  Should not report seen mail.  Directories, whose
 * name are mentioned in "exclude" parameter, are ignored.
 *
 */
void print_maildir_info(yajl_gen json_gen, char *buffer, const char *path, const char *prefix, const char *format, const char *separator, const char *no_mail, const char *exclude) {
    char *outwalk = buffer;
    bool separate = false;
    mailbox_t *mailbox;
    char *p;

    INSTANCE(path);

    ignored = NULL;
    mailboxes = NULL;
    maildir = path;

    setup_ignore_list(exclude);

    if (find_maildirs(path) == false) {
        START_COLOR("color_bad");
        outwalk += sprintf(outwalk, "mail check failed");
        goto out;
    }

    if (mailboxes == NULL) {
        START_COLOR("color_good");
        if (*no_mail != '\0') {
            outwalk += sprintf(outwalk, "%s", no_mail);
        }
        goto out;
    }

    START_COLOR("color_bad");
    outwalk += sprintf(outwalk, "%s", prefix);
    for (mailbox = mailboxes; mailbox != NULL; mailbox = mailbox->next) {
        if (separate) {
            outwalk += sprintf(outwalk, "%s", separator);
        } else {
            separate = true;
        }

        p = format;
        while (*p != '\0') {
            if (strncmp(p, "%dir", strlen("%dir")) == 0) {
                outwalk += sprintf(outwalk, "%s", mailbox->path);
                p += strlen("%dir");
            } else if (strncmp(p, "%count", strlen("%count")) == 0) {
                outwalk += sprintf(outwalk, "%zu", mailbox->count);
                p += strlen("%count");
            } else if (strncmp(p, "%%", strlen("%%")) == 0) {
                outwalk += sprintf(outwalk, "%c", '%');
                p += strlen("%%");
            } else {
                *outwalk++ = *p++;
            }
        }
    }

out:
    free_ignore_list();
    free_mailboxes();
    END_COLOR;
    OUTPUT_FULL_TEXT(buffer);
}
