/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2006 David Necas (Yeti), Petr Klapetek.
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
#include <string.h>
#include <stdio.h>
#include <errno.h>

#if (defined(HAVE_SYS_STAT_H) || defined(_WIN32))
#include <sys/stat.h>
/* And now we are in a deep s... */
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#include <glib/gstdio.h>

#ifdef HAVE_GTKGLEXT
#include <gtk/gtkglinit.h>
#endif

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwyserializable.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>

static gboolean create_config_dir_real         (const gchar *cfgdir,
                                                GError **error);
static void     gwy_app_settings_set_defaults  (GwyContainer *settings);
static void     gwy_app_settings_gather        (GwyContainer *settings);
static void     gwy_app_settings_apply         (GwyContainer *settings);

static const gchar *magic_header = "Gwyddion Settings 1.0\n";

static GwyContainer *gwy_settings = NULL;
static gboolean gwy_gl_ok = FALSE;

/**
 * gwy_app_settings_get:
 *
 * Gets the Gwyddion settings.
 *
 * The settings are a #GwyContainer automatically loaded at program startup
 * and saved ad its exit.  For storing persistent module data you should
 * use <literal>"/module/YOUR_MODULE_NAME/"</literal> prefix.
 *
 * Returns: The settings as a #GwyContainer.
 **/
GwyContainer*
gwy_app_settings_get(void)
{
    if (!gwy_settings) {
        gwy_settings = GWY_CONTAINER(gwy_container_new());
        gwy_app_settings_set_defaults(gwy_settings);
        gwy_app_settings_apply(gwy_settings);
    }

    return gwy_settings;
}

/**
 * gwy_app_settings_free:
 *
 * Frees Gwyddion settings.
 *
 * Should not be called only by main application.
 **/
void
gwy_app_settings_free(void)
{
    gwy_object_unref(gwy_settings);
}

/**
 * gwy_app_settings_save:
 * @filename: A filename to save the settings to.
 * @error: Location to store loading error to, or %NULL.
 *
 * Saves the settings.
 *
 * Use gwy_app_settings_get_settings_filename() to obtain a suitable default
 * filename.
 *
 * Returns: Whether it succeeded.
 **/
gboolean
gwy_app_settings_save(const gchar *filename,
                      GError **error)
{
    GwyContainer *settings;
    GPtrArray *pa;
    guint i;
    gboolean ok;
    FILE *fh;
    gchar *cfgdir;

    cfgdir = g_path_get_dirname(filename);
    ok = create_config_dir_real(cfgdir, error);
    g_free(cfgdir);
    if (!ok)
        return FALSE;

    gwy_debug("Saving text settings to `%s'", filename);
    settings = gwy_app_settings_get();
    gwy_app_settings_gather(settings);
    g_return_val_if_fail(GWY_IS_CONTAINER(settings), FALSE);
    fh = g_fopen(filename, "w");
    if (!fh) {
        g_set_error(error,
                    GWY_APP_SETTINGS_ERROR, GWY_APP_SETTINGS_ERROR_FILE,
                    _("Cannot open file for writing: %s."),
                    g_strerror(errno));
        return FALSE;
    }
    if (fputs(magic_header, fh) == EOF) {
        g_set_error(error,
                    GWY_APP_SETTINGS_ERROR, GWY_APP_SETTINGS_ERROR_FILE,
                    _("Cannot write to file: %s."), g_strerror(errno));
        fclose(fh);
        return FALSE;
    }

    ok = TRUE;
    pa = gwy_container_serialize_to_text(settings);
    for (i = 0; i < pa->len; i++) {
        if (fputs((gchar*)pa->pdata[i], fh) == EOF) {
            g_set_error(error,
                        GWY_APP_SETTINGS_ERROR, GWY_APP_SETTINGS_ERROR_FILE,
                        _("Cannot write to file: %s."), g_strerror(errno));
            while (i < pa->len)
                g_free(pa->pdata[i]);
            ok = FALSE;
            break;
        }
        fputc('\n', fh);
        g_free(pa->pdata[i]);
    }
    g_ptr_array_free(pa, TRUE);
    fclose(fh);

    return ok;
}

/**
 * gwy_app_settings_load:
 * @filename: A filename to read settings from.
 * @error: Location to store loading error to, or %NULL.
 *
 * Loads settings file.
 *
 * Returns: Whether it succeeded.  In either case you can call
 *          gwy_app_settings_get() then to obtain either the loaded settings
 *          or the old ones (if failed), or an empty #GwyContainer.
 **/
gboolean
gwy_app_settings_load(const gchar *filename,
                      GError **error)
{
    GwyContainer *new_settings;
    GError *err = NULL;
    gchar *buffer = NULL;
    gsize size = 0;

    gwy_debug("Loading settings from `%s'", filename);
    if (!g_file_get_contents(filename, &buffer, &size, &err)) {
        g_set_error(error,
                    GWY_APP_SETTINGS_ERROR, GWY_APP_SETTINGS_ERROR_FILE,
                    _("Cannot read file contents: %s"), err->message);
        g_clear_error(&err);
        return FALSE;
    }

    if (!size || !buffer) {
        g_set_error(error,
                    GWY_APP_SETTINGS_ERROR, GWY_APP_SETTINGS_ERROR_CORRUPT,
                    _("File is empty."));
        g_free(buffer);
        return FALSE;
    }
#ifdef G_OS_WIN32
    gwy_strkill(buffer, "\r");
#endif
    if (!g_str_has_prefix(buffer, magic_header)) {
        g_set_error(error,
                    GWY_APP_SETTINGS_ERROR, GWY_APP_SETTINGS_ERROR_CORRUPT,
                    _("File is corrupted, magic header does not match."));
        g_free(buffer);
        return FALSE;
    }
    new_settings = gwy_container_deserialize_from_text(buffer
                                                       + strlen(magic_header));
    g_free(buffer);
    if (!GWY_IS_CONTAINER(new_settings)) {
        gwy_object_unref(new_settings);
        g_set_error(error,
                    GWY_APP_SETTINGS_ERROR, GWY_APP_SETTINGS_ERROR_CORRUPT,
                    _("File is corrupted, deserialization failed."));
        return FALSE;
    }
    gwy_app_settings_free();
    gwy_settings = new_settings;
    gwy_app_settings_set_defaults(gwy_settings);
    gwy_app_settings_apply(gwy_settings);

    return TRUE;
}

/**
 * gwy_app_settings_create_config_dir:
 * @error: Location to store loading error to, or %NULL.
 *
 * Create gwyddion config directory.
 *
 * Returns: Whether it succeeded (also returns %TRUE if the directory already
 * exists).
 **/
gboolean
gwy_app_settings_create_config_dir(GError **error)
{
    return create_config_dir_real(gwy_get_user_dir(), error);
}

static gboolean
create_config_dir_real(const gchar *cfgdir,
                       GError **error)
{
    gboolean ok;
    gchar **moddirs;
    gint i, n;

    ok = g_file_test(cfgdir, G_FILE_TEST_IS_DIR);
    moddirs = gwy_app_settings_get_module_dirs();
    for (n = 0; moddirs[n]; n++)
        ;
    n /= 2;
    g_assert(n > 0);
    /* put the toplevel module dir before particula module dirs */
    g_free(moddirs[n-1]);
    moddirs[n-1] = g_path_get_dirname(moddirs[n]);

    if (!ok) {
        gwy_debug("Trying to create user config directory %s", cfgdir);
        ok = !g_mkdir(cfgdir, 0700);
        if (!ok)
            g_set_error(error,
                        GWY_APP_SETTINGS_ERROR, GWY_APP_SETTINGS_ERROR_CFGDIR,
                        _("Cannot create user config directory %s: %s"),
                        cfgdir, g_strerror(errno));
    }

    if (ok) {
        for (i = n-1; i < 2*n; i++) {
            if (g_file_test(moddirs[i], G_FILE_TEST_IS_DIR))
                continue;
            gwy_debug("Trying to create user module directory %s", moddirs[i]);
            ok = !g_mkdir(moddirs[i], 0700);
            if (!ok) {
                g_set_error(error,
                            GWY_APP_SETTINGS_ERROR,
                            GWY_APP_SETTINGS_ERROR_CFGDIR,
                            _("Cannot create user module directory %s: %s"),
                            moddirs[i], g_strerror(errno));
                break;
            }
        }
    }
    g_strfreev(moddirs);

    return ok;
}

static void
gwy_app_settings_set_defaults(GwyContainer *settings)
{
    static const GwyRGBA default_mask_color = { 1.0, 0.0, 0.0, 0.5 };
    static const gchar default_preferred_gradients[] =
        "BW1\n"
        "Blend2\n"
        "Caribbean\n"
        "Cold\n"
        "DFit\n"
        "Gray\n"
        "Green-Violet\n"
        "Gwyddion.net\n"
        "Lines\n"
        "Olive\n"
        "Rainbow2\n"
        "Red\n"
        "Sky\n"
        "Spectral\n"
        "Spring\n"
        "Warm";
    static const gchar default_preferred_gl_materials[] =
        "Brass\n"
        "Cyan-Plastic\n"
        "OpenGL-Default\n"
        "Emerald\n"
        "Obsidian\n"
        "Pewter\n"
        "Polished-Gold\n"
        "Red-Rubber\n"
        "Silver\n"
        "Warmish-White";

    if (!gwy_container_contains_by_name(settings, "/mask/alpha")) {
        gwy_container_set_double_by_name(settings, "/mask/red",
                                         default_mask_color.r);
        gwy_container_set_double_by_name(settings, "/mask/green",
                                         default_mask_color.g);
        gwy_container_set_double_by_name(settings, "/mask/blue",
                                         default_mask_color.b);
        gwy_container_set_double_by_name(settings, "/mask/alpha",
                                         default_mask_color.a);
    }
    if (!gwy_container_contains_by_name(settings, "/app/gradients/preferred"))
        gwy_container_set_string_by_name(settings, "/app/gradients/preferred",
                                         g_strdup(default_preferred_gradients));
    if (!gwy_container_contains_by_name(settings, "/app/glmaterials/preferred"))
        gwy_container_set_string_by_name(settings, "/app/glmaterials/preferred",
                                         g_strdup(default_preferred_gl_materials));
    if (!gwy_container_contains_by_name(settings,
                                        "/app/toolbox/visible/graph"))
        gwy_container_set_boolean_by_name(settings,
                                          "/app/toolbox/visible/graph", TRUE);
    if (!gwy_container_contains_by_name(settings,
                                        "/app/toolbox/visible/proc"))
        gwy_container_set_boolean_by_name(settings,
                                          "/app/toolbox/visible/proc", TRUE);
    if (!gwy_container_contains_by_name(settings,
                                        "/app/toolbox/visible/tool"))
        gwy_container_set_boolean_by_name(settings,
                                          "/app/toolbox/visible/tool", TRUE);
    if (!gwy_container_contains_by_name(settings,
                                        "/app/toolbox/visible/zoom"))
        gwy_container_set_boolean_by_name(settings,
                                          "/app/toolbox/visible/zoom", TRUE);
}

static void
add_preferred_resource_name(G_GNUC_UNUSED gpointer key,
                            gpointer item,
                            gpointer user_data)
{
    GwyResource *resource = GWY_RESOURCE(item);
    GPtrArray *array = (GPtrArray*)user_data;

    if (gwy_resource_get_is_preferred(resource))
        g_ptr_array_add(array, (gpointer)gwy_resource_get_name(resource));
}

/**
 * gwy_app_settings_gather:
 * @settings: App settings.
 *
 * Perform settings update that needs to be done only once at shutdown.
 **/
/* TODO: refactor common resource code */
static void
gwy_app_settings_gather(GwyContainer *settings)
{
    GPtrArray *preferred;
    const gchar *name;

    /* Preferred resources */
    preferred = g_ptr_array_new();
    gwy_inventory_foreach(gwy_gradients(),
                          &add_preferred_resource_name, preferred);
    g_ptr_array_add(preferred, NULL);
    gwy_container_set_string_by_name(settings, "/app/gradients/preferred",
                                     g_strjoinv("\n",
                                                (gchar**)preferred->pdata));
    g_ptr_array_set_size(preferred, 0);
    gwy_inventory_foreach(gwy_gl_materials(),
                          &add_preferred_resource_name, preferred);
    g_ptr_array_add(preferred, NULL);
    gwy_container_set_string_by_name(settings, "/app/glmaterials/preferred",
                                     g_strjoinv("\n",
                                                (gchar**)preferred->pdata));
    g_ptr_array_free(preferred, TRUE);

    /* Default resources */
    name = gwy_inventory_get_default_item_name(gwy_gradients());
    if (name)
        gwy_container_set_string_by_name(settings, "/app/gradients/default",
                                         g_strdup(name));
    name = gwy_inventory_get_default_item_name(gwy_gl_materials());
    if (name)
        gwy_container_set_string_by_name(settings, "/app/glmaterials/default",
                                         g_strdup(name));
}

/**
 * gwy_app_settings_apply:
 * @settings: App settings.
 *
 * Applies initial settings to things that need it.
 **/
/* TODO: refactor common resource code */
static void
gwy_app_settings_apply(GwyContainer *settings)
{
    GwyInventory *inventory;
    GwyResource *resource;
    const guchar *s;
    gchar **preferred, **p;
    gboolean disabled;

    /* Preferred resources */
    if (gwy_container_gis_string_by_name(settings, "/app/gradients/preferred",
                                         &s)) {
        inventory = gwy_gradients();
        preferred = g_strsplit(s, "\n", 0);
        for (p = preferred; *p; p++) {
            if ((resource = gwy_inventory_get_item(inventory, *p)))
                gwy_resource_set_is_preferred(resource, TRUE);
        }
        g_strfreev(preferred);
    }
    if (gwy_container_gis_string_by_name(settings, "/app/glmaterials/preferred",
                                         &s)) {
        inventory = gwy_gl_materials();
        preferred = g_strsplit(s, "\n", 0);
        for (p = preferred; *p; p++) {
            if ((resource = gwy_inventory_get_item(inventory, *p)))
                gwy_resource_set_is_preferred(resource, TRUE);
        }
        g_strfreev(preferred);
    }

    /* Default resources */
    if (gwy_container_gis_string_by_name(settings, "/app/gradients/default",
                                         &s))
        gwy_inventory_set_default_item_name(gwy_gradients(), s);
    if (gwy_container_gis_string_by_name(settings, "/app/glmaterials/default",
                                         &s))
        gwy_inventory_set_default_item_name(gwy_gl_materials(), s);

    /* Globally disabled 3D view axes */
    if (gwy_container_gis_boolean_by_name(settings, "/app/3d/axes/disable",
                                          &disabled)
        && disabled)
        gwy_3d_view_class_disable_axis_drawing(disabled);
}

/**
 * gwy_app_settings_get_module_dirs:
 *
 * Returns a list of directories to search modules in.
 *
 * Returns: The list of module directories as a newly allocated array of
 *          newly allocated strings, to be freed with g_str_freev() when
 *          not longer needed.
 **/
gchar**
gwy_app_settings_get_module_dirs(void)
{
    const gchar *module_types[] = {
        "layer", "file", "process", "graph", "tool"
    };
    gchar **module_dirs;
    gchar *p;
    const gchar *q;
    gsize n, i;

    n = G_N_ELEMENTS(module_types);
    module_dirs = g_new(gchar*, 2*(n+1) + 1);

    p = gwy_find_self_dir("modules");
    for (i = 0; i < n; i++)
        module_dirs[i] = g_build_filename(p, module_types[i], NULL);
    module_dirs[i++] = p;

    q = gwy_get_user_dir();
    for (i = 0; i < n; i++)
        module_dirs[n+1 + i] = g_build_filename(q, "modules", module_types[i],
                                                NULL);
    module_dirs[2*n + 1] = g_build_filename(q, "modules", NULL);

    module_dirs[2*n + 2] = NULL;

    return module_dirs;
}

/**
 * gwy_app_settings_get_settings_filename:
 *
 * Returns a suitable human-readable settings file name.
 *
 * Returns: The file name as a newly allocated string.
 **/
gchar*
gwy_app_settings_get_settings_filename(void)
{
    return g_build_filename(gwy_get_user_dir(), "settings", NULL);
}

/**
 * gwy_app_settings_get_log_filename:
 *
 * Returns a suitable log file name.
 *
 * Returns: The file name as a newly allocated string.
 **/
gchar*
gwy_app_settings_get_log_filename(void)
{
    return g_build_filename(gwy_get_user_dir(), "gwyddion.log", NULL);
}

/**
 * gwy_app_settings_get_recent_file_list_filename:
 *
 * Returns a suitable recent file list file name.
 *
 * Returns: The file name as a newly allocated string.
 **/
gchar*
gwy_app_settings_get_recent_file_list_filename(void)
{
    return g_build_filename(gwy_get_user_dir(), "recent-files", NULL);
}

/**
 * gwy_app_gl_init:
 * @argc: Address of the argc parameter of main(). Passed to
 *        gtk_gl_init_check().
 * @argv: Address of the argv parameter of main(). Passed to
 *        gtk_gl_init_check().
 *
 * Checks for working OpenGL and initializes it.
 *
 * When OpenGL support is not compiled in, this function does not do anything.
 * When OpenGL is supported, it calls gtk_gl_init_check() and
 * gwy_widgets_gl_init() (if the former succeeeds).
 *
 * Returns: %TRUE if OpenGL initialization succeeeded.
 **/
gboolean
gwy_app_gl_init(G_GNUC_UNUSED int *argc,
                G_GNUC_UNUSED char ***argv)
{
#ifdef HAVE_GTKGLEXT
    gwy_gl_ok = gtk_gl_init_check(argc, argv) && gwy_widgets_gl_init();
#endif

    return gwy_gl_ok;
}

/**
 * gwy_app_gl_is_ok:
 *
 * Returns OpenGL availability.
 *
 * Returns: The return value is the same as the return value of
 *          gwy_app_gl_init() which needs to be called prior to this function
 *          (until then, the return value is always %FALSE).
 **/
gboolean
gwy_app_gl_is_ok(void)
{
    return gwy_gl_ok;
}

/**
 * gwy_app_settings_error_quark:
 *
 * Returns error domain for application settings operations.
 *
 * See and use %GWY_APP_SETTINGS_ERROR.
 *
 * Returns: The error domain.
 **/
GQuark
gwy_app_settings_error_quark(void)
{
    static GQuark error_domain = 0;

    if (!error_domain)
        error_domain
            = g_quark_from_static_string("gwy-app-settings-error-quark");

    return error_domain;
}

/************************** Documentation ****************************/

/**
 * SECTION:settings
 * @title: settings
 * @short_description: Application and module settings
 *
 * All application and module settings are stored in a one big #GwyContainer
 * which can be obtained by gwy_app_settings_get(). Then you can use
 * #GwyContainer functions to get and save settings.
 *
 * The rest of the setting manipulating functions is normally useful only in
 * main application.
 **/

/**
 * GWY_APP_SETTINGS_ERROR:
 *
 * Error domain for application settings operations. Errors in this domain will
 * be from the #GwyAppSettingsError enumeration. See #GError for information
 * on error domains.
 **/

/**
 * GwyAppSettingsError:
 * @GWY_APP_SETTINGS_ERROR_FILE: Settings file is not readable or writable.
 * @GWY_APP_SETTINGS_ERROR_CORRUPT: Settings file contents is corrupted.
 * @GWY_APP_SETTINGS_ERROR_CFGDIR: User configuration directory is not
 *                                 readable or writable or it does not exist
 *                                 and its creation failed.
 *
 * Error codes returned by application settings functions.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
