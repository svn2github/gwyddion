/*
 *  $Id$
 *  Copyright (C) 2009 David Nečas (Yeti).
 *  E-mail: yeti@gwyddion.net.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include <string.h>
#include <stdio.h>
#include <glib/gstdio.h>

#ifdef G_OS_WIN32
#  define STRICT
#  include <windows.h>
#  undef STRICT
#endif

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif

#ifndef R_OK
#define R_OK 4
#endif

#ifndef W_OK
#define W_OK 2
#endif

#ifndef X_OK
#define X_OK 1
#endif

#include "libgwy/libgwy.h"
#include "libgwy/field-internal.h"

static gpointer init_types(G_GNUC_UNUSED gpointer arg);

#ifdef G_OS_WIN32
#define PACKAGEDIR "Gwyddion3"
#endif
#ifdef G_OS_UNIX
#define PACKAGEDIR "gwyddion3"
#endif

static gchar *libdir = NULL;
static gchar *datadir = NULL;
static gchar *localedir = NULL;
static gchar *userdir = NULL;

#ifdef G_OS_WIN32
static HMODULE libgwy_dll = NULL;
#endif

typedef gchar* (*GetModuleDirectoryFunc)(void);

/**
 * gwy_type_init:
 *
 * Forces the registration of all serializable libgwy types.
 *
 * It calls g_type_init() first to ensure the GLib object system is
 * initialised.
 *
 * Calling this function is not necessary to use object types defined in
 * libgwy, except perhaps if you implement custom deserialisers.  It is safe to
 * call this function more than once, subsequent calls are no-op.  It is safe
 * to call this function from more threads if GLib thread support is
 * initialised.
 **/
void
gwy_type_init(void)
{
    static GOnce types_initialized = G_ONCE_INIT;
    g_once(&types_initialized, init_types, NULL);
}

static gpointer
init_types(G_GNUC_UNUSED gpointer arg)
{
    g_type_init();

    g_type_class_peek(GWY_TYPE_SERIALIZABLE);

    // Data objects
    g_type_class_peek(GWY_TYPE_UNIT);
    g_type_class_peek(GWY_TYPE_MASK_LINE);
    g_type_class_peek(GWY_TYPE_MASK_FIELD);
    g_type_class_peek(GWY_TYPE_LINE);
    g_type_class_peek(GWY_TYPE_CURVE);
    g_type_class_peek(GWY_TYPE_FIELD);
    g_type_class_peek(GWY_TYPE_SURFACE);
    g_type_class_peek(GWY_TYPE_BRICK);
    g_type_class_peek(GWY_TYPE_CONTAINER);

    // Selections
    g_type_class_peek(GWY_TYPE_SELECTION);

    // Resources
    g_type_class_peek(GWY_TYPE_RESOURCE);
    g_type_class_peek(GWY_TYPE_GRADIENT);
    g_type_class_peek(GWY_TYPE_GL_MATERIAL);
    g_type_class_peek(GWY_TYPE_USER_FIT_FUNC);

    // Serialisable boxed
    g_type_class_peek(GWY_TYPE_RGBA);
    g_type_class_peek(GWY_TYPE_XY);
    g_type_class_peek(GWY_TYPE_XYZ);
    g_type_class_peek(GWY_TYPE_LINE_PART);
    g_type_class_peek(GWY_TYPE_FIELD_PART);
    g_type_class_peek(GWY_TYPE_BRICK_PART);
    g_type_class_peek(GWY_TYPE_FIT_PARAM);

    return NULL;
}

/*
 * Finding directiores.
 *
 * We deliberately ignore XDG which unfortunately means ignoring most GLib
 * provided functions.  Rationale:
 * - Doing things the Unix way has preference to some piece of PDF.
 * - The difference between configuration and data is artificial.  Resources
 *   are both.
 * - How do I get a search path for loadable modules from the Base Directory
 *   Specification?  And if I cannot, how am I supposed to be consistent then?
 * - It does not seem that openssh, bash or vim are going to follow some
 *   XDG_THINGS_IN_WEIRD_PLACES variables.
 *
 * 0. OVERVIEW
 * We have the following primary tasks:
 *
 * 1. Find self.  This means the directory this library was actually loaded
 * from.
 *
 * 2. Find the user configuration directory.  This is in home, or in other
 * place if we are on Win32 and other weird systems.
 *
 * 3. Find all directories where we should look for "foo".  These can be of
 * the following kinds:
 *
 *   (a) Related to the system directory, defined in point 1.
 *
 *   (b) Related to the user directory, defined in point 2.
 *
 *   (c) Arbitrary, unrelated to anything.
 *
 * 1. SEARCHING SELF
 * The following can be tried:
 * - environment variables GWYDDION_DATADIR, GWYDDION_LIBDIR,
 *   GWYDDION_LOCALE_DIR
 * - /proc/self/map (only on Linux)
 * - g_win32_get_package_installation_directory_of_module() (only on Win32)
 * - bundle installation directory (only on Mac OS X)
 * - compile-time prefix
 * - gwyddion program path
 *
 * These possibilities are tried in the following order and the first that
 * results in finding self is used.  The order is given by decreasing
 * reliability, except of course GWYDDION_ROOTDIR, which is not very reliable
 * but the user it always right.
 *
 * If none of them produces a match, fail.  This means that a fool-proof user
 * of libgwy must check whether gwy_data_directory(), gwy_lib_directory(),
 * gwy_locale_directory() return non-NULL.
 *
 * 2. SEARCHING USER DIRECTORY
 * The following can be tried:
 * - environment variable HOME
 * - g_get_home_dir()
 *
 * If neither leads to a suitable directory, fail.  An application should give
 * a warning if it finds that gwy_user_directory() returns NULL and avoid
 * saving configuration.  It needs to check whether it actually succeeded
 * anyway.
 *
 * 3. GATHERING ALL DIRECTORIES
 * The following can be tried:
 * - specific environment variables with modules, data, etc. paths (c)
 * - user directory (b)
 * - our installation directory (a)
 * - default system directories (such as /usr) (c)
 *
 * All are concatenated into one list in given order.  If some directory
 * happens to occur multiple times the first occurrence counts.
 */

#ifdef G_OS_WIN32
BOOL WINAPI
DllMain(HINSTANCE hinstDLL,
        DWORD     fdwReason,
        LPVOID    lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
        libgwy_dll = hinstDLL;

    return TRUE;
}

static gchar*
get_win32_module_directory(void)
{
    return g_win32_get_package_installation_directory_of_module(libgwy_dll);
}
#else
static gchar*
get_win32_module_directory(void)
{
    return NULL;
}
#endif

#ifdef G_OS_UNIX
static gchar*
get_unix_module_directory(void)
{
    gchar *buffer = NULL;
    gsize length = 0;

    /* Do not bother with error reporting, if the file does not exist, we are
     * on some other Unix than Linux. */
    if (!g_file_get_contents("/proc/self/maps", &buffer, &length, NULL))
        return NULL;

    gchar *path = NULL;
    gsize address = (gsize)&get_unix_module_directory;
    gchar *p = buffer;
    for (gchar *line = gwy_str_next_line(&p);
         line;
         line = gwy_str_next_line(&p)) {
        gsize start, end;

        if (sscanf(line, "%" G_GSIZE_MODIFIER "x-%" G_GSIZE_MODIFIER "x",
                   &start, &end) != 2
            || address < start
            || address >= end)
            continue;

        /* Once we found our address, either succeed or fail, but do not scan
         * anything more. */
        if (!(p = strchr(line, '/')))
            break;

        /* If the slash is not preceeded by a space, it is something weird. */
        if (!g_ascii_isspace(*(p-1)))
            break;

        g_strstrip(p);
        path = g_path_get_dirname(p);
        break;
    }

    g_free(buffer);
    return path;
}
#else
static gchar*
get_unix_module_directory(void)
{
    return NULL;
}
#endif

#ifdef __APPLE__
/* Note: This returns directly the main directory, not library directory. */
static gchar*
get_osx_module_directory(const gchar *dirname)
{
    CFBundleRef bundle = CFBundleGetMainBundle();
    CFURLRef res_url = CFBundleCopyResourcesDirectoryURL(bundle);
    CFURLRef bundle_url = CFBundleCopyBundleURL(bundle);
    gchar *retval = NULL;

    if (res_url && bundle_url && !CFEqual(res_url, bundle_url)) {
        guint len = 1024;
        gchar *path = g_new(gchar, len);

        while (!CFURLGetFileSystemRepresentation(res_url, true,
                                                 (UInt8*)path, len)) {
            len *= 2;
            path = g_renew(path, gchar, len);
        }
        retval = g_strdup(path);
        g_free(path);
    }
    if (res_url)
        CFRelease(res_url);
    if (bundle_url)
        CFRelease(bundle_url);

    return retval;
}
#else
static gchar*
get_osx_module_directory(void)
{
    return NULL;
}
#endif

static gboolean
try_remove_trailing_directory(gchar *path, guint len,
                              const gchar *suffix)
{
    guint slen = strlen(suffix);

    if (slen < len)
        return FALSE;

    if (memcmp(path + len - slen, suffix, slen) == 0) {
        path[len - slen] = '\0';
        return TRUE;
    }
    return FALSE;
}

/* FIXME: We must keep the suffix somewhere if it's lib64 or lib32 for
 * module path reconstruction. */
static void
fix_module_directory(gchar *path)
{
    if (!path)
        return;

    guint len = strlen(path);
    if (try_remove_trailing_directory(path, len, G_DIR_SEPARATOR_S "bin")
        || try_remove_trailing_directory(path, len, G_DIR_SEPARATOR_S "lib")
        || try_remove_trailing_directory(path, len, G_DIR_SEPARATOR_S "lib64")
        || try_remove_trailing_directory(path, len, G_DIR_SEPARATOR_S "lib32"))
        return;

    return;
}

static gboolean
directory_seems_good(const gchar *path, guint mode)
{
#if 0
    g_printerr("absolute: %d\n", g_path_is_absolute(path));
    g_printerr("is-dir: %d\n", g_file_test(path, G_FILE_TEST_IS_DIR));
    g_printerr("r-access: %d\n", g_access(path, R_OK) == 0);
    g_printerr("w-access: %d\n", g_access(path, W_OK) == 0);
    g_printerr("x-access: %d\n", g_access(path, X_OK) == 0);
#endif
    return (g_path_is_absolute(path)
            && g_file_test(path, G_FILE_TEST_IS_DIR)
            && g_access(path, mode) == 0);
}

static gboolean
libdir_seems_good(const gchar *path)
{
    if (!directory_seems_good(path, R_OK | X_OK))
        return FALSE;

    /* FIXME FIXME: If libgwy is installed separately from the application, the
     * "modules" subdirectory might not exist! */
    /*
    gchar *subdir = g_build_filename(path, "modules", NULL);
    gboolean ok = directory_seems_good(subdir, R_OK | X_OK);
    g_free(subdir);

    return ok;
    */
    return TRUE;
}

static gboolean
datadir_seems_good(const gchar *path)
{
    if (!directory_seems_good(path, R_OK | X_OK))
        return FALSE;

    /* FIXME: We do not have any data yet. */
    /*
    gboolean ok = FALSE;
    const gchar const* const components[] = {
        "gradients", "glmaterials", "pixmaps", NULL
    };
    for (const gchar *const* comp = components; ok && *comp; comp++) {
        gchar *subdir = g_build_filename(path, *comp, NULL);
        ok = directory_seems_good(subdir, R_OK | X_OK);
        g_free(subdir);
    }

    return ok;
    */
    return TRUE;
}

static gboolean
localedir_seems_good(const gchar *path)
{
    if (!directory_seems_good(path, R_OK | X_OK))
        return FALSE;

    /* FIXME: We do not have any translations yet. */
    /*
    gchar *subdir = g_build_filename(path, "cs", NULL);
    gboolean ok = directory_seems_good(subdir, R_OK | X_OK);
    g_free(subdir);

    return ok;
    */
    return TRUE;
}

static void
check_base_dir(const gchar *basedir,
               gchar **plibdir,
               gchar **pdatadir,
               gchar **plocaledir)
{
    /* TODO: Multilib support: (1) from fix_module_directory()
     * (2) from AC_LIB_PREPARE_MULTILIB result */
    if (plibdir) {
        gchar *path = g_build_filename(basedir, "lib", PACKAGEDIR, NULL);
        if (libdir_seems_good(path))
            *plibdir = path;
        else
            g_free(path);
    }

    if (pdatadir) {
        gchar *path = g_build_filename(basedir, "share", PACKAGEDIR, NULL);
        if (datadir_seems_good(path))
            *pdatadir = path;
        else
            g_free(path);
    }

    if (plocaledir) {
        gchar *path = g_build_filename(basedir, "share", "locale", NULL);
        if (localedir_seems_good(path))
            *plocaledir = path;
        else
            g_free(path);
    }
}

static gpointer
find_self_impl(G_GNUC_UNUSED gpointer arg)
{
    GetModuleDirectoryFunc get_module_directory[] = {
        &get_osx_module_directory,
        &get_unix_module_directory,
        &get_win32_module_directory,
    };
    gchar *basedir, *path;
    const gchar *dir;
    guint i;

    /* Explicite variables */
    if ((dir = g_getenv("GWYDDION_LIBDIR")) && libdir_seems_good(dir))
        libdir = g_strdup(dir);
    if ((dir = g_getenv("GWYDDION_DATADIR")) && datadir_seems_good(dir))
        datadir = g_strdup(dir);
    if ((dir = g_getenv("GWYDDION_LOCALE_DIR")) && localedir_seems_good(dir))
        localedir = g_strdup(dir);
    if (libdir && datadir && localedir)
        return GUINT_TO_POINTER(TRUE);

    /* Operating system dependent location mechanisms */
    for (i = 0; i < G_N_ELEMENTS(get_module_directory); i++) {
        if ((basedir = get_module_directory[i]())) {
            fix_module_directory(basedir);
            check_base_dir(basedir,
                           libdir ? NULL : &libdir,
                           datadir ? NULL : &datadir,
                           localedir ? NULL : &localedir);
            g_free(basedir);
        }
        if (libdir && datadir && localedir)
            return GUINT_TO_POINTER(TRUE);
    }

    /* Compile-time defaults */
    if ((dir = GWY_LIBDIR) && libdir_seems_good(dir))
        libdir = g_strdup(dir);
    if ((dir = GWY_DATADIR) && datadir_seems_good(dir))
        datadir = g_strdup(dir);
    if ((dir = GWY_LOCALE_DIR) && localedir_seems_good(dir))
        localedir = g_strdup(dir);
    if (libdir && datadir && localedir)
        return GUINT_TO_POINTER(TRUE);

    /* Gwyddion program path */
    if ((path = g_find_program_in_path("gwyddion"))) {
        basedir = g_path_get_dirname(path);
        fix_module_directory(basedir);
        check_base_dir(basedir,
                       libdir ? NULL : &libdir,
                       datadir ? NULL : &datadir,
                       localedir ? NULL : &localedir);
        g_free(basedir);
        g_free(path);
    }
    if (libdir && datadir && localedir)
        return GUINT_TO_POINTER(TRUE);

    return GUINT_TO_POINTER(FALSE);
}

static void
find_self(void)
{
    static GOnce found_self = G_ONCE_INIT;
    g_once(&found_self, find_self_impl, NULL);
}

/**
 * gwy_library_directory:
 * @subdir: Subdirectory to return, or %NULL for the base directory.
 *
 * Obtains the Gwyddion system library directory.
 *
 * This is where modules and other architecture-dependent files reside.
 * For instance, for a standard Unix installation and %NULL @subdir the return
 * value would be "/usr/local/lib64/gwyddion3".  On MS Windows, it can be
 * something like "C:\Program Files\Gwyddion3\lib\gwyddion3".
 *
 * The highest precedence for determinig this directory has the environment
 * variable <envar>GWYDDION_LIBDIR</envar>, the libgwy and Gwyddion
 * installation directory follow.  Note if the environment variable holds
 * a value that does not seem to be a Gwyddion library directory, its value is
 * ignored.
 *
 * Returns: The requested subdirectory of Gwyddion system library directory
 *          as a newly allocated string.
 *          %NULL is returned if such directory could not be determined.
 **/
gchar*
gwy_library_directory(const gchar *subdir)
{
    find_self();
    if (libdir)
        return g_build_filename(libdir, subdir, NULL);
    return NULL;
}

/**
 * gwy_data_directory:
 * @subdir: Subdirectory to return, or %NULL for the base directory.
 *
 * Obtains the Gwyddion system data directory.
 *
 * This is where pixmaps, false color gradients and other
 * architecture-independent files reside.  For instance, for a standard Unix
 * installation and %NULL @subdir the return value would be
 * "/usr/local/share/gwyddion3".  On MS Windows, it can be something like
 * "C:\Program Files\Gwyddion3\share\gwyddion3".
 *
 * The highest precedence for determinig this directory has the environment
 * variable <envar>GWYDDION_DATADIR</envar>, the libgwy and Gwyddion
 * installation directory follow.  Note if the environment variable holds
 * a value that does not seem to be a Gwyddion data directory, its value is
 * ignored.
 *
 * Returns: The requested subdirectory of Gwyddion system data directory
 *          as a newly allocated string.
 *          %NULL is returned if such directory could not be determined.
 **/
gchar*
gwy_data_directory(const gchar *subdir)
{
    find_self();
    if (datadir)
        return g_build_filename(datadir, subdir, NULL);
    return NULL;
}

/**
 * gwy_locale_directory:
 * @subdir: Subdirectory to return, or %NULL for the base directory.
 *
 * Obtains the Gwyddion system locale directory.
 *
 * This is where the translation files files reside.  For instance, for a
 * standard Unix installation and %NULL @subdir the return value would be
 * "/usr/local/share/locale".  On MS Windows, it can be something like
 * "C:\Program Files\Gwyddion\share\locale".
 *
 * The highest precedence for determinig this directory has the environment
 * variable <envar>GWYDDION_LOCALE_DIR</envar>, the libgwy and Gwyddion
 * installation directory follows.  Note if the environment variable holds
 * a value that does not seem to be a locale directory, its value is ignored.
 *
 * Returns: The requested subdirectory of Gwyddion system locale directory
 *          as a newly allocated string.
 *          %NULL is returned if such directory could not be determined.
 **/
gchar*
gwy_locale_directory(const gchar *subdir)
{
    find_self();
    if (localedir)
        return g_build_filename(localedir, subdir, NULL);
    return NULL;
}

static gboolean
userdir_seems_good(const gchar *path)
{
    if (!directory_seems_good(path, R_OK | X_OK | W_OK)) {
        return FALSE;
    }

    /* No further tests? */

    return TRUE;
}

// Return %TRUE if @dir seems to be a fine user directory.  Free @dir and set
// it to %NULL and return %FALSE if it does not.
static gboolean
ensure_user_dir(gchar **dir)
{
    if (directory_seems_good(*dir, R_OK | X_OK | W_OK))
        return TRUE;

    if (g_mkdir(*dir, 0700) == 0)
        return TRUE;

    GWY_FREE(*dir);
    return FALSE;
}

static gpointer
find_user_dir(G_GNUC_UNUSED gpointer arg)
{
    const gchar *dir;

    /* Explicite variables */
    if ((dir = g_getenv("HOME")) && userdir_seems_good(dir)) {
        userdir = g_build_filename(dir, "." PACKAGEDIR, NULL);
        if (ensure_user_dir(&userdir))
            return GUINT_TO_POINTER(TRUE);
    }

    /* GLib */
#ifdef G_OS_WIN32
    if ((dir = g_get_user_config_dir()) && userdir_seems_good(dir)) {
        userdir = g_build_filename(dir, PACKAGEDIR, NULL);
        if (ensure_user_dir(&userdir))
            return GUINT_TO_POINTER(TRUE);
    }
#endif

#ifdef G_OS_UNIX
    if ((dir = g_get_home_dir()) && userdir_seems_good(dir)) {
        userdir = g_build_filename(dir, "." PACKAGEDIR, NULL);
        if (ensure_user_dir(&userdir))
            return GUINT_TO_POINTER(TRUE);
    }
#endif

    return GUINT_TO_POINTER(FALSE);
}

/**
 * gwy_user_directory:
 * @subdir: Subdirectory to return, or %NULL for the base directory.
 *
 * Obtains the Gwyddion user directory.
 *
 * This is where the user configuration and resources reside.  For instance,
 * for a typical Unix user yeti and %NULL @subdir the return value would be
 * "/home/yeti/.gwyddion3".  On MS Windows, it can be something like
 * "C:\Documents and Settings\yeti\Application Data\Gwyddion3".  Even though it
 * is unlikely that Yeti has an MS Windows account.
 *
 * The highest precedence for determinig this directory has the environment
 * variable <envar>HOME</envar>, then the user directory determined by system
 * specific means.  Note if the environment variable holds a value that does
 * not seem to be a writable directory, its value is ignored.
 *
 * Returns: The requested subdirectory of Gwyddion user directory
 *          as a newly allocated string.
 *          %NULL is returned if such directory could not be determined.
 **/
gchar*
gwy_user_directory(const gchar *subdir)
{
    static GOnce found_user = G_ONCE_INIT;
    g_once(&found_user, find_user_dir, NULL);

    if (userdir) {
        gchar *dir = g_build_filename(userdir, subdir, NULL);
        if (ensure_user_dir(&dir))
            return dir;
    }
    return NULL;
}

static void
add_unique_string(GPtrArray *paths,
                  gchar *str)
{
    if (!str)
        return;

    for (gsize i = 0; i < paths->len; i++) {
        if (gwy_strequal(g_ptr_array_index(paths, i), str)) {
            g_free(str);
            return;
        }
    }

    if (directory_seems_good(str, R_OK | X_OK))
        g_ptr_array_add(paths, str);
    else
        g_free(str);
}

static void
add_from_list(GPtrArray *paths,
              const gchar *list,
              const gchar *subdir)
{
    if (!list)
        return;

    gchar *s = g_strdup(list);
    gchar *end = s + strlen(s) + 1;

    g_strdelimit(s, G_SEARCHPATH_SEPARATOR_S, '\0');
    for (gchar *p = s; p != end; p += strlen(p) + 1)
        add_unique_string(paths, g_build_filename(p, PACKAGEDIR, subdir, NULL));

    g_free(s);
}

/**
 * gwy_library_search_path:
 * @subdir: Subdirectory to return, or %NULL for the base directory.
 *
 * Obtains a list of library directories to look for libraries in.
 *
 * The list should be searched in given order.
 *
 * On a typical Unix system the list might look for instance
 * "/home/yeti/.gwyddion3", "/usr/local/lib64/gwyddion3",
 * "/usr/lib64/gwyddion3".  On MS Windows, it will be probably quite short.
 *
 * Returns: (transfer full):
 *          %NULL-terminated list of directories, to be freed with
 *          g_strfreev().
 **/
gchar**
gwy_library_search_path(const gchar *subdir)
{
    GPtrArray *paths = g_ptr_array_new();

    /* TODO: Specific environment variables, if necessary. */
    add_unique_string(paths, gwy_user_directory(subdir));
    add_unique_string(paths, gwy_library_directory(subdir));
    add_from_list(paths, GWY_LIB_PATH, subdir);
    g_ptr_array_add(paths, NULL);
    return (gchar**)g_ptr_array_free(paths, FALSE);
}

/**
 * gwy_data_search_path:
 * @subdir: Subdirectory to return, or %NULL for the base directory.
 *
 * Obtains a list of data directories to look for data in.
 *
 * The list should be searched in given order.
 *
 * On a typical Unix system the list might look for instance
 * "/home/yeti/.gwyddion3", "/usr/local/share/gwyddion3",
 * "/usr/share/gwyddion3".  On MS Windows, it will be probably quite short.
 *
 * Returns: (transfer full):
 *          %NULL-terminated list of directories, to be freed with
 *          g_strfreev().
 **/
gchar**
gwy_data_search_path(const gchar *subdir)
{
    GPtrArray *paths = g_ptr_array_new();

    /* TODO: Specific environment variables, if necessary. */
    add_unique_string(paths, gwy_user_directory(subdir));
    add_unique_string(paths, gwy_data_directory(subdir));
    add_from_list(paths, g_getenv("XDG_DATA_DIRS"), subdir);
    g_ptr_array_add(paths, NULL);
    return (gchar**)g_ptr_array_free(paths, FALSE);
}

/**
 * gwy_tune_algorithms:
 * @key: Identification of the parameter to set.
 * @value: String representation of new value of the parameter.
 *
 * Sets tunable parameters of internal algorithms.
 *
 * Do not use this method in applications.  It exists for the purpose of
 * testing, benchmarking and tuning libgwy algorithms and there is no guarantee
 * what parameters might exist, what values they might take and what effect
 * changing them might have.  Tests and benchmarks that use it are always bound
 * to a specific version of Gwyddion.
 **/
void
gwy_tune_algorithms(const gchar *key,
                    const gchar *value)
{
    g_return_if_fail(key);
    if (gwy_strequal(key, "convolution-method"))
        _gwy_tune_convolution_method(value);
    else if (gwy_strequal(key, "median-filter-method"))
        _gwy_tune_median_filter_method(value);
    else
        g_warning("Unknown tunable parameter %s.", key);
}

/**
 * SECTION: main
 * @title: main
 * @short_description: Library-level functions
 *
 * Note that none of the directories returned by the path finding functions is
 * guaranteed to be writable, readable or exist at all.  Even though the
 * existence and/or properties of the directories are verified on certain
 * occassions, the directory might be deleted just between the check and your
 * attempt to do something with it.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
