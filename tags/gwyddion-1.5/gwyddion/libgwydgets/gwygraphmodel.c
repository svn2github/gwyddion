/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
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

#include <string.h>

#include <libgwyddion/gwyddion.h>
#include "gwygraphcurvemodel.h"
#include "gwygraphmodel.h"

#define GWY_GRAPH_MODEL_TYPE_NAME "GwyGraphModel"
#define GRAPH_GLOBALS_FAKE_TYPE_NAME "GwyGraphModel-graph"

#ifdef I_WANT_A_BROKEN_GWY_GRAPH_MODEL

static void   gwy_graph_model_class_init        (GwyGraphModelClass *klass);
static void   gwy_graph_model_init              (GwyGraphModel *gmodel);
static void   gwy_graph_model_finalize          (GObject *object);
static void   gwy_graph_model_serializable_init (GwySerializableIface *iface);
static void   gwy_graph_model_watchable_init    (GwyWatchableIface *iface);
static GByteArray* gwy_graph_model_serialize    (GObject *obj,
                                                 GByteArray*buffer);
static GObject* gwy_graph_model_deserialize     (const guchar *buffer,
                                                 gsize size,
                                                 gsize *position);
static GObject* gwy_graph_model_duplicate       (GObject *object);
static void   gwy_graph_model_graph_destroyed   (GwyGraph *graph,
                                                 GwyGraphModel *gmodel);
static void   gwy_graph_model_save_graph        (GwyGraphModel *gmodel,
                                                 GwyGraph *graph);


static GObjectClass *parent_class = NULL;


GType
gwy_graph_model_get_type(void)
{
    static GType gwy_graph_model_type = 0;

    if (!gwy_graph_model_type) {
        static const GTypeInfo gwy_graph_model_info = {
            sizeof(GwyGraphModelClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_graph_model_class_init,
            NULL,
            NULL,
            sizeof(GwyGraphModel),
            0,
            (GInstanceInitFunc)gwy_graph_model_init,
            NULL,
        };

        GInterfaceInfo gwy_serializable_info = {
            (GInterfaceInitFunc)gwy_graph_model_serializable_init, NULL, 0
        };
        GInterfaceInfo gwy_watchable_info = {
            (GInterfaceInitFunc)gwy_graph_model_watchable_init, NULL, 0
        };

        gwy_debug("");
        gwy_graph_model_type
          = g_type_register_static(G_TYPE_OBJECT,
                                   GWY_GRAPH_MODEL_TYPE_NAME,
                                   &gwy_graph_model_info,
                                   0);
        g_type_add_interface_static(gwy_graph_model_type,
                                    GWY_TYPE_SERIALIZABLE,
                                    &gwy_serializable_info);
        g_type_add_interface_static(gwy_graph_model_type,
                                    GWY_TYPE_WATCHABLE,
                                    &gwy_watchable_info);
    }

    return gwy_graph_model_type;
}

static void
gwy_graph_model_serializable_init(GwySerializableIface *iface)
{
    gwy_debug("");
    /* initialize stuff */
    iface->serialize = gwy_graph_model_serialize;
    iface->deserialize = gwy_graph_model_deserialize;
    iface->duplicate = gwy_graph_model_duplicate;
}

static void
gwy_graph_model_watchable_init(GwyWatchableIface *iface)
{
    gwy_debug("");
    /* initialize stuff */
    iface->value_changed = NULL;
}

static void
gwy_graph_model_class_init(GwyGraphModelClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gwy_debug("");

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_graph_model_finalize;
}

static void
gwy_graph_model_init(GwyGraphModel *gmodel)
{
    gwy_debug("");
    gwy_debug_objects_creation((GObject*)gmodel);

    gmodel->graph = NULL;
    gmodel->graph_destroy_hid = 0;

    gmodel->ncurves = 0;
    gmodel->nautocurves = 0;
    gmodel->curves = NULL;

    gmodel->x_reqmin = 0.0;
    gmodel->x_reqmax = 0.0;
    gmodel->y_reqmin = 0.0;
    gmodel->y_reqmax = 0.0;

    gmodel->has_x_unit = FALSE;
    gmodel->has_y_unit = FALSE;
    gmodel->x_unit = gwy_si_unit_new("");
    gmodel->y_unit = gwy_si_unit_new("");

    /* XXX: GwyGraph has no such thing */
    gmodel->title = g_string_new("Fix bug #23!");
    gmodel->top_label = g_string_new("");
    gmodel->bottom_label = g_string_new("");
    gmodel->left_label = g_string_new("");
    gmodel->right_label = g_string_new("");

    gmodel->label_position = GWY_GRAPH_LABEL_NORTHEAST;
    gmodel->label_has_frame = 1;
    gmodel->label_frame_thickness = 1;
}

/**
 * gwy_graph_model_new:
 * @graph: A graph to represent.
 *
 * Creates a new graph model.
 *
 * Returns: New graph model as a #GObject.
 **/
GObject*
gwy_graph_model_new(GwyGraph *graph)
{
    GwyGraphModel *gmodel;

    gwy_debug("");
    gmodel = g_object_new(GWY_TYPE_GRAPH_MODEL, NULL);

    gmodel->graph = graph;
    if (graph) {
        g_assert(GWY_IS_GRAPH(graph));
        gmodel->graph_destroy_hid
            = g_signal_connect(graph, "destroy",
                               G_CALLBACK(gwy_graph_model_graph_destroyed),
                               gmodel);
    }

    return (GObject*)(gmodel);
}

gint
gwy_graph_model_get_n_curves(GwyGraphModel *gmodel)
{
    g_return_val_if_fail(GWY_IS_GRAPH_MODEL(gmodel), 0);

    if (gmodel->graph)
        return gwy_graph_get_number_of_curves(gmodel->graph);
    else
        return gmodel->ncurves;
}

static void
gwy_graph_model_finalize(GObject *object)
{
    GwyGraphModel *gmodel;
    gint i;

    gwy_debug("");

    gmodel = GWY_GRAPH_MODEL(object);
    if (gmodel->graph_destroy_hid) {
        g_assert(GWY_IS_GRAPH(gmodel->graph));
        g_signal_handler_disconnect(gmodel->graph,
                                    gmodel->graph_destroy_hid);
    }

    g_object_unref(gmodel->x_unit);
    g_object_unref(gmodel->y_unit);

    g_string_free(gmodel->title, TRUE);
    g_string_free(gmodel->top_label, TRUE);
    g_string_free(gmodel->bottom_label, TRUE);
    g_string_free(gmodel->left_label, TRUE);
    g_string_free(gmodel->right_label, TRUE);

    for (i = 0; i < gmodel->ncurves; i++)
        g_object_unref(gmodel->curves[i]);
    g_free(gmodel->curves);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void
gwy_graph_model_graph_destroyed(GwyGraph *graph,
                                  GwyGraphModel *gmodel)
{
    gwy_debug("");
    gwy_graph_model_save_graph(gmodel, graph);
    g_signal_handler_disconnect(gmodel->graph, gmodel->graph_destroy_hid);
    gmodel->graph_destroy_hid = 0;
    gmodel->graph = NULL;
}

/* actually copy save from a -- usually just dying -- graph */
static void
gwy_graph_model_save_graph(GwyGraphModel *gmodel,
                           GwyGraph *graph)
{
    gint i, nacurves;
    GwyGraphCurveModel *gcmodel;

    gwy_debug("");
    g_assert(graph && graph == gmodel->graph);

    /* FIXME: we access object fields directly now as we are supposed to know
     * some their internals anyway. */
    /* graph */
    if ((gmodel->has_x_unit = graph->has_x_unit))
        gwy_si_unit_set_unit_string(GWY_SI_UNIT(gmodel->x_unit),
                                    graph->x_unit);
    else
        gwy_object_unref(graph->x_unit);

    if ((gmodel->has_y_unit = graph->has_y_unit))
        gwy_si_unit_set_unit_string(GWY_SI_UNIT(gmodel->y_unit),
                                    graph->y_unit);
    else
        gwy_object_unref(graph->y_unit);

    gmodel->x_reqmin = graph->x_reqmin;
    gmodel->y_reqmin = graph->y_reqmin;
    gmodel->x_reqmax = graph->x_reqmax;
    gmodel->y_reqmax = graph->y_reqmax;

    /* axes */
    g_string_assign(gmodel->top_label,
                    gwy_axis_get_label(graph->axis_top)->str);
    g_string_assign(gmodel->bottom_label,
                    gwy_axis_get_label(graph->axis_bottom)->str);
    g_string_assign(gmodel->left_label,
                    gwy_axis_get_label(graph->axis_left)->str);
    g_string_assign(gmodel->right_label,
                    gwy_axis_get_label(graph->axis_right)->str);

    /* label */
    gmodel->label_position = graph->area->lab->par.position;
    gmodel->label_has_frame = graph->area->lab->par.is_frame;
    gmodel->label_frame_thickness = graph->area->lab->par.frame_thickness;

    /* curves */
    /* somewhat hairy; trying to avoid redundant reallocations:
     * 1. clear extra curves that model has and graph has not
     * 2. realloc curves to the right size
     * 3. replace already existing curves  <-- if lucky, only this happens
     * 4. fill new curves
     */
    gmodel->nautocurves = graph->n_of_autocurves;
    nacurves = graph->area->curves->len;
    /* 1. clear */
    for (i = nacurves; i < gmodel->ncurves; i++)
        gwy_object_unref(gmodel->curves[i]);
    /* 2. realloc */
    gmodel->curves = g_renew(GObject*, gmodel->curves, nacurves);
    /* 3. replace */
    for (i = 0; i < gmodel->ncurves; i++) {
        gcmodel = GWY_GRAPH_CURVE_MODEL(gmodel->curves[i]);
        gwy_graph_curve_model_save_curve(gcmodel, graph, i);
    }
    /* 4. fill */
    for (i = gmodel->ncurves; i < nacurves; i++) {
        gmodel->curves[i] = gwy_graph_curve_model_new();
        gcmodel = GWY_GRAPH_CURVE_MODEL(gmodel->curves[i]);
        gwy_graph_curve_model_save_curve(gcmodel, graph, i);
    }
    gmodel->ncurves = nacurves;
}

GtkWidget*
gwy_graph_new_from_model(GwyGraphModel *gmodel)
{
    GtkWidget *graph_widget;
    GwyGraphCurveModel *gcmodel;
    gchar *BRAINDEAD_SI_UNIT_CANT_RETURN_CONSTANT_STRINGS;
    GwyGraph *graph;
    gint i;

    g_return_val_if_fail(gmodel->graph == NULL, gwy_graph_new());

    graph_widget = gwy_graph_new();
    graph = GWY_GRAPH(graph_widget);

    gmodel->graph = graph;
    gmodel->graph_destroy_hid
        = g_signal_connect(graph, "destroy",
                           G_CALLBACK(gwy_graph_model_graph_destroyed), gmodel);

    graph->area->lab->par.position = gmodel->label_position;
    graph->area->lab->par.is_frame = gmodel->label_has_frame;
    graph->area->lab->par.frame_thickness = gmodel->label_frame_thickness;

    graph->n_of_autocurves = gmodel->nautocurves;

    for (i = 0; i < gmodel->ncurves; i++) {
        gcmodel = GWY_GRAPH_CURVE_MODEL(gmodel->curves[i]);
        gwy_graph_add_curve_from_model(graph, gcmodel);
    }

    gwy_axis_set_label(graph->axis_top, gmodel->top_label);
    gwy_axis_set_label(graph->axis_bottom, gmodel->bottom_label);
    gwy_axis_set_label(graph->axis_left, gmodel->left_label);
    gwy_axis_set_label(graph->axis_right, gmodel->right_label);
    if (gmodel->has_x_unit) {
        BRAINDEAD_SI_UNIT_CANT_RETURN_CONSTANT_STRINGS
            = gwy_si_unit_get_unit_string(GWY_SI_UNIT(gmodel->x_unit));
        gwy_axis_set_unit(graph->axis_top,
                          BRAINDEAD_SI_UNIT_CANT_RETURN_CONSTANT_STRINGS);
        BRAINDEAD_SI_UNIT_CANT_RETURN_CONSTANT_STRINGS
            = g_strdup(BRAINDEAD_SI_UNIT_CANT_RETURN_CONSTANT_STRINGS);
        gwy_axis_set_unit(graph->axis_bottom,
                          BRAINDEAD_SI_UNIT_CANT_RETURN_CONSTANT_STRINGS);
    }
    if (gmodel->has_y_unit) {
        BRAINDEAD_SI_UNIT_CANT_RETURN_CONSTANT_STRINGS
            = gwy_si_unit_get_unit_string(GWY_SI_UNIT(gmodel->y_unit));
        gwy_axis_set_unit(graph->axis_left,
                          BRAINDEAD_SI_UNIT_CANT_RETURN_CONSTANT_STRINGS);
        BRAINDEAD_SI_UNIT_CANT_RETURN_CONSTANT_STRINGS
            = g_strdup(BRAINDEAD_SI_UNIT_CANT_RETURN_CONSTANT_STRINGS);
        gwy_axis_set_unit(graph->axis_right,
                          BRAINDEAD_SI_UNIT_CANT_RETURN_CONSTANT_STRINGS);
    }

    gwy_graph_set_boundaries(graph,
                             gmodel->x_reqmin, gmodel->x_reqmax,
                             gmodel->y_reqmin, gmodel->y_reqmax);

    return graph_widget;
}

static GByteArray*
gwy_graph_model_serialize(GObject *obj,
                            GByteArray*buffer)
{
    GwyGraphModel *gmodel;
    gsize before_obj;
    gint i;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_GRAPH_MODEL(obj), NULL);

    buffer = gwy_serialize_pack(buffer, "si", GWY_GRAPH_MODEL_TYPE_NAME, 0);
    before_obj = buffer->len;

    gmodel = GWY_GRAPH_MODEL(obj);
    if (gmodel->graph)
        gwy_graph_model_save_graph(gmodel, gmodel->graph);
    /* Global data, serialized as a fake subobject GwyGraphModel-graph */
    {
        GwySerializeSpec spec[] = {
            { 'b', "has_x_unit", &gmodel->has_x_unit, NULL },
            { 'b', "has_y_unit", &gmodel->has_y_unit, NULL },
            { 'o', "x_unit", &gmodel->x_unit, NULL },
            { 'o', "y_unit", &gmodel->y_unit, NULL },
            { 's', "title", &gmodel->title->str, NULL },
            { 's', "top_label", &gmodel->top_label->str, NULL },
            { 's', "bottom_label", &gmodel->bottom_label->str, NULL },
            { 's', "left_label", &gmodel->left_label->str, NULL },
            { 's', "right_label", &gmodel->right_label->str, NULL },
            { 'd', "x_reqmin", &gmodel->x_reqmin, NULL },
            { 'd', "y_reqmin", &gmodel->y_reqmin, NULL },
            { 'd', "x_reqmax", &gmodel->x_reqmax, NULL },
            { 'd', "y_reqmax", &gmodel->y_reqmax, NULL },
            { 'i', "label.position", &gmodel->label_position, NULL },
            { 'b', "label.has_frame", &gmodel->label_has_frame, NULL },
            { 'i', "label.frame_thickness", &gmodel->label_frame_thickness,
                NULL },
            { 'i', "ncurves", &gmodel->ncurves, NULL },
            { 'i', "nautocurves", &gmodel->nautocurves, NULL },
        };

        gwy_serialize_pack_object_struct(buffer,
                                         GRAPH_GLOBALS_FAKE_TYPE_NAME,
                                         G_N_ELEMENTS(spec), spec);
    }
    /* Curves */
    for (i = 0; i < gmodel->ncurves; i++)
        gwy_serializable_serialize(G_OBJECT(gmodel->curves[i]), buffer);

    gwy_serialize_store_int32(buffer, before_obj - sizeof(guint32),
                              buffer->len - before_obj);
    return buffer;
}

static GObject*
gwy_graph_model_deserialize(const guchar *buffer,
                            gsize size,
                            gsize *position)
{
    GwyGraphModel *gmodel;
    gsize mysize, pos;
    gint i;

    mysize = gwy_serialize_check_string(buffer, size, *position,
                                        GWY_GRAPH_MODEL_TYPE_NAME);
    g_return_val_if_fail(mysize, NULL);
    *position += mysize;
    mysize = gwy_serialize_unpack_int32(buffer, size, position);

    /* Unpack the fake "GwyGraphModel-data" subobject */
    pos = 0;
    gmodel = (GwyGraphModel*)gwy_graph_model_new(NULL);
    {
        gchar *top_label, *bottom_label, *left_label, *right_label, *title;
        GwySerializeSpec spec[] = {
            { 'b', "has_x_unit", &gmodel->has_x_unit, NULL },
            { 'b', "has_y_unit", &gmodel->has_y_unit, NULL },
            { 'o', "x_unit", &gmodel->x_unit, NULL },
            { 'o', "y_unit", &gmodel->y_unit, NULL },
            { 's', "title", &title, NULL },
            { 's', "top_label", &top_label, NULL },
            { 's', "bottom_label", &bottom_label, NULL },
            { 's', "left_label", &left_label, NULL },
            { 's', "right_label", &right_label, NULL },
            { 'd', "x_reqmin", &gmodel->x_reqmin, NULL },
            { 'd', "y_reqmin", &gmodel->y_reqmin, NULL },
            { 'd', "x_reqmax", &gmodel->x_reqmax, NULL },
            { 'd', "y_reqmax", &gmodel->y_reqmax, NULL },
            { 'i', "label.position", &gmodel->label_position, NULL },
            { 'b', "label.has_frame", &gmodel->label_has_frame, NULL },
            { 'i', "label.frame_thickness", &gmodel->label_frame_thickness,
                NULL },
            { 'i', "ncurves", &gmodel->ncurves, NULL },
            { 'i', "nautocurves", &gmodel->nautocurves, NULL },
        };

        top_label = bottom_label = left_label = right_label = title = NULL;
        if (!gwy_serialize_unpack_object_struct(buffer + *position,
                                                mysize, &pos,
                                                GRAPH_GLOBALS_FAKE_TYPE_NAME,
                                                G_N_ELEMENTS(spec), spec)) {
            g_free(top_label);
            g_free(bottom_label);
            g_free(left_label);
            g_free(right_label);
            g_free(title);
            g_object_unref(gmodel);
            return NULL;
        }

        if (title) {
            g_string_assign(gmodel->title, title);
            g_free(title);
        }
        if (top_label) {
            g_string_assign(gmodel->top_label, top_label);
            g_free(top_label);
        }
        if (bottom_label) {
            g_string_assign(gmodel->bottom_label, bottom_label);
            g_free(bottom_label);
        }
        if (left_label) {
            g_string_assign(gmodel->left_label, left_label);
            g_free(left_label);
        }
        if (right_label) {
            g_string_assign(gmodel->right_label, right_label);
            g_free(right_label);
        }
    }

    /* Then unpack curves, they are real objects. */
    gmodel->curves = g_new(GObject*, gmodel->ncurves);
    for (i = 0; i < gmodel->ncurves; i++) {
        gmodel->curves[i] = gwy_serializable_deserialize(buffer + *position,
                                                         mysize, &pos);
        if (!gmodel->curves[i]) {
            gmodel->ncurves = i;    /* free only existing curves */
            g_object_unref(gmodel);
            return NULL;
        }
    }

    *position += mysize;
    return (GObject*)gmodel;
}

static GObject*
gwy_graph_model_duplicate(GObject *object)
{
    GwyGraphModel *gmodel, *duplicate;
    gint i;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_GRAPH_MODEL(object), NULL);

    gmodel = GWY_GRAPH_MODEL(object);
    if (gmodel->graph)
        return gwy_graph_model_new(gmodel->graph);

    duplicate = (GwyGraphModel*)gwy_graph_model_new(NULL);
    /* widget stuff is already initialized to NULL */
    duplicate->title = g_string_new(gmodel->title->str);;
    duplicate->has_x_unit = gmodel->has_x_unit;
    duplicate->has_y_unit = gmodel->has_y_unit;
    duplicate->x_reqmin = gmodel->x_reqmin;
    duplicate->y_reqmin = gmodel->y_reqmin;
    duplicate->x_reqmax = gmodel->x_reqmax;
    duplicate->y_reqmax = gmodel->y_reqmax;
    duplicate->label_position = gmodel->label_position;
    duplicate->label_has_frame = gmodel->label_has_frame;
    duplicate->label_frame_thickness = gmodel->label_frame_thickness;
    duplicate->x_unit = gwy_serializable_duplicate(gmodel->x_unit);
    duplicate->y_unit = gwy_serializable_duplicate(gmodel->y_unit);
    duplicate->top_label = g_string_new(gmodel->top_label->str);
    duplicate->bottom_label = g_string_new(gmodel->bottom_label->str);
    duplicate->left_label = g_string_new(gmodel->left_label->str);
    duplicate->right_label = g_string_new(gmodel->right_label->str);
    duplicate->ncurves = gmodel->ncurves;
    duplicate->nautocurves = gmodel->nautocurves;
    duplicate->curves = g_new(GObject*, gmodel->ncurves);
    for (i = 0; i < gmodel->ncurves; i++)
        duplicate->curves[i] = gwy_serializable_duplicate(gmodel->curves[i]);

    return (GObject*)duplicate;
}

#endif  /* I_WANT_A_BROKEN_GWY_GRAPH_MODEL */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
