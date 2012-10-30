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

#include "libgwy/macros.h"
#include "libgwy/math.h"
#include "libgwy/strfuncs.h"
#include "libgwy/object-utils.h"
#include "libgwyui/types.h"
#include "libgwyui/axis.h"

#define EPS 1e-12
#define ALMOST_BLOODY_INFINITY (1e-3*G_MAXDOUBLE)
#define ALMOST_BLOODY_NOTHING (1e3*G_MINDOUBLE)

#define MIN_TICK_DIST 5.0
#define MIN_MAJOR_DIST 50.0

#define pangoscale ((gdouble)PANGO_SCALE)

#define tick_index(a, i) g_array_index((a), GwyAxisTick, (i))

typedef enum {
    GWY_AXIS_STEP_0,
    GWY_AXIS_STEP_1,
    GWY_AXIS_STEP_2,
    GWY_AXIS_STEP_2_5,
    GWY_AXIS_STEP_5,
    GWY_AXIS_STEP_NSTEPS
} GwyAxisStepType;

typedef enum {
   FINALLY_OK,
   FIRST_TRY,
   ADD_DIGITS,
   LESS_TICKS
} State;

enum {
    PROP_0,
    PROP_UNIT,
    PROP_EDGE,
    PROP_RANGE_REQUEST,
    PROP_RANGE,
    PROP_SNAP_TO_TICKS,
    PROP_TICKS_AT_EDGES,
    PROP_SHOW_LABELS,
    PROP_SHOW_UNITS,   // FIXME: Make this a class property?  Or handle graphs
                       // specifically (they have units elsewhere, normally).
    PROP_MAX_TICK_LEVEL,
    N_PROPS,
};

struct _GwyAxisPrivate {
    GdkWindow *input_window;

    GwyUnit *unit;
    gulong unit_changed_id;

    GwyRange request;
    GwyRange range;

    GtkPositionType edge;
    GwyAxisTickLevel max_tick_level;
    gboolean snap_to_ticks : 1;
    gboolean ticks_at_edges : 1;
    gboolean show_labels : 1;
    gboolean show_units : 1;

    gboolean ticks_are_valid : 1;
    gboolean must_fix_units : 1;
    guint length;
    GwyValueFormat *vf;
    GArray *ticks;

    // Scratch space
    PangoLayout *layout;
    GString *str;
    gdouble units_at;
    guint split_width;
};

typedef struct _GwyAxisPrivate Axis;

static void            gwy_axis_dispose          (GObject *object);
static void            gwy_axis_finalize         (GObject *object);
static void            gwy_axis_set_property     (GObject *object,
                                                  guint prop_id,
                                                  const GValue *value,
                                                  GParamSpec *pspec);
static void            gwy_axis_get_property     (GObject *object,
                                                  guint prop_id,
                                                  GValue *value,
                                                  GParamSpec *pspec);
static void            gwy_axis_realize          (GtkWidget *widget);
static void            gwy_axis_unrealize        (GtkWidget *widget);
static void            gwy_axis_style_updated    (GtkWidget *widget);
static void            gwy_axis_size_allocate    (GtkWidget *widget,
                                                  GtkAllocation *allocation);
static void            create_input_window       (GwyAxis *axis);
static void            destroy_input_window      (GwyAxis *axis);
static gboolean        set_snap_to_ticks         (GwyAxis *axis,
                                                  gboolean setting);
static gboolean        set_ticks_at_edges        (GwyAxis *axis,
                                                  gboolean setting);
static gboolean        set_show_labels           (GwyAxis *axis,
                                                  gboolean setting);
static gboolean        set_show_units            (GwyAxis *axis,
                                                  gboolean setting);
static gboolean        set_edge                  (GwyAxis *axis,
                                                  GtkPositionType edge);
static gboolean        set_max_tick_level        (GwyAxis *axis,
                                                  GwyAxisTickLevel level);
static gboolean        request_range             (GwyAxis *axis,
                                                  const GwyRange *range);
static void            unit_changed              (GwyAxis *axis,
                                                  GwyUnit *unit);
static void            invalidate_ticks          (GwyAxis *axis);
static void            position_set_style_classes(GtkWidget *widget,
                                                  GtkPositionType position);
static void            calculate_ticks           (GwyAxis *axis);
static void            fill_tick_arrays          (GwyAxis *axis,
                                                  GwyAxisTickLevel level,
                                                  gdouble bs,
                                                  gdouble largerbs);
static void            remove_too_close_ticks    (GwyAxis *axis);
static gboolean        zero_is_inside            (gdouble start,
                                                  guint n,
                                                  gdouble bs);
static gboolean        precision_is_sufficient   (GwyValueFormat *vf,
                                                  const GwyRange *range,
                                                  gdouble bs,
                                                  GString *str);
static void            snap_range_to_ticks       (GwyRange *range,
                                                  gdouble bs);
static GwyAxisStepType choose_step_type          (gdouble *step,
                                                  gdouble *base);
static GwyAxisStepType decrease_step_type        (GwyAxisStepType steptype,
                                                  gdouble *base,
                                                  gdouble dx,
                                                  gdouble min_incr);
static void            ensure_layout_and_ticks   (GwyAxis *axis);
static void            rotate_pango_layout       (GwyAxis *axis);
static void            clear_tick                (gpointer item);
static gdouble         estimate_major_distance   (GwyAxis *axis,
                                                  const GwyRange *request);
static void            fix_request               (GwyRange *request);
static void            format_value_label        (GwyAxis *axis,
                                                  gdouble value,
                                                  gboolean with_units);

static GParamSpec *properties[N_PROPS];

static const GwyRange default_range = { -1.0, 1.0 };

static const gdouble step_sizes[GWY_AXIS_STEP_NSTEPS] = {
    0.0, 1.0, 2.0, 2.5, 5.0,
};

G_DEFINE_ABSTRACT_TYPE(GwyAxis, gwy_axis, GTK_TYPE_WIDGET);
G_DEFINE_BOXED_TYPE(GwyAxisTick, gwy_axis_tick,
                    gwy_axis_tick_copy, gwy_axis_tick_free);

/**
 * gwy_axis_tick_copy:
 * @tick: Axis tick.
 *
 * Copies an axis tick.
 *
 * Returns: A copy of @tick. The result must be freed using
 *          gwy_axis_tick_free(), not g_free().
 **/
GwyAxisTick*
gwy_axis_tick_copy(const GwyAxisTick *tick)
{
    g_return_val_if_fail(tick, NULL);
    GwyAxisTick *copy = g_slice_copy(sizeof(GwyAxisTick), tick);
    copy->label = g_strdup(tick->label);
    return copy;
}

/**
 * gwy_axis_tick_free:
 * @tick: Axis tick.
 *
 * Frees axis tick created with gwy_axis_tick_copy().
 **/
void
gwy_axis_tick_free(GwyAxisTick *tick)
{
    GWY_FREE(tick->label);
    g_slice_free1(sizeof(GwyAxisTick), tick);
}

static void
gwy_axis_class_init(GwyAxisClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    g_type_class_add_private(klass, sizeof(Axis));

    gobject_class->dispose = gwy_axis_dispose;
    gobject_class->finalize = gwy_axis_finalize;
    gobject_class->get_property = gwy_axis_get_property;
    gobject_class->set_property = gwy_axis_set_property;

    widget_class->realize = gwy_axis_realize;
    widget_class->unrealize = gwy_axis_unrealize;
    widget_class->style_updated = gwy_axis_style_updated;
    widget_class->size_allocate = gwy_axis_size_allocate;

    properties[PROP_UNIT]
         = g_param_spec_object("unit",
                               "Unit",
                               "Units of the quantity shown on the axis.",
                               GWY_TYPE_UNIT,
                               G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_EDGE]
         = g_param_spec_enum("edge",
                             "Edge",
                             "Edge on which this axis is placed with "
                             "respect to the data widget.  Ticks are "
                             "drawn on the opposite edge of the axis "
                             "so that they are adjacent to the data widget.",
                             GTK_TYPE_POSITION_TYPE,
                             GTK_POS_BOTTOM,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_RANGE_REQUEST]
         = g_param_spec_boxed("range-request",
                              "Range request",
                              "Requested value range of the axis.",
                              GWY_TYPE_RANGE,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_RANGE]
         = g_param_spec_boxed("range",
                              "Range",
                              "Actual value range of the axis.",
                              GWY_TYPE_RANGE,
                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_SNAP_TO_TICKS]
        = g_param_spec_boolean("snap-to-ticks",
                               "Snap to ticks",
                               "Whether the range should start and end at "
                               "a major tick.",
                               FALSE,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_TICKS_AT_EDGES]
        = g_param_spec_boolean("ticks-at-edges",
                               "Ticks at edges",
                               "Whether ticks should be drawn at the "
                               "axis edges even if no major ticks occur there.",
                               FALSE,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_SHOW_LABELS]
        = g_param_spec_boolean("show-labels",
                               "Show labels",
                               "Whether labels should be shown at major ticks.",
                               TRUE,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_SHOW_UNITS]
        = g_param_spec_boolean("show-units",
                               "Show units",
                               "Whether units should be shown next to a major "
                               "tick.",
                               TRUE,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_MAX_TICK_LEVEL]
        = g_param_spec_enum("max-tick-level",
                            "Max tick level",
                            "Maximum level of ticks to draw.  Zero means "
                            "no ticks at all, except possibly end-point "
                            "ticks.  One means major ticks, etc.",
                            GWY_TYPE_AXIS_TICK_LEVEL,
                            GWY_AXIS_TICK_MINOR,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, properties[i]);
}

static void
gwy_axis_init(GwyAxis *axis)
{
    axis->priv = G_TYPE_INSTANCE_GET_PRIVATE(axis, GWY_TYPE_AXIS, Axis);
    Axis *priv = axis->priv;

    priv->unit = gwy_unit_new();
    priv->unit_changed_id = g_signal_connect_swapped(priv->unit, "changed",
                                                     G_CALLBACK(unit_changed),
                                                     axis);
    priv->show_labels = TRUE;
    priv->show_units = TRUE;
    priv->edge = GTK_POS_BOTTOM;
    priv->max_tick_level = GWY_AXIS_TICK_MINOR;
    priv->length = 1;
    priv->range = priv->request = default_range;
    gtk_widget_set_has_window(GTK_WIDGET(axis), FALSE);
}

static void
gwy_axis_finalize(GObject *object)
{
    Axis *priv = GWY_AXIS(object)->priv;
    g_signal_handler_disconnect(priv->unit, priv->unit_changed_id);
    g_object_unref(priv->unit);
    GWY_STRING_FREE(priv->str);
    GWY_ARRAY_FREE(priv->ticks);
    G_OBJECT_CLASS(gwy_axis_parent_class)->finalize(object);
}

static void
gwy_axis_dispose(GObject *object)
{
    Axis *priv = GWY_AXIS(object)->priv;
    // Unref even our own member objects if they are of the may-not-exist kind.
    GWY_OBJECT_UNREF(priv->vf);
    GWY_OBJECT_UNREF(priv->layout);
    G_OBJECT_CLASS(gwy_axis_parent_class)->dispose(object);
}

static void
gwy_axis_set_property(GObject *object,
                      guint prop_id,
                      const GValue *value,
                      GParamSpec *pspec)
{
    GwyAxis *axis = GWY_AXIS(object);

    switch (prop_id) {
        case PROP_EDGE:
        set_edge(axis, g_value_get_enum(value));
        break;

        case PROP_MAX_TICK_LEVEL:
        set_max_tick_level(axis, g_value_get_enum(value));
        break;

        case PROP_RANGE_REQUEST:
        request_range(axis, (const GwyRange*)g_value_get_boxed(value));
        break;

        case PROP_SNAP_TO_TICKS:
        set_snap_to_ticks(axis, g_value_get_boolean(value));
        break;

        case PROP_TICKS_AT_EDGES:
        set_ticks_at_edges(axis, g_value_get_boolean(value));
        break;

        case PROP_SHOW_LABELS:
        set_show_labels(axis, g_value_get_boolean(value));
        break;

        case PROP_SHOW_UNITS:
        set_show_units(axis, g_value_get_boolean(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_axis_get_property(GObject *object,
                      guint prop_id,
                      GValue *value,
                      GParamSpec *pspec)
{
    Axis *priv = GWY_AXIS(object)->priv;

    switch (prop_id) {
        case PROP_UNIT:
        g_value_set_object(value, priv->unit);
        break;

        case PROP_EDGE:
        g_value_set_enum(value, priv->edge);
        break;

        case PROP_MAX_TICK_LEVEL:
        g_value_set_enum(value, priv->max_tick_level);
        break;

        case PROP_RANGE_REQUEST:
        g_value_set_boxed(value, &priv->request);
        break;

        case PROP_RANGE:
        g_value_set_boxed(value, &priv->range);
        break;

        case PROP_SNAP_TO_TICKS:
        g_value_set_boolean(value, priv->snap_to_ticks);
        break;

        case PROP_TICKS_AT_EDGES:
        g_value_set_boolean(value, priv->ticks_at_edges);
        break;

        case PROP_SHOW_LABELS:
        g_value_set_boolean(value, priv->show_labels);
        break;

        case PROP_SHOW_UNITS:
        g_value_set_boolean(value, priv->show_units);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_axis_realize(GtkWidget *widget)
{
    GwyAxis *axis = GWY_AXIS(widget);
    GTK_WIDGET_CLASS(gwy_axis_parent_class)->realize(widget);
    create_input_window(axis);
}

static void
gwy_axis_unrealize(GtkWidget *widget)
{
    GwyAxis *axis = GWY_AXIS(widget);
    Axis *priv = axis->priv;
    destroy_input_window(axis);
    priv->ticks_are_valid = FALSE;
    GWY_OBJECT_UNREF(priv->layout);
    GTK_WIDGET_CLASS(gwy_axis_parent_class)->unrealize(widget);
}

static void
gwy_axis_style_updated(GtkWidget *widget)
{
    GwyAxis *axis = GWY_AXIS(widget);
    Axis *priv = axis->priv;
    if (priv->layout)
        pango_layout_context_changed(priv->layout);
    invalidate_ticks(axis);
    GTK_WIDGET_CLASS(gwy_axis_parent_class)->style_updated(widget);
}

static void
gwy_axis_size_allocate(GtkWidget *widget,
                       GtkAllocation *allocation)
{
    GwyAxis *axis = GWY_AXIS(widget);
    Axis *priv = axis->priv;
    GtkPositionType edge = priv->edge;

    GTK_WIDGET_CLASS(gwy_axis_parent_class)->size_allocate(widget, allocation);

    if (edge == GTK_POS_TOP || edge == GTK_POS_BOTTOM)
        priv->length = allocation->width;
    if (edge == GTK_POS_LEFT || edge == GTK_POS_RIGHT)
        priv->length = allocation->height;

    invalidate_ticks(axis);

    if (priv->input_window)
        gdk_window_move_resize(priv->input_window,
                               allocation->x, allocation->y,
                               allocation->width, allocation->height);
}

/**
 * gwy_axis_get_range:
 * @axis: An axis.
 * @range: (out):
 *         Location to store the actual axis range.
 *
 * Obtains the actual range of an axis.
 *
 * The actual range may differ from the requested range namely when snapping
 * to ticks is enabled.  Generally, it should contain the entire requested
 * range but in less usual circumstances (e.g. logarithmic axes) it may not.
 * Also the actual range may be unrelated to requested between the request and
 * the recalculation of ticks.
 **/
void
gwy_axis_get_range(const GwyAxis *axis,
                   GwyRange *range)
{
    g_return_if_fail(range);
    g_return_if_fail(GWY_IS_AXIS(axis));
    *range = axis->priv->range;
}

/**
 * gwy_axis_request_range:
 * @axis: An axis.
 * @request: Requested range for the axis.
 *
 * Requests the range an axis should cover.
 **/
void
gwy_axis_request_range(GwyAxis *axis,
                       const GwyRange *request)
{
    g_return_if_fail(GWY_IS_AXIS(axis));
    if (!request_range(axis, request))
        return;

    g_object_notify_by_pspec(G_OBJECT(axis), properties[PROP_RANGE_REQUEST]);
}

/**
 * gwy_axis_get_requested_range:
 * @axis: An axis.
 * @range: (out):
 *         Location to store the requested axis range.
 *
 * Obtains the actual range of an axis.
 *
 * See gwy_axis_get_range() for discussion.
 **/
void
gwy_axis_get_requested_range(GwyAxis *axis,
                             GwyRange *range)
{
    g_return_if_fail(range);
    g_return_if_fail(GWY_IS_AXIS(axis));
    *range = axis->priv->request;
}

/**
 * gwy_axis_get_unit:
 * @axis: An axis.
 *
 * Obtains the unit object used by an axis to determine units.
 *
 * Returns: (transfer none):
 *          The units of axis coordinates.
 **/
GwyUnit*
gwy_axis_get_unit(const GwyAxis *axis)
{
    g_return_val_if_fail(GWY_IS_AXIS(axis), NULL);
    return axis->priv->unit;
}

/**
 * gwy_axis_get_show_labels:
 * @axis: An axis.
 *
 * Reports whether an axis shows tick labels.
 *
 * Returns: %TRUE if @axis shows tick labels; %FALSE if they are not shown.
 **/
gboolean
gwy_axis_get_show_labels(const GwyAxis *axis)
{
    g_return_val_if_fail(GWY_IS_AXIS(axis), FALSE);
    return !!axis->priv->show_labels;
}

/**
 * gwy_axis_set_show_labels:
 * @axis: An axis.
 * @showlabels: %TRUE to show tick labels; %FALSE to not show them.
 *
 * Sets whether an axis should show tick labels.
 **/
void
gwy_axis_set_show_labels(GwyAxis *axis,
                         gboolean showlabels)
{
    g_return_if_fail(GWY_IS_AXIS(axis));
    if (!set_show_labels(axis, showlabels))
        return;

    g_object_notify_by_pspec(G_OBJECT(axis), properties[PROP_SHOW_LABELS]);
}

/**
 * gwy_axis_get_edge:
 * @axis: An axis.
 *
 * Gets the edge of some data visualisation area where the axis belongs.
 *
 * Returns: The axis edge.
 **/
GtkPositionType
gwy_axis_get_edge(const GwyAxis *axis)
{
    g_return_val_if_fail(GWY_IS_AXIS(axis), GTK_POS_BOTTOM);
    return GWY_AXIS(axis)->priv->edge;
}

/**
 * gwy_axis_set_edge:
 * @axis: An axis.
 * @edge: Edge the axis belongs to.
 *
 * Sets the edge of some data visualisation area where the axis belongs.
 *
 * The edge where the axis belongs means the edge of some other data
 * visualisation area to which the axis is supposed to be attached.  So the
 * main edge of the axis itself will be, generally, the oposite to @edge.
 **/
void
gwy_axis_set_edge(GwyAxis *axis,
                  GtkPositionType edge)
{
    g_return_if_fail(GWY_IS_AXIS(axis));
    if (!set_edge(axis, edge))
        return;

    g_object_notify_by_pspec(G_OBJECT(axis), properties[PROP_EDGE]);
}

/**
 * gwy_axis_get_snap_to_ticks:
 * @axis: An axis.
 *
 * Gets the range snapping behaviour of an axis.
 *
 * Returns: %TRUE if the axis range is rounded to contain the requested range
 *          but start and end at a major tick.  %FALSE if the axis range is
 *          always the requested range.
 **/
gboolean
gwy_axis_get_snap_to_ticks(const GwyAxis *axis)
{
    g_return_val_if_fail(GWY_IS_AXIS(axis), FALSE);
    return GWY_AXIS(axis)->priv->snap_to_ticks;
}

/**
 * gwy_axis_set_snap_to_ticks:
 * @axis: An axis.
 * @snaptoticks: %TRUE to round the axis range to contain the requested range
 *               but start and end at a major tick.  %FALSE to start and end
 *               the range at requested values.
 *
 * Sets the range snapping behaviour of an axis.
 **/
void
gwy_axis_set_snap_to_ticks(GwyAxis *axis,
                           gboolean snaptoticks)
{
    g_return_if_fail(GWY_IS_AXIS(axis));
    if (!set_snap_to_ticks(axis, snaptoticks))
        return;

    g_object_notify_by_pspec(G_OBJECT(axis), properties[PROP_SNAP_TO_TICKS]);
}

/**
 * gwy_axis_get_pango_layout:
 * @axis: An axis.
 *
 * Gets the Pango layout used for laying out tick of an axis.
 *
 * This method is namely intended for subclasses.
 *
 * The layout exists only if the widget is realized.  It is not recommend to
 * cache it, however, if you do so make sure that you stop using it if @axis
 * is unrealized.
 *
 * Returns: (allow-none):
 *          The Pango layout @axis uses to calculate tick positions.  %NULL is
 *          returned if @axis is not realized.
 **/
PangoLayout*
gwy_axis_get_pango_layout(GwyAxis *axis)
{
    g_return_val_if_fail(GWY_IS_AXIS(axis), NULL);
    Axis *priv = axis->priv;
    if (!priv->layout && gtk_widget_has_screen(GTK_WIDGET(axis)))
        ensure_layout_and_ticks(axis);
    return priv->layout;
}

/**
 * gwy_axis_ticks:
 * @axis: An axis.
 * @nticks: (out):
 *          Location where the number of returned ticks will be stored.
 *
 * Obtains the list of ticks for an axis.
 *
 * This method is namely intended for subclasses.
 *
 * If tick positions and labels have not been calculated yet or they have to
 * be recalculated due to changes in widget size, axis range, properties,
 * etc., this method will recalculate them first.
 *
 * The widget must be realised in order to calculate the ticks.  If it is
 * not realised this method just sets @nticks to 0 and returns %NULL.
 *
 * Thisk labels are set to %NULL if labels are disabled so the caller does not
 * to have handle this.  However it still has to handle overlapping labels.
 *
 * Returns: (allow-none) (transfer none) (array length=nticks):
 *          Array of ticks for the axis.  The ticks are ordered by position
 *          on the axis, i.e. ticks of different levels are interleaved.
 *          The returned array is owned by @axis and valid only until it is
 *          recalculated.
 **/
const GwyAxisTick*
gwy_axis_ticks(GwyAxis *axis,
               guint *nticks)
{
    g_return_val_if_fail(GWY_IS_AXIS(axis), NULL);
    g_return_val_if_fail(nticks, NULL);

    Axis *priv = axis->priv;
    if (!priv->ticks_are_valid)
        calculate_ticks(axis);

    *nticks = priv->ticks->len;
    return (const GwyAxisTick*)priv->ticks->data;
}

/**
 * gwy_axis_position_to_value:
 * @axis: An axis.
 * @position: Pixel (widget) position on the axis.
 *
 * Converts pixel (widget) position on an axis to real value.
 *
 * Returns: Real value corresponding to @position.
 **/
gdouble
gwy_axis_position_to_value(GwyAxis *axis,
                           gdouble position)
{
    g_return_val_if_fail(GWY_IS_AXIS(axis), NAN);
    Axis *priv = axis->priv;
    gdouble from = priv->range.from, to = priv->range.to, length = priv->length;
    return position/length*(to - from) + from;
}

/**
 * gwy_axis_value_to_position:
 * @axis: An axis.
 * @value: Real value on the axis.
 *
 * Converts real value on an axis to pixel (widget) position.
 *
 * Returns: Pixel position corresponding to @value.
 **/
gdouble
gwy_axis_value_to_position(GwyAxis *axis,
                           gdouble value)
{
    g_return_val_if_fail(GWY_IS_AXIS(axis), NAN);
    Axis *priv = axis->priv;
    gdouble from = priv->range.from, to = priv->range.to, length = priv->length;
    return (value - from)/(to - from)*length;
}

static void
create_input_window(GwyAxis *axis)
{
    Axis *priv = axis->priv;
    GtkWidget *widget = GTK_WIDGET(axis);
    g_assert(gtk_widget_get_realized(widget));
    if (priv->input_window)
        return;

    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);
    GdkWindowAttr attributes = {
        .x = allocation.x,
        .y = allocation.y,
        .width = allocation.width,
        .height = allocation.height,
        .window_type = GDK_WINDOW_CHILD,
        .wclass = GDK_INPUT_ONLY,
        .override_redirect = TRUE,
        .event_mask = 0,    // Users are expected to set required mask.
    };
    gint attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_NOREDIR | GDK_WA_CURSOR;
    priv->input_window = gdk_window_new(gtk_widget_get_window(widget),
                                        &attributes, attributes_mask);
    gdk_window_set_user_data(priv->input_window, widget);
}

static void
destroy_input_window(GwyAxis *axis)
{
    Axis *priv = axis->priv;
    if (!priv->input_window)
        return;
    gdk_window_set_user_data(priv->input_window, NULL);
    gdk_window_destroy(priv->input_window);
    priv->input_window = NULL;
}

static gboolean
set_edge(GwyAxis *axis,
         GtkPositionType edge)
{
    Axis *priv = axis->priv;
    if (edge == priv->edge)
        return FALSE;

    priv->edge = edge;
    priv->ticks_are_valid = FALSE;
    gtk_widget_queue_resize(GTK_WIDGET(axis));
    position_set_style_classes(GTK_WIDGET(axis), edge);
    return TRUE;
}

static gboolean
set_max_tick_level(GwyAxis *axis,
                   GwyAxisTickLevel level)
{
    Axis *priv = axis->priv;
    if (level == priv->max_tick_level)
        return FALSE;

    priv->max_tick_level = level;
    invalidate_ticks(axis);
    return TRUE;
}

static gboolean
request_range(GwyAxis *axis,
              const GwyRange *request)
{
    Axis *priv = axis->priv;
    if (request->from == priv->request.from && request->to == priv->request.to)
        return FALSE;

    priv->request = *request;
    invalidate_ticks(axis);
    return TRUE;
}

static gboolean
set_snap_to_ticks(GwyAxis *axis,
                  gboolean setting)
{
    Axis *priv = axis->priv;
    if (!setting == !priv->snap_to_ticks)
        return FALSE;

    priv->snap_to_ticks = setting;
    invalidate_ticks(axis);
    return TRUE;
}

static gboolean
set_show_labels(GwyAxis *axis,
                gboolean setting)
{
    Axis *priv = axis->priv;
    if (!setting == !priv->show_labels)
        return FALSE;

    priv->show_labels = setting;
    // TODO: Subclasses may want to queue resize here?
    invalidate_ticks(axis);
    return TRUE;
}

static gboolean
set_show_units(GwyAxis *axis,
               gboolean setting)
{
    Axis *priv = axis->priv;
    if (!setting == !priv->show_units)
        return FALSE;

    priv->show_units = setting;
    invalidate_ticks(axis);
    return TRUE;
}

static gboolean
set_ticks_at_edges(GwyAxis *axis,
                   gboolean setting)
{
    Axis *priv = axis->priv;
    if (!setting == !priv->ticks_at_edges)
        return FALSE;

    priv->ticks_at_edges = setting;
    invalidate_ticks(axis);
    return TRUE;
}

static void
unit_changed(GwyAxis *axis,
             G_GNUC_UNUSED GwyUnit *unit)
{
    Axis *priv = axis->priv;
    if (priv->show_units)
        invalidate_ticks(axis);
}

static void
invalidate_ticks(GwyAxis *axis)
{
    // FIXME: If the widget has no size allocated yet there is no way to
    // calculate tics.  Is it OK to just set range to the request then?
    Axis *priv = axis->priv;
    if (priv->ticks_are_valid) {
        priv->ticks_are_valid = FALSE;
        gtk_widget_queue_draw(GTK_WIDGET(axis));
    }
}

// FIXME: This should be called when the widget is initially created.
// FIXME: This should go to general widget utils.
static void
position_set_style_classes(GtkWidget *widget,
                           GtkPositionType position)
{
    g_return_if_fail(GTK_IS_WIDGET(widget));

    GtkStyleContext *context = gtk_widget_get_style_context(widget);
    if (position == GTK_POS_TOP || position == GTK_POS_BOTTOM) {
        gtk_style_context_add_class(context, GTK_STYLE_CLASS_HORIZONTAL);
        gtk_style_context_remove_class(context, GTK_STYLE_CLASS_VERTICAL);
    }
    else if (position == GTK_POS_LEFT || position == GTK_POS_RIGHT) {
        gtk_style_context_add_class(context, GTK_STYLE_CLASS_VERTICAL);
        gtk_style_context_remove_class(context, GTK_STYLE_CLASS_HORIZONTAL);
    }
    else {
        g_assert_not_reached();
    }
}

static inline gdouble
if_zero_then_exactly(gdouble value, gdouble bs)
{
    if (G_UNLIKELY(fabs(value) < EPS*fabs(bs)))
        return 0.0;
    return value;
}

static gint
compare_ticks_ascending(gconstpointer a, gconstpointer b)
{
    const GwyAxisTick *ta = (const GwyAxisTick*)a;
    const GwyAxisTick *tb = (const GwyAxisTick*)b;

    if (ta->value < tb->value)
        return -1;
    if (ta->value > tb->value)
        return 1;

    g_warning("Two ticks at the same value %g; positions %g and %g, "
              "levels %u and %u.",
              ta->value, ta->position, tb->position, ta->level, tb->level);
    return 0;
}

static gint
compare_ticks_descending(gconstpointer a, gconstpointer b)
{
    const GwyAxisTick *ta = (const GwyAxisTick*)a;
    const GwyAxisTick *tb = (const GwyAxisTick*)b;

    if (ta->value > tb->value)
        return -1;
    if (ta->value < tb->value)
        return 1;

    g_warning("Two ticks at the same value %g; positions %g and %g, "
              "levels %u and %u.",
              ta->value, ta->position, tb->position, ta->level, tb->level);
    return 0;
}

// Tick algorithm.
// (1) Estimate minimum distance required between major ticks.  This is a
//     handful of pixels for no-labels case; approx. the dimension of the
//     label when tick labels are requested.
// (2) Estimate the major step.
// (3) If we cannot place even two major ticks, enter a fallback mode and just
//     try to draw something (possibly disabling labels even if they are
//     requested).  End here.
// (4) Calculate the label precision necessary for given major step.  Can we
//     do this exactly?
// (5) Try to actually format ticks to see whether they fit. This includes
//     sufficient spacing of minor ticks (if requested).
// (6) Ticks fit: Remember this base as LAST_GOOD and decrease the major step
//     to the next smaller, go to (4).
// (7) Ticks do not fit: If we have LAST_GOOD use that.  Otherwise increase the
//     majot step to the next larger.  Go to (3).
// (8) Take minor step as the next smaller for obtained major step.
static void
calculate_ticks(GwyAxis *axis)
{
    Axis *priv = axis->priv;
    GwyRange request = priv->request;
    fix_request(&request);

    // XXX: This is for labels *along* the axis.  The labels can be also
    // perpendicular.  OTOH for consistent interlabel distance this might be
    // good, in fact.
    GWY_OBJECT_UNREF(priv->vf);
    priv->vf = gwy_unit_format_with_resolution
                               (priv->unit, GWY_VALUE_FORMAT_PANGO,
                                fmax(fabs(request.from), fabs(request.to)),
                                fabs(request.to - request.from)/12.0);
    ensure_layout_and_ticks(axis);
    priv->split_width = gwy_axis_get_split_width(axis);

    gboolean descending = (request.to < request.from);
    guint length = priv->length;
    gdouble majdist = estimate_major_distance(axis, &request);
    GArray *ticks = priv->ticks;

    g_array_set_size(ticks, 0);

    if (length < 2) {
        g_warning("Cannot fit any major ticks.  Implement some fallback.");
        priv->range = request;
        priv->ticks_are_valid = TRUE;
        // TODO: We could, more or less, simply continue, without any of the
        // ugly iterative business below.  There will not be any labels or
        // even ticks drawn at reasonable places, but what the fuck.  The
        // only thing to prevent seriously is overlapping labels.
        return;
    }

    State state = FIRST_TRY;
    GwyAxisStepType steptype = GWY_AXIS_STEP_0;
    // Silence GCC.  And make any uninitialised use pretty obvious.
    gdouble base = NAN, step = NAN, bs = NAN;
    do {
        priv->range = request;
        if (state != LESS_TICKS) {
            majdist = estimate_major_distance(axis, &request);
            step = fabs(priv->range.to - priv->range.from)/(length/majdist);
            steptype = choose_step_type(&step, &base);
        }
        step = step_sizes[steptype];
        bs = descending ? -base*step : base*step;
        if (priv->snap_to_ticks)
            snap_range_to_ticks(&priv->range, bs);

        guint precision = gwy_value_format_get_precision(priv->vf);
        if (precision_is_sufficient(priv->vf, &priv->range, bs, priv->str)) {
            if (state == FIRST_TRY && precision > 0)
                gwy_value_format_set_precision(priv->vf, precision-1);
            else
                state = FINALLY_OK;
        }
        else {
            if (state == FIRST_TRY) {
                state = ADD_DIGITS;
                gwy_value_format_set_precision(priv->vf, precision+1);
            }
            else {
                state = LESS_TICKS;
                base *= 10;
                steptype = GWY_AXIS_STEP_1;
            }
        }
    } while (state != FINALLY_OK);

    gdouble dx = fabs(priv->range.to - priv->range.from)/length;
    priv->units_at = NAN;
    fill_tick_arrays(axis, GWY_AXIS_TICK_MAJOR, bs, 0.0);
    for (guint i = GWY_AXIS_TICK_MINOR; i <= priv->max_tick_level; i++) {
        gdouble largerbs = bs;
        steptype = decrease_step_type(steptype, &base, dx, MIN_TICK_DIST);
        if (!steptype)
            break;
        step = step_sizes[steptype];
        bs = descending ? -base*step : base*step;
        fill_tick_arrays(axis, i, bs, largerbs);
    }

    g_array_sort(ticks, descending
                 ? compare_ticks_descending
                 : compare_ticks_ascending);
    priv->must_fix_units = FALSE;
    remove_too_close_ticks(axis);

    if (priv->must_fix_units) {
        g_assert(isfinite(priv->units_at));
        for (guint i = 0; i < ticks->len; i++) {
            GwyAxisTick *tick = &tick_index(ticks, i);
            if (tick->value == priv->units_at) {
                format_value_label(axis, tick->value, TRUE);
                pango_layout_get_extents(priv->layout, NULL, &tick->extents);
                g_free(tick->label);
                tick->label = g_strdup(priv->str->str);
                break;
            }
        }
    }

    priv->ticks_are_valid = TRUE;
}

// NB: If level is GWY_AXIS_TICK_MAJOR it creates also EDGE-level ticks.
// Furthermore edge+major ticks must be placed first (for proper units
// position location).
static void
fill_tick_arrays(GwyAxis *axis, guint level,
                 gdouble bs, gdouble largerbs)
{
    const PangoRectangle no_extents = { 0, 0, 0, 0 };
    Axis *priv = axis->priv;
    gdouble from = priv->range.from, to = priv->range.to;
    gdouble start = if_zero_then_exactly(bs > 0.0
                                         ? ceil(from/bs - EPS)*bs
                                         : floor(from/bs + EPS)*bs,
                                         bs);
    guint n = (guint)floor((to - start)/bs + EPS);
    GArray *ticks = priv->ticks;
    GString *str = priv->str;
    guint length = priv->length;
    GwyAxisUnitPlacement units_pos = GWY_AXIS_UNITS_NEVER;

    if (priv->show_units) {
        GwyAxisUnitPlacement units_pos_secondary;
        gwy_axis_get_units_affinity(axis, &units_pos, &units_pos_secondary);
        if (units_pos == GWY_AXIS_UNITS_ZERO) {
            if (!zero_is_inside(start, n, bs)) {
                units_pos = units_pos_secondary;
                if (units_pos == GWY_AXIS_UNITS_ZERO) {
                    g_warning("Secondary units affinity should not be "
                              "GWY_AXIS_UNITS_ZERO again.");
                    units_pos = GWY_AXIS_UNITS_FIRST;
                }
            }
        }
    }

    // Tick at the leading edge.
    if (level == GWY_AXIS_TICK_MAJOR && priv->ticks_at_edges) {
        GwyAxisTick tick = {
            .value = from, .position = 0.0,
            .label = NULL, .extents = no_extents,
            .level = GWY_AXIS_TICK_EDGE
        };

        if (priv->show_labels) {
            format_value_label(axis, tick.value,
                               units_pos == GWY_AXIS_UNITS_FIRST
                               || (units_pos == GWY_AXIS_UNITS_ZERO
                                   && tick.value == 0.0));
            pango_layout_get_extents(priv->layout, NULL, &tick.extents);
            tick.label = g_strdup(str->str);
        }
        g_array_append_val(ticks, tick);
    }

    // Tick at the trailing edge.  Needs to be moved backward to fit within.
    if (level == GWY_AXIS_TICK_MAJOR && priv->ticks_at_edges) {
        GwyAxisTick tick = {
            .value = to, .position = length,
            .label = NULL, .extents = no_extents,
            .level = GWY_AXIS_TICK_EDGE
        };

        if (priv->show_labels) {
            format_value_label(axis, tick.value,
                               units_pos == GWY_AXIS_UNITS_LAST
                               || (units_pos == GWY_AXIS_UNITS_ZERO
                                   && tick.value == 0.0));
            pango_layout_get_extents(priv->layout, NULL, &tick.extents);
            tick.label = g_strdup(str->str);
        }
        g_array_append_val(ticks, tick);
    }

    if (priv->max_tick_level < GWY_AXIS_TICK_MAJOR)
        return;

    // Normal ticks between
    for (guint i = 0; i <= n; i++) {
        GwyAxisTick tick = {
            .value = if_zero_then_exactly(i*bs + start, bs), .position = 0.0,
            .label = NULL, .extents = no_extents, .level = level
        };

        // Skip ticks coinciding with more major ones.
        if (largerbs
            && fabs(tick.value/largerbs - gwy_round(tick.value/largerbs)) < EPS)
            continue;
        if (priv->ticks_at_edges && (fabs((tick.value - from)/bs) < EPS
                                     || fabs((tick.value - to)/bs) < EPS))
            continue;

        tick.position = (tick.value - from)/(to - from)*length;
        if (priv->show_labels && level == GWY_AXIS_TICK_MAJOR) {
            format_value_label(axis, tick.value,
                               (units_pos == GWY_AXIS_UNITS_ZERO
                                && tick.value == 0.0)
                               || (units_pos == GWY_AXIS_UNITS_FIRST
                                   && ticks->len == 0)
                               || (units_pos == GWY_AXIS_UNITS_LAST
                                   && !priv->ticks_at_edges
                                   && i == n));
            pango_layout_get_extents(priv->layout, NULL, &tick.extents);
            tick.label = g_strdup(str->str);
        }
        g_array_append_val(ticks, tick);
    }
}

static void
remove_too_close_ticks(GwyAxis *axis)
{
    Axis *priv = axis->priv;
    GArray *ticks = priv->ticks;
    GwyAxisTick *tick1, *tick2;
    guint n;

    if (!priv->ticks_at_edges)
        return;

    // Tick mark collisions.
    n = ticks->len;
    if (n <= 2)
        return;

    tick1 = &tick_index(ticks, n-1);
    tick2 = &tick_index(ticks, n-2);
    if (tick1->position - tick2->position < 0.85*MIN_TICK_DIST) {
        if (tick2->value == priv->units_at) {
            priv->must_fix_units = TRUE;
            priv->units_at = tick1->value;
        }
        g_array_remove_index(ticks, n-2);
    }

    n = ticks->len;
    if (n <= 2)
        return;

    tick1 = &tick_index(ticks, 0);
    tick2 = &tick_index(ticks, 1);
    if (tick2->position - tick1->position < 0.85*MIN_TICK_DIST) {
        if (tick2->value == priv->units_at) {
            priv->must_fix_units = TRUE;
            priv->units_at = tick1->value;
        }
        g_array_remove_index(ticks, 1);
    }

    if (!priv->show_labels)
        return;

    // Tick label collisions.  This is a bit tricky as labels can have
    // different orientations.  Also, some of the removed labels may carry
    // units.
    guint next_labelled = 1;
    while (next_labelled < n-1
           && !tick_index(ticks, next_labelled).label)
        next_labelled++;

    tick1 = &tick_index(ticks, 0);
    tick2 = &tick_index(ticks, next_labelled);
    if (next_labelled == n)
        return;

    GtkPositionType edge = priv->edge;
    gboolean horizontal = gwy_axis_get_horizontal_labels(axis);
    gdouble beg2, end1;
    if (horizontal && (edge == GTK_POS_LEFT || edge == GTK_POS_RIGHT)) {
        end1 = tick1->position + tick1->extents.height/pangoscale;
        beg2 = tick2->position - tick1->extents.height/pangoscale;
    }
    else {
        end1 = tick1->position + PANGO_RBEARING(tick1->extents)/pangoscale;
        beg2 = tick2->position + PANGO_LBEARING(tick2->extents)/pangoscale;
    }
    if (beg2 - end1 < MIN_TICK_DIST) {
        if (tick2->value == priv->units_at) {
            priv->must_fix_units = TRUE;
            priv->units_at = tick1->value;
        }
        GWY_FREE(tick2->label);
    }

    n = ticks->len;
    if (n <= 2)
        return;

    next_labelled = n-2;
    while (next_labelled
           && !tick_index(ticks, next_labelled).label)
        next_labelled--;

    tick1 = &tick_index(ticks, n-1);
    tick2 = &tick_index(ticks, next_labelled);
    if (!next_labelled)
        return;

    gdouble end2, beg1;
    if (horizontal && (edge == GTK_POS_LEFT || edge == GTK_POS_RIGHT)) {
        beg1 = tick1->position - 2.0 - tick1->extents.height/pangoscale;
        end2 = tick2->position + 2.0;
    }
    else {
        beg1 = tick1->position - 2.0 - tick1->extents.width/pangoscale;
        end2 = tick2->position + 2.0 + PANGO_RBEARING(tick2->extents)/pangoscale;
    }
    if (beg1 - end2 < MIN_TICK_DIST) {
        if (tick2->value == priv->units_at) {
            priv->must_fix_units = TRUE;
            priv->units_at = tick1->value;
        }
        GWY_FREE(tick2->label);
    }
}

static gboolean
zero_is_inside(gdouble start, guint n, gdouble bs)
{
    // Does not matter, we have only one label anyway.
    if (n == 0)
        return FALSE;

    return start*if_zero_then_exactly((n-1)*bs + start, bs) <= 0.0;
}

static gboolean
precision_is_sufficient(GwyValueFormat *vf,
                        const GwyRange *range,
                        gdouble bs,
                        GString *str)
{
    gdouble from = range->from, to = range->to;
    guint n = gwy_round((to - from)/bs);
    g_string_set_size(str, 0);
    for (guint i = 0; i <= n; i++) {
        gdouble value = if_zero_then_exactly(from + i*bs, bs);
        const gchar *repr = gwy_value_format_print_number(vf, value);
        if (gwy_strequal(str->str, repr))
            return FALSE;
        g_string_assign(str, repr);
    }
    return TRUE;
}

static void
snap_range_to_ticks(GwyRange *range,
                    gdouble bs)
{
    if (bs < 0.0) {
        range->from = ceil(range->from/bs)*bs;
        range->to = floor(range->to/bs)*bs;
    }
    else {
        range->from = floor(range->from/bs)*bs;
        range->to = ceil(range->to/bs)*bs;
    }
}

static GwyAxisStepType
choose_step_type(gdouble *step, gdouble *base)
{
    *base = gwy_powi(10.0, (gint)floor(log10(*step) + EPS));
    *step /= *base;

    while (*step <= 0.5) {
        *base /= 10.0;
        *step *= 10.0;
    }
    while (*step > 5.0) {
        *base *= 10.0;
        *step /= 10.0;
    }

    if (*step <= 1.0)
        return GWY_AXIS_STEP_1;
    if (*step <= 2.0)
        return GWY_AXIS_STEP_2;
    if (*step <= 2.5)
        return GWY_AXIS_STEP_2_5;
    return GWY_AXIS_STEP_5;
}

static GwyAxisStepType
decrease_step_type(GwyAxisStepType steptype,
                   gdouble *base,
                   gdouble dx,
                   gdouble min_incr)
{
    GwyAxisStepType new_scale = GWY_AXIS_STEP_0;

    switch (steptype) {
        case GWY_AXIS_STEP_1:
        *base /= 10.0;
        if (*base*2.0/dx >= min_incr)
            new_scale = GWY_AXIS_STEP_5;
        else if (*base*2.5/dx >= min_incr)
            new_scale = GWY_AXIS_STEP_2_5;
        else if (*base*5.0/dx >= min_incr)
            new_scale = GWY_AXIS_STEP_5;
        break;

        case GWY_AXIS_STEP_2:
        if (*base/dx >= min_incr)
            new_scale = GWY_AXIS_STEP_1;
        break;

        case GWY_AXIS_STEP_2_5:
        *base /= 10.0;
        if (*base*5.0/dx > min_incr)
            new_scale = GWY_AXIS_STEP_5;
        break;

        case GWY_AXIS_STEP_5:
        if (*base/dx >= min_incr)
            new_scale = GWY_AXIS_STEP_1;
        else if (*base*2.5/dx >= min_incr)
            new_scale = GWY_AXIS_STEP_2_5;
        break;

        default:
        g_assert_not_reached();
        break;
    }

    return new_scale;
}

static void
ensure_layout_and_ticks(GwyAxis *axis)
{
    Axis *priv = axis->priv;

    if (!priv->layout) {
        priv->layout = gtk_widget_create_pango_layout(GTK_WIDGET(axis), NULL);
        pango_layout_set_alignment(priv->layout, PANGO_ALIGN_LEFT);
        rotate_pango_layout(axis);
    }

    if (!priv->ticks) {
        priv->ticks = g_array_new(FALSE, FALSE, sizeof(GwyAxisTick));
        g_array_set_clear_func(priv->ticks, clear_tick);
    }

    if (!priv->str)
        priv->str = g_string_new(NULL);
}

static void
rotate_pango_layout(GwyAxis *axis)
{
    gboolean horizontal = gwy_axis_get_horizontal_labels(axis);
    GtkPositionType edge = axis->priv->edge;
    PangoLayout *layout = axis->priv->layout;
    g_return_if_fail(layout);

    if (edge == GTK_POS_TOP || edge == GTK_POS_BOTTOM || horizontal)
        return;

    PangoMatrix matrix = PANGO_MATRIX_INIT;
    const PangoMatrix *cmatrix;
    PangoContext *context = pango_layout_get_context(layout);
    if ((cmatrix = pango_context_get_matrix(context)))
        matrix = *cmatrix;
    if (edge == GTK_POS_LEFT)
        pango_matrix_rotate(&matrix, -90.0);
    else
        pango_matrix_rotate(&matrix, 90.0);
    pango_context_set_matrix(context, &matrix);
    pango_layout_context_changed(layout);
}

static void
clear_tick(gpointer item)
{
    GwyAxisTick *tick = (GwyAxisTick*)item;
    GWY_FREE(tick->label);
}

static void
fix_request(GwyRange *request)
{
    // Refuse to show anything that is not a normal number.  Also handle
    // specially zero range around zero.
    if (!isfinite(request->from) || !isfinite(request->to)
        || (request->from == 0.0 && request->to == 0.0)) {
        *request = default_range;
        return;
    }

    // Refuse to go to almost infinity.
    if (fabs(request->from) > ALMOST_BLOODY_INFINITY)
        request->from = copysign(ALMOST_BLOODY_INFINITY, request->from);
    if (fabs(request->to) > ALMOST_BLOODY_INFINITY)
        request->to = copysign(ALMOST_BLOODY_INFINITY, request->to);

    // Refuse to work with just a few last bits of precision (and show more
    // than 12 digits).
    gdouble s = 5e-13*(fabs(request->from) + fabs(request->to));
    s = fmax(s, ALMOST_BLOODY_NOTHING);
    if (fabs(request->from - request->to) <= s) {
        gdouble m = 0.5*(request->from + request->to);
        if (request->to < request->from)
            s = -s;
        request->from = m - s;
        request->to = m + s;
    }
}

// FIXME: How to support logarithmic (or possibly other) axes?
static gdouble
estimate_major_distance(GwyAxis *axis,
                        const GwyRange *request)
{
    Axis *priv = axis->priv;
    if (!priv->show_labels)
        return fmin(MIN_MAJOR_DIST, priv->length);

    // FIXME: With perpendicular labels the label + units may be actually
    // two-line.  The label measurement probably needs to involve subclasses.
    gint width, height, w, h;
    format_value_label(axis, request->from, priv->show_units);
    pango_layout_get_size(priv->layout, &width, &height);

    format_value_label(axis, request->to, priv->show_units);
    pango_layout_get_size(priv->layout, &w, &h);
    width = MAX(width, w);
    height = MAX(height, h);

    gdouble size;
    if (priv->edge == GTK_POS_TOP || priv->edge == GTK_POS_BOTTOM)
        size = width;
    else
        size = (gwy_axis_get_horizontal_labels(axis) ? height : width);

    return fmax(size/pangoscale + 0.5*MIN_TICK_DIST, MIN_MAJOR_DIST);
}

static void
format_value_label(GwyAxis *axis,
                   gdouble value,
                   gboolean with_units)
{
    Axis *priv = axis->priv;
    GString *str = priv->str;
    g_string_assign(str, "<small>");
    if (with_units) {
        priv->units_at = value;
        g_string_append(str, gwy_value_format_print(priv->vf, value));
    }
    else
        g_string_append(str, gwy_value_format_print_number(priv->vf, value));
    g_string_append(str, "</small>");

    pango_layout_set_markup(priv->layout, str->str, str->len);
    if (!with_units || priv->split_width == G_MAXUINT)
        return;

    gint width, height;
    pango_layout_get_size(priv->layout, &width, &height);
    if ((guint)(width + PANGO_SCALE-1)/PANGO_SCALE <= priv->split_width)
        return;

    // Remove any leading whitespace from glue and put a newline there.
    gchar *oldglue = g_strdup(gwy_value_format_get_glue(priv->vf));
    gchar *glue = oldglue;
    for (gunichar u = g_utf8_get_char(glue);
         u && g_unichar_isspace(u);
         glue = g_utf8_next_char(glue), u = g_utf8_get_char(glue))
        ;
    glue = g_strconcat("\n", glue, NULL);
    gwy_value_format_set_glue(priv->vf, glue);

    g_string_assign(str, "<small>");
    g_string_append(str, gwy_value_format_print(priv->vf, value));
    g_string_append(str, "</small>");
    pango_layout_set_markup(priv->layout, str->str, str->len);

    gwy_value_format_set_glue(priv->vf, oldglue);
    g_free(glue);
    g_free(oldglue);
}

/**
 * gwy_axis_get_horizontal_labels:
 * @axis: An axis.
 *
 * Obtains the request of an axis subclass how labels on vertical axes should
 * be placed.
 *
 * Returns: %TRUE to display labels on vertical axes horizontally, %FALSE to
 *          display labels along axes.
 **/
gboolean
gwy_axis_get_horizontal_labels(const GwyAxis *axis)
{
    g_return_val_if_fail(GWY_IS_AXIS(axis), FALSE);
    GwyAxisClass *klass = GWY_AXIS_GET_CLASS(axis);
    if (klass->get_horizontal_labels)
        return klass->get_horizontal_labels(axis);
    return FALSE;
}

/**
 * gwy_axis_get_split_width:
 * @axis: An axis.
 *
 * Obtains the request of an axis subclass how labels with units should be
 * split.
 *
 * Returns: Width, in pixels, which causes the label to be split to two lines
 *          if exceeded.
 **/
guint
gwy_axis_get_split_width(const GwyAxis *axis)
{
    g_return_val_if_fail(GWY_IS_AXIS(axis), G_MAXUINT);
    GwyAxisClass *klass = GWY_AXIS_GET_CLASS(axis);
    if (klass->get_split_width)
        return klass->get_split_width(axis);
    return G_MAXUINT;
}

/**
 * gwy_axis_get_units_affinity:
 * @axis: An axis.
 * @primary: Location to fill with the primary units placement affinity.
 * @secondary: Location to fill with the primary units placement affinity.
 *
 * Obtains the request of an axis subclass where units are to be placed.
 **/
void
gwy_axis_get_units_affinity(const GwyAxis *axis,
                            GwyAxisUnitPlacement *primary,
                            GwyAxisUnitPlacement *secondary)
{
    g_return_if_fail(GWY_IS_AXIS(axis));
    GwyAxisUnitPlacement p = GWY_AXIS_UNITS_ZERO, s = GWY_AXIS_UNITS_FIRST;
    GwyAxisClass *klass = GWY_AXIS_GET_CLASS(axis);
    if (klass->get_units_affinity)
        klass->get_units_affinity(axis, &p, &s);
    GWY_MAYBE_SET(primary, p);
    GWY_MAYBE_SET(secondary, s);
}

/**
 * SECTION: axis
 * @section_id: GwyAxis
 * @title: GwyAxis
 * @short_description: Base class for widgets displaying axes
 **/

/**
 * GwyAxis:
 *
 * Abstraction of widgets displaying graph and data view axes.
 *
 * The #GwyAxis struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyAxisClass:
 * @get_horizontal_labels: If non-%NULL then return value of %TRUE means to
 *                         display horizontal labels even on vertical axes,
 *                         %FALSE to display labels along axes.  If %NULL,
 *                         %FALSE is assumed.
 * @get_split_width: If non-%NULL then labels with units widger than the
 *                   return value of this method will be split to two lines,
 *                   value and units.  If %NULL, value of %G_MAXUINT is
 *                   assumed which corresponds to no splitting.
 * @get_units_affinity: If non-%NULL then units are preferrably placed at
 *                      specified locations.  Some primary location types,
 *                      such as %GWY_AXIS_UNITS_NEVER, are definite and the
 *                      secondary affinity does not matter; it should be still
 *                      filled though, identical to the primary affinity.
 *                      If %NULL, the default placement is assumed which is
 *                      at zero and then, if zero is not possible, at the first
 *                      tick.
 *
 * Class of graphs and data view axes.
 **/

/**
 * GwyAxisTickLevel:
 * @GWY_AXIS_TICK_EDGE: Edge ticks that appear on the begining and end of
 *                      the axis if GwyAxis:ticks-at-edges it enabled.
 * @GWY_AXIS_TICK_MAJOR: Major ticks that can get labels.
 * @GWY_AXIS_TICK_MINOR: Minor ticks, always unlabelled.
 * @GWY_AXIS_TICK_MICRO: Micro ticks (second level of minor ticks), always
 *                       unlabelled.
 *
 * Level of axis ticks.
 **/

/**
 * GwyAxisUnitPlacement:
 * @GWY_AXIS_UNITS_NEVER: Do not show units at any ticks.  This is namely
 *                        useful if units are displayed elsewhere.
 * @GWY_AXIS_UNITS_FIRST: Show units at the first edge or major tick.
 * @GWY_AXIS_UNITS_LAST: Show units at the last edge or major tick.
 * @GWY_AXIS_UNITS_ZERO: If zero is present within the displayed range show
 *                       units there.
 *
 * Type of units placement affinity for axes.
 **/

/**
 * GwyAxisTick:
 * @level: Tick level from #GwyAxisTickLevel enum.
 * @value: Real value at the tick.
 * @position: Pixel position of the tick in the axis.
 * @label: Label as a Pango markup string; %NULL if the tick has no label.
 * @extents: Logical layout extents of @label in Pango units.  Does not contain
 *           any meaningful value if @label is %NULL.
 *
 * Representation of a single axis tick.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
