// vim:ts=4:sw=4:expandtab
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#include <pwd.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>

#include "queue.h"
#include "i3status.h"

#define LINELEN 1000
#define IGNORE_ANYWAY ".:..:cur:tmp"

struct mailroot {
    char path[PATH_MAX];

    enum {
        WRONG,
        MBOX,
        MAILDIR
    } type;

    long long messages;

    /* needed for mboxes */
    struct timeval times[2];
    bool checked;

    struct mailchecker *checker;

    SLIST_ENTRY(mailroot) mailroots;
};

struct mailchecker {
    const char *title;

    int width;

    const char *header;
    const char *body;
    const char *footer;
    const char *format_no_mail;

    long long messages;

    char ignore[PATH_MAX];

    SLIST_HEAD(mailboxes, mailroot) mailroots;

    bool initialized;
    char error_message[MAIL_REPORT_WIDTH];

    SLIST_ENTRY(mailchecker) checkers;
};

static SLIST_HEAD(, mailchecker) mailcheckers;

static struct mailchecker *checker;
static struct mailroot *root;

static char buf[BUFFER_SIZE];

/*
 * strchr(3) which ignores backslash escapes and returns pointer to end of
 * string if doesn't find c.
 *
 */
static char *find_unescaped(const char *s, char c) {
    const char *p;

    for (p = s; *p != '\0'; p++)
        if (*p == c && (p == s || *(p - 1) != '\\'))
            break;
    return p;
}

/*
 * Verify that supplied directory name is not on ignore list.  Apart from
 * user-supplied entries, check for ".", "..", "cur" and "tmp" (which are
 * added to ignore list in print_maildir_info() during initialization.
 *
 */
static bool filter(const char *name) {
    char *sp = checker->ignore, *ep;

    do {
        ep = find_unescaped(sp + 1, ':');

        if (sp != ep && strncmp(name, sp, ep - sp) == 0)
            return false;
    } while (sp = ep, *sp++);

    return true;
}

/*
 * Record found mail in output buffer, always checking for output limit.
 * If output limit is reached, return "false" and let print_maildir_info()
 * trim buffer contents as needed.
 *
 * This is the only function in the module that does format string parsing.
 *
 */
static bool report_mailbox(const char *name, const size_t messages) {
    const char *walk = checker->body;
    char *outwalk = buf + strnlen(buf, sizeof(buf));

    if (*buf != '\0')
        *outwalk++ = ' ';

    for (; *walk != '\0' && outwalk - buf < BUFFER_SIZE; walk++) {
        if (*walk == '%') {
            if (BEGINS_WITH(walk + 1, "mailbox")) {
                outwalk += snprintf(outwalk, BUFFER_SIZE - (outwalk - buf), "%s", name);
                walk += strlen("mailbox");
                continue;
            }

            if (BEGINS_WITH(walk + 1, "messages")) {
                if (messages < 0) {
                    *outwalk++ = '?';
                } else {
                    outwalk += snprintf(outwalk, BUFFER_SIZE - (outwalk - buf), "%zu", messages);
                }
                walk += strlen("messages");
                continue;
            }

            if (*walk + 1 == '%') {
                *outwalk++ = '%';
                walk++;
                continue;
            }

            /* If nothing matched, fallthrough to print normally */
        }
        *outwalk++ = *walk;
    }

    return outwalk - buf < checker->width;
}

/*
 * Count regular files in directory.  If count > 0, format maildir name for
 * printing (strip "path"), call report_mailbox() and return its return code.
 *
 */
static bool check_maildir(char *path) {
    DIR *dir = opendir(path);
    struct dirent *dent;
    char npath[PATH_MAX];
    root->messages = 0;

    if (!dir)
        return true;

    while ((dent = readdir(dir)) != NULL)
        if (dent->d_type == DT_REG)
            root->messages++;
    (void)closedir(dir);

    if (root->messages) {
        checker->messages += root->messages;
        (void)snprintf(npath, strnlen(path, PATH_MAX) - strlen(root->path) - 4, "%s", path + strlen(root->path) + 1);
        return report_mailbox(npath, root->messages);
    }

    return true;
}

/*
 * Recursively walk through subdirectories, checking their names with
 * filter().  If subdirectory "new" found, call check_maildir().
 *
 * Return value indicates whether output size limit was reached.
 *
 */
static bool find_maildirs(const char *path) {
    DIR *dir = opendir(path);
    struct dirent *dent;
    char npath[PATH_MAX];
    bool status = true;

    if (!dir)
        return true;

    while ((dent = readdir(dir)) != NULL) {
        if (dent->d_type != DT_DIR)
            continue;

        (void)snprintf(npath, sizeof(npath), "%s/%s", path, dent->d_name);
        if (!strcmp(dent->d_name, "new")) {
            if (!check_maildir(npath)) {
                status = false;
                break;
            }
        } else if (filter(dent->d_name)) {
            if (!find_maildirs(npath)) {
                status = false;
                break;
            }
        }
    }

    (void)closedir(dir);
    return status;
}

static bool check_mbox(void) {
    struct stat sb;
    bool exists, mtime_changed, after_blank_line = true, in_header = false;
    FILE *f;
    char s[LINELEN];

    root->checked = true;

    /* If there is no file, nothing to report or save.  If there are other
     * problems with stat, return to caller. */
    if (!(exists = !stat(root->path, &sb))) {
        if (errno == ENOENT)
            return true;

        root->checked = false;
        return true;
    }

    /* If file is empty, nothing to report or save */
    if (sb.st_size == 0)
        return true;

    mtime_changed = sb.st_mtim.tv_sec != root->times[1].tv_sec;
    /* The mail reader modifies mbox to flag seen messages.  If modification
     * time did not change since last check, there were no changes to mbox. */
    if (!mtime_changed) {
        if (root->messages != 0) {
            checker->messages += root->messages;
            return report_mailbox(basename(root->path), root->messages);
        }

        return true;
    }

    /* Modification time changed, so store it.  Store access time as well -
     * it has to be restored afterwards. */
    TIMESPEC_TO_TIMEVAL(&root->times[0], &sb.st_atim);
    TIMESPEC_TO_TIMEVAL(&root->times[1], &sb.st_mtim);

    /* Read the file and count the "From" header instances.  In case of errors
     * set "root->checked" to false to report the error to checker. */
    if ((f = fopen(root->path, "rw")) == NULL) {
        if (errno == EACCES) {
            /* Looks like user is spying on someone else's mbox.  All that
             * could be learned here is that there is mail.  (Otherwise
             * this function would return after checking file size.) */
            root->messages = -1;
            return report_mailbox(basename(root->path), root->messages);
        }

        root->checked = false;
        return true;
    }

    root->messages = 0;

    while (fgets(s, LINELEN, f) != NULL) {
        if (after_blank_line && BEGINS_WITH(s, "From ")) {
            /* "From " line should be unique in mboxo format.  All lines starting
             * with "From " in body should be quoted. */
            root->messages += 1;
            in_header = true;
        } else if (in_header && BEGINS_WITH(s, "Status:") && strchr(s, 'O') != NULL) {
            /* "Status" header is a set of flags that MUAs store. "O" flag means
             * that user have seen the message. */
            root->messages -= 1;
        }

        if (*s == '\n') {
            after_blank_line = true;
            in_header = false;
        } else {
            after_blank_line = false;
        }
    }

    /* If fgets(3) returned NULL due to some error, message count
     * is likely wrong.  Print error instead of message count. */
    if (!feof(f)) {
        (void)fclose(f);
        checker->messages += root->messages;
        root->messages = -1;
        root->checked = false;
        return report_mailbox(basename(root->path), root->messages);
    }
    (void)fclose(f);

    /* Restore access and modification times from stat(2) call
     * and apply back to the file */
    (void)utimes(root->path, root->times);

    if (root->messages != 0) {
        checker->messages += root->messages;
        return report_mailbox(basename(root->path), root->messages);
    }

    return true;
}

/*
 * If path does not exist, or references a regular file, assume it is an mbox.
 * If it is directory, assume maildir.  Otherwise report WRONG format.
 *
 */
static int guess_type(const char *path) {
    struct stat sb;

    /* Some mail readers delete mboxes instead of trunkating them */
    if (stat(path, &sb) == -1)
        return MBOX;

    if (S_ISREG(sb.st_mode))
        return MBOX;

    if (S_ISDIR(sb.st_mode))
        return MAILDIR;

    return WRONG;
}

/*
 * Free mailroots, allocated for given mailchecker.  This is needed only when
 * checker encounters configuration error.
 *
 */
static void free_mailroots(void) {
    struct mailroot *p;
    while ((p = SLIST_FIRST(&checker->mailroots)) != NULL) {
        SLIST_REMOVE_HEAD(&checker->mailroots, mailroots);
        free(p);
    }
}

/*
 * Initialize module, spawn find_maildirs(path), and print the report,
 * trimming it as needed.  Or print error messages, if something went wrong.
 *
 */
void print_mail_info(yajl_gen json_gen, char *buffer, const char *title, char *mailpath, const char *ignore, const size_t width, char *format, const char *format_no_mail, const char *replacement) {
    const char *walk;
    char *outwalk = buffer;

    static bool first_run = true;
    bool too_wide = false, at_end = false;
    char *p;
    size_t leadsz;

    INSTANCE(title);

    if (first_run) {
        SLIST_INIT(&mailcheckers);
        first_run = false;
    }

    SLIST_FOREACH(checker, &mailcheckers, checkers) {
        if (checker->title == title)
            break;
    }

    /* Checker is not queued.  Set it up and add to mailcheckers queue. */
    if (checker == NULL) {
        if ((checker = malloc(sizeof(struct mailchecker))) == NULL) {
            START_COLOR("color_bad");
            outwalk += sprintf(outwalk, "maildir: ");
            (void)strerror_r(errno, outwalk, BUFFER_SIZE - strnlen(buffer, BUFFER_SIZE));
            goto out;
        }
        SLIST_INSERT_HEAD(&mailcheckers, checker, checkers);

        (void)memset(checker->error_message, '\0', sizeof(checker->error_message));

        /* Mark checker uninitialized until mailroots are allocated */
        checker->initialized = false;

        /* It should be safe to point title to the parsed config file value.
         * This way pointer comparison can be used instead of more expensive
         * strings comparison */
        checker->title = title;

        /* If size limit is smaller then format string, it is likely that
         * no data will fit at all. */
        checker->width = min(max(strlen(format), width), MAIL_REPORT_WIDTH);

        /* Tear format string into subparts, of which header and footer are
         * needed only once per iteration. */
        checker->header = NULL;
        checker->body = NULL;
        checker->footer = NULL;

        if (*(p = find_unescaped(format, '{')) == '\0') {
            checker->header = checker->title;
            checker->body = format;
        } else {
            checker->header = format;

            format = find_unescaped(format, '{');
            *format++ = '\0';
            checker->body = format;

            if (*(format = find_unescaped(format, '}')) == '\0') {
                START_COLOR("color_bad");
                (void)snprintf(checker->error_message, sizeof(checker->error_message), "maildir: no \'}\' in format");
                (void)strncat(buffer, checker->error_message, BUFFER_SIZE - 1);
                free(checker);
                goto out;
            }
            *format++ = '\0';
            checker->footer = format;
        }

        /* Save pointer to format_no_mail, for consistency */
        checker->format_no_mail = format_no_mail;

        /* Setup ignore list */
        (void)memset(checker->ignore, '\0', PATH_MAX);
        if (ignore == NULL || *ignore == '\0')
            (void)strncpy(checker->ignore, IGNORE_ANYWAY, PATH_MAX);
        else
            (void)snprintf(checker->ignore, PATH_MAX, IGNORE_ANYWAY ":%s", ignore);

        /* Initialize list of mailroots for this checker */
        SLIST_INIT(&(checker->mailroots));
    }

    if (*(checker->error_message) != '\0') {
        START_COLOR("color_bad");
        (void)strncat(buffer, checker->error_message, BUFFER_SIZE - 1);
    }

    /* Checker was allocated, but at least some mailroots were not */
    if (!(checker->initialized)) {
        /* Skip chunks of mailroot that were already domesticated */
        if (!SLIST_EMPTY(&checker->mailroots))
            for (root = SLIST_FIRST(&checker->mailroots); root != NULL; root = SLIST_NEXT(root, mailroots))
                mailpath = strchr(mailpath, '\0') + 1;
        if (mailpath == NULL) {
            struct passwd *pw = getpwuid(getuid());

            if ((root = malloc(sizeof(struct mailroot))) == NULL) {
                START_COLOR("color_bad");
                outwalk += sprintf(outwalk, "maildir: ");
                (void)strerror_r(errno, outwalk, BUFFER_SIZE - strnlen(buffer, BUFFER_SIZE));
                goto out;
            }

            SLIST_INSERT_HEAD(&checker->mailroots, root, mailroots);

            (void)snprintf(root->path, PATH_MAX, "/var/mail/%s", pw->pw_name);
            root->checker = checker;
            root->times[0].tv_sec = 0;
            root->times[0].tv_usec = 0;
            root->times[1].tv_sec = 0;
            root->times[1].tv_usec = 0;
            root->type = MBOX;
        } else {
            for (p = find_unescaped(mailpath, ':'); !at_end; mailpath = p + 1, p = find_unescaped(mailpath, ':')) {
                if (!(at_end = (*p == '\0')))
                    *p = '\0';

                /* If malloc fails, don't free().  Instead, attempt to allocate
                 * during next iteration. */
                if ((root = malloc(sizeof(struct mailroot))) == NULL) {
                    START_COLOR("color_bad");
                    outwalk += sprintf(outwalk, "maildir: ");
                    (void)strerror_r(errno, outwalk, BUFFER_SIZE - strnlen(buffer, BUFFER_SIZE));
                    goto out;
                }

                /* This will canonicalize path and make it more predictable */
                if (realpath(mailpath, root->path) == NULL) {
                    START_COLOR("color_bad");
                    (void)snprintf(checker->error_message, sizeof(checker->error_message), "maildir: ");
                    (void)strerror_r(errno, checker->error_message, sizeof(checker->error_message) - strlen(checker->error_message));
                    (void)strncat(buffer, checker->error_message, BUFFER_SIZE - 1);
                    free_mailroots();
                    goto out;
                }
                if ((root->type = guess_type(root->path)) == WRONG) {
                    (void)snprintf(checker->error_message, sizeof(checker->error_message), "maildir: %s: unknown type of mailbox", root->path);
                    (void)strncat(buffer, checker->error_message, BUFFER_SIZE - 1);
                    free_mailroots();
                    goto out;
                }

                root->checker = checker;

                root->times[0].tv_sec = 0;
                root->times[0].tv_usec = 0;
                root->times[1].tv_sec = 0;
                root->times[1].tv_usec = 0;

                SLIST_INSERT_HEAD(&checker->mailroots, root, mailroots);
            }
        }
        checker->initialized = true;
    }

    /* Check mailboxes.  For mbox call check_mbox, which will fill the struct
     * with data for reuse.  For maildir just call find_maildirs() */
    *buf = '\0';

    checker->messages = 0;

    SLIST_FOREACH(root, &checker->mailroots, mailroots) {
        if ((too_wide = !(root->type == MBOX ? check_mbox() : find_maildirs(root->path))))
            break;
    }

    /* Print report.  While this module has a real chance to overflow the
     * buffer - user may have a lot of maildirs - report_mailbox() function
     * checked against much smaller checker->width limit, so it is unlikely
     * that buffer ever gets overflown.  Still, user could provide a really
     * long string for header or footer, so extra care should be taken to
     * avoid overflow. */
    if (*buf == '\0') {
        START_COLOR("color_good");
        if (checker->format_no_mail)
            outwalk += snprintf(outwalk, BUFFER_SIZE - (outwalk - buffer), "%s", checker->format_no_mail);
        goto out;
    }

    START_COLOR("color_degraded");
    leadsz = strnlen(buffer, BUFFER_SIZE);
    if (checker->header != NULL) {
        for (walk = checker->header; *walk != '\0' && BUFFER_SIZE > outwalk - buffer; walk++) {
            if (*walk != '%') {
                *outwalk++ = *walk;
                continue;
            }

            if (BEGINS_WITH(walk + 1, "total")) {
                *outwalk += snprintf(outwalk, BUFFER_SIZE - (outwalk - buffer), "%lld", checker->messages);
                walk += strlen("total");

            } else if (*walk + 1 == '%') {
                *outwalk++ = '%';
                walk++;
            }
        }
    }

    outwalk += snprintf(outwalk, BUFFER_SIZE - (outwalk - buffer), " %s ", buf);

    if (checker->footer != NULL) {
        for (walk = checker->footer; *walk != '\0' && BUFFER_SIZE > outwalk - buffer; walk++) {
            if (*walk != '%') {
                *outwalk++ = *walk;
                continue;
            }

            if (BEGINS_WITH(walk + 1, "total")) {
                outwalk += snprintf(outwalk, BUFFER_SIZE - (outwalk - buffer), "%lld", checker->messages);
                walk += strlen("total");

            } else if (*walk + 1 == '%') {
                *outwalk++ = '%';
                walk++;
            }
        }
    }
    *outwalk = '\0';

    /* Compress whitespace */
    p = outwalk = buffer;
    do {
        if (isspace(*p)) {
            while (isspace(*p))
                p++;

            if (*p != '\0')
                *outwalk++ = ' ';
        }

        if (outwalk != p)
            *outwalk = *p;
    } while (p++, *outwalk++ != '\0');

    if (outwalk - buffer > leadsz + checker->width) {
        outwalk = buffer + leadsz + checker->width - strlen(replacement);
        while (!isspace(*outwalk))
            outwalk--;
        outwalk += sprintf(outwalk, replacement);
    }

out:
    outwalk = buffer + strnlen(buffer, BUFFER_SIZE);
    END_COLOR;
    OUTPUT_FULL_TEXT(buffer);
}
