/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2004 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#include "gwymacros.h"

/* To be able to mmap() files.
 * On Linux we have all, on Win32 we have none, on others who knows */
#if (HAVE_MMAP \
     && HAVE_UNISTD_H && HAVE_SYS_STAT_H && HAVE_SYS_TYPES_H && HAVE_FCNTL_H)
#define USE_MMAP 1
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#else
#undef USE_MMAP
#endif

#include <stdlib.h>
#include <string.h>
#include "gwyutils.h"

static gchar *gwy_argv0 = NULL;

static GQuark error_domain = 0;

/**
 * gwy_hash_table_to_slist_cb:
 * @unused_key: Hash key (unused).
 * @value: Hash value.
 * @user_data: User data (a pointer to #GSList*).
 *
 * #GHashTable to #GSList convertor.
 *
 * Usble in g_hash_table_foreach(), pass a pointer to a #GSList* as user
 * data to it.
 **/
void
gwy_hash_table_to_slist_cb(G_GNUC_UNUSED gpointer unused_key,
                           gpointer value,
                           gpointer user_data)
{
    GSList **list = (GSList**)user_data;

    *list = g_slist_prepend(*list, value);
}

/**
 * gwy_hash_table_to_list_cb:
 * @unused_key: Hash key (unused).
 * @value: Hash value.
 * @user_data: User data (a pointer to #GList*).
 *
 * #GHashTable to #GList convertor.
 *
 * Usble in g_hash_table_foreach(), pass a pointer to a #GList* as user
 * data to it.
 **/
void
gwy_hash_table_to_list_cb(G_GNUC_UNUSED gpointer unused_key,
                          gpointer value,
                          gpointer user_data)
{
    GList **list = (GList**)user_data;

    *list = g_list_prepend(*list, value);
}

/* FIXME: the enum and flags stuff duplicates GLib functionality.
 * However the GLib stuff requires enum class registration and thus is
 * hardly usable for more-or-less random, or even dynamic stuff. */

/**
 * gwy_string_to_enum:
 * @str: A string containing one of @enum_table string values.
 * @enum_table: A table of corresponding string-integer pairs.
 * @n: The number of elements in @enum_table, may be -1 when @enum_table is
 *     terminated by a #NULL name.
 *
 * Creates an integer representation of a string enum value @str.
 *
 * Returns: The integer enum value (NOT index in the table), or -1 if @str
 *          was not found.
 **/
gint
gwy_string_to_enum(const gchar *str,
                   const GwyEnum *enum_table,
                   gint n)
{
    gint j;

    for (j = n; j && enum_table->name; j--, enum_table++) {
        if (strcmp(str, enum_table->name) == 0)
            return enum_table->value;
    }

    return -1;
}

/**
 * gwy_enum_to_string:
 * @enumval: A one integer value from @enum_table.
 * @enum_table: A table of corresponding string-integer pairs.
 * @n: The number of elements in @enum_table, may be -1 when @enum_table is
 *     terminated by a #NULL name.
 *
 * Creates a string representation of an integer enum value @enumval.
 *
 * Returns: The name as a string from @enum_table, thus it generally should
 *          not be modified or freed, unless @enum_table is supposed to be
 *          modified too. If the value is not found, an empty string is
 *          returned.
 **/
G_CONST_RETURN gchar*
gwy_enum_to_string(gint enumval,
                   const GwyEnum *enum_table,
                   gint n)
{
    gint j;

    for (j = n; j && enum_table->name; j--, enum_table++) {
        if (enumval == enum_table->value)
            return enum_table->name;
    }

    return "";
}

/**
 * gwy_string_to_flags:
 * @str: A string containing one of @enum_table string values.
 * @enum_table: A table of corresponding string-integer pairs.
 * @n: The number of elements in @enum_table, may be -1 when @enum_table is
 *     terminated by a #NULL name.
 * @delimiter: A delimiter to split @str on, when #NULL space is used.
 *
 * Creates an integer flag combination of its string representation @str.
 *
 * Returns: All the flags present in @str, bitwise ORer.
 **/
gint
gwy_string_to_flags(const gchar *str,
                    const GwyEnum *enum_table,
                    gint n,
                    const gchar *delimiter)
{
    gchar **strings;
    gint i, j, enumval;

    strings = g_strsplit(str, delimiter ? delimiter : " ", 0);
    if (!strings)
        return 0;

    enumval = 0;
    for (i = 0; strings[i]; i++) {
        const GwyEnum *e = enum_table;

        for (j = n; j && e->name; j--, e++) {
            if (strcmp(strings[i], e->name) == 0) {
                enumval |= e->value;
                break;
            }
        }
    }
    g_strfreev(strings);

    return enumval;
}

/**
 * gwy_flags_to_string:
 * @enumval: Some ORed integer flags from @enum_table.
 * @enum_table: A table of corresponding string-integer pairs.
 * @n: The number of elements in @enum_table, may be -1 when @enum_table is
 *     terminated by a #NULL name.
 * @glue: A glue to join string values with, when #NULL space is used.
 *
 * Creates a string representation of integer flag combination @enumval.
 *
 * Returns: The string representation as a newly allocated string.  It should
 *          be freed when no longer used.
 **/
gchar*
gwy_flags_to_string(gint enumval,
                    const GwyEnum *enum_table,
                    gint n,
                    const gchar *glue)
{
    gint j;
    GString *str = NULL;
    gchar *result;

    if (!enumval)
        return "";

    if (!glue)
        glue = " ";

    for (j = n; j && enum_table->name; j--, enum_table++) {
        if (enumval & enum_table->value) {
            if (!str)
                str = g_string_new(enum_table->name);
            else {
                str = g_string_append(str, glue);
                str = g_string_append(str, enum_table->name);
            }
        }
    }
    result = str->str;
    g_string_free(str, FALSE);

    return result;
}

/**
 * gwy_strkill:
 * @s: A NUL-terminated string.
 * @killchars: A string containing characters to kill.
 *
 * Removes characters in @killchars from string @s, modifying it in place.
 *
 * Use gwy_strkill(g_strdup(@s), @killchars) to get a modified copy.
 *
 * Returns: @s itself, the return value is to allow function call nesting.
 **/
gchar*
gwy_strkill(gchar *s,
            const gchar *killchars)
{
    gchar *p, *q;
    gchar killc;

    if (!killchars || !*killchars)
        return s;
    killc = *killchars;
    if (killchars[1])
        g_strdelimit(s, killchars+1, killc);
    if ((p = strchr(s, killc))) {
        for (q = p; *p; p++) {
            if (*p != killc) {
                *q = *p;
                q++;
            }
        }
        *q = '\0';
    }

    return s;
}

/**
 * gwy_strreplace:
 * @haystack: A NUL-terminated string to search in.
 * @needle: A NUL-terminated string to search for.
 * @replacement: A NUL-terminated string to replace @needle with.
 * @maxrepl: Maximum number of occurences to replace (use (gsize)-1 to replace
 *           all occurences).
 *
 * Replaces occurences of string @needle in @haystack with @replacement.
 *
 * Returns: A newly allocated string.
 **/
gchar*
gwy_strreplace(const gchar *haystack,
               const gchar *needle,
               const gchar *replacement,
               gsize maxrepl)
{
    gsize n, hlen, nlen, rlen, newlen;
    const gchar *p, *pp;
    gchar *dest, *q;

    nlen = strlen(needle);
    g_return_val_if_fail(nlen, NULL);
    n = 0;
    p = haystack;
    while ((p = strstr(p, needle)) && n < maxrepl) {
        p += nlen;
        n++;
    }
    if (!n)
        return g_strdup(haystack);

    hlen = strlen(haystack);
    rlen = strlen(replacement);
    newlen = hlen + n*rlen - n*nlen;

    dest = g_new(gchar, newlen+1);
    pp = haystack;
    q = dest;
    n = 0;
    while ((p = strstr(pp, needle)) && n < maxrepl) {
        memcpy(q, pp, p - pp);
        q += p - pp;
        memcpy(q, replacement, rlen);
        q += rlen;
        pp = p + nlen;
        n++;
    }
    strcpy(q, pp);

    return dest;
}

/**
 * gwy_strdiffpos:
 * @s1: A string.
 * @s2: A string.
 *
 * Finds position where two strings differ.
 *
 * Returns: The last position where the strings do not differ yet.
 *          Particularly, -1 is returned if either string is %NULL,
 *          zero-length, or they differ in the very first character.
 **/
gint
gwy_strdiffpos(const gchar *s1, const gchar *s2)
{
    const gchar *ss = s1;

    if (!s1 || !s2)
        return -1;

    while (*s1 && *s1 && *s1 == *s2) {
        s1++;
        s2++;
    }

    return (s1 - ss) - 1;
}

/**
 * gwy_file_get_contents:
 * @filename: A file to read contents of.
 * @buffer: Buffer to store the file contents.
 * @size: Location to store buffer (file) size.
 * @error: Return location for a #GError.
 *
 * Reads or mmaps file @filename into memory.
 *
 * The buffer must be treated as read-only and must be freed with
 * gwy_file_abandon_contents().  It is NOT guaranteed to be NUL-terminated,
 * use @size to find its end.
 *
 * Returns: Whether it succeeded.  In case of failure @buffer and @size are
 *          reset too.
 **/
gboolean
gwy_file_get_contents(const gchar *filename,
                      guchar **buffer,
                      gsize *size,
                      GError **error)
{
#ifdef USE_MMAP
    struct stat st;
    int fd;

    if (!error_domain)
        error_domain = g_quark_from_static_string("GWY_UTILS_ERROR");

    *buffer = NULL;
    *size = 0;
    fd = open(filename, O_RDONLY);
    if (fd == -1) {
        g_set_error(error, error_domain, errno, "Cannot open file `%s': %s",
                    filename, g_strerror(errno));
        return FALSE;
    }
    if (fstat(fd, &st) == -1) {
        close(fd);
        g_set_error(error, error_domain, errno, "Cannot stat file `%s': %s",
                    filename, g_strerror(errno));
        return FALSE;
    }
    *buffer = (gchar*)mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (!*buffer) {
        g_set_error(error, error_domain, errno, "Cannot mmap file `%s': %s",
                    filename, g_strerror(errno));
        return FALSE;
    }
    *size = st.st_size;

    return TRUE;
#else
    return g_file_get_contents(filename, (gchar**)buffer, size, error);
#endif
}

/**
 * gwy_file_abandon_contents:
 * @buffer: Buffer with file contents as created by gwy_file_get_contents().
 * @size: Buffer size.
 * @error: Return location for a #GError.
 *
 * Frees or unmmaps memory allocated by gwy_file_get_contents().
 *
 * Returns: Whether it succeeded.
 **/
gboolean
gwy_file_abandon_contents(guchar *buffer,
                          G_GNUC_UNUSED gsize size,
                          G_GNUC_UNUSED GError **error)
{
#ifdef USE_MMAP
    if (!error_domain)
        error_domain = g_quark_from_static_string("GWY_UTILS_ERROR");

    if (munmap(buffer, size) == 1) {
        g_set_error(error, error_domain, errno, "Cannot unmap memory: %s",
                    g_strerror(errno));
        return FALSE;
    }
    return TRUE;
#else
    g_free(buffer);

    return TRUE;
#endif
}

/**
 * gwy_debug_gnu:
 * @domain: Log domain.
 * @fileline: File and line info.
 * @funcname: Function name.
 * @format: Message format.
 * @...: Message parameters.
 *
 * Print a debugging message.
 *
 * To be used via gwy_debug(), should not be used directly.
 **/
void
gwy_debug_gnu(const gchar *domain,
              const gchar *fileline,
              const gchar *funcname,
              const gchar *format,
              ...)
{
    gchar *fmt2 = g_strconcat(fileline, ": ", funcname, ": ", format, NULL);
    va_list args;
    va_start(args, format);
    g_logv(domain, G_LOG_LEVEL_DEBUG, fmt2, args);
    va_end(args);
    g_free(fmt2);
}

/**
 * gwy_sgettext:
 * @msgid: Message id to translate, containing `|'-separated prefix.
 *
 * Translate a message id containing disambiguating prefix ending with `|'.
 *
 * Returns: Translated message, or @msgid itself with all text up to the last
 *          `|' removed if there is no translation.
 **/
gchar*
gwy_sgettext(const gchar *msgid)
{
    char *msgstr, *p;

    msgstr = gettext(msgid);
    if (msgstr == msgid) {
        p = strrchr(msgstr, '|');
        return p ? p+1 : msgstr;
    }

    return msgstr;
}

/**
 * gwy_find_self_dir:
 * @dirname: A gwyddion directory name like "pixmaps" or "modules".
 *
 * Finds some [system] gwyddion directory.
 *
 * This function exists only because of insane Win32 instalation manners
 * where user can decide to put precompiled binaries anywhere.
 * On sane systems it just returns a copy of GWY_PIXMAP_DIR, etc. instead.
 *
 * The returned value is not actually tested for existence, it's up to caller.
 *
 * On Win32, gwy_find_self_set_argv0() must be called before any call to
 * gwy_find_self_dir().
 *
 * Returns: The path as a newly allocated string.
 **/
gchar*
gwy_find_self_dir(const gchar *dirname)
{
#ifdef G_OS_WIN32
    gchar *p, *q, *b;

    /* TODO: to be sure, we should put the path to the registry, too */
    /* argv[0] */
    p = g_strdup(gwy_argv0);
    if (!g_path_is_absolute(p)) {
        b = g_get_current_dir();
        q = g_build_filename(b, p, NULL);
        g_free(p);
        g_free(b);
        p = q;
    }
    /* now p contains an absolute path, the dir should be there */
    gwy_debug("gwyddion full path seems to be `%s'", p);
    q = g_path_get_dirname(p);
    g_free(p);
    p = q;
    q = g_build_filename(p, dirname, NULL);
    g_free(p);

    return q;
#endif /* G_OS_WIN32 */

#ifdef G_OS_UNIX
    static const struct { gchar *id; gchar *path; } paths[] = {
        { "modules",   GWY_MODULE_DIR },
        { "pixmaps",   GWY_PIXMAP_DIR },
        { "plugins",   GWY_PLUGIN_DIR },
    };
    gsize i;

    for (i = 0; i < G_N_ELEMENTS(paths); i++) {
        if (strcmp(dirname, paths[i].id) == 0)
            return g_strdup(paths[i].path);
    }
    g_critical("Cannot find directory for `%s'", dirname);
    return NULL;
#endif /* G_OS_UNIX */
}

/**
 * gwy_find_self_set_argv0:
 * @argv0: Program's argv[0].
 *
 * Sets argv0 so that gwy_find_self_dir() can find self.
 **/
void
gwy_find_self_set_argv0(const gchar *argv0)
{
    g_free(gwy_argv0);
    gwy_argv0 = g_strdup(argv0);
}

/**
 * gwy_get_user_dir:
 *
 * Return directory where Gwyddion user settings and data should be stored.
 *
 * On Unix this is normally in home directory.  On silly platforms or silly
 * occasions, silly locations can be returned as fallback.
 *
 * Returns: The directory as a string that should not be freed.
 **/
G_CONST_RETURN gchar*
gwy_get_user_dir(void)
{
    const gchar *gwydir =
#ifdef G_OS_WIN32
        "gwyddion";
#else
        ".gwyddion";
#endif
    static gchar *gwyhomedir = NULL;

    if (gwyhomedir)
        return gwyhomedir;

    gwyhomedir = g_build_filename(gwy_get_home_dir(), gwydir, NULL);
    return gwyhomedir;
}

/**
 * gwy_get_home_dir:
 *
 * Returns home directory, or temporary directory as a fallback.
 *
 * Under normal circumstances the same string as g_get_home_dir() would return
 * is returned.  But on MS Windows, something like "C:\Windows\Temp" can be
 * returned too, as it is as good as anything else (we can write there).
 *
 * Returns: Something usable as user home directory.  It may be silly, but
 *          never %NULL or empty.
 **/
G_CONST_RETURN gchar*
gwy_get_home_dir(void)
{
    const gchar *homedir = NULL;

    if (homedir)
        return homedir;

    homedir = g_get_home_dir();
    if (!homedir || !*homedir)
        homedir = g_get_tmp_dir();
#ifdef G_OS_WIN32
    if (!homedir || !*homedir)
        homedir = "C:\\Windows";  /* XXX :-))) */
#else
    if (!homedir || !*homedir)
        homedir = "/tmp";
#endif

    return homedir;
}

/**
 * gwy_canonicalize_path:
 * @path: A filesystem path.
 *
 * Canonicalizes a filesystem path.
 *
 * Particularly it makes the path absolute, resolves `..' and `.', and fixes
 * slash sequences to single slashes.  On Win32 it also converts all
 * backslashes to slashes along the way.
 *
 * Note this function does NOT resolve symlinks, use g_file_read_link() for
 * that.
 *
 * Returns: The canonical path, as a newly created string.
 **/
gchar*
gwy_canonicalize_path(const gchar *path)
{
    gchar *spath, *p0, *p, *last_slash;
    gsize i;

    g_return_val_if_fail(path, NULL);

    /* absolutize */
    if (!g_path_is_absolute(path)) {
        p = g_get_current_dir();
        spath = g_build_filename(p, path, NULL);
        g_free(p);
    }
    else
        spath = g_strdup(path);
    p = spath;

#ifdef G_OS_WIN32
    /* convert backslashes to slashes */
    while (*p) {
        if (*p == '\\')
            *p = '/';
        p++;
    }
    p = spath;

    /* skip c:, //server */
    if (g_ascii_isalpha(*p) && p[1] == ':')
        p += 2;
    else if (*p == '/' && p[1] == '/') {
        p = strchr(p+2, '/');
        /* silly, but better this than a coredump... */
        if (!p)
            return spath;
    }
    /* now p starts with the `root' / on all systems */
#endif
    g_return_val_if_fail(*p == '/', spath);

    p0 = p;
    while (*p) {
        if (*p == '/') {
            if (p[1] == '.') {
                if (p[2] == '/' || !p[2]) {
                    /* remove from p here */
                    for (i = 0; p[i+2]; i++)
                        p[i] = p[i+2];
                    p[i] = '\0';
                }
                else if (p[2] == '.' && (p[3] == '/' || !p[3])) {
                    /* remove from last_slash here */
                    /* ignore if root element */
                    if (p == p0) {
                        for (i = 0; p[i+3]; i++)
                            p[i] = p[i+3];
                        p[i] = '\0';
                    }
                    else {
                        for (last_slash = p-1; *last_slash != '/'; last_slash--)
                          ;
                        for (i = 0; p[i+3]; i++)
                            last_slash[i] = p[i+3];
                        last_slash[i] = '\0';
                        p = last_slash;
                    }
                }
                else
                    p++;
            }
            else {
                /* remove a continouos sequence of slashes */
                for (last_slash = p; *last_slash == '/'; last_slash++)
                    ;
                last_slash--;
                if (last_slash > p) {
                    for (i = 0; last_slash[i]; i++)
                        p[i] = last_slash[i];
                    p[i] = '\0';
                }
                else
                    p++;
            }
        }
        else
            p++;
    }
    /* a final `..' could fool us into discarding the starting slash */
    if (!*p0) {
      *p0 = '/';
      p0[1] = '\0';
    }

    return spath;
}

/* XXX: Note this function is backported from GLib 2.4 */
/**
 * gwy_setenv:
 * @variable: The environment variable to set, must not contain '='.
 * @value: The value for to set the variable to.
 * @overwrite: Whether to change the variable if it already exists.
 *
 * Sets an environment variable.
 *
 * Note that on some systems, the memory used for the variable and its value
 * can't be reclaimed later.
 *
 * Returns: %FALSE if the environment variable couldn't be set.
 */
gboolean
gwy_setenv(const gchar *variable,
           const gchar *value,
           gboolean overwrite)
{
    gint result;
#ifndef HAVE_SETENV
    gchar *string;
#endif

    g_return_val_if_fail(strchr(variable, '=') == NULL, FALSE);

#ifdef HAVE_SETENV
    result = setenv(variable, value, overwrite);
#else
    if (!overwrite && g_getenv(variable) != NULL)
        return TRUE;

    /* This results in a leak when you overwrite existing
     * settings. It would be fairly easy to fix this by keeping
     * our own parallel array or hash table.
     */
    string = g_strconcat(variable, "=", value, NULL);
    result = putenv(string);
#endif
    return result == 0;
}

/**
 * gwy_str_next_line:
 * @buffer: A character buffer containing some text.
 *
 * Extracts a next line from a character buffer, modifying it in place.
 *
 * @buffer is updated to point after the end of the line and the "\n"
 * (or "\r\n") is replaced with "\0", if present.
 *
 * Returns: The start of the line.  %NULL if the buffer is empty or %NULL.
 *          NOT a new string, the returned pointer points somewhere to @buffer.
 **/
gchar*
gwy_str_next_line(gchar **buffer)
{
    gchar *p, *q;

    if (!buffer || !*buffer)
        return NULL;

    q = *buffer;
    p = strchr(*buffer, '\n');
    if (p) {
        if (p > *buffer && *(p-1) == '\r')
            *(p-1) = '\0';
        *buffer = p+1;
        *p = '\0';
    }
    else
        *buffer = NULL;

    return q;
}

/************************** Documentation ****************************/
/* NB: gwymacros.h, gwywin32unistd.h documentation is also here. */

/**
 * GwyEnum:
 * @name: Value name.
 * @value: The (integer) enum value.
 *
 * Enumerated type with named values.
 **/

/**
 * gwy_debug:
 * @format...: A format string followed by stuff to print.
 *
 * Prints a debugging message.
 *
 * Does nothing if compiled without DEBUG defined.
 **/

/**
 * GWY_SWAP:
 * @t: A C type.
 * @x: A variable of type @t to swap with @x.
 * @y: A variable of type @t to swap with @y.
 *
 * Swaps two variables (more precisely lhs and rhs expressions) of type @t
 * in a single statement.
 */

/**
 * gwy_object_unref:
 * @obj: A pointer to #GObject or %NULL (must be an l-value).
 *
 * If @obj is not %NULL, unreferences @obj.  In all cases sets @obj to %NULL.
 *
 * If the object reference count is greater than one, assure it should be
 * referenced elsewhere, otherwise it leaks memory.
 **/

/**
 * chmod:
 * @file: File name.
 * @mode: Permissions to set on @file.
 *
 * Macro usable as chmod() on Win32.
 * See its Unix manual page for details.
 **/

/**
 * getpid:
 *
 * Macro usable as getpid() on Win32.
 * See its Unix manual page for details.
 **/

/**
 * mkdir:
 * @dir: Directory name.
 * @mode: Permissions of the newly created directory (ignored on Win32).
 *
 * Macro usable as mkdir() on Win32.
 * See its manual page for details.
 **/

/**
 * unlink:
 * @file: File name.
 *
 * Macro usable as unlink() on Win32.
 * See its Unix manual page for details.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

