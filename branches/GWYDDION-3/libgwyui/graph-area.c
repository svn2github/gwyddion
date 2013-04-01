/*
 *  $Id$
 *  Copyright (C) 2013 David Nečas (Yeti).
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

#include "libgwy/macros.h"
#include "libgwy/object-utils.h"
#include "libgwyui/types.h"
#include "libgwyui/cairo-utils.h"
#include "libgwyui/graph-area.h"

enum {
    PROP_0,
    N_PROPS
};

typedef struct _GwyGraphAreaPrivate GraphArea;

struct _GwyGraphAreaPrivate {
    gchar dummy;
};

static void gwy_graph_area_finalize    (GObject *object);
static void gwy_graph_area_dispose     (GObject *object);
static void gwy_graph_area_set_property(GObject *object,
                                        guint prop_id,
                                        const GValue *value,
                                        GParamSpec *pspec);
static void gwy_graph_area_get_property(GObject *object,
                                        guint prop_id,
                                        GValue *value,
                                        GParamSpec *pspec);

static GParamSpec *properties[N_PROPS];

G_DEFINE_TYPE(GwyGraphArea, gwy_graph_area, GTK_TYPE_WIDGET);

static void
gwy_graph_area_class_init(GwyGraphAreaClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    g_type_class_add_private(klass, sizeof(GraphArea));

    gobject_class->finalize = gwy_graph_area_finalize;
    gobject_class->dispose = gwy_graph_area_dispose;
    gobject_class->get_property = gwy_graph_area_get_property;
    gobject_class->set_property = gwy_graph_area_set_property;

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, properties[i]);
}

static void
gwy_graph_area_init(GwyGraphArea *grapharea)
{
    grapharea->priv = G_TYPE_INSTANCE_GET_PRIVATE(grapharea,
                                                  GWY_TYPE_GRAPH_AREA,
                                                  GraphArea);
}

static void
gwy_graph_area_finalize(GObject *object)
{
    GraphArea *priv = GWY_GRAPH_AREA(object)->priv;
    G_OBJECT_CLASS(gwy_graph_area_parent_class)->finalize(object);
}

static void
gwy_graph_area_dispose(GObject *object)
{
    GwyGraphArea *grapharea = GWY_GRAPH_AREA(object);
    G_OBJECT_CLASS(gwy_graph_area_parent_class)->dispose(object);
}

static void
gwy_graph_area_set_property(GObject *object,
                             guint prop_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
    GwyGraphArea *grapharea = GWY_GRAPH_AREA(object);

    switch (prop_id) {
        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_graph_area_get_property(GObject *object,
                             guint prop_id,
                             GValue *value,
                             GParamSpec *pspec)
{
    GraphArea *priv = GWY_GRAPH_AREA(object)->priv;

    switch (prop_id) {
        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

/**
 * gwy_graph_area_new:
 *
 * Creates a new graph curve.
 *
 * Returns: The newly created graph curve.
 **/
GwyGraphArea*
gwy_graph_area_new(void)
{
    return (GwyGraphArea*)g_object_newv(GWY_TYPE_GRAPH_AREA, 0, NULL);
}

/**
 * SECTION: graph-area
 * @title: GwyGraphArea
 * @short_description: Area of graph containing the plots
 **/

/**
 * GwyGraphArea:
 *
 * Object representing a graph area.
 *
 * The #GwyGraphArea struct contains private data only and should be
 * accessed using the functions below.
 **/

/**
 * GwyGraphAreaClass:
 *
 * Class of graph areas.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
