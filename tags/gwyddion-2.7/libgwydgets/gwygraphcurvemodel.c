/*
 *  @(#) $Id$
 *  Copyright (C) 2005-2006 David Necas (Yeti), Petr Klapetek.
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
#include <gtk/gtk.h>
#include <libdraw/gwyrgba.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwydebugobjects.h>
#include <libprocess/dataline.h>
#include <libgwydgets/gwygraphcurvemodel.h>
#include <libgwydgets/gwydgettypes.h>

#define GWY_GRAPH_CURVE_MODEL_TYPE_NAME "GwyGraphCurveModel"

/* Cache operations */
#define CVAL(cmodel, b)  ((cmodel)->cache[GWY_GRAPH_CURVE_MODEL_CACHE_##b])
#define CBIT(b)          (1 << GWY_GRAPH_CURVE_MODEL_CACHE_##b)
#define CTEST(cmodel, b) ((cmodel)->cached & CBIT(b))

typedef enum {
    GWY_GRAPH_CURVE_MODEL_CACHE_XMIN = 0,
    GWY_GRAPH_CURVE_MODEL_CACHE_XMAX,
    GWY_GRAPH_CURVE_MODEL_CACHE_YMIN,
    GWY_GRAPH_CURVE_MODEL_CACHE_YMAX
} GwyGraphCurveModelCached;

static void     gwy_graph_curve_model_finalize         (GObject *object);
static void     gwy_graph_curve_model_serializable_init(GwySerializableIface *iface);
static GByteArray* gwy_graph_curve_model_serialize     (GObject *object,
                                                        GByteArray*buffer);
static gsize    gwy_graph_curve_model_get_size         (GObject *object);
static GObject* gwy_graph_curve_model_deserialize      (const guchar *buffer,
                                                        gsize size,
                                                        gsize *position);
static GObject* gwy_graph_curve_model_duplicate_real   (GObject *object);
static void     gwy_graph_curve_model_set_property     (GObject *object,
                                                        guint prop_id,
                                                        const GValue *value,
                                                        GParamSpec *pspec);
static void     gwy_graph_curve_model_get_property     (GObject*object,
                                                        guint prop_id,
                                                        GValue *value,
                                                        GParamSpec *pspec);
static void   gwy_graph_curve_model_data_changed  (GwyGraphCurveModel *gcmodel);

enum {
    DATA_CHANGED,
    LAST_SIGNAL
};

enum {
    PROP_0,
    PROP_DESCRIPTION,
    PROP_MODE,
    PROP_POINT_TYPE,
    PROP_POINT_SIZE,
    PROP_LINE_STYLE,
    PROP_LINE_WIDTH,
    PROP_COLOR,
    PROP_LAST
};

static guint graph_curve_model_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_EXTENDED
    (GwyGraphCurveModel, gwy_graph_curve_model, G_TYPE_OBJECT, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_graph_curve_model_serializable_init))

static void
gwy_graph_curve_model_serializable_init(GwySerializableIface *iface)
{
    iface->serialize = gwy_graph_curve_model_serialize;
    iface->deserialize = gwy_graph_curve_model_deserialize;
    iface->get_size = gwy_graph_curve_model_get_size;
    iface->duplicate = gwy_graph_curve_model_duplicate_real;
}

static void
gwy_graph_curve_model_class_init(GwyGraphCurveModelClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gwy_debug("");

    gobject_class->finalize = gwy_graph_curve_model_finalize;
    gobject_class->set_property = gwy_graph_curve_model_set_property;
    gobject_class->get_property = gwy_graph_curve_model_get_property;

    /**
     * GwyGraphCurveModel::data-changed:
     * @gwygraphcurvemodel: The #GwyGraphCurveModel which received the signal.
     *
     * The ::data-changed signal is emitted whenever curve data is set with
     * a function like gwy_graph_curve_model_set_data().
     **/
    graph_curve_model_signals[DATA_CHANGED]
        = g_signal_new("data-changed",
                       G_OBJECT_CLASS_TYPE(gobject_class),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET(GwyGraphCurveModelClass, data_changed),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);

    g_object_class_install_property
        (gobject_class,
         PROP_DESCRIPTION,
         g_param_spec_string("description",
                             "Curve description",
                             "Curve description.  It appears on graph key.",
                             "curve",
                             G_PARAM_READABLE | G_PARAM_WRITABLE));

    g_object_class_install_property
        (gobject_class,
         PROP_MODE,
         g_param_spec_enum("mode",
                           "Mode",
                           "Curve plotting mode (line, points, ...)",
                           GWY_TYPE_GRAPH_CURVE_TYPE,
                           GWY_GRAPH_CURVE_LINE,
                           G_PARAM_READABLE | G_PARAM_WRITABLE));

     g_object_class_install_property
         (gobject_class,
          PROP_POINT_TYPE,
          g_param_spec_enum("point-type",
                            "Point type",
                            "Curve point symbol type.  Curve mode has to"
                            "include points for the symbols to be visible.",
                            GWY_TYPE_GRAPH_POINT_TYPE,
                            GWY_GRAPH_POINT_SQUARE,
                            G_PARAM_READABLE | G_PARAM_WRITABLE));

     g_object_class_install_property
         (gobject_class,
          PROP_POINT_SIZE,
          g_param_spec_int("point-size",
                           "Point size",
                           "Curve point symbol size",
                           0, 100,
                           5,
                           G_PARAM_READABLE | G_PARAM_WRITABLE));

     g_object_class_install_property
         (gobject_class,
          PROP_LINE_STYLE,
          g_param_spec_enum("line-style",
                            "Line style",
                            "Curve line style.  Curve mode has to include "
                            "lines for the line to be visible.",
                            GDK_TYPE_LINE_STYLE,
                            GDK_LINE_SOLID,
                            G_PARAM_READABLE | G_PARAM_WRITABLE));

     g_object_class_install_property
         (gobject_class,
          PROP_LINE_WIDTH,
          g_param_spec_int("line-width",
                           "Line width",
                           "Curve line width.",
                           0, 100,
                           1,
                           G_PARAM_READABLE | G_PARAM_WRITABLE));

     g_object_class_install_property
         (gobject_class,
          PROP_COLOR,
          g_param_spec_boxed("color",
                             "Color",
                             "Curve color",
                             GWY_TYPE_RGBA,
                             G_PARAM_READABLE | G_PARAM_WRITABLE));
}

static void
gwy_graph_curve_model_set_property(GObject *object,
                                   guint prop_id,
                                   const GValue *value,
                                   GParamSpec *pspec)
{
    GwyGraphCurveModel *gcmodel = GWY_GRAPH_CURVE_MODEL(object);
    GwyRGBA *color;

    switch (prop_id) {
        case PROP_DESCRIPTION:
        g_string_assign(gcmodel->description, g_value_get_string(value));
        break;

        case PROP_MODE:
        gcmodel->mode = g_value_get_enum(value);
        break;

        case PROP_POINT_TYPE:
        gcmodel->point_type = g_value_get_enum(value);
        break;

        case PROP_LINE_STYLE:
        gcmodel->line_style = g_value_get_enum(value);
        break;

        case PROP_LINE_WIDTH:
        gcmodel->line_width = g_value_get_int(value);
        break;

        case PROP_POINT_SIZE:
        gcmodel->point_size = g_value_get_int(value);
        break;

        case PROP_COLOR:
        color = g_value_get_boxed(value);
        gcmodel->color = *color;
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
gwy_graph_curve_model_get_property(GObject*object,
                                   guint prop_id,
                                   GValue *value,
                                   GParamSpec *pspec)
{
    GwyGraphCurveModel *gcmodel = GWY_GRAPH_CURVE_MODEL(object);

    switch (prop_id) {
        case PROP_DESCRIPTION:
        g_value_set_string(value, gcmodel->description->str);
        break;

        case PROP_MODE:
        g_value_set_enum(value, gcmodel->mode);
        break;

        case PROP_POINT_TYPE:
        g_value_set_enum(value, gcmodel->point_type);
        break;

        case PROP_LINE_STYLE:
        g_value_set_enum(value, gcmodel->line_style);
        break;

        case PROP_LINE_WIDTH:
        g_value_set_int(value, gcmodel->line_width);
        break;

        case PROP_POINT_SIZE:
        g_value_set_int(value, gcmodel->point_size);
        break;

        case PROP_COLOR:
        g_value_set_boxed(value, &gcmodel->color);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}


static void
gwy_graph_curve_model_init(GwyGraphCurveModel *gcmodel)
{
    gwy_debug("");
    gwy_debug_objects_creation((GObject*)gcmodel);

    gcmodel->description = g_string_new("");
    gcmodel->color.r = 0;
    gcmodel->color.g = 0;
    gcmodel->color.b = 0;
    gcmodel->color.a = 1;

    gcmodel->mode = GWY_GRAPH_CURVE_LINE_POINTS;

    gcmodel->point_type = GWY_GRAPH_POINT_SQUARE;
    gcmodel->point_size = 8;

    gcmodel->line_style = GDK_LINE_SOLID;
    gcmodel->line_width = 1;
}

/**
 * gwy_graph_curve_model_new:
 *
 * Creates a new graph curve model.
 *
 * Returns: New empty graph curve model as a #GObject.
 **/
GwyGraphCurveModel*
gwy_graph_curve_model_new(void)
{
    GwyGraphCurveModel *gcmodel;

    gwy_debug("");
    gcmodel = (GwyGraphCurveModel*)g_object_new(GWY_TYPE_GRAPH_CURVE_MODEL,
                                                NULL);

    return (gcmodel);
}

/**
 * gwy_graph_model_new_alike:
 * @gmodel: A graph model.
 *
 * Creates new graph model object that has the same settings as @gmodel.
 *
 * This includes axis/label visibility, actual plotting range, etc.
 * Curves are not duplicated or referenced.
 *
 * Returns: New graph model.
 **/
GwyGraphCurveModel*
gwy_graph_curve_model_new_alike(GwyGraphCurveModel *gcmodel)
{
    GwyGraphCurveModel *duplicate;

    gwy_debug("");

    duplicate = gwy_graph_curve_model_new();
    duplicate->description = g_string_new(gcmodel->description->str); 
    duplicate->color = gcmodel->color;
    duplicate->mode = gcmodel->mode;
    duplicate->point_type = gcmodel->point_type;
    duplicate->point_size = gcmodel->point_size;
    duplicate->line_style = gcmodel->line_style;
    duplicate->line_width = gcmodel->line_width;
    
    return duplicate;
}

static void
gwy_graph_curve_model_finalize(GObject *object)
{
    GwyGraphCurveModel *gcmodel;

    gwy_debug("");
    gcmodel = GWY_GRAPH_CURVE_MODEL(object);

    g_string_free(gcmodel->description, TRUE);
    g_free(gcmodel->xdata);
    g_free(gcmodel->ydata);
    G_OBJECT_CLASS(gwy_graph_curve_model_parent_class)->finalize(object);
}

static GByteArray*
gwy_graph_curve_model_serialize(GObject *object,
                                GByteArray *buffer)
{
    GwyGraphCurveModel *gcmodel;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_GRAPH_CURVE_MODEL(object), NULL);

    gcmodel = GWY_GRAPH_CURVE_MODEL(object);
    {
        GwySerializeSpec spec[] = {
            { 'D', "xdata", &gcmodel->xdata, &gcmodel->n },
            { 'D', "ydata", &gcmodel->ydata, &gcmodel->n },
            { 's', "description", &gcmodel->description->str, NULL },
            { 'd', "color.red", &gcmodel->color.r, NULL },
            { 'd', "color.green", &gcmodel->color.g, NULL },
            { 'd', "color.blue", &gcmodel->color.b, NULL },
            /* XXX: Legacy */
            { 'i', "type", &gcmodel->mode, NULL },
            { 'i', "point_type", &gcmodel->point_type, NULL },
            { 'i', "point_size", &gcmodel->point_size, NULL },
            { 'i', "line_style", &gcmodel->line_style, NULL },
            /* XXX: Legacy */
            { 'i', "line_size", &gcmodel->line_width, NULL },
        };

        return gwy_serialize_pack_object_struct(buffer,
                                                GWY_GRAPH_CURVE_MODEL_TYPE_NAME,
                                                G_N_ELEMENTS(spec), spec);
    }
}

static gsize
gwy_graph_curve_model_get_size(GObject *object)
{
    GwyGraphCurveModel *gcmodel;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_GRAPH_CURVE_MODEL(object), 0);

    gcmodel = GWY_GRAPH_CURVE_MODEL(object);
    {
        GwySerializeSpec spec[] = {
            { 'D', "xdata", &gcmodel->xdata, &gcmodel->n },
            { 'D', "ydata", &gcmodel->ydata, &gcmodel->n },
            { 's', "description", &gcmodel->description->str, NULL },
            { 'd', "color.red", &gcmodel->color.r, NULL },
            { 'd', "color.green", &gcmodel->color.g, NULL },
            { 'd', "color.blue", &gcmodel->color.b, NULL },
            /* XXX: Legacy */
            { 'i', "type", &gcmodel->mode, NULL },
            { 'i', "point_type", &gcmodel->point_type, NULL },
            { 'i', "point_size", &gcmodel->point_size, NULL },
            { 'i', "line_style", &gcmodel->line_style, NULL },
            /* XXX: Legacy */
            { 'i', "line_size", &gcmodel->line_width, NULL },
        };

        return gwy_serialize_get_struct_size(GWY_GRAPH_CURVE_MODEL_TYPE_NAME,
                                             G_N_ELEMENTS(spec), spec);
    }
}

static GObject*
gwy_graph_curve_model_deserialize(const guchar *buffer,
                                  gsize size,
                                  gsize *position)
{
    GwyGraphCurveModel *gcmodel;

    gwy_debug("");
    g_return_val_if_fail(buffer, NULL);

    gcmodel = gwy_graph_curve_model_new();
    {
        gint nxdata, nydata;
        gchar *description = NULL;
        GwySerializeSpec spec[] = {
            { 'D', "xdata", &gcmodel->xdata, &nxdata },
            { 'D', "ydata", &gcmodel->ydata, &nydata },
            { 's', "description", &description, NULL },
            { 'd', "color.red", &gcmodel->color.r, NULL },
            { 'd', "color.green", &gcmodel->color.g, NULL },
            { 'd', "color.blue", &gcmodel->color.b, NULL },
            /* XXX: Legacy */
            { 'i', "type", &gcmodel->mode, NULL },
            /* Accept mode too */
            { 'i', "mode", &gcmodel->mode, NULL },
            { 'i', "point_type", &gcmodel->point_type, NULL },
            { 'i', "point_size", &gcmodel->point_size, NULL },
            { 'i', "line_style", &gcmodel->line_style, NULL },
            /* XXX: Legacy */
            { 'i', "line_size", &gcmodel->line_width, NULL },
            /* Accept line_width too */
            { 'i', "line_width", &gcmodel->line_width, NULL },
        };
        if (!gwy_serialize_unpack_object_struct(buffer, size, position,
                                                GWY_GRAPH_CURVE_MODEL_TYPE_NAME,
                                                G_N_ELEMENTS(spec), spec)) {
            g_free(description);
            g_object_unref(gcmodel);
            return NULL;
        }
        if (nxdata != nydata) {
            g_critical("Serialized xdata and ydata array sizes differ");
            g_free(description);
            g_object_unref(gcmodel);
            return NULL;
        }
        if (description) {
            g_string_assign(gcmodel->description, description);
            g_free(description);
        }
        gcmodel->n = nxdata;
    }

    return (GObject*)gcmodel;
}

static GObject*
gwy_graph_curve_model_duplicate_real(GObject *object)
{
    GwyGraphCurveModel *gcmodel, *duplicate;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_GRAPH_CURVE_MODEL(object), NULL);

    gcmodel = GWY_GRAPH_CURVE_MODEL(object);
    duplicate = gwy_graph_curve_model_new_alike(gcmodel);

    if ((duplicate->n = gcmodel->n)) {
        duplicate->xdata = g_memdup(gcmodel->xdata, gcmodel->n*sizeof(gdouble));
        duplicate->ydata = g_memdup(gcmodel->ydata, gcmodel->n*sizeof(gdouble));
    }

    return (GObject*)duplicate;
}

/**
 * gwy_graph_curve_model_set_data:
 * @gcmodel: A graph curve model.
 * @xdata: X data points (array of size @n).
 * @ydata: Y data points (array of size @n).
 * @n: Number of points, i.e. items in @xdata and @ydata.
 *
 * Sets curve model data.
 **/
void
gwy_graph_curve_model_set_data(GwyGraphCurveModel *gcmodel,
                               const gdouble *xdata,
                               const gdouble *ydata,
                               gint n)
{
    if (gcmodel->n == n) {
        memcpy(gcmodel->xdata, xdata, n*sizeof(gdouble));
        memcpy(gcmodel->ydata, ydata, n*sizeof(gdouble));
    }
    else {
        gdouble *old;

        old = gcmodel->xdata;
        gcmodel->xdata = g_memdup(xdata, n*sizeof(gdouble));
        g_free(old);

        old = gcmodel->ydata;
        gcmodel->ydata = g_memdup(ydata, n*sizeof(gdouble));
        g_free(old);

        gcmodel->n = n;
    }
    gwy_graph_curve_model_data_changed(gcmodel);
}

/**
 * gwy_graph_curve_model_get_xdata:
 * @gcmodel: A graph curve model.
 *
 * Gets pointer to x data points.
 *
 * Data are used within the graph and cannot be freed.
 *
 * Returns: X data points, owned by the curve model.
 **/
const gdouble*
gwy_graph_curve_model_get_xdata(GwyGraphCurveModel *gcmodel)
{
    return gcmodel->xdata;
}

/**
 * gwy_graph_curve_model_get_ydata:
 * @gcmodel: A graph curve model.
 *
 * Gets pointer to y data points.
 *
 * Data are used within the graph and cannot be freed.
 *
 * Returns: Y data points, owned by the curve model.
 **/
const gdouble*
gwy_graph_curve_model_get_ydata(GwyGraphCurveModel *gcmodel)
{
    return gcmodel->ydata;
}

/**
 * gwy_graph_curve_model_get_ndata:
 * @gcmodel: A graph curve model.
 *
 * Returns: number of data points within the curve data
 **/
/* XXX: Malformed documentation. */
gint
gwy_graph_curve_model_get_ndata(GwyGraphCurveModel *gcmodel)
{
    return gcmodel->n;
}

/**
 * gwy_graph_curve_model_set_data_from_dataline:
 * @gcmodel: A graph curve model.
 * @dline: A #GwyDataLine
 * @from_index: index where to start
 * @to_index: where to stop
 *
 * Sets the curve data from #GwyDataLine. The range of import can be
 * modified using parameters @from_index and @to_index that are
 * interpreted directly as data indices within the #GwyDataLine.
 * In the case that @from_index == @to_index, the full #GwyDataLine is used.
 **/
/* XXX: Malformed documentation. */
void
gwy_graph_curve_model_set_data_from_dataline(GwyGraphCurveModel *gcmodel,
                                             GwyDataLine *dline,
                                             gint from_index,
                                             gint to_index)
{
    gdouble *xdata;
    gdouble *ydata;
    const gdouble *ldata;
    gint res, i;
    gdouble realmin, realmax, offset;

    if (from_index == to_index || from_index > to_index) {
        res = gwy_data_line_get_res(dline);
        realmin = 0;
        realmax = gwy_data_line_get_real(dline);
        from_index = 0;
    }
    else {
        res = to_index - from_index;
        realmin = gwy_data_line_itor(dline, from_index);
        realmax = gwy_data_line_itor(dline, to_index);
    }

    xdata = g_new(gdouble, res);
    ydata = g_new(gdouble, res);

    offset = gwy_data_line_get_offset(dline);

    ldata = gwy_data_line_get_data(dline);
    for (i = 0; i < res; i++) {
        xdata[i] = realmin + i*(realmax - realmin)/res + offset;
        ydata[i] = ldata[i + from_index];
    }

    gwy_graph_curve_model_set_data(gcmodel, xdata, ydata, res);

    g_free(xdata);
    g_free(ydata);
    gwy_graph_curve_model_data_changed(gcmodel);
}


/**
 * gwy_graph_curve_model_get_x_range:
 * @gcmodel: A graph curve model.
 * @x_min: Location to store minimum abscissa value, or %NULL.
 * @x_max: Location to store maximum abscissa value, or %NULL.
 *
 * Gets the abscissa range of a graph curve.
 *
 * The values are cached in the curve model therefore repeated calls to this
 * function (with unchanged data) are cheap.
 *
 * If there are no data points in the curve, @x_min and @x_max are untouched
 * and the function returns %FALSE.
 *
 * Returns: %TRUE if there are any data points in the curve and @x_min, @x_max
 *          were set.
 **/
gboolean
gwy_graph_curve_model_get_x_range(GwyGraphCurveModel *gcmodel,
                                  gdouble *x_min,
                                  gdouble *x_max)
{
    gdouble xmin, xmax;
    gboolean must_calculate = FALSE;
    gint i;

    g_return_val_if_fail(GWY_IS_GRAPH_CURVE_MODEL(gcmodel), FALSE);

    if (!gcmodel->n)
        return FALSE;

    if (x_min) {
        if (CTEST(gcmodel, XMIN))
            *x_min = CVAL(gcmodel, XMIN);
        else
            must_calculate = TRUE;
    }

    if (x_max) {
        if (CTEST(gcmodel, XMAX))
            *x_max = CVAL(gcmodel, XMAX);
        else
            must_calculate = TRUE;
    }

    if (!must_calculate)
        return TRUE;

    xmin = xmax = gcmodel->xdata[0];
    for (i = 1; i < gcmodel->n; i++) {
        if (G_UNLIKELY(gcmodel->xdata[i] < xmin))
            xmin = gcmodel->xdata[i];
        if (G_LIKELY(gcmodel->xdata[i] > xmax))
            xmax = gcmodel->xdata[i];
    }

    CVAL(gcmodel, XMIN) = xmin;
    CVAL(gcmodel, XMAX) = xmax;

    if (x_min)
        *x_min = xmin;
    if (x_max)
        *x_max = xmax;

    return TRUE;
}

/**
 * gwy_graph_curve_model_get_y_range:
 * @gcmodel: A graph curve model.
 * @y_min: Location to store minimum ordinate value, or %NULL.
 * @y_max: Location to store maximum ordinate value, or %NULL.
 *
 * Gets the ordinate range of a graph curve.
 *
 * The values are cached in the curve model therefore repeated calls to this
 * function (with unchanged data) are cheap.
 *
 * If there are no data points in the curve, @x_min and @x_max are untouched
 * and the function returns %FALSE.
 *
 * Returns: %TRUE if there are any data points in the curve and @x_min, @x_max
 *          were set.
 **/
gboolean
gwy_graph_curve_model_get_y_range(GwyGraphCurveModel *gcmodel,
                                  gdouble *y_min,
                                  gdouble *y_max)
{
    gdouble ymin, ymax;
    gboolean must_calculate = FALSE;
    gint i;

    g_return_val_if_fail(GWY_IS_GRAPH_CURVE_MODEL(gcmodel), FALSE);
    if (!gcmodel->n)
        return FALSE;

    if (y_min) {
        if (CTEST(gcmodel, YMIN))
            *y_min = CVAL(gcmodel, YMIN);
        else
            must_calculate = TRUE;
    }

    if (y_max) {
        if (CTEST(gcmodel, YMAX))
            *y_max = CVAL(gcmodel, YMAX);
        else
            must_calculate = TRUE;
    }

    if (!must_calculate)
        return TRUE;

    ymin = ymax = gcmodel->ydata[0];
    for (i = 1; i < gcmodel->n; i++) {
        if (gcmodel->ydata[i] < ymin)
            ymin = gcmodel->ydata[i];
        if (gcmodel->ydata[i] > ymax)
            ymax = gcmodel->ydata[i];
    }

    CVAL(gcmodel, YMIN) = ymin;
    CVAL(gcmodel, YMAX) = ymax;

    if (y_min)
        *y_min = ymin;
    if (y_max)
        *y_max = ymax;

    return TRUE;
}

static void
gwy_graph_curve_model_data_changed(GwyGraphCurveModel *gcmodel)
{
    gcmodel->cached = 0;
    g_signal_emit(gcmodel, graph_curve_model_signals[DATA_CHANGED], 0);
}

/************************** Documentation ****************************/

/**
 * SECTION:gwygraphcurvemodel
 * @title: GwyGraphCurveModel
 * @short_description: Representation of one graph curve
 *
 * #GwyGraphCurveModel represents information about a graph curve necessary to
 * fully reconstruct it.
 **/

/**
 * gwy_graph_curve_model_duplicate:
 * @gcmodel: A graph curve model to duplicate.
 *
 * Convenience macro doing gwy_serializable_duplicate() with all the necessary
 * typecasting.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
