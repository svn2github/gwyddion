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
#include "libgwyui/axis.h"

#define IGNORE_ME N_("A translatable string.")

#define ALMOST_BLOODY_INFINITY (1e-3*G_MAXDOUBLE)
#define ALMOST_BLOODY_NOTHING (1e3*G_MINDOUBLE)

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
    PROP_SHOW_LABELS,
    PROP_SHOW_UNITS,
    PROP_DRAW_MINOR,
    N_PROPS,
};

typedef struct {
    gdouble value;
    gdouble position;
    gchar *label;
} GwyAxisTick;

struct _GwyAxisPrivate {
    GwyUnit *unit;
    gulong unit_changed_id;

    GwyRange request;
    GwyRange range;

    GtkPositionType edge;
    gboolean snap_to_ticks : 1;
    gboolean show_labels : 1;
    gboolean show_units : 1;
    gboolean draw_minor : 1;

    gboolean ticks_are_valid : 1;
    // TODO: Here we will keep the precalculated ticks and stuff.
    GwyValueFormat *vf;
    GArray *major_ticks;
    GArray *minor_ticks;
    GArray *micro_ticks;
    // Scratch space
    PangoLayout *layout;
    GString *str;
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
static void            gwy_axis_style_updated    (GtkWidget *widget);
static gboolean        set_snap_to_ticks         (GwyAxis *axis,
                                                  gboolean setting);
static gboolean        set_show_labels           (GwyAxis *axis,
                                                  gboolean setting);
static gboolean        set_show_units            (GwyAxis *axis,
                                                  gboolean setting);
static gboolean        set_draw_minor            (GwyAxis *axis,
                                                  gboolean setting);
static gboolean        set_edge                  (GwyAxis *axis,
                                                  GtkPositionType edge);
static gboolean        request_range             (GwyAxis *axis,
                                                  const GwyRange *range);
static void            unit_changed              (GwyAxis *axis,
                                                  GwyUnit *unit);
static void            invalidate_ticks          (GwyAxis *axis);
static void            position_set_style_classes(GtkWidget *widget,
                                                  GtkPositionType position);
static void            calculate_ticks           (GwyAxis *axis);
static gboolean        precision_is_sufficient   (GwyValueFormat *vf,
                                                  const GwyRange *range,
                                                  gdouble bs,
                                                  GString *str);
static void            snap_range_to_ticks       (GwyRange *range,
                                                  gdouble bs);
static GwyAxisStepType choose_step_type          (gdouble *step,
                                                  gdouble *base);
static void            ensure_layout_and_ticks   (GwyAxis *axis);
static void            clear_tick                (gpointer item);
static gdouble         get_pixel_length          (const GwyAxis *axis);
static gdouble         estimate_major_distance   (GwyAxis *axis,
                                                  const GwyRange *request,
                                                  gdouble length);
static void            fix_request               (GwyRange *request);

static GParamSpec *properties[N_PROPS];

static const GwyRange default_range = { -1.0, 1.0 };

static const gdouble step_sizes[GWY_AXIS_STEP_NSTEPS] = {
    0.0, 1.0, 2.0, 2.5, 5.0,
};

G_DEFINE_ABSTRACT_TYPE(GwyAxis, gwy_axis, GTK_TYPE_WIDGET);

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

    widget_class->style_updated = gwy_axis_style_updated;

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
                               TRUE,
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

    properties[PROP_DRAW_MINOR]
        = g_param_spec_boolean("draw-minor",
                               "Draw-minor",
                               "Whether minor ticks should be drawn.",
                               TRUE,
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
    priv->snap_to_ticks = TRUE;
    priv->show_labels = TRUE;
    priv->show_units = TRUE;
    priv->draw_minor = TRUE;
    priv->edge = GTK_POS_BOTTOM;
    priv->range = priv->request = default_range;
}

static void
gwy_axis_finalize(GObject *object)
{
    Axis *priv = GWY_AXIS(object)->priv;
    g_signal_handler_disconnect(priv->unit, priv->unit_changed_id);
    g_object_unref(priv->unit);
    GWY_STRING_FREE(priv->str);
    GWY_ARRAY_FREE(priv->major_ticks);
    GWY_ARRAY_FREE(priv->minor_ticks);
    GWY_ARRAY_FREE(priv->micro_ticks);
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

        case PROP_RANGE_REQUEST:
        request_range(axis, (const GwyRange*)g_value_get_boxed(value));
        break;

        case PROP_SNAP_TO_TICKS:
        set_snap_to_ticks(axis, g_value_get_boolean(value));
        break;

        case PROP_SHOW_LABELS:
        set_show_labels(axis, g_value_get_boolean(value));
        break;

        case PROP_SHOW_UNITS:
        set_show_units(axis, g_value_get_boolean(value));
        break;

        case PROP_DRAW_MINOR:
        set_draw_minor(axis, g_value_get_boolean(value));
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

        case PROP_RANGE_REQUEST:
        g_value_set_boxed(value, &priv->request);
        break;

        case PROP_RANGE:
        g_value_set_boxed(value, &priv->range);
        break;

        case PROP_SNAP_TO_TICKS:
        g_value_set_boolean(value, priv->snap_to_ticks);
        break;

        case PROP_SHOW_LABELS:
        g_value_set_boolean(value, priv->show_labels);
        break;

        case PROP_SHOW_UNITS:
        g_value_set_boolean(value, priv->show_units);
        break;

        case PROP_DRAW_MINOR:
        g_value_set_boolean(value, priv->draw_minor);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_axis_style_updated(GtkWidget *widget)
{
    Axis *priv = GWY_AXIS(widget)->priv;
    if (priv->layout)
        pango_layout_context_changed(priv->layout);
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
 * @range: Requested range for the axis.
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
    setting = !!setting;
    if (setting == priv->snap_to_ticks)
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
    setting = !!setting;
    if (setting == priv->show_labels)
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
    setting = !!setting;
    if (setting == priv->show_units)
        return FALSE;

    priv->show_units = setting;
    invalidate_ticks(axis);
    return TRUE;
}

static gboolean
set_draw_minor(GwyAxis *axis,
               gboolean setting)
{
    Axis *priv = axis->priv;
    setting = !!setting;
    if (setting == priv->draw_minor)
        return FALSE;

    priv->draw_minor = setting;
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
    // perpendicular.
    GWY_OBJECT_UNREF(priv->vf);
    priv->vf = gwy_unit_format_with_resolution
                               (priv->unit, GWY_VALUE_FORMAT_PANGO,
                                fmax(fabs(request.from), fabs(request.to)),
                                fabs(request.to - request.from)/12.0);

    gboolean descending = (request.to < request.from);
    guint length = get_pixel_length(axis);
    gdouble majdist = estimate_major_distance(axis, &request, length);
    g_printerr("request [%.13g,%.13g]\n", priv->request.from, priv->request.to);
    g_printerr("fixed to [%.13g,%.13g]\n", request.from, request.to);
    g_printerr("descending: %d\n", descending);
    g_printerr("pixel length: %u\n", length);
    g_printerr("majdist estimate: %g\n", majdist);

    ensure_layout_and_ticks(axis);
    if (majdist >= length) {
        g_warning("Cannot fit any major ticks.  Implement some fallback.");
        priv->range = request;
        priv->ticks_are_valid = TRUE;
        return;
    }

    State state = FIRST_TRY;
    GwyAxisStepType steptype = GWY_AXIS_STEP_0;
    gdouble base, step;
    do {
        g_printerr("ITERATION with state %u\n", state);
        priv->range = request;
        if (state != LESS_TICKS) {
            majdist = estimate_major_distance(axis, &request, length);
            // TODO: Update majdist here?  Or at the end of the cycle?
            // Take step as positive here to simplify the conditionals.
            // Use @descending where direction is necessary.
            step = fabs(priv->range.to - priv->range.from)/(length/majdist);
            steptype = choose_step_type(&step, &base);
            g_printerr("base=%g, step=%g (steptype %u)\n",
                       base, step, steptype);
        }

        gdouble bs = descending ? -base*step : base*step;
        if (priv->snap_to_ticks) {
            snap_range_to_ticks(&priv->range, bs);
            g_printerr("snapped to [%.13g,%.13g]\n", request.from, request.to);
        }

        guint precision = gwy_value_format_get_precision(priv->vf);
        step = step_sizes[steptype];
        if (precision_is_sufficient(priv->vf, &priv->range, bs, priv->str)) {
            g_printerr("precision %u is sufficient\n", precision);
            if (state == FIRST_TRY && precision > 0)
                gwy_value_format_set_precision(priv->vf, precision-1);
            else
                state = FINALLY_OK;
        }
        else {
            g_printerr("precision %u is insufficient\n", precision);
            if (state == FIRST_TRY) {
                state = ADD_DIGITS;
                gwy_value_format_set_precision(priv->vf, precision+1);
                g_printerr("increasing precision to %u\n", precision+1);
            }
            else {
                state = LESS_TICKS;
                base *= 10;
                steptype = GWY_AXIS_STEP_1;
                g_printerr("removing ticks\n");
            }
        }
    } while (state != FINALLY_OK);

    // TODO: Now we need to actually create the ticks and labels.

    priv->ticks_are_valid = TRUE;
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
        gdouble value = from + i*bs;
        // Fix negative zeroes.
        if (G_UNLIKELY(fabs(value) < 1e-12*fabs(bs)))
            value = 0.0;
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
    *base = gwy_powi(10.0, (gint)floor(log10(*step) + 1e-12));
    *step /= *base;

    while (*step <= 0.5) {
        *base /= 10.0;
        *step += 10.0;
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

static void
ensure_layout_and_ticks(GwyAxis *axis)
{
    Axis *priv = axis->priv;

    if (!priv->layout) {
        priv->layout = gtk_widget_create_pango_layout(GTK_WIDGET(axis), NULL);
        pango_layout_set_alignment(priv->layout, PANGO_ALIGN_LEFT);

        PangoAttrList *attrlist = pango_attr_list_new();
        PangoAttribute *attr = pango_attr_scale_new(PANGO_SCALE_SMALL);
        pango_attr_list_insert(attrlist, attr);
        pango_layout_set_attributes(priv->layout, attrlist);
        pango_attr_list_unref(attrlist);
    }

    if (!priv->major_ticks) {
        priv->major_ticks = g_array_new(FALSE, FALSE, sizeof(GwyAxisTick));
        g_array_set_clear_func(priv->major_ticks, clear_tick);
    }
    else
        g_array_set_size(priv->major_ticks, 0);

    if (!priv->minor_ticks) {
        priv->minor_ticks = g_array_new(FALSE, FALSE, sizeof(GwyAxisTick));
        g_array_set_clear_func(priv->minor_ticks, clear_tick);
    }
    else
        g_array_set_size(priv->minor_ticks, 0);

    if (!priv->micro_ticks) {
        priv->micro_ticks = g_array_new(FALSE, FALSE, sizeof(GwyAxisTick));
        g_array_set_clear_func(priv->micro_ticks, clear_tick);
    }
    else
        g_array_set_size(priv->micro_ticks, 0);

    if (!priv->str)
        priv->str = g_string_new(NULL);
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
    if (!isnormal(request->from) || !isnormal(request->to)
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

static gdouble
get_pixel_length(const GwyAxis *axis)
{
    Axis *priv = axis->priv;
    GtkPositionType edge = priv->edge;
    GtkAllocation alloc;

    gtk_widget_get_allocation(GTK_WIDGET(axis), &alloc);
    if (edge == GTK_POS_TOP || edge == GTK_POS_BOTTOM)
        return alloc.width;
    if (edge == GTK_POS_LEFT || edge == GTK_POS_RIGHT)
        return alloc.height;

    g_return_val_if_reached(0);
}

// FIXME: How to support logarithmic (or possibly other) axes?
static gdouble
estimate_major_distance(GwyAxis *axis,
                        const GwyRange *request,
                        gdouble length)
{
    const gdouble min_dist = 50.0;

    Axis *priv = axis->priv;

    if (!priv->show_labels)
        return fmin(min_dist, length);

    const gchar *label;
    // FIXME: With perpendicular labels the label + units may be actually
    // two-line.  The label measurement probably needs to involve subclasses.
    // TODO: Must measure at least values at both ends.
    if (priv->show_units)
        label = gwy_value_format_print(priv->vf, request->to);
    else
        label = gwy_value_format_print_number(priv->vf, request->to);

    int width, height;
    pango_layout_set_markup(priv->layout, label, -1);
    pango_layout_get_size(priv->layout, &width, &height);

    gint dimen = (GWY_AXIS_GET_CLASS(axis)->perpendicular_labels
                  ? height
                  : width);
    return fmax((gdouble)dimen/PANGO_SCALE, min_dist);
}

/**
 * SECTION: axis
 * @section_id: GwyAxis
 * @title: Axis
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
 *
 * Class of graphs and data view axes.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
