/*
 *  $Id$
 *  Copyright (C) 2011 David Necas (Yeti).
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

#include <stdlib.h>
#include <glib/gi18n-lib.h>
#include "libgwy/macros.h"
#include "libgwy/math.h"
#include "libgwy/serialize.h"
#include "libgwy/field-arithmetic.h"
#include "libgwy/surface.h"
#include "libgwy/surface-statistics.h"
#include "libgwy/field-internal.h"
#include "libgwy/object-internal.h"
#include "libgwy/surface-internal.h"

enum { N_ITEMS = 3 };

enum {
    DATA_CHANGED,
    N_SIGNALS
};

enum {
    PROP_0,
    PROP_N_POINTS,
    PROP_UNIT_XY,
    PROP_UNIT_Z,
    N_PROPS
};

static void     gwy_surface_finalize         (GObject *object);
static void     gwy_surface_dispose          (GObject *object);
static void     gwy_surface_serializable_init(GwySerializableInterface *iface);
static gsize    gwy_surface_n_items          (GwySerializable *serializable);
static gsize    gwy_surface_itemize          (GwySerializable *serializable,
                                            GwySerializableItems *items);
static gboolean gwy_surface_construct        (GwySerializable *serializable,
                                            GwySerializableItems *items,
                                            GwyErrorList **error_list);
static GObject* gwy_surface_duplicate_impl   (GwySerializable *serializable);
static void     gwy_surface_assign_impl      (GwySerializable *destination,
                                            GwySerializable *source);
static void     gwy_surface_set_property     (GObject *object,
                                            guint prop_id,
                                            const GValue *value,
                                            GParamSpec *pspec);
static void     gwy_surface_get_property     (GObject *object,
                                            guint prop_id,
                                            GValue *value,
                                            GParamSpec *pspec);

static const GwySerializableItem serialize_items[N_ITEMS] = {
    /*0*/ { .name = "unit-xy", .ctype = GWY_SERIALIZABLE_OBJECT,       },
    /*1*/ { .name = "unit-z",  .ctype = GWY_SERIALIZABLE_OBJECT,       },
    /*2*/ { .name = "data",    .ctype = GWY_SERIALIZABLE_DOUBLE_ARRAY, },
};

static guint surface_signals[N_SIGNALS];
static GParamSpec *surface_pspecs[N_PROPS];

G_DEFINE_TYPE_EXTENDED
    (GwySurface, gwy_surface, G_TYPE_OBJECT, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_surface_serializable_init))

static void
gwy_surface_serializable_init(GwySerializableInterface *iface)
{
    iface->n_items   = gwy_surface_n_items;
    iface->itemize   = gwy_surface_itemize;
    iface->construct = gwy_surface_construct;
    iface->duplicate = gwy_surface_duplicate_impl;
    iface->assign    = gwy_surface_assign_impl;
}

static void
gwy_surface_class_init(GwySurfaceClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(Surface));

    gobject_class->dispose = gwy_surface_dispose;
    gobject_class->finalize = gwy_surface_finalize;
    gobject_class->get_property = gwy_surface_get_property;
    gobject_class->set_property = gwy_surface_set_property;

    surface_pspecs[PROP_N_POINTS]
        = g_param_spec_uint("n-points",
                            "N points",
                            "Number of surface points.",
                            0, G_MAXUINT, 0,
                            G_PARAM_READABLE | STATICP);

    surface_pspecs[PROP_UNIT_XY]
        = g_param_spec_object("unit-xy",
                              "XY unit",
                              "Physical units of the lateral dimensions, this "
                              "means x and y values.",
                              GWY_TYPE_UNIT,
                              G_PARAM_READABLE | STATICP);

    surface_pspecs[PROP_UNIT_Z]
        = g_param_spec_object("unit-z",
                              "Z unit",
                              "Physical units of the ordinate values.",
                              GWY_TYPE_UNIT,
                              G_PARAM_READABLE | STATICP);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, surface_pspecs[i]);

    /**
     * GwySurface::data-changed:
     * @gwysurface: The #GwySurface which received the signal.
     *
     * The ::data-changed signal is emitted whenever surface data changes.
     **/
    surface_signals[DATA_CHANGED]
        = g_signal_new_class_handler("data-changed",
                                     G_OBJECT_CLASS_TYPE(klass),
                                     G_SIGNAL_RUN_FIRST,
                                     NULL, NULL, NULL,
                                     g_cclosure_marshal_VOID__VOID,
                                     G_TYPE_NONE, 0);
}

static void
gwy_surface_init(GwySurface *surface)
{
    surface->priv = G_TYPE_INSTANCE_GET_PRIVATE(surface,
                                                GWY_TYPE_SURFACE,
                                                Surface);
}

static void
alloc_data(GwySurface *surface)
{
    GWY_FREE(surface->data);
    if (surface->n)
        surface->data = g_new(GwyXYZ, surface->n);
}

static void
free_data(GwySurface *surface)
{
    GWY_FREE(surface->data);
}

static void
gwy_surface_finalize(GObject *object)
{
    GwySurface *surface = GWY_SURFACE(object);
    free_data(surface);
    G_OBJECT_CLASS(gwy_surface_parent_class)->finalize(object);
}

static void
gwy_surface_dispose(GObject *object)
{
    GwySurface *surface = GWY_SURFACE(object);
    GWY_OBJECT_UNREF(surface->priv->unit_xy);
    GWY_OBJECT_UNREF(surface->priv->unit_z);
    G_OBJECT_CLASS(gwy_surface_parent_class)->dispose(object);
}

static gsize
gwy_surface_n_items(GwySerializable *serializable)
{
    GwySurface *surface = GWY_SURFACE(serializable);
    Surface *priv = surface->priv;
    gsize n = N_ITEMS;
    if (priv->unit_xy)
       n += gwy_serializable_n_items(GWY_SERIALIZABLE(priv->unit_xy));
    if (priv->unit_z)
       n += gwy_serializable_n_items(GWY_SERIALIZABLE(priv->unit_z));
    return n;
}

static gsize
gwy_surface_itemize(GwySerializable *serializable,
                    GwySerializableItems *items)
{
    GwySurface *surface = GWY_SURFACE(serializable);
    Surface *priv = surface->priv;
    GwySerializableItem it;
    guint n = 0;

    g_return_val_if_fail(items->len - items->n >= N_ITEMS, 0);

    if (priv->unit_xy) {
        g_return_val_if_fail(items->len - items->n, 0);
        it = serialize_items[0];
        it.value.v_object = (GObject*)priv->unit_xy;
        items->items[items->n++] = it;
        gwy_serializable_itemize(GWY_SERIALIZABLE(priv->unit_xy), items);
        n++;
    }

    if (priv->unit_z) {
        g_return_val_if_fail(items->len - items->n, 0);
        it = serialize_items[1];
        it.value.v_object = (GObject*)priv->unit_z;
        items->items[items->n++] = it;
        gwy_serializable_itemize(GWY_SERIALIZABLE(priv->unit_z), items);
        n++;
    }

    g_return_val_if_fail(items->len - items->n, 0);
    it = serialize_items[2];
    it.value.v_double_array = (gdouble*)surface->data;
    it.array_size = 3*surface->n;
    items->items[items->n++] = it;
    n++;

    return n;
}

static gboolean
gwy_surface_construct(GwySerializable *serializable,
                      GwySerializableItems *items,
                      GwyErrorList **error_list)
{
    GwySurface *surface = GWY_SURFACE(serializable);
    Surface *priv = surface->priv;

    GwySerializableItem its[N_ITEMS];
    memcpy(its, serialize_items, sizeof(serialize_items));
    gwy_deserialize_filter_items(its, N_ITEMS, items, NULL,
                                 "GwySurface", error_list);

    if (!_gwy_check_object_component(its + 0, surface, GWY_TYPE_UNIT,
                                     error_list))
        goto fail;
    if (!_gwy_check_object_component(its + 1, surface, GWY_TYPE_UNIT,
                                     error_list))
        goto fail;

    guint len = its[2].array_size;
    if (len && its[2].value.v_double_array) {
        if (len % 3 != 0) {
            gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                               GWY_DESERIALIZE_ERROR_INVALID,
                               _("Surface data length is %lu which is not "
                                 "a multiple of 3."),
                               (gulong)its[2].array_size);
            goto fail;
        }
        surface->n = its[2].array_size/3;
        surface->data = (GwyXYZ*)its[2].value.v_double_array;
    }
    priv->unit_xy = (GwyUnit*)its[0].value.v_object;
    priv->unit_z = (GwyUnit*)its[1].value.v_object;

    return TRUE;

fail:
    GWY_OBJECT_UNREF(its[0].value.v_object);
    GWY_OBJECT_UNREF(its[1].value.v_object);
    GWY_FREE(its[2].value.v_double_array);
    return FALSE;
}

static void
copy_info(GwySurface *dest,
          const GwySurface *src)
{
    Surface *dpriv = dest->priv, *spriv = src->priv;
    ASSIGN_UNITS(dpriv->unit_xy, spriv->unit_xy);
    ASSIGN_UNITS(dpriv->unit_z, spriv->unit_z);
}

static void
copy_cache(GwySurface *dest,
           const GwySurface *src)
{
    Surface *dpriv = dest->priv, *spriv = src->priv;
    dpriv->cached_range = spriv->cached_range;
    dpriv->xmin = spriv->xmin;
    dpriv->xmax = spriv->xmax;
    dpriv->ymin = spriv->ymin;
    dpriv->ymax = spriv->ymax;
    dpriv->cached = spriv->cached;
    gwy_assign(dpriv->cache, spriv->cache, GWY_SURFACE_CACHE_SIZE);
}

static GObject*
gwy_surface_duplicate_impl(GwySerializable *serializable)
{
    GwySurface *surface = GWY_SURFACE(serializable);
    GwySurface *duplicate = gwy_surface_new_from_data(surface->data,
                                                      surface->n);
    copy_info(duplicate, surface);
    return G_OBJECT(duplicate);
}

static void
gwy_surface_assign_impl(GwySerializable *destination,
                        GwySerializable *source)
{
    GwySurface *dest = GWY_SURFACE(destination);
    GwySurface *src = GWY_SURFACE(source);

    gboolean notify = FALSE;
    if (dest->n != src->n) {
        dest->n = src->n;
        alloc_data(dest);
        notify = TRUE;
    }
    gwy_assign(dest->data, src->data, src->n);
    copy_info(dest, src);
    copy_cache(dest, src);
    if (notify)
        g_object_notify_by_pspec(G_OBJECT(dest), surface_pspecs[PROP_N_POINTS]);
}

static void
gwy_surface_set_property(GObject *object,
                       guint prop_id,
                       G_GNUC_UNUSED const GValue *value,
                       GParamSpec *pspec)
{
    switch (prop_id) {
        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_surface_get_property(GObject *object,
                      guint prop_id,
                      GValue *value,
                      GParamSpec *pspec)
{
    GwySurface *surface = GWY_SURFACE(object);
    Surface *priv = surface->priv;

    switch (prop_id) {
        case PROP_N_POINTS:
        g_value_set_uint(value, surface->n);
        break;

        case PROP_UNIT_XY:
        // Instantiate the units to be consistent with the direct interface
        // that never admits the units are NULL.
        if (!priv->unit_xy)
            priv->unit_xy = gwy_unit_new();
        g_value_set_object(value, priv->unit_xy);
        break;

        case PROP_UNIT_Z:
        // Instantiate the units to be consistent with the direct interface
        // that never admits the units are NULL.
        if (!priv->unit_z)
            priv->unit_z = gwy_unit_new();
        g_value_set_object(value, priv->unit_z);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

/**
 * gwy_surface_new:
 *
 * Creates a new empty surface.
 *
 * The surface will not contain any points. This parameterless constructor
 * exists mainly for language bindings, gwy_surface_new_from_data() is usually
 * more useful.
 *
 * Returns: A new surface.
 **/
GwySurface*
gwy_surface_new(void)
{
    return g_object_newv(GWY_TYPE_SURFACE, 0, NULL);
}

/**
 * gwy_surface_new_sized:
 * @n: Number of points.
 *
 * Creates a new surface with preallocated size.
 *
 * The surface will contain the speficied number of points with uninitialised
 * values.
 *
 * Returns: A new surface.
 **/
GwySurface*
gwy_surface_new_sized(guint n)
{
    GwySurface *surface = g_object_newv(GWY_TYPE_SURFACE, 0, NULL);
    surface->n = n;
    alloc_data(surface);
    return surface;
}

/**
 * gwy_surface_new_from_data:
 * @points: Array of @n points with the surface data.
 * @n: Number of points.
 *
 * Creates a new surface, filling it with provided points.
 *
 * Returns: A new surface.
 **/
GwySurface*
gwy_surface_new_from_data(const GwyXYZ *points,
                        guint n)
{
    g_return_val_if_fail(!n || points, NULL);

    GwySurface *surface = g_object_newv(GWY_TYPE_SURFACE, 0, NULL);
    surface->n = n;
    alloc_data(surface);
    gwy_assign(surface->data, points, n);
    return surface;
}

/**
 * gwy_surface_new_alike:
 * @model: A surface to use as the template.
 *
 * Creates a new empty surface similar to another surface.
 *
 * The units of the new surface will be identical to those of @model but the
 * new surface will not contain any points.  Use gwy_surface_duplicate() to
 * completely duplicate a surface including data.
 *
 * Returns: A new empty data surface.
 **/
GwySurface*
gwy_surface_new_alike(const GwySurface *model)
{
    g_return_val_if_fail(GWY_IS_SURFACE(model), NULL);
    GwySurface *surface = g_object_newv(GWY_TYPE_SURFACE, 0, NULL);
    copy_info(surface, model);
    return surface;
}

/**
 * gwy_surface_new_part:
 * @surface: A surface.
 * @xfrom: Minimum x-coordinate value.
 * @xto: Maximum x-coordinate value.
 * @yfrom: Minimum y-coordinate value.
 * @yto: Maximum y-coordinate value.
 *
 * Creates a new surface as a part of another surface.
 *
 * The new surface consits of data with lateral coordinates within the
 * specified ranges (inclusively).  It may be empty.
 *
 * Data are physically copied, i.e. changing the new surface data does not
 * change @surface's data and vice versa.
 *
 * Returns: A new surface.
 **/
GwySurface*
gwy_surface_new_part(const GwySurface *surface,
                     gdouble xfrom,
                     gdouble xto,
                     gdouble yfrom,
                     gdouble yto)
{
    g_return_val_if_fail(GWY_IS_SURFACE(surface), NULL);

    GwySurface *part = gwy_surface_new_alike(surface);

    if (!surface->n
        || xfrom > xto
        || yfrom > yto)
        return part;

    guint n = 0;
    for (guint i = 0; i < surface->n; i++) {
        gdouble x = surface->data[i].x, y = surface->data[i].y;
        if (x >= xfrom && x <= xto && y >= yfrom && y <= yto)
            n++;
    }
    part->n = n;
    alloc_data(part);

    n = 0;
    for (guint i = 0; i < surface->n; i++) {
        gdouble x = surface->data[i].x, y = surface->data[i].y;
        if (x >= xfrom && x <= xto && y >= yfrom && y <= yto)
            part->data[n++] = surface->data[i];
    }
    return part;
}

static void
copy_field_to_surface(const GwyField *field,
                      GwySurface *surface)
{
    gdouble dx = gwy_field_dx(field), dy = gwy_field_dy(field);
    gdouble xoff = 0.5*dx + field->xoff, yoff = 0.5*dy + field->yoff;
    guint k = 0;

    for (guint i = 0; i < field->yres; i++) {
        for (guint j = 0; j < field->xres; j++) {
            surface->data[k].x = dx*j + xoff;
            surface->data[k].y = dy*i + yoff;
            surface->data[k].z = field->data[k];
            k++;
        }
    }
    ASSIGN_UNITS(surface->priv->unit_xy, field->priv->unit_xy);
    ASSIGN_UNITS(surface->priv->unit_z, field->priv->unit_z);

    gwy_surface_invalidate(surface);
    surface->priv->cached_range = TRUE;
    surface->priv->xmin = xoff;
    surface->priv->xmax = dx*(field->xres - 1) + xoff;
    surface->priv->ymin = yoff;
    surface->priv->ymax = dy*(field->yres - 1) + yoff;
}

/**
 * gwy_surface_new_from_field:
 * @field: A one-dimensional data field.
 *
 * Creates a new surface from a one-dimensional data field.
 *
 * The number of points in the new surface will be equal to the number of
 * points in the field.  Lateral coordinates will be equal to the corresponding
 * @field coordinates; values will be created in regular grid according to
 * @field's physical size and offset.
 *
 * Lateral and value units will correspond to @field's units.
 *
 * Returns: A new surface.
 **/
GwySurface*
gwy_surface_new_from_field(const GwyField *field)
{
    g_return_val_if_fail(GWY_IS_FIELD(field), NULL);

    GwySurface *surface = g_object_newv(GWY_TYPE_SURFACE, 0, NULL);
    surface->n = field->xres * field->yres;
    alloc_data(surface);
    copy_field_to_surface(field, surface);

    return surface;
}

/**
 * gwy_surface_data_changed:
 * @surface: A surface.
 *
 * Emits signal GwySurface::data-changed on a surface.
 **/
void
gwy_surface_data_changed(GwySurface *surface)
{
    g_return_if_fail(GWY_IS_SURFACE(surface));
    g_signal_emit(surface, surface_signals[DATA_CHANGED], 0);
}

/**
 * gwy_surface_copy:
 * @src: Source surface.
 * @dest: Destination surface.
 *
 * Copies the data of a surface to another surface of the same dimensions.
 *
 * Only the data points are copied.  To make a surface completely identical to
 * another, including units and change of dimensions, you can use
 * gwy_surface_assign().
 **/
void
gwy_surface_copy(const GwySurface *src,
                 GwySurface *dest)
{
    g_return_if_fail(GWY_IS_SURFACE(src));
    g_return_if_fail(GWY_IS_SURFACE(dest));
    g_return_if_fail(dest->n == src->n);
    gwy_assign(dest->data, src->data, src->n);
    copy_cache(dest, src);
}

/**
 * gwy_surface_invalidate:
 * @surface: A surface.
 *
 * Invalidates cached surface statistics.
 *
 * See gwy_field_invalidate() for discussion of invalidation and examples.
 **/
void
gwy_surface_invalidate(GwySurface *surface)
{
    g_return_if_fail(GWY_IS_SURFACE(surface));
    surface->priv->cached_range = FALSE;
    surface->priv->cached = 0;
}

/**
 * gwy_surface_set_from_field:
 * @surface: A surface.
 * @field: A one-dimensional data field.
 *
 * Sets the data and units of a surface from a field.
 *
 * See gwy_surface_new_from_field() for details.
 **/
void
gwy_surface_set_from_field(GwySurface *surface,
                           const GwyField *field)
{
    g_return_if_fail(GWY_IS_SURFACE(surface));
    g_return_if_fail(GWY_IS_FIELD(field));

    gboolean notify = FALSE;
    if (surface->n != field->xres * field->yres) {
        free_data(surface);
        surface->n = field->xres * field->yres;
        alloc_data(surface);
        notify = TRUE;
    }
    copy_field_to_surface(field, surface);
    if (notify)
        g_object_notify_by_pspec(G_OBJECT(surface),
                                 surface_pspecs[PROP_N_POINTS]);
}

static void
regularise_preview(const GwySurface *surface,
                   GwyField *field)
{
    guint xres = field->xres, yres = field->yres, totalcount = 0;
    gdouble dx = gwy_field_dx(field), dy = gwy_field_dy(field);
    guint *counters = g_new0(guint, xres*yres);

    gwy_field_clear_full(field);

    for (guint k = 0; k < surface->n; k++) {
        const GwyXYZ *pt = surface->data + k;
        gint j = (gint)floor((pt->x - field->xoff)/dx);
        gint i = (gint)floor((pt->y - field->yoff)/dy);
        if (j < 0 || j >= (gint)xres || i < 0 || i >= (gint)yres)
            continue;

        if (!counters[i*xres + j]++)
            totalcount++;
        field->data[i*xres + j] += pt->z;
    }

    if (!totalcount) {
        // FIXME: Do something clever instead.
        g_free(counters);
        return;
    }

    // If the pixels contain at leasr something use the mean value as the
    // representation and mark them as fixed.  Otherwise mark them as to be
    // interpolated in iter 1.
    for (guint k = 0; k < xres*yres; k++) {
        if (counters[k]) {
            field->data[k] /= counters[k];
            counters[k] = 0;
        }
        else
            counters[k] = G_MAXUINT;
    }

    guint todo = xres*yres - totalcount;
    if (!todo) {
        g_free(counters);
        return;
    }

    GwyField *buffer = gwy_field_duplicate(field);

    for (guint iter = 0; todo; iter++) {
        // Propagate already initialised values further to the uninitialised
        // area.  At the same time interpolate in the already initialised
        // area.
        for (gint i = 0; i < (gint)yres; i++) {
            for (gint j = 0; j < (gint)xres; j++) {
                gint k = i*xres-xres + j-1;
                guint *cb = counters + k;
                gdouble *db = field->data + k;
                gdouble *bb = buffer->data + k;
                guint s = 0;
                gdouble z = 0.0;

                // Skip fixed data, everything else is a candidate for 
                // interpolation.
                if (!cb[xres+1])
                    continue;

                // Scan the pixel neighbourhood and gather already initialised
                // values.
                if (i && j && cb[0] <= iter) {
                    z += db[0];
                    s++;
                }
                if (i && cb[1] <= iter) {
                    z += db[1];
                    s++;
                }
                if (i && j < (gint)xres-1 && cb[2] <= iter) {
                    z += db[2];
                    s++;
                }
                if (j && cb[xres] <= iter) {
                    z += db[xres];
                    s++;
                }
                if (j < (gint)xres-1 && cb[xres+2] <= iter) {
                    z += db[xres+2];
                    s++;
                }
                if (i < (gint)yres-1 && j && cb[2*xres] <= iter) {
                    z += db[2*xres];
                    s++;
                }
                if (i < (gint)yres-1 && cb[2*xres+1] <= iter) {
                    z += db[2*xres+1];
                    s++;
                }
                if (i < (gint)yres-1 && j < (gint)xres-1
                    && cb[2*xres+2] <= iter) {
                    z += db[2*xres+2];
                    s++;
                }

                if (cb[xres+1] <= iter) {
                    // Interpolate previously initialised data further.
                    bb[xres+1] = z/s;
                }
                else if (s) {
                    // Mark it as done for the next iter but do not let the
                    // value propagate in this iter.
                    cb[xres+1] = iter+1;
                    bb[xres+1] = z/s;
                    todo--;
                }
            }
        }
        GWY_SWAP(GwyField*, field, buffer);
    }

    g_object_unref(buffer);
    g_free(counters);
}

static GwyField*
regularise(const GwySurface *surface,
           GwySurfaceRegularizeType method,
           gboolean full,
           gdouble xfrom, gdouble xto,
           gdouble yfrom, gdouble yto,
           guint xres, guint yres)
{
    gdouble xmin, xmax, ymin, ymax;
    gwy_surface_xrange(surface, &xmin, &xmax);
    gwy_surface_yrange(surface, &ymin, &ymax);

    if (full) {
        xfrom = xmin;
        xto = xmax;
        yfrom = ymin;
        yto = ymax;
    }

    gdouble xlen = xto - xfrom, ylen = yto - yfrom;

    if (!xres || !yres) {
        gdouble alpha = xlen/ylen, alpha1 = 1.0 - alpha;
        gdouble sqrtD = sqrt(4.0*alpha*surface->n + alpha1*alpha1);
        gdouble xresfull = 0.5*(alpha1 + sqrtD),
                yresfull = 0.5*(sqrtD - alpha1)/alpha;
        gdouble p = sqrt(xlen*ylen/(xresfull - 1)/(yresfull - 1));

        if (!xres) {
            if (!p || isnan(p) || !xlen)
                xres = 1;
            else {
                xres = gwy_round(xlen/p + 1);
                xres = CLAMP(xres, 1, surface->n);
            }
        }
        if (!yres) {
            if (!p || isnan(p) || !ylen)
                yres = 1;
            else {
                yres = gwy_round(ylen/p + 1);
                yres = CLAMP(yres, 1, surface->n);
            }
        }
    }

    GwyField *field = gwy_field_new_sized(xres, yres, FALSE);

    if (xres == 1 || !(xlen))
        field->xreal = xfrom ? fabs(xfrom) : 1.0;
    else
        field->xreal = (xlen)*xres/(xres - 1.0);
    field->xoff = xmin - 0.5*gwy_field_dx(field);

    if (yres == 1 || !(ylen))
        field->yreal = yfrom ? fabs(yfrom) : 1.0;
    else
        field->yreal = (ylen)*yres/(yres - 1.0);
    field->yoff = ymin - 0.5*gwy_field_dy(field);

    if (method == GWY_SURFACE_REGULARIZE_PREVIEW)
        regularise_preview(surface, field);
    else
        g_assert_not_reached();

    ASSIGN_UNITS(field->priv->unit_xy, surface->priv->unit_xy);
    ASSIGN_UNITS(field->priv->unit_z, surface->priv->unit_z);

    return field;
}

/**
 * gwy_surface_regularize_full:
 * @surface: A surface.
 * @method: Regularisation method.
 * @xres: Required horizontal resolution.  Pass 0 to choose a resolution
 *        automatically.
 * @yres: Required vertical resolution.  Pass 0 to choose a resolution
 *        automatically.
 *
 * Creates a two-dimensional data field from an entire surface.
 *
 * If the surface is non-degenerate of size 2×2, composed of squares and
 * the requested @xres×@yres matches the @surfaces's number of points, then
 * one-to-one data point mapping can be used and the conversion will be
 * information-preserving.  In other words, if the surface was created from a
 * #GwyField this function can perform a perfect reversal, possibly up to some
 * rounding errors. This is true for any @method although the method choice
 * still can has a dramatic effect on speed and resource consumption.
 * Otherwise the interpolated and exterpolated values are method-dependent.
 *
 * Returns: (allow-none):
 *          A new two-dimensional data field or %NULL if the surface contains
 *          no points.
 **/
GwyField*
gwy_surface_regularize_full(const GwySurface *surface,
                            GwySurfaceRegularizeType method,
                            guint xres, guint yres)
{
    g_return_val_if_fail(GWY_IS_SURFACE(surface), NULL);

    if (!surface->n)
        return NULL;

    return regularise(surface, method, TRUE, 0.0, 0.0, 0.0, 0.0, xres, yres);
}

/**
 * gwy_surface_regularize:
 * @surface: A surface.
 * @method: Regularisation method.
 * @xfrom: Start the horizontal interval.
 * @xto: End of the horizontal interval.
 * @yfrom: Start the vertical interval.
 * @yto: End of the vertical interval.
 * @xres: Required horizontal resolution.  Pass 0 to choose a resolution
 *        automatically.
 * @yres: Required vertical resolution.  Pass 0 to choose a resolution
 *        automatically.
 *
 * Creates a two-dimensional data field from a surface.
 *
 * Returns: (allow-none):
 *          A new two-dimensional data field or %NULL if the surface contains
 *          no points.
 **/
GwyField*
gwy_surface_regularize(const GwySurface *surface,
                       GwySurfaceRegularizeType method,
                       gdouble xfrom, gdouble xto,
                       gdouble yfrom, gdouble yto,
                       guint xres, guint yres)
{
    g_return_val_if_fail(GWY_IS_SURFACE(surface), NULL);
    g_return_val_if_fail(xfrom >= xto, NULL);
    g_return_val_if_fail(yfrom >= yto, NULL);

    if (!surface->n)
        return NULL;

    return regularise(surface, method, FALSE,
                      xfrom, xto, yfrom, yto, xres, yres);
}

/**
 * gwy_surface_get_unit_xy:
 * @surface: A surface.
 *
 * Obtains the lateral units of a surface.
 *
 * Returns: (transfer none):
 *          The lateral units of @surface.
 **/
GwyUnit*
gwy_surface_get_unit_xy(GwySurface *surface)
{
    g_return_val_if_fail(GWY_IS_SURFACE(surface), NULL);
    Surface *priv = surface->priv;
    if (!priv->unit_xy)
        priv->unit_xy = gwy_unit_new();
    return priv->unit_xy;
}

/**
 * gwy_surface_get_unit_z:
 * @surface: A surface.
 *
 * Obtains the value units of a surface.
 *
 * Returns: (transfer none):
 *          The value units of @surface.
 **/
GwyUnit*
gwy_surface_get_unit_z(GwySurface *surface)
{
    g_return_val_if_fail(GWY_IS_SURFACE(surface), NULL);
    Surface *priv = surface->priv;
    if (!priv->unit_z)
        priv->unit_z = gwy_unit_new();
    return priv->unit_z;
}

/**
 * gwy_surface_format_xy:
 * @surface: A surface.
 * @style: Value format style.
 *
 * Finds a suitable format for displaying lateral values of a surface.
 *
 * The created format usually has a sufficient precision to represent lateral
 * values of neighbour points as different values.  However, if the intervals
 * differ by several orders of magnitude this is not really guaranteed.
 *
 * Returns: A newly created value format.
 **/
GwyValueFormat*
gwy_surface_format_xy(GwySurface *surface,
                      GwyValueFormatStyle style)
{
    g_return_val_if_fail(GWY_IS_SURFACE(surface), NULL);
    if (!surface->n)
        return gwy_unit_format_with_resolution(gwy_surface_get_unit_xy(surface),
                                               style, 1.0, 0.1);
    // XXX: If we have the triangulation a better estimate can be made.  Maybe.
    if (surface->n == 1) {
        gdouble m = MAX(fabs(surface->data[0].x), fabs(surface->data[0].y));
        if (!m)
            m = 1.0;
        return gwy_unit_format_with_resolution(gwy_surface_get_unit_xy(surface),
                                               style, m, m/10.0);
    }
    gdouble xmin, xmax, ymin, ymax;
    gwy_surface_xrange(surface, &xmin, &xmax);
    gwy_surface_yrange(surface, &ymin, &ymax);
    gdouble max = MAX(MAX(fabs(xmax), fabs(xmin)), MAX(fabs(ymax), fabs(ymin)));
    if (!max)
        max = 1.0;
    gdouble unit = hypot(ymax - ymin, xmax - xmin)/sqrt(surface->n);
    if (!unit)
        unit = max/10.0;
    return gwy_unit_format_with_resolution(gwy_surface_get_unit_xy(surface),
                                           style, max, 0.3*unit);
}

/**
 * gwy_surface_format_z:
 * @surface: A surface.
 * @style: Value format style.
 *
 * Finds a suitable format for displaying values in a data surface.
 *
 * Returns: A newly created value format.
 **/
GwyValueFormat*
gwy_surface_format_z(GwySurface *surface,
                     GwyValueFormatStyle style)
{
    g_return_val_if_fail(GWY_IS_SURFACE(surface), NULL);
    gdouble min, max;
    if (surface->n) {
        gwy_surface_min_max_full(surface, &min, &max);
        if (max == min) {
            max = ABS(max);
            min = 0.0;
        }
    }
    else {
        min = 0.0;
        max = 1.0;
    }
    return gwy_unit_format_with_digits(gwy_surface_get_unit_z(surface),
                                       style, max - min, 3);
}


/**
 * SECTION: surface
 * @title: GwySurface
 * @short_description: General two-dimensional data
 *
 * #GwySurface represents general, i.e. possibly unevenly spaced,
 * two-dimensional data, also called XYZ data.
 *
 * Surface points are stored in a flat array #GwySurface-struct.data of #GwyXYZ
 * values.
 *
 * Unlike #GwyField, a surface can also be empty, i.e. contain zero points.
 **/

/**
 * GwySurface:
 * @n: Number of points.
 * @data: Surface data.  See the introductory section for details.
 *
 * Object representing surface data.
 *
 * The #GwySurface struct contains some public fields that can be directly
 * accessed for reading.  To set them, you must use the #GwySurface methods.
 **/

/**
 * GwySurfaceClass:
 *
 * Class of surfaces.
 **/

/**
 * gwy_surface_duplicate:
 * @surface: A surface.
 *
 * Duplicates a surface.
 *
 * This is a convenience wrapper of gwy_serializable_duplicate().
 **/

/**
 * gwy_surface_assign:
 * @dest: Destination surface.
 * @src: Source surface.
 *
 * Copies the value of a surface.
 *
 * This is a convenience wrapper of gwy_serializable_assign().
 **/

/**
 * gwy_surface_index:
 * @surface: A surface.
 * @pos: Position in @surface.
 *
 * Accesses a surface point.
 *
 * This macro may evaluate its arguments several times.
 * This macro expands to a left-hand side expression.
 *
 * No argument validation is performed.  If you process the data in a loop,
 * you are encouraged to access #GwySurface-struct.data directly.
 **/

/**
 * GwySurfaceRegularizeType:
 * @GWY_SURFACE_REGULARIZE_PREVIEW: Quick and dirty method that does not
 *                                  require triangulation.  It should, however,
 *                                  provide a result usable for displaying the
 *                                  surface.
 *
 * Surface regularisation method.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
