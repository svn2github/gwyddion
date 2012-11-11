/*
 *  $Id$
 *  Copyright (C) 2012 David Nečas (Yeti).
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

#include <math.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf/gdk-pixdata.h>
#include "libgwy/strfuncs.h"
#include "libgwyui/stock.h"
#include "libgwyui/icons-gresource.h"

// We use PFX only in error messages.
#define PFX "resource://"
#define RESOURCE_PATH "/net/gwyddion/pixmaps/"

#define ICON_SIZE_NAME_ABOUT "gwy-about"
#define ICON_SIZE_NAME_COLOR_AXIS "gwy-color-axis"

// These are indexes into icon_sizes[] below.
enum {
    ICON_SIZE_IDX_COLOR_AXIS = 6,
    ICON_SIZE_IDX_ABOUT = 7,
};

static void           slurp_icons_gresource          (GHashTable *icons);
static void           slurp_icon_directory_gresource (GHashTable *icons,
                                                      GResource *gresource,
                                                      GString *path);
static GtkIconSource* bytes_to_icon_source           (GBytes *bytes,
                                                      const gchar *fullpath,
                                                      gchar *filename,
                                                      gchar **id);
static void           register_and_free_icon_set_list(const gchar *id,
                                                      GList *list,
                                                      GtkIconFactory *factory);
static void           register_icon_size             (const gchar *name,
                                                      guint i);

static struct {
    gchar letter;
    GtkStateType state;
}
const state_letters[] = {
    { 'n', GTK_STATE_NORMAL },
    { 'a', GTK_STATE_ACTIVE },
    { 'p', GTK_STATE_PRELIGHT },
    { 's', GTK_STATE_SELECTED },
    { 'i', GTK_STATE_INSENSITIVE },
};

/* FIXME: Of course, this is conceptually wrong.  However some estimate is
 * better than nothing when we have more than one size of the same icon. */
static struct {
    guint width;
    guint height;
    GtkIconSize gtksize;
}
icon_sizes[] = {
    /*0*/ { 16, 16, GTK_ICON_SIZE_MENU },
    /*1*/ { 18, 18, GTK_ICON_SIZE_SMALL_TOOLBAR },
    /*2*/ { 20, 20, GTK_ICON_SIZE_BUTTON },
    /*3*/ { 24, 24, GTK_ICON_SIZE_LARGE_TOOLBAR },
    /*4*/ { 32, 32, GTK_ICON_SIZE_DND },
    /*5*/ { 48, 48, GTK_ICON_SIZE_DIALOG },
    /*6*/ { 16, 21, GTK_ICON_SIZE_INVALID },
    /*7*/ { 60, 60, GTK_ICON_SIZE_INVALID },
};

static GtkIconFactory *the_icon_factory = NULL;

/**
 * gwy_icon_size_about:
 *
 * Provides icon size for about dialogs.
 *
 * Use %GWY_ICON_SIZE_ABOUT macro instead for formal consistency with
 * #GtkIconSize enumerated values.
 *
 * This function returns %GTK_ICON_SIZE_INVALID until Gwyddion stock items are
 * registered.
 *
 * Returns: Icon size for about dialogs.
 **/
GtkIconSize
gwy_icon_size_about(void)
{
    return icon_sizes[ICON_SIZE_IDX_ABOUT].gtksize;
}

/**
 * gwy_icon_size_color_axis:
 *
 * Provides icon size for raster view colour axis buttons.
 *
 * Use %GWY_ICON_SIZE_COLOR_AXIS macro instead for formal consistency with
 * #GtkIconSize enumerated values.
 *
 * This function returns %GTK_ICON_SIZE_INVALID until Gwyddion stock items are
 * registered.
 *
 * Returns: Icon size for raster view colour axis buttons.
 **/
GtkIconSize
gwy_icon_size_color_axis(void)
{
    return icon_sizes[ICON_SIZE_IDX_COLOR_AXIS].gtksize;
}

/**
 * gwy_stock_register_stock_items:
 *
 * Registers stock items.
 *
 * This function must be called before any Gwyddion stock items are used.
 **/
void
gwy_register_stock_items(void)
{
    g_return_if_fail(!the_icon_factory);
    register_icon_size(ICON_SIZE_NAME_ABOUT, ICON_SIZE_IDX_ABOUT);
    register_icon_size(ICON_SIZE_NAME_COLOR_AXIS, ICON_SIZE_IDX_COLOR_AXIS);

    GHashTable *icons = g_hash_table_new_full(g_str_hash, g_str_equal,
                                              g_free, NULL);
    the_icon_factory = gtk_icon_factory_new();
    slurp_icons_gresource(icons);
    g_hash_table_foreach(icons, (GHFunc)register_and_free_icon_set_list,
                         the_icon_factory);
    g_hash_table_destroy(icons);
    gtk_icon_factory_add_default(the_icon_factory);
}

static void
slurp_icons_gresource(GHashTable *icons)
{
    _gwy_icons_register_resource();
    GResource *gresource = _gwy_icons_get_resource();
    g_assert(gresource);
    GString *path = g_string_new(RESOURCE_PATH);
    slurp_icon_directory_gresource(icons, gresource, path);
    g_string_free(path, TRUE);
}

static void
slurp_icon_directory_gresource(GHashTable *icons,
                               GResource *gresource,
                               GString *path)
{
    GError *error = NULL;
    gchar **files = g_resource_enumerate_children(gresource, path->str, 0,
                                                  &error);
    if (!files) {
        g_warning("Cannot enumerate children in builtin resource within "
                  "path \"%s%s\": %s",
                  PFX, path->str, error->message);
        g_clear_error(&error);
        return;
    }

    for (gchar **filename = files; *filename; filename++) {
        guint len = path->len;
        g_string_append(path, *filename);

        if (g_str_has_suffix(*filename, "/"))
            slurp_icon_directory_gresource(icons, gresource, path);
        else {
            GBytes *bytes = g_resource_lookup_data(gresource, path->str, 0,
                                                   &error);
            if (!bytes) {
                g_warning("Cannot open \"%s%s\" in builtin resource: %s.\n",
                          PFX, path->str, error->message);
                g_clear_error(&error);
                continue;
            }
            gchar *id = NULL;
            GtkIconSource *source = bytes_to_icon_source(bytes, path->str,
                                                         *filename, &id);
            if (source) {
                GList *list = (GList*)g_hash_table_lookup(icons, id);
                list = g_list_append(list, source);
                g_hash_table_replace(icons, id, list);
            }
            else
                g_bytes_unref(bytes);
        }
        g_string_truncate(path, len);
    }
    g_strfreev(files);
}

// XXX: Uses @filename as a scratch space!
// @bytes are unreferenced (a) on failure (b) if the resource is not pixdata so
// we make a copy anyway.  Case (a) is handled by caller if we return NULL.
static GtkIconSource*
bytes_to_icon_source(GBytes *bytes, const gchar *fullpath,
                     gchar *filename, gchar **id)
{
    gboolean unref_bytes = FALSE;
    gsize size;
    const guchar *data = g_bytes_get_data(bytes, &size);

    if (size < 4) {
        g_warning("Icon resource \"%s%s\" is smaller than 4 bytes.",
                  PFX, fullpath);
        return NULL;
    }

    GdkPixbuf *pixbuf = NULL;
    GError *error = NULL;
    if (memcmp(data, "GdkP", 4) == 0) {
        GdkPixdata pixdata;
        if (!gdk_pixdata_deserialize(&pixdata, size, data, &error)
            || !(pixbuf = gdk_pixbuf_from_pixdata(&pixdata, FALSE, &error))) {
            g_warning("Failed to load %s icon resource \"%s%s\": %s.",
                      "GdkPixdata", PFX, fullpath, error->message);
            g_clear_error(&error);
            return NULL;
        }
    }
    else if (memcmp(data, "\x89PNG", 4) == 0) {
        GdkPixbufLoader *loader = gdk_pixbuf_loader_new();
        if (!gdk_pixbuf_loader_write(loader, data, size, &error)) {
            g_warning("Failed to load %s icon resource \"%s%s\": %s.",
                      "PNG", PFX, fullpath, error->message);
            g_clear_error(&error);
            gdk_pixbuf_loader_close(loader, NULL);
            g_object_unref(loader);
            return NULL;
        }
        if (!gdk_pixbuf_loader_close(loader, &error)) {
            g_warning("Failed to load %s icon resource \"%s%s\": %s.",
                      "PNG", PFX, fullpath, error->message);
            g_clear_error(&error);
            g_object_unref(loader);
            return NULL;
        }
        pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
        g_assert(pixbuf);
        g_object_ref(pixbuf);
        g_object_unref(loader);
        unref_bytes = TRUE;
    }

    if (!pixbuf) {
        g_warning("Icon resource \"%s%s\" is in an unknown format.",
                  PFX, fullpath);
        return NULL;
    }

    gwy_str_remove_suffix(filename, ".png", NULL);
    g_strdelimit(filename, "_", '-');

    GtkStateType state = -1;
    guint len = strlen(filename);
    if (len > 2 && filename[len-2] == '.') {
        gchar letter = filename[len-1];
        for (guint i = 0; i < G_N_ELEMENTS(state_letters); i++) {
            if (letter == state_letters[i].letter) {
                state = state_letters[i].state;
                break;
            }
        }
        if (state == (GtkStateType)-1) {
            g_warning("Ignoring unknown state letter \"%c\" "
                      "in filename \"%s%s\".",
                      letter, PFX, fullpath);
        }
        filename[len-2] = '\0';
    }

    if (g_str_has_prefix(filename, "gwy-"))
        *id = g_strdup(filename);
    else
        *id = g_strconcat("gwy-", filename, NULL);

    guint width = gdk_pixbuf_get_width(pixbuf),
          height = gdk_pixbuf_get_height(pixbuf);
    GtkIconSize gtksize = -1;

    for (guint i = 0; i < G_N_ELEMENTS(icon_sizes); i++) {
        if (width == icon_sizes[i].width && height == icon_sizes[i].height) {
            gtksize = icon_sizes[i].gtksize;
            break;
        }
    }

    if (G_UNLIKELY(gtksize == (GtkIconSize)-1)) {
        g_warning("Dimensions %ux%u of icon \"%s%s\" do not match any known "
                  "icon size.",
                  width, height, PFX, fullpath);

        gdouble bestmatch = G_MAXDOUBLE;
        gtksize = GTK_ICON_SIZE_LARGE_TOOLBAR;
        for (guint i = 0; i < G_N_ELEMENTS(icon_sizes); i++) {
            gdouble mw = log(width/(gdouble)icon_sizes[i].width),
                    mh = log(height/(gdouble)icon_sizes[i].height),
                    m = mw*mw + mh*mh;
            if (m < bestmatch) {
                bestmatch = m;
                gtksize = icon_sizes[i].gtksize;
            }
        }
    }

    GtkIconSource *source = gtk_icon_source_new();
    gtk_icon_source_set_size(source, gtksize);
    gtk_icon_source_set_direction_wildcarded(source, TRUE);
    gtk_icon_source_set_size_wildcarded(source, FALSE);
    if (state != (GtkStateType)-1) {
        gtk_icon_source_set_state_wildcarded(source, FALSE);
        gtk_icon_source_set_state(source, state);
    }
    gtk_icon_source_set_pixbuf(source, pixbuf);
    g_object_unref(pixbuf);

    if (unref_bytes)
        g_bytes_unref(bytes);

    return source;
}

static void
register_and_free_icon_set_list(const gchar *id,
                                GList *list,
                                GtkIconFactory *factory)
{
    GtkIconSet *icon_set;
    GtkIconSource *icon_source, *largest;
    GtkIconSize gtksize;
    GList *l;
    gint max, w, h;

    icon_set = gtk_icon_set_new();
    max = 0;
    largest = NULL;
    for (l = list; l; l = g_list_next(l)) {
        icon_source = (GtkIconSource*)l->data;
        gtksize = gtk_icon_source_get_size(icon_source);
        g_assert(gtk_icon_size_lookup(gtksize, &w, &h));
        if (w*h > max)
            largest = icon_source;
    }
    if (!largest) {
        g_warning("No icon of nonzero size in the set");
        return;
    }
    gtk_icon_source_set_size_wildcarded(largest, TRUE);

    for (l = list; l; l = g_list_next(l)) {
        icon_source = (GtkIconSource*)l->data;
        gtk_icon_set_add_source(icon_set, icon_source);
        gtk_icon_source_free(icon_source);
    }
    gtk_icon_factory_add(factory, id, icon_set);
    g_list_free(list);
}

static void
register_icon_size(const gchar *name, guint i)
{
    GtkIconSize icon_size = gtk_icon_size_register(name,
                                                   icon_sizes[i].width,
                                                   icon_sizes[i].height);
    icon_sizes[i].gtksize = icon_size;
}

/**
 * SECTION: stock
 * @section_id: libgwyui-stock
 * @title: stock
 * @short_description: Stock items
 *
 * Gwyddion registers a number of stock icons and also some new icon sizes.
 * They must be registered explicitly with gwy_register_stock_items().
 **/

/**
 * GWY_ICON_SIZE_ABOUT:
 *
 * Icon size for about dialogs.
 *
 * Note this macro does not resolve to a constant expression and cannot be used
 * in constant initialisers.
 **/

/**
 * GWY_ICON_SIZE_COLOR_AXIS:
 *
 * Icon size for raster view colour axis buttons.
 *
 * Note this macro does not resolve to a constant expression and cannot be used
 * in constant initialisers.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
