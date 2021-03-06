/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
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

/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

/*
 * Modified by Yeti 2003-2004.  In fact, rewritten, now includes a single
 * tick'n'label design function, and Gwy[HV]Rulers only do the actual
 * drawing at computed positions.  GtkMetric was removed, replaced with
 * a scale-free solution and later support for GwySIUnit was added.
 */

#include <string.h>
#include <libgwyddion/gwyddion.h>
#include "gwyruler.h"

typedef enum {
    GWY_SCALE_0,
    GWY_SCALE_1,
    GWY_SCALE_2,
    GWY_SCALE_2_5,
    GWY_SCALE_5,
    GWY_SCALE_LAST
} GwyScaleScale;

static const gdouble steps[GWY_SCALE_LAST] = {
    0.0, 1.0, 2.0, 2.5, 5.0,
};

enum {
    PROP_0,
    PROP_LOWER,
    PROP_UPPER,
    PROP_POSITION,
    PROP_MAX_SIZE,
    PROP_UNITS_PLACEMENT,
};

static void          gwy_ruler_class_init    (GwyRulerClass  *klass);
static void          gwy_ruler_init          (GwyRuler       *ruler);
static void          gwy_ruler_realize       (GtkWidget      *widget);
static void          gwy_ruler_unrealize     (GtkWidget      *widget);
static void          gwy_ruler_size_allocate (GtkWidget      *widget,
                                              GtkAllocation  *allocation);
static gint          gwy_ruler_expose        (GtkWidget      *widget,
                                              GdkEventExpose *event);
static void          gwy_ruler_make_pixmap   (GwyRuler       *ruler);
static void          gwy_ruler_set_property  (GObject        *object,
                                              guint           prop_id,
                                              const GValue   *value,
                                              GParamSpec     *pspec);
static void          gwy_ruler_get_property  (GObject        *object,
                                              guint           prop_id,
                                              GValue         *value,
                                              GParamSpec     *pspec);
static gdouble       compute_magnitude       (gdouble max);
static gdouble       compute_base            (gdouble max,
                                              gdouble basebase);
static GwyScaleScale next_scale              (GwyScaleScale scale,
                                              gdouble *base,
                                              gdouble measure,
                                              gint min_incr);

static GtkWidgetClass *parent_class;


GType
gwy_ruler_get_type(void)
{
    static GType ruler_type = 0;

    if (!ruler_type) {
        static const GTypeInfo ruler_info = {
            sizeof(GwyRulerClass),
            NULL,           /* base_init */
            NULL,           /* base_finalize */
            (GClassInitFunc)gwy_ruler_class_init,
            NULL,           /* class_finalize */
            NULL,           /* class_data */
            sizeof(GwyRuler),
            0,              /* n_preallocs */
            (GInstanceInitFunc)gwy_ruler_init,
            NULL,
        };

        ruler_type = g_type_register_static(GTK_TYPE_WIDGET, "GwyRuler",
                                            &ruler_info, 0);
    }

    return ruler_type;
}

static void
gwy_ruler_class_init(GwyRulerClass *class)
{
    GObjectClass   *gobject_class;
    GtkWidgetClass *widget_class;

    gobject_class = G_OBJECT_CLASS(class);
    widget_class = (GtkWidgetClass*)class;

    parent_class = g_type_class_peek_parent(class);

    gobject_class->set_property = gwy_ruler_set_property;
    gobject_class->get_property = gwy_ruler_get_property;

    widget_class->realize = gwy_ruler_realize;
    widget_class->unrealize = gwy_ruler_unrealize;
    widget_class->size_allocate = gwy_ruler_size_allocate;
    widget_class->expose_event = gwy_ruler_expose;

    class->draw_ticks = NULL;
    class->draw_pos = NULL;

    g_object_class_install_property(gobject_class,
                                    PROP_LOWER,
                                    g_param_spec_double("lower",
                                                        _("Lower"),
                                                        _("Lower limit of ruler"),
                                                        -G_MAXDOUBLE,
                                                        G_MAXDOUBLE,
                                                        0.0,
                                                        G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class,
                                    PROP_UPPER,
                                    g_param_spec_double("upper",
                                                        _("Upper"),
                                                        _("Upper limit of ruler"),
                                                        -G_MAXDOUBLE,
                                                        G_MAXDOUBLE,
                                                        0.0,
                                                        G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class,
                                    PROP_POSITION,
                                    g_param_spec_double("position",
                                                        _("Position"),
                                                        _("Position of mark on the ruler"),
                                                        -G_MAXDOUBLE,
                                                        G_MAXDOUBLE,
                                                        0.0,
                                                        G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class,
                                    PROP_MAX_SIZE,
                                    g_param_spec_double("max_size",
                                                        _("Max Size"),
                                                        _("Maximum size of the ruler"),
                                                        -G_MAXDOUBLE,
                                                        G_MAXDOUBLE,
                                                        0.0,
                                                        G_PARAM_READWRITE));

    /* FIXME: in fact it's an enum... */
    g_object_class_install_property(gobject_class,
                                    PROP_UNITS_PLACEMENT,
                                    g_param_spec_uint("units_placement",
                                                      _("Units Placement"),
                                                      _("The placement of units on the ruler, if any"),
                                                      0,
                                                      GWY_UNITS_PLACEMENT_AT_ZERO,
                                                      GWY_UNITS_PLACEMENT_NONE,
                                                      G_PARAM_READWRITE));
}

static void
gwy_ruler_init(GwyRuler *ruler)
{
    ruler->backing_store = NULL;
    ruler->non_gr_exp_gc = NULL;
    ruler->xsrc = 0;
    ruler->ysrc = 0;
    ruler->slider_size = 0;
    ruler->lower = 0;
    ruler->upper = 0;
    ruler->position = 0;
    ruler->max_size = 0;
    ruler->units_placement = GWY_UNITS_PLACEMENT_NONE;
    ruler->units = GWY_SI_UNIT(gwy_si_unit_new("m"));
}

static void
gwy_ruler_set_property(GObject      *object,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
    GwyRuler *ruler = GWY_RULER(object);

    switch (prop_id) {
        case PROP_LOWER:
        gwy_ruler_set_range(ruler, g_value_get_double(value), ruler->upper,
                            ruler->position, ruler->max_size);
        break;

        case PROP_UPPER:
        gwy_ruler_set_range(ruler, ruler->lower, g_value_get_double(value),
                            ruler->position, ruler->max_size);
        break;

        case PROP_POSITION:
        gwy_ruler_set_range(ruler, ruler->lower, ruler->upper,
                            g_value_get_double(value), ruler->max_size);
        break;

        case PROP_MAX_SIZE:
        gwy_ruler_set_range(ruler, ruler->lower, ruler->upper,
                            ruler->position,  g_value_get_double(value));
        break;

        case PROP_UNITS_PLACEMENT:
        gwy_ruler_set_units_placement(ruler,
                                      (GwyUnitsPlacement)g_value_get_uint(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_ruler_get_property(GObject      *object,
                       guint         prop_id,
                       GValue       *value,
                       GParamSpec   *pspec)
{
    GwyRuler *ruler = GWY_RULER(object);

    switch (prop_id) {
        case PROP_LOWER:
        g_value_set_double(value, ruler->lower);
        break;

        case PROP_UPPER:
        g_value_set_double(value, ruler->upper);
        break;

        case PROP_POSITION:
        g_value_set_double(value, ruler->position);
        break;

        case PROP_MAX_SIZE:
        g_value_set_double(value, ruler->max_size);
        break;

        case PROP_UNITS_PLACEMENT:
        g_value_set_uint(value, (guint)ruler->units_placement);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

/**
 * gwy_ruler_set_range:
 * @ruler: A #GwyRuler
 * @lower: Lower limit of the ruler.
 * @upper: Upper limit of the ruler.
 * @position: Current position of the mark on the ruler.
 * @max_size: Maximum value used for calculating size of text labels.
 *
 * Sets range and current value of a ruler.
 **/
void
gwy_ruler_set_range(GwyRuler *ruler,
                    gdouble   lower,
                    gdouble   upper,
                    gdouble   position,
                    gdouble   max_size)
{
    g_return_if_fail(GWY_IS_RULER(ruler));

    g_object_freeze_notify(G_OBJECT(ruler));
    if (ruler->lower != lower) {
        ruler->lower = lower;
        g_object_notify(G_OBJECT(ruler), "lower");
    }
    if (ruler->upper != upper) {
        ruler->upper = upper;
        g_object_notify(G_OBJECT(ruler), "upper");
    }
    if (ruler->position != position) {
        ruler->position = position;
        g_object_notify(G_OBJECT(ruler), "position");
    }
    if (ruler->max_size != max_size) {
        ruler->max_size = max_size;
        g_object_notify(G_OBJECT(ruler), "max_size");
    }
    g_object_thaw_notify(G_OBJECT(ruler));

    if (GTK_WIDGET_DRAWABLE(ruler))
        gtk_widget_queue_draw(GTK_WIDGET(ruler));
}

/**
 * gwy_ruler_get_range:
 * @ruler: A #GwyRuler
 * @lower: Location to store lower limit of the ruler, or %NULL
 * @upper: Location to store upper limit of the ruler, or %NULL
 * @position: Location to store the current position of the mark on the ruler,
 *            or %NULL
 * @max_size: Location to store the maximum size of the ruler used when
 *            calculating the space to leave for the text, or %NULL.
 *
 * Retrieves values indicating the range and current position of a #GwyRuler.
 * See gwy_ruler_set_range().
 **/
void
gwy_ruler_get_range(GwyRuler *ruler,
                    gdouble  *lower,
                    gdouble  *upper,
                    gdouble  *position,
                    gdouble  *max_size)
{
    g_return_if_fail(GWY_IS_RULER(ruler));

    if (lower)
        *lower = ruler->lower;
    if (upper)
        *upper = ruler->upper;
    if (position)
        *position = ruler->position;
    if (max_size)
        *max_size = ruler->max_size;
}

/**
 * gwy_ruler_set_units_placement:
 * @ruler: A #GwyRuler
 * @placement: Units placement specification.
 *
 * Sets whether and where units should be placed on the ruler.
 **/
void
gwy_ruler_set_units_placement(GwyRuler *ruler,
                              GwyUnitsPlacement placement)
{
    g_return_if_fail(GWY_IS_RULER(ruler));
    placement = MIN(placement, GWY_UNITS_PLACEMENT_AT_ZERO);
    if (ruler->units_placement == placement)
        return;

    g_object_freeze_notify(G_OBJECT(ruler));
    ruler->units_placement = placement;
    g_object_notify(G_OBJECT(ruler), "units_placement");
    g_object_thaw_notify(G_OBJECT(ruler));

    if (GTK_WIDGET_DRAWABLE(ruler))
        gtk_widget_queue_draw(GTK_WIDGET(ruler));
}

/**
 * gwy_ruler_get_units_placement:
 * @ruler: A #GwyRuler
 *
 * Gets current units placement of ruler @ruler.
 *
 * Returns: The units placement.
 **/
GwyUnitsPlacement
gwy_ruler_get_units_placement(GwyRuler *ruler)
{
    g_return_val_if_fail(GWY_IS_RULER(ruler), GWY_UNITS_PLACEMENT_NONE);
    return ruler->units_placement;
}

void
gwy_ruler_draw_ticks(GwyRuler *ruler)
{
    g_return_if_fail(GWY_IS_RULER(ruler));

    if (GWY_RULER_GET_CLASS(ruler)->draw_ticks)
        GWY_RULER_GET_CLASS(ruler)->draw_ticks(ruler);
}

void
gwy_ruler_draw_pos(GwyRuler *ruler)
{
    g_return_if_fail(GWY_IS_RULER(ruler));

    if (GWY_RULER_GET_CLASS(ruler)->draw_pos)
        GWY_RULER_GET_CLASS(ruler)->draw_pos(ruler);
}


static void
gwy_ruler_realize(GtkWidget *widget)
{
    GwyRuler *ruler;
    GdkWindowAttr attributes;
    gint attributes_mask;

    ruler = GWY_RULER(widget);
    GTK_WIDGET_SET_FLAGS(ruler, GTK_REALIZED);

    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.x = widget->allocation.x;
    attributes.y = widget->allocation.y;
    attributes.width = widget->allocation.width;
    attributes.height = widget->allocation.height;
    attributes.wclass = GDK_INPUT_OUTPUT;
    attributes.visual = gtk_widget_get_visual(widget);
    attributes.colormap = gtk_widget_get_colormap(widget);
    attributes.event_mask = gtk_widget_get_events(widget);
    attributes.event_mask |= (GDK_EXPOSURE_MASK |
                              GDK_POINTER_MOTION_MASK |
                              GDK_POINTER_MOTION_HINT_MASK);

    attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

    widget->window = gdk_window_new(gtk_widget_get_parent_window(widget), &attributes, attributes_mask);
    gdk_window_set_user_data(widget->window, ruler);

    widget->style = gtk_style_attach(widget->style, widget->window);
    gtk_style_set_background(widget->style, widget->window, GTK_STATE_ACTIVE);

    gwy_ruler_make_pixmap(ruler);
}

static void
gwy_ruler_unrealize(GtkWidget *widget)
{
    GwyRuler *ruler = GWY_RULER(widget);

    gwy_object_unref(ruler->backing_store);
    gwy_object_unref(ruler->non_gr_exp_gc);
    gwy_object_unref(ruler->units);

    if (GTK_WIDGET_CLASS(parent_class)->unrealize)
        (GTK_WIDGET_CLASS(parent_class)->unrealize)(widget);
}

static void
gwy_ruler_size_allocate(GtkWidget     *widget,
                        GtkAllocation *allocation)
{
    GwyRuler *ruler = GWY_RULER(widget);

    widget->allocation = *allocation;

    if (GTK_WIDGET_REALIZED(widget)) {
        gdk_window_move_resize(widget->window,
                               allocation->x, allocation->y,
                               allocation->width, allocation->height);

        gwy_ruler_make_pixmap(ruler);
    }
}

static gint
gwy_ruler_expose(GtkWidget *widget,
                 G_GNUC_UNUSED GdkEventExpose *event)
{
    GwyRuler *ruler;

    if (GTK_WIDGET_DRAWABLE(widget)) {
        ruler = GWY_RULER(widget);

        gwy_ruler_draw_ticks(ruler);

        gdk_draw_drawable(widget->window,
                          ruler->non_gr_exp_gc,
                          ruler->backing_store,
                          0, 0, 0, 0,
                          widget->allocation.width,
                          widget->allocation.height);

        gwy_ruler_draw_pos(ruler);
    }

    return FALSE;
}

static void
gwy_ruler_make_pixmap(GwyRuler *ruler)
{
    GtkWidget *widget;
    gint width;
    gint height;

    widget = GTK_WIDGET(ruler);

    if (ruler->backing_store) {
        gdk_drawable_get_size(ruler->backing_store, &width, &height);
        if ((width == widget->allocation.width) &&
            (height == widget->allocation.height))
            return;

        g_object_unref(ruler->backing_store);
    }

    ruler->backing_store = gdk_pixmap_new(widget->window,
                                          widget->allocation.width,
                                          widget->allocation.height,
                                          -1);

    ruler->xsrc = 0;
    ruler->ysrc = 0;

    if (!ruler->non_gr_exp_gc) {
        ruler->non_gr_exp_gc = gdk_gc_new(widget->window);
        gdk_gc_set_exposures(ruler->non_gr_exp_gc, FALSE);
    }
}

void
_gwy_ruler_real_draw_ticks(GwyRuler *ruler,
                           gint pixelsize,
                           gint min_label_spacing,
                           gint min_tick_spacing,
                           void (*label_callback)(GwyRuler *ruler,
                                                  gint position,
                                                  const gchar *label,
                                                  PangoLayout *layout,
                                                  gint digit_height,
                                                  gint digit_offset),
                           void (*tick_callback)(GwyRuler *ruler,
                                                 gint position,
                                                 gint depth))
{
    gdouble lower, upper, max;
    gint text_size, labels, i, scale_depth;
    gdouble range, measure, base, step, first;
    GwyScaleScale scale;
    GwySIValueFormat *format;
    PangoLayout *layout;
    PangoRectangle ink_rect;
    gchar *unit_str;
    gboolean units_drawn;
    GtkWidget *widget;
    gint digit_height, digit_offset;
    struct { GwyScaleScale scale; double base; } tick_info[4];

    widget = GTK_WIDGET(ruler);

    layout = gtk_widget_create_pango_layout(widget, "012456789");
    pango_layout_get_extents(layout, &ink_rect, NULL);

    digit_height = PANGO_PIXELS(ink_rect.height) + 1;
    digit_offset = ink_rect.y;

    upper = ruler->upper;
    lower = ruler->lower;
    if (upper <= lower || pixelsize < 2 || pixelsize > 10000)
        return;
    max = ruler->max_size;
    if (max == 0)
        max = MAX(fabs(lower), fabs(upper));

    range = upper - lower;
    format = gwy_si_unit_get_format_with_resolution(ruler->units,
                                                    max, max/12, NULL);
    measure = range/format->magnitude / pixelsize;
    max /= format->magnitude;

    switch (ruler->units_placement && ruler->units) {
        case GWY_UNITS_PLACEMENT_AT_ZERO:
        unit_str
            = g_strdup_printf("%d %s",
                              (lower > 0) ? (gint)(lower/format->magnitude) : 0,
                              format->units);
        break;

        default:
        unit_str = g_strdup_printf("%d", (gint)max);
        break;
    }
    if (strchr(unit_str, '<')) {
        GError *err = NULL;
        PangoAttrList *attr_list = NULL;
        gchar *text = NULL;
        gboolean ok;

        ok = pango_parse_markup(unit_str, -1, 0, &attr_list, &text, NULL, &err);
        if (!ok) {
            g_warning("Cannot parse unit string: %s", err->message);
            text_size = digit_height + 1;
        }
        else {
            /* FIXME: this is very aproximate */
            text_size = g_utf8_strlen(text, -1)*digit_height + 1;
            pango_attr_list_unref(attr_list);
            g_free(text);
        }
        g_clear_error(&err);
    }
    else
        text_size = g_utf8_strlen(unit_str, -1)*digit_height + 1;
    g_free(unit_str);

    /* fit as many labels as you can */
    labels = floor(pixelsize/(text_size + min_label_spacing));
    if (labels > 5)    /* but at five slow down */
        labels = 5 + (labels - 5)/2;
    if (labels == 0)    /* at least one */
        labels = 1;

    step = range/format->magnitude / labels;
    base = compute_base(step, 10);
    step /= base;
    if (step >= 5.0 || base < 1.0) {
        scale = GWY_SCALE_1;
        base *= 10;
    }
    else if (step >= 2.5)
        scale = GWY_SCALE_5;
    else if (step >= 2.0)
        scale = GWY_SCALE_2_5;
    else
        scale = GWY_SCALE_2;
    step = steps[scale];

    /* draw labels */
    units_drawn = FALSE;
    first = floor(lower/format->magnitude / (base*step))*base*step;
    for (i = 0; ; i++) {
        gint pos;
        gdouble val;

        val = i*step*base + first;
        pos = floor((val - lower/format->magnitude)/measure);
        if (pos >= pixelsize)
            break;
        if (pos < 0)
            continue;
        if (!units_drawn
            && (upper < 0 || val >= 0)
            && ruler->units_placement == GWY_UNITS_PLACEMENT_AT_ZERO
            && ruler->units) {
            unit_str = g_strdup_printf("%d %s", ROUND(val), format->units);
            units_drawn = TRUE;
        }
        else
            unit_str = g_strdup_printf("%d", ROUND(val));
        label_callback(ruler, pos, unit_str, layout,
                       digit_height, digit_offset);
        g_free(unit_str);
    }

    /* draw tick marks, from smallest to largest */
    scale_depth = 0;
    while (scale && scale_depth < (gint)G_N_ELEMENTS(tick_info)) {
        tick_info[scale_depth].scale = scale;
        tick_info[scale_depth].base = base;
        scale = next_scale(scale, &base, measure, min_tick_spacing);
        scale_depth++;
    }
    scale_depth--;

    while (scale_depth > -1) {
        scale = tick_info[scale_depth].scale;
        base = tick_info[scale_depth].base;
        step = steps[scale];
        first = floor(lower/format->magnitude / (base*step))*base*step;
        for (i = 0; ; i++) {
            gint pos;
            gdouble val;

            val = (i + 0.000001)*step*base + first;
            pos = floor((val - lower/format->magnitude)/measure);
            if (pos >= pixelsize)
                break;
            if (pos < 0)
                continue;
            tick_callback(ruler, pos, scale_depth);
        }
        scale_depth--;
    }

    gwy_si_unit_value_format_free(format);
    g_object_unref(layout);
}

static gdouble
compute_base(gdouble max, gdouble basebase)
{
    gint i;
    gdouble base;

    i = floor(log(max)/log(basebase));
    base = 1.0;
    if (i > 0)
        while (i--)
            base *= basebase;
    else
        while (i++)
            base /= basebase;
    return base;
}

static GwyScaleScale
next_scale(GwyScaleScale scale,
           gdouble *base,
           gdouble measure,
           gint min_incr)
{
    GwyScaleScale new_scale = GWY_SCALE_0;

    switch (scale) {
        case GWY_SCALE_1:
        *base /= 10.0;
        if ((gint)floor(*base*2.0/measure) > min_incr)
            new_scale = GWY_SCALE_5;
        else if ((gint)floor(*base*2.5/measure) > min_incr)
            new_scale = GWY_SCALE_2_5;
        else if ((gint)floor(*base*5.0/measure) > min_incr)
            new_scale = GWY_SCALE_5;
        break;

        case GWY_SCALE_2:
        if ((gint)floor(*base/measure) > min_incr)
            new_scale = GWY_SCALE_1;
        break;

        case GWY_SCALE_2_5:
        *base /= 10.0;
        if ((gint)floor(*base*5.0/measure) > min_incr)
            new_scale = GWY_SCALE_5;
        break;

        case GWY_SCALE_5:
        if ((gint)floor(*base/measure) > min_incr)
            new_scale = GWY_SCALE_1;
        else if ((gint)floor(*base*2.5/measure) > min_incr)
            new_scale = GWY_SCALE_2_5;
        break;

        default:
        g_assert_not_reached();
        break;
    }

    return new_scale;
}

/**
 * gwy_ruler_set_units:
 * @ruler: A #GwyRuler.
 * @units: The base units this ruler should display.
 *
 * Sets the base units to display to @units.
 *
 * Setting units to %NULL effectively disables them.
 **/
void
gwy_ruler_set_units(GwyRuler *ruler,
                    GwySIUnit *units)
{
    g_return_if_fail(GWY_IS_RULER(ruler));
    gwy_object_unref(ruler->units);
    if (units)
        g_object_ref(units);
    ruler->units = units;
    gtk_widget_queue_draw(GTK_WIDGET(ruler));
}

/**
 * gwy_ruler_get_units:
 * @ruler: A #GwyRuler.
 *
 * Returns the base units @ruler uses.
 *
 * Returns: The units the rules uses.
 **/
GwySIUnit*
gwy_ruler_get_units(GwyRuler *ruler)
{
    g_return_val_if_fail(GWY_IS_RULER(ruler), NULL);
    return ruler->units;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
