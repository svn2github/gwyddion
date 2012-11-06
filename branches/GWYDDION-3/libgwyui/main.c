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

#include "libgwyui/main.h"
#include "libgwyui/ui-gresource.h"

static gpointer process_ui_gresource(gpointer arg);

static GtkStyleProvider *provider = NULL;
static GOnce ui_gresource_processed = G_ONCE_INIT;

GtkStyleProvider*
gwy_get_style_provider(void)
{
    g_once(&ui_gresource_processed, process_ui_gresource, NULL);
    return provider;
}

static gpointer
process_ui_gresource(G_GNUC_UNUSED gpointer arg)
{
    _gwy_ui_register_resource();
    GResource *gresource = _gwy_ui_get_resource();
    g_assert(gresource);
    GError *error = NULL;
    const gchar *path = "/net/gwyddion/ui/gwy.css";
    GBytes *bytes = g_resource_lookup_data(gresource, path, 0, &error);
    if (!bytes) {
        g_critical("Cannot open \"%s\" in builtin resource: %s.\n",
                   path, error->message);
        g_clear_error(&error);
        return NULL;
    }
    gsize size;
    const gchar *data = g_bytes_get_data(bytes, &size);
    GtkCssProvider *cssprovider = gtk_css_provider_new();
    if (!gtk_css_provider_load_from_data(cssprovider, data, size, &error)) {
        g_warning("GtkCssProvider cannot load data \"%s\" in builtin "
                  "resource: %s.\n",
                   path, error->message);
        g_clear_error(&error);
    }
    g_bytes_unref(bytes);
    provider = GTK_STYLE_PROVIDER(cssprovider);
    return NULL;
}

/**
 * SECTION: main
 * @section_id: libgwyui-main
 * @title: main
 * @short_description: Library-level functions
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
