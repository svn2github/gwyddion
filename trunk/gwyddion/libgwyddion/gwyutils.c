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

#include "config.h"
#include <libgwyddion/gwymacros.h>
#include <gobject/gvaluecollector.h>

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
#include <libgwyddion/gwyutils.h>

#ifdef G_OS_WIN32
#include <windows.h>

G_WIN32_DLLMAIN_FOR_DLL_NAME(static, dll_name);
#endif

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
 * gwy_strisident:
 * @s: A NUL-terminated string.
 * @more: List of additional ASCII characters allowed inside identifier, empty
 *        list can be passed as %NULL.
 * @startmore: List of additional ASCII characters allowed as the first
 *             identifier characters, empty list can be passed as %NULL.
 *
 * Checks whether a string is valid identifier.
 *
 * Valid identifier must start with an alphabetic character or a character from
 * @startmore, and it must continue with alphanumeric characters or characters
 * from @more.
 *
 * Note underscore is not allowed by default, you have to pass it in @more
 * and/or @startmore.
 *
 * Returns: %TRUE if @s is valid identifier, %FALSE otherwise.
 **/
gboolean
gwy_strisident(const gchar *s,
               const gchar *more,
               const gchar *startmore)
{
    const gchar *m;

    g_return_val_if_fail(s, FALSE);
    if (!g_ascii_isalpha(*s)) {
        if (!startmore)
            return FALSE;
        for (m = startmore; *m; m++) {
            if (*s == *m)
                break;
        }
        if (!*m)
            return FALSE;
    }
    s++;

    while (*s) {
        if (!g_ascii_isalnum(*s)) {
            if (!more)
                return FALSE;
            for (m = more; *m; m++) {
                if (*s == *m)
                    break;
            }
            if (!*m)
                return FALSE;
        }
        s++;
    }

    return TRUE;
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
 * @dirname: A gwyddion directory name:
 *           <literal>"modules"</literal>,
 *           <literal>"plugins"</literal>,
 *           <literal>"pixmaps"</literal>,
 *           <literal>"locale"</literal>, or
 *           <literal>"data"</literal>.
 *
 * Finds a system Gwyddion directory.
 *
 * On Unix, a compiled-in path is returned, unless it's overriden with
 * environment variables (see gwyddion manual page).
 *
 * On Win32, the directory where the libgwyddion DLL from which this function
 * was called resides is taken as the base and the location of other Gwyddion
 * directories is calculated from it.
 *
 * The returned value is not actually tested for existence, it's up to caller.
 *
 * To obtain the Gwyddion user directory see gwy_get_user_dir().
 *
 * Returns: The path as a newly allocated string.
 **/
gchar*
gwy_find_self_dir(const gchar *dirname)
{
#ifdef G_OS_WIN32
    static gchar *basedir = NULL;

    if (!basedir)
        basedir = g_win32_get_package_installation_directory(NULL, dll_name);
    if (gwy_strequal(dirname, "data"))
        dirname = NULL;

    return g_build_filename(basedir, dirname, NULL);
#endif    /* G_OS_WIN32 */

#ifdef G_OS_UNIX
    static const struct {
        const gchar *id;
        const gchar *base;
        const gchar *env;
        const gchar *dir;
    }
    paths[] = {
        {
            "modules",
            GWY_LIBDIR,
            "GWYDDION_LIBDIR",
            "gwyddion/modules"
        },
        {
            "plugins",
            GWY_LIBEXECDIR,
            "GWYDDION_LIBEXECDIR",
            "gwyddion/plugins"
        },
        {
            "pixmaps",
            GWY_DATADIR,
            "GWYDDION_DATADIR",
            "gwyddion/pixmaps",
        },
        {
            "data",
            GWY_DATADIR,
            "GWYDDION_DATADIR",
            "gwyddion",
        },
        {
            "locale",
            GWY_LOCALEDIR,
            "GWYDDION_LOCALEDIR",
            NULL,
        },
    };
    gsize i;
    const gchar *base;

    for (i = 0; i < G_N_ELEMENTS(paths); i++) {
        if (!gwy_strequal(dirname, paths[i].id))
            continue;

        if (!(base = g_getenv(paths[i].env)))
            base = paths[i].base;

        gwy_debug("for <%s> base = <%s>, dir = <%s>",
                  dirname, base, paths[i].dir);
        return g_build_filename(base, paths[i].dir, NULL);
    }
    g_critical("Cannot find directory for `%s'", dirname);
    return NULL;
#endif    /* G_OS_UNIX */
}

/**
 * gwy_get_user_dir:
 *
 * Returns the directory where Gwyddion user settings and data should be stored.
 *
 * On Unix this is usually a dot-directory in user's home directory.  On modern
 * Win32 the returned directory resides in user's Documents and Settings.
 * On silly platforms or silly occasions, silly locations (namely a temporary
 * directory) can be returned as fallback.
 *
 * To obtain a Gwyddion system directory see gwy_find_self_dir().
 *
 * Returns: The directory as a constant string that should not be freed.
 **/
const gchar*
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
const gchar*
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

/**
 * gwy_filename_ignore:
 * @filename_sys: File name in GLib encoding.
 *
 * Checks whether file should be ignored.
 *
 * This function checks for common file names indicating files that should
 * be normally ignored.  Currently it means backup files (ending with ~ or
 * .bak) and Unix hidden files (starting with a dot).
 *
 * Returns: %TRUE to ignore this file, %FALSE otherwise.
 **/
gboolean
gwy_filename_ignore(const gchar *filename_sys)
{
    if (!filename_sys
        || !*filename_sys
        || filename_sys[0] == '.'
        || g_str_has_suffix(filename_sys, "~")
        || g_str_has_suffix(filename_sys, ".bak")
        || g_str_has_suffix(filename_sys, ".BAK"))
        return TRUE;

    return FALSE;
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

/**
 * gwy_object_set_or_reset:
 * @object: A #GObject.
 * @type: The type whose properties are to reset, may be zero for all types.
 *        The object must be of this type (more precisely @object is-a
 *        @type must hold).
 * @...: %NULL-terminated list of name/value pairs of properties to set as
 *       in g_object_set().  It can be %NULL alone to only reset properties
 *       to defaults.
 *
 * Sets object properties, resetting other properties to defaults.
 *
 * All explicitly specified properties are set.  In addition, all unspecified
 * settable properties of type @type (or all unspecified properties if @type is
 * 0) are reset to defaults.  Settable means the property is writable and not
 * construction-only.
 *
 * The order in which properties are set is undefined beside keeping the
 * relative order of explicitly specified properties, therefore this function
 * is not generally usable for objects with interdependent properties.
 *
 * Unlike g_object_set(), it does not set properties that already have the
 * requested value, as a consequences notifications are emitted only for
 * properties which actually change.
 **/
void
gwy_object_set_or_reset(gpointer object,
                        GType type,
                        ...)
{
    GValue value, cur_value, new_value;
    GObjectClass *klass;
    GParamSpec **pspec;
    gboolean *already_set;
    const gchar *name;
    va_list ap;
    gint nspec, i;

    klass = G_OBJECT_GET_CLASS(object);
    g_return_if_fail(G_IS_OBJECT_CLASS(klass));
    g_return_if_fail(!type || G_TYPE_CHECK_INSTANCE_TYPE(object, type));

    g_object_freeze_notify(object);

    pspec = g_object_class_list_properties(klass, &nspec);
    already_set = g_newa(gboolean, nspec);
    memset(already_set, 0, nspec*sizeof(gboolean));

    memset(&cur_value, 0, sizeof(GValue));
    memset(&new_value, 0, sizeof(GValue));

    va_start(ap, type);
    for (name = va_arg(ap, gchar*); name; name = va_arg(ap, gchar*)) {
        gchar *error = NULL;

        for (i = 0; i < nspec; i++) {
            if (gwy_strequal(pspec[i]->name, name))
                break;
        }

        /* Do only minimal checking, because we cannot avoid re-checking in
         * g_object_set_property(), or at least not w/o copying a large
         * excerpt of GObject here.  Considerable amount of work is still
         * performed inside GObject again... */
        if (i == nspec) {
            g_warning("object class `%s' has no property named `%s'",
                      G_OBJECT_TYPE_NAME(object), name);
            continue;
        }

        g_value_init(&new_value, pspec[i]->value_type);
        G_VALUE_COLLECT(&new_value, ap, 0, &error);
        if (error) {
            g_warning("%s", error);
            g_free(error);
            g_value_unset(&new_value);
            break;
        }

        g_value_init(&cur_value, pspec[i]->value_type);
        g_object_get_property(object, name, &cur_value);
        if (g_param_values_cmp(pspec[i], &new_value, &cur_value) != 0)
            g_object_set_property(object, name, &new_value);

        g_value_unset(&cur_value);
        g_value_unset(&new_value);
        already_set[i] = TRUE;
    }
    va_end(ap);

    memset(&value, 0, sizeof(GValue));
    for (i = 0; i < nspec; i++) {
        if (already_set[i]
            || !(pspec[i]->flags & G_PARAM_WRITABLE)
            || (pspec[i]->flags & G_PARAM_CONSTRUCT_ONLY)
            || (type && pspec[i]->owner_type != type))
            continue;

        g_value_init(&value, pspec[i]->value_type);
        g_object_get_property(object, pspec[i]->name, &value);
        if (!g_param_value_defaults(pspec[i], &value)) {
            g_param_value_set_default(pspec[i], &value);
            g_object_set_property(object, pspec[i]->name, &value);
        }

        g_value_unset(&value);
    }

    g_free(pspec);

    g_object_thaw_notify(object);
}

/************************** Documentation ****************************/
/* Note: gwymacros.h documentation is also here. */

/**
 * SECTION:gwyutils
 * @title: gwyutils
 * @short_description: Various utility functions
 * @see_also: <link linkend="libgwyddion-gwymacros">gwymacros</link> --
 *            utility macros
 *
 * Various utility functions: creating GLib lists from hash tables
 * gwy_hash_table_to_list_cb()), protably finding Gwyddion application
 * directories (gwy_find_self_dir()), string functions (gwy_strreplace()), path
 * manipulation (gwy_canonicalize_path()).
 **/

/**
 * SECTION:gwymacros
 * @title: gwymacros
 * @short_description: Utility macros
 * @see_also: <link linkend="libgwyddion-gwyutils">gwyutils</link> -- utility
 *            functions
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
 * GWY_FIND_PSPEC:
 * @type: Object type (e.g. %GWY_TYPE_CONTAINER).
 * @id: Property id.
 * @spectype: Param spec type (e.g. <literal>DOUBLE</literal>).
 *
 * A convenience g_object_class_find_property() wrapper.
 *
 * It expands to property spec cast to correct type (@spec).
 **/

/**
 * gwy_strequal:
 * @a: A string.
 * @b: Another string.
 *
 * Expands to %TRUE if strings are equal, to %FALSE otherwise.
 **/

/**
 * GWY_CLAMP:
 * @x: The value to clamp.
 * @low: The minimum value allowed.
 * @hi: The maximum value allowed.
 *
 * Ensures that @x is between the limits set by @low and @hi.
 *
 * This macro differs from GLib's CLAMP() by G_UNLIKELY() assertions on the
 * tests that @x is smaller than @low and larger than @hi.  This makes @x
 * already being in the right range the fast code path.
 *
 * It is supposed to be used on results of floating-point operations that
 * should fall to a known range but may occasionaly fail to due to rounding
 * errors and in similar situations.  Under normal circumstances, use CLAMP().
 **/

/**
 * gwy_object_unref:
 * @obj: A pointer to #GObject or %NULL (must be an l-value).
 *
 * Unreferences an object if it exists.
 *
 * If @obj is not %NULL, g_object_unref() is called on it.
 * In all cases @obj is set to %NULL.
 *
 * A useful property of this macro is its idempotence.
 *
 * If the object reference count is greater than one, assure it should be
 * referenced elsewhere, otherwise it leaks memory.
 **/

/**
 * gwy_signal_handler_disconnect:
 * @obj: A pointer to #GObject or %NULL.
 * @hid: An @obj signal handler id or 0 (must be an l-value).
 *
 * Disconnect a signal handler if it exists.
 *
 * If @hid is nonzero and @obj is not %NULL, the signal handler identified by
 * @hid is disconnected.  In all cases @hid is set to 0.
 *
 * A useful property of this macro is its idempotence.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

