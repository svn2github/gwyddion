/*
 *  $Id$
 *  Copyright (C) 2009 David Necas (Yeti).
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
#include "libgwy/field.h"
#include "libgwy/field-statistics.h"
#include "libgwy/libgwy-aliases.h"
#include "libgwy/processing-internal.h"

enum { N_ITEMS = 9 };

enum {
    DATA_CHANGED,
    N_SIGNALS
};

enum {
    PROP_0,
    PROP_XRES,
    PROP_YRES,
    PROP_XREAL,
    PROP_YREAL,
    PROP_XOFFSET,
    PROP_YOFFSET,
    PROP_UNIT_XY,
    PROP_UNIT_Z,
    N_PROPS
};

static void     gwy_field_finalize         (GObject *object);
static void     gwy_field_dispose          (GObject *object);
static void     gwy_field_serializable_init(GwySerializableInterface *iface);
static gsize    gwy_field_n_items          (GwySerializable *serializable);
static gsize    gwy_field_itemize          (GwySerializable *serializable,
                                            GwySerializableItems *items);
static gboolean gwy_field_construct        (GwySerializable *serializable,
                                            GwySerializableItems *items,
                                            GwyErrorList **error_list);
static GObject* gwy_field_duplicate_impl   (GwySerializable *serializable);
static void     gwy_field_assign_impl      (GwySerializable *destination,
                                            GwySerializable *source);
static void     gwy_field_set_property     (GObject *object,
                                            guint prop_id,
                                            const GValue *value,
                                            GParamSpec *pspec);
static void     gwy_field_get_property     (GObject *object,
                                            guint prop_id,
                                            GValue *value,
                                            GParamSpec *pspec);

static const GwySerializableItem serialize_items[N_ITEMS] = {
    /*0*/ { .name = "xres",    .ctype = GWY_SERIALIZABLE_INT32,        },
    /*1*/ { .name = "yres",    .ctype = GWY_SERIALIZABLE_INT32,        },
    /*2*/ { .name = "xreal",   .ctype = GWY_SERIALIZABLE_DOUBLE,       },
    /*3*/ { .name = "yreal",   .ctype = GWY_SERIALIZABLE_DOUBLE,       },
    /*4*/ { .name = "xoff",    .ctype = GWY_SERIALIZABLE_DOUBLE,       },
    /*5*/ { .name = "yoff",    .ctype = GWY_SERIALIZABLE_DOUBLE,       },
    /*6*/ { .name = "unit-xy", .ctype = GWY_SERIALIZABLE_OBJECT,       },
    /*7*/ { .name = "unit-z",  .ctype = GWY_SERIALIZABLE_OBJECT,       },
    /*8*/ { .name = "data",    .ctype = GWY_SERIALIZABLE_DOUBLE_ARRAY, },
};

static guint field_signals[N_SIGNALS];

G_DEFINE_TYPE_EXTENDED
    (GwyField, gwy_field, G_TYPE_OBJECT, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_field_serializable_init))

static void
gwy_field_serializable_init(GwySerializableInterface *iface)
{
    iface->n_items   = gwy_field_n_items;
    iface->itemize   = gwy_field_itemize;
    iface->construct = gwy_field_construct;
    iface->duplicate = gwy_field_duplicate_impl;
    iface->assign    = gwy_field_assign_impl;
}

static void
gwy_field_class_init(GwyFieldClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(Field));

    gobject_class->dispose = gwy_field_dispose;
    gobject_class->finalize = gwy_field_finalize;
    gobject_class->get_property = gwy_field_get_property;
    gobject_class->set_property = gwy_field_set_property;

    g_object_class_install_property
        (gobject_class,
         PROP_XRES,
         g_param_spec_uint("x-res",
                           "X resolution",
                           "Pixel width of the field.",
                           1, G_MAXUINT, 1,
                           G_PARAM_READABLE | STATICP));

    g_object_class_install_property
        (gobject_class,
         PROP_YRES,
         g_param_spec_uint("y-res",
                           "Y resolution",
                           "Pixel height of the field.",
                           1, G_MAXUINT, 1,
                           G_PARAM_READABLE | STATICP));

    g_object_class_install_property
        (gobject_class,
         PROP_XREAL,
         g_param_spec_double("x-real",
                             "X real size",
                             "Width of the field in physical units.",
                             G_MINDOUBLE, G_MAXDOUBLE, 1.0,
                             G_PARAM_READWRITE | STATICP));

    g_object_class_install_property
        (gobject_class,
         PROP_YREAL,
         g_param_spec_double("y-real",
                             "Y real size",
                             "Height of the field in physical units.",
                             G_MINDOUBLE, G_MAXDOUBLE, 1.0,
                             G_PARAM_READWRITE | STATICP));

    g_object_class_install_property
        (gobject_class,
         PROP_XOFFSET,
         g_param_spec_double("x-offset",
                             "X offset",
                             "Horizontal offset of the field top left corner "
                             "in physical units.",
                             -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
                             G_PARAM_READWRITE | STATICP));

    g_object_class_install_property
        (gobject_class,
         PROP_YOFFSET,
         g_param_spec_double("y-offset",
                             "Y offset",
                             "Vertical offset of the field top left corner "
                             "in physical units.",
                             -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
                             G_PARAM_READWRITE | STATICP));

    g_object_class_install_property
        (gobject_class,
         PROP_UNIT_XY,
         g_param_spec_object("unit-xy",
                             "XY unit",
                             "Physical units of lateral dimensions of the "
                             "field.",
                             GWY_TYPE_UNIT,
                             G_PARAM_READABLE | STATICP));

    g_object_class_install_property
        (gobject_class,
         PROP_UNIT_Z,
         g_param_spec_object("unit-z",
                             "Z unit",
                             "Physical units of field values.",
                             GWY_TYPE_UNIT,
                             G_PARAM_READABLE | STATICP));

    /**
     * GwyField::data-changed:
     * @gwyfield: The #GwyField which received the signal.
     *
     * The ::data-changed signal is emitted whenever field data changes.
     **/
    field_signals[DATA_CHANGED]
        = g_signal_new_class_handler("data-changed",
                                     G_OBJECT_CLASS_TYPE(klass),
                                     G_SIGNAL_RUN_FIRST,
                                     NULL, NULL, NULL,
                                     g_cclosure_marshal_VOID__VOID,
                                     G_TYPE_NONE, 0);
}

static void
gwy_field_init(GwyField *field)
{
    field->priv = G_TYPE_INSTANCE_GET_PRIVATE(field, GWY_TYPE_FIELD, Field);
    field->xres = field->yres = 1;
    field->xreal = field->yreal = 1.0;
    field->data = g_slice_new(gdouble);
}

static void
alloc_data(GwyField *field,
           gboolean clear)
{
    if (clear) {
        field->data = g_new0(gdouble, field->xres * field->yres);
        field->priv->cached = (CBIT(MIN) | CBIT(MAX) | CBIT(AVG) | CBIT(RMS)
                               | CBIT(MED) | CBIT(ARF) | CBIT(ART));
    }
    else {
        field->data = g_new(gdouble, field->xres * field->yres);
        gwy_field_invalidate(field);
    }
    field->priv->allocated = TRUE;
}

static void
free_data(GwyField *field)
{
    if (field->priv->allocated)
        GWY_FREE(field->data);
    else
        GWY_SLICE_FREE(gdouble, field->data);
}

static void
gwy_field_finalize(GObject *object)
{
    GwyField *field = GWY_FIELD(object);
    free_data(field);
    G_OBJECT_CLASS(gwy_field_parent_class)->finalize(object);
}

static void
gwy_field_dispose(GObject *object)
{
    GwyField *field = GWY_FIELD(object);
    GWY_OBJECT_UNREF(field->priv->unit_xy);
    GWY_OBJECT_UNREF(field->priv->unit_z);
    G_OBJECT_CLASS(gwy_field_parent_class)->dispose(object);
}

static gsize
gwy_field_n_items(GwySerializable *serializable)
{
    GwyField *field = GWY_FIELD(serializable);
    Field *priv = field->priv;
    gsize n = N_ITEMS;
    if (priv->unit_xy)
       n += gwy_serializable_n_items(GWY_SERIALIZABLE(priv->unit_xy));
    if (priv->unit_z)
       n += gwy_serializable_n_items(GWY_SERIALIZABLE(priv->unit_z));
    return n;
}

static gsize
gwy_field_itemize(GwySerializable *serializable,
                  GwySerializableItems *items)
{
    GwyField *field = GWY_FIELD(serializable);
    Field *priv = field->priv;
    GwySerializableItem it;
    guint n = 0;

    g_return_val_if_fail(items->len - items->n >= N_ITEMS, 0);

    it = serialize_items[0];
    it.value.v_uint32 = field->xres;
    items->items[items->n++] = it;
    n++;

    it = serialize_items[1];
    it.value.v_uint32 = field->yres;
    items->items[items->n++] = it;
    n++;

    it = serialize_items[2];
    it.value.v_double = field->xreal;
    items->items[items->n++] = it;
    n++;

    it = serialize_items[3];
    it.value.v_double = field->yreal;
    items->items[items->n++] = it;
    n++;

    if (field->xoff) {
        it = serialize_items[4];
        it.value.v_double = field->xoff;
        items->items[items->n++] = it;
        n++;
    }

    if (field->yoff) {
        it = serialize_items[5];
        it.value.v_double = field->yoff;
        items->items[items->n++] = it;
        n++;
    }

    if (priv->unit_xy) {
        g_return_val_if_fail(items->len - items->n, 0);
        it = serialize_items[6];
        it.value.v_object = (GObject*)priv->unit_xy;
        items->items[items->n++] = it;
        gwy_serializable_itemize(GWY_SERIALIZABLE(priv->unit_xy), items);
        n++;
    }

    if (priv->unit_z) {
        g_return_val_if_fail(items->len - items->n, 0);
        it = serialize_items[7];
        it.value.v_object = (GObject*)priv->unit_z;
        items->items[items->n++] = it;
        gwy_serializable_itemize(GWY_SERIALIZABLE(priv->unit_z), items);
        n++;
    }

    g_return_val_if_fail(items->len - items->n, 0);
    it = serialize_items[8];
    it.value.v_double_array = field->data;
    it.array_size = field->xres * field->yres;
    items->items[items->n++] = it;
    n++;

    return n;
}

static gboolean
gwy_field_construct(GwySerializable *serializable,
                    GwySerializableItems *items,
                    GwyErrorList **error_list)
{
    GwySerializableItem its[N_ITEMS];
    memcpy(its, serialize_items, sizeof(serialize_items));
    its[2].value.v_double = its[3].value.v_double = 1.0;
    gwy_deserialize_filter_items(its, N_ITEMS, items, "GwyField", error_list);

    GwyField *field = GWY_FIELD(serializable);
    Field *priv = field->priv;

    if (G_UNLIKELY(!its[0].value.v_uint32 || !its[1].value.v_uint32)) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_INVALID,
                           _("Field dimensions %u×%u are invalid."),
                           its[0].value.v_uint32, its[1].value.v_uint32);
        return FALSE;
    }

    if (G_UNLIKELY(its[0].value.v_uint32 * its[1].value.v_uint32
                   != its[8].array_size)) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_INVALID,
                           _("Field dimensions %u×%u do not match data size "
                             "%lu."),
                           its[0].value.v_uint32, its[1].value.v_uint32,
                           (gulong)its[8].array_size);
        return FALSE;
    }

    if (G_UNLIKELY(its[6].value.v_object
                   && !GWY_IS_UNIT(its[6].value.v_object))) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_INVALID,
                           _("Field xy units are of type %s "
                             "instead of GwyUnit."),
                           G_OBJECT_TYPE_NAME(its[6].value.v_object));
        return FALSE;
    }

    if (G_UNLIKELY(its[7].value.v_object
                   && !GWY_IS_UNIT(its[7].value.v_object))) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_INVALID,
                           _("Field z units are of type %s "
                             "instead of GwyUnit."),
                           G_OBJECT_TYPE_NAME(its[7].value.v_object));
        return FALSE;
    }

    field->xres = its[0].value.v_uint32;
    field->yres = its[1].value.v_uint32;
    // FIXME: Catch near-zero and near-infinity values.
    field->xreal = CLAMP(its[2].value.v_double, G_MINDOUBLE, G_MAXDOUBLE);
    field->yreal = CLAMP(its[3].value.v_double, G_MINDOUBLE, G_MAXDOUBLE);
    field->xoff = CLAMP(its[4].value.v_double, -G_MAXDOUBLE, G_MAXDOUBLE);
    field->yoff = CLAMP(its[5].value.v_double, -G_MAXDOUBLE, G_MAXDOUBLE);
    priv->unit_xy = (GwyUnit*)its[6].value.v_object;
    its[6].value.v_object = NULL;
    priv->unit_z = (GwyUnit*)its[7].value.v_object;
    its[7].value.v_object = NULL;
    free_data(field);
    field->data = its[8].value.v_double_array;
    priv->allocated = TRUE;
    its[8].value.v_double_array = NULL;
    its[8].array_size = 0;

    return TRUE;
}

static GObject*
gwy_field_duplicate_impl(GwySerializable *serializable)
{
    GwyField *field = GWY_FIELD(serializable);
    Field *priv = field->priv;

    GwyField *duplicate = gwy_field_new_alike(field, FALSE);
    Field *dpriv = duplicate->priv;

    ASSIGN(duplicate->data, field->data, field->xres*field->yres);
    ASSIGN(dpriv->cache, priv->cache, GWY_FIELD_CACHE_SIZE);
    dpriv->cached = priv->cached;

    return G_OBJECT(duplicate);
}

void
_gwy_notify_properties(GObject *object,
                       const gchar **properties,
                       guint nproperties)
{
    if (!nproperties || !properties[0])
        return;
    if (nproperties == 1) {
        g_object_notify(object, properties[0]);
        return;
    }
    g_object_freeze_notify(object);
    for (guint i = 0; i < nproperties && properties[i]; i++)
        g_object_notify(object, properties[i]);
    g_object_thaw_notify(object);
}

static void
copy_info(GwyField *dest,
          const GwyField *src)
{
    dest->xres = src->xres;
    dest->yres = src->yres;
    dest->xreal = src->xreal;
    dest->yreal = src->yreal;
    dest->xoff = src->xoff;
    dest->yoff = src->yoff;
    Field *dpriv = dest->priv, *spriv = src->priv;
    ASSIGN_UNITS(dpriv->unit_xy, spriv->unit_xy);
    ASSIGN_UNITS(dpriv->unit_z, spriv->unit_z);
}

static void
gwy_field_assign_impl(GwySerializable *destination,
                      GwySerializable *source)
{
    GwyField *dest = GWY_FIELD(destination);
    GwyField *src = GWY_FIELD(source);
    Field *spriv = src->priv, *dpriv = dest->priv;

    const gchar *notify[N_PROPS];
    guint nn = 0;
    if (dest->xres != src->xres)
        notify[nn++] = "x-res";
    if (dest->yres != src->yres)
        notify[nn++] = "y-res";
    if (dest->xreal != src->xreal)
        notify[nn++] = "x-real";
    if (dest->yreal != src->yreal)
        notify[nn++] = "y-real";
    if (dest->xoff != src->xoff)
        notify[nn++] = "x-offset";
    if (dest->yoff != src->yoff)
        notify[nn++] = "y-offset";

    if (dest->xres * dest->yres != src->xres * src->yres) {
        free_data(dest);
        dest->data = g_new(gdouble, src->xres * src->yres);
        dpriv->allocated = TRUE;
    }
    ASSIGN(dest->data, src->data, src->xres * src->yres);
    copy_info(dest, src);
    ASSIGN(dpriv->cache, spriv->cache, GWY_FIELD_CACHE_SIZE);
    dpriv->cached = spriv->cached;
    _gwy_notify_properties(G_OBJECT(dest), notify, nn);
}

static void
gwy_field_set_property(GObject *object,
                       guint prop_id,
                       const GValue *value,
                       GParamSpec *pspec)
{
    GwyField *field = GWY_FIELD(object);

    switch (prop_id) {
        case PROP_XREAL:
        field->xreal = g_value_get_double(value);
        break;

        case PROP_YREAL:
        field->yreal = g_value_get_double(value);
        break;

        case PROP_XOFFSET:
        field->xoff = g_value_get_double(value);
        break;

        case PROP_YOFFSET:
        field->yoff = g_value_get_double(value);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_field_get_property(GObject *object,
                       guint prop_id,
                       GValue *value,
                       GParamSpec *pspec)
{
    GwyField *field = GWY_FIELD(object);
    Field *priv = field->priv;

    switch (prop_id) {
        case PROP_XRES:
        g_value_set_uint(value, field->xres);
        break;

        case PROP_YRES:
        g_value_set_uint(value, field->yres);
        break;

        case PROP_XREAL:
        g_value_set_double(value, field->xreal);
        break;

        case PROP_YREAL:
        g_value_set_double(value, field->yreal);
        break;

        case PROP_XOFFSET:
        g_value_set_double(value, field->xoff);
        break;

        case PROP_YOFFSET:
        g_value_set_double(value, field->yoff);
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
 * gwy_field_new:
 *
 * Creates a new two-dimensional data field.
 *
 * The field dimensions will be 1×1 and it will be zero-filled.  This
 * paremterless constructor exists mainly for language bindings,
 * gwy_field_new_sized() and gwy_field_new_alike() are usually more useful.
 *
 * Returns: A new two-dimensional data field.
 **/
GwyField*
gwy_field_new(void)
{
    return g_object_newv(GWY_TYPE_FIELD, 0, NULL);
}

/**
 * gwy_field_new_sized:
 * @xres: Horizontal resolution (width).
 * @yres: Vertical resolution (height).
 * @clear: %TRUE to fill the new field data with zeroes, %FALSE to leave it
 *         unitialized.
 *
 * Creates a new two-dimensional data field of specified dimensions.
 *
 * Returns: A new two-dimensional data field.
 **/
GwyField*
gwy_field_new_sized(guint xres,
                    guint yres,
                    gboolean clear)
{
    g_return_val_if_fail(xres && yres, NULL);

    GwyField *field = g_object_newv(GWY_TYPE_FIELD, 0, NULL);
    free_data(field);
    field->xres = xres;
    field->yres = yres;
    alloc_data(field, clear);
    return field;
}

/**
 * gwy_field_new_alike:
 * @model: A two-dimensional data field to use as the template.
 * @clear: %TRUE to fill the new field data with zeroes, %FALSE to leave it
 *         unitialized.
 *
 * Creates a new two-dimensional data field similar to another field.
 *
 * All properties of the newly created field will be identical to @model,
 * except the data that will be either zeroes or unitialized.  Use
 * gwy_field_duplicate() to completely duplicate a field including data.
 *
 * Returns: A new two-dimensional data field.
 **/
GwyField*
gwy_field_new_alike(const GwyField *model,
                    gboolean clear)
{
    g_return_val_if_fail(GWY_IS_FIELD(model), NULL);

    GwyField *field = gwy_field_new_sized(model->xres, model->yres, clear);
    copy_info(field, model);
    return field;
}

/**
 * gwy_field_new_part:
 * @field: A two-dimensional data field.
 * @col: Column index of the upper-left corner of the rectangle.
 * @row: Row index of the upper-left corner of the rectangle.
 * @width: Rectangle width (number of columns).
 * @height: Rectangle height (number of rows).
 * @keep_offsets: %TRUE to set the X and Y offsets of the new field
 *                using @col, @row and @field offsets.  %FALSE to set offsets
 *                of the new field to zeroes.
 *
 * Creates a new two-dimensional field as a rectangular part of another field.
 *
 * The rectangle of size @width×@height starting at (@col,@row) must be
 * entirely contained in @field.  Both dimensions must be non-zero.
 *
 * Data are physically copied, i.e. changing the new field data does not change
 * @field's data and vice versa.  Physical dimensions of the new field are
 * calculated to correspond to the extracted part.
 *
 * Returns: A new two-dimensional data field.
 **/
GwyField*
gwy_field_new_part(const GwyField *field,
                   guint col,
                   guint row,
                   guint width,
                   guint height,
                   gboolean keep_offsets)
{
    g_return_val_if_fail(GWY_IS_FIELD(field), NULL);
    g_return_val_if_fail(width && height, NULL);
    g_return_val_if_fail(col + width <= field->xres, NULL);
    g_return_val_if_fail(row + height <= field->yres, NULL);

    GwyField *part;

    if (width == field->xres && height == field->yres) {
        part = gwy_field_duplicate(field);
        if (!keep_offsets)
            part->xoff = part->yoff = 0.0;
        return part;
    }

    part = gwy_field_new_sized(width, height, FALSE);
    gwy_field_part_copy(field, col, row, width, height, part, 0, 0);
    part->xreal = width*gwy_field_dx(field);
    part->yreal = height*gwy_field_dy(field);
    ASSIGN_UNITS(part->priv->unit_xy, field->priv->unit_xy);
    ASSIGN_UNITS(part->priv->unit_z, field->priv->unit_z);
    if (keep_offsets) {
        part->xoff = field->xoff + col*gwy_field_dx(field);
        part->yoff = field->yoff + row*gwy_field_dy(field);
    }
    return part;
}

/**
 * gwy_field_new_resampled:
 * @field: A two-dimensional data field.
 * @xres: Desired X resolution.
 * @yres: Desired Y resolution.
 * @interpolation: Interpolation method to use.
 *
 * Creates a new two-dimensional data field by resampling another field.
 *
 * Returns: A new two-dimensional data field.
 **/
GwyField*
gwy_field_new_resampled(const GwyField *field,
                        guint xres,
                        guint yres,
                        GwyInterpolationType interpolation)
{
    g_return_val_if_fail(GWY_IS_FIELD(field), NULL);
    g_return_val_if_fail(xres && yres, NULL);
    if (xres == field->xres && yres == field->yres)
        return gwy_field_duplicate(field);

    GwyField *dest;
    dest = gwy_field_new_sized(xres, yres, FALSE);
    copy_info(dest, field);
    dest->xres = xres;  // Undo
    dest->yres = yres;  // Undo
    gwy_interpolation_resample_block_2d(field->xres, field->yres, field->xres,
                                        field->data,
                                        dest->xres, dest->yres, dest->xres,
                                        dest->data,
                                        interpolation, TRUE);

    return dest;
}

/**
 * gwy_field_set_size:
 * @field: A two-dimensional data field.
 * @xres: Desired X resolution.
 * @yres: Desired Y resolution.
 * @clear: %TRUE to fill the new field data with zeroes, %FALSE to leave it
 *         unitialized.
 *
 * Resizes a two-dimensional data field.
 *
 * If the new data size differs from the old data size this method is only
 * marginally more efficient than destroying the old field and creating a new
 * one.
 *
 * In no case the original data are preserved, not even if @xres and @yres are
 * equal to the current field dimensions.  Use gwy_field_new_part() to extract
 * a part of a field into a new field.  Only the dimensions are changed; all
 * other properies, such as physical dimensions, offsets and units, are kept.
 **/
void
gwy_field_set_size(GwyField *field,
                   guint xres,
                   guint yres,
                   gboolean clear)
{
    g_return_if_fail(GWY_IS_FIELD(field));
    g_return_if_fail(xres && yres);

    const gchar *notify[2];
    guint nn = 0;
    if (field->xres != xres)
        notify[nn++] = "x-res";
    if (field->yres != yres)
        notify[nn++] = "y-res";

    if (field->xres*field->yres != xres*yres) {
        free_data(field);
        field->xres = xres;
        field->yres = yres;
        alloc_data(field, clear);
    }
    else {
        field->xres = xres;
        field->yres = yres;
        if (clear)
            gwy_field_clear(field);
        else
            gwy_field_invalidate(field);
    }
    _gwy_notify_properties(G_OBJECT(field), notify, nn);
}

/**
 * gwy_field_data_changed:
 * @field: A two-dimensional data field.
 *
 * Emits signal GwyField::data-changed on a field.
 **/
void
gwy_field_data_changed(GwyField *field)
{
    g_return_if_fail(GWY_IS_FIELD(field));
    g_signal_emit(field, field_signals[DATA_CHANGED], 0);
}

/**
 * gwy_field_copy:
 * @src: Source two-dimensional data field.
 * @dest: Destination two-dimensional data field.
 *
 * Copies the data of a field to another field of the same dimensions.
 *
 * Only the data are copied.  To make a field completely identical to another,
 * including units, offsets and change of dimensions, you can use
 * gwy_field_assign().
 **/
void
gwy_field_copy(const GwyField *src,
               GwyField *dest)
{
    g_return_if_fail(GWY_IS_FIELD(src));
    g_return_if_fail(GWY_IS_FIELD(dest));
    g_return_if_fail(dest->xres == src->xres && dest->yres == src->yres);
    ASSIGN(dest->data, src->data, src->xres * src->yres);
    ASSIGN(dest->priv->cache, src->priv->cache, GWY_FIELD_CACHE_SIZE);
    dest->priv->cached = src->priv->cached;
}

/**
 * gwy_field_part_copy:
 * @src: Source two-dimensional data data field.
 * @col: Column index of the upper-left corner of the rectangle in @src.
 * @row: Row index of the upper-left corner of the rectangle in @src.
 * @width: Rectangle width (number of columns).
 * @height: Rectangle height (number of rows).
 * @dest: Destination two-dimensional data field.
 * @destcol: Destination column in @dest.
 * @destrow: Destination row in @dest.
 *
 * Copies a rectangular part from one field to another.
 *
 * The rectangle starts at (@col, @row) in @src and its dimensions are
 * @width×@height. It is copied to @dest starting from (@destcol, @destrow).
 *
 * There are no limitations on the row and column indices or dimensions.  Only
 * the part of the rectangle that is corrsponds to data inside @src and @dest
 * is copied.  This can also mean nothing is copied at all.
 *
 * If @src is equal to @dest, the areas may not overlap.
 **/
void
gwy_field_part_copy(const GwyField *src,
                    guint col,
                    guint row,
                    guint width,
                    guint height,
                    GwyField *dest,
                    guint destcol,
                    guint destrow)
{
    g_return_if_fail(GWY_IS_FIELD(src));
    g_return_if_fail(GWY_IS_FIELD(dest));

    if (col >= src->xres || destcol >= dest->xres
        || row >= src->yres || destrow >= dest->yres)
        return;

    width = MIN(width, src->xres - col);
    height = MIN(height, src->yres - row);
    width = MIN(width, dest->xres - destcol);
    height = MIN(height, dest->yres - destrow);
    if (!width || !height)
        return;

    if (width == src->xres && width == dest->xres) {
        /* make it as fast as gwy_data_field_copy() if possible */
        g_assert(col == 0 && destcol == 0);
        ASSIGN(dest->data + width*destrow, src->data + width*row, width*height);
    }
    else {
        const gdouble *src0 = dest->data + dest->xres*destrow + destcol;
        gdouble *dest0 = src->data + src->xres*row + col;
        for (guint i = 0; i < height; i++)
            ASSIGN(dest0 + dest->xres*i, src0 + src->xres*i, width);
    }
    gwy_field_invalidate(dest);
}

/**
 * gwy_field_get_data:
 * @field: A two-dimensional data field.
 *
 * Obtains the data of a two-dimensional data field.
 *
 * This is the preferred method to obtain the data array for writing as it
 * invalidates cached values.
 *
 * Do not use it if you only want to read data because it invalidates cached
 * values.
 *
 * Returns: #GwyField-struct.data, but it invalidates the field for you.
 **/
gdouble*
gwy_field_get_data(GwyField *field)
{
    g_return_val_if_fail(GWY_IS_FIELD(field), NULL);
    gwy_field_invalidate(field);
    return field->data;
}

/**
 * gwy_field_invalidate:
 * @field: A two-dimensional data field.
 *
 * Invalidates cached field statistics.
 *
 * User code should seldom need this method since all #GwyField methods
 * correctly invalidate cached values when they change data, also
 * gwy_field_get_data() does.
 *
 * However, if you mix writing to the field data with calls to methods
 * providing overall field characteristics (minimum, maximum, mean value, etc.)
 * you may have to explicitly invalidate the cached values:
 * |[
 * gdouble *data;
 *
 * data = gwy_field_get_data(field);    // This calls gwy_field_invalidate().
 * for (i = 0; i < xres*yres; i++) {
 *     // Change data.
 * }
 * med = gwy_field_median(field);       // This is OK, cache was invalidated.
 *                                      // But now the median is cached.
 * for (i = 0; i < xres*yres; i++) {
 *     // Change data more.
 * }
 * gwy_field_invalidate(field);         // Forget the cached median value.
 * med = gwy_field_median(field);       // OK again, median is recalculated.
 * ]|
 **/
void
gwy_field_invalidate(GwyField *field)
{
    g_return_if_fail(GWY_IS_FIELD(field));
    field->priv->cached = 0;
}

/**
 * gwy_field_set_xreal:
 * @field: A two-dimensional data field.
 * @xreal: Width in physical units.
 *
 * Sets the physical width of a two-dimensional data field.
 **/
void
gwy_field_set_xreal(GwyField *field,
                    gdouble xreal)
{
    g_return_if_fail(GWY_IS_FIELD(field));
    g_return_if_fail(xreal > 0.0);
    if (xreal != field->xreal) {
        field->xreal = xreal;
        g_object_notify(G_OBJECT(field), "x-real");
    }
}

/**
 * gwy_field_set_yreal:
 * @field: A two-dimensional data field.
 * @yreal: Width in physical units.
 *
 * Sets the physical height of a two-dimensional data field.
 **/
void
gwy_field_set_yreal(GwyField *field,
                    gdouble yreal)
{
    g_return_if_fail(GWY_IS_FIELD(field));
    g_return_if_fail(yreal > 0.0);
    if (yreal != field->yreal) {
        field->yreal = yreal;
        g_object_notify(G_OBJECT(field), "y-real");
    }
}

/**
 * gwy_field_set_xoffset:
 * @field: A two-dimensional data field.
 * @xoffset: Horizontal offset of the top-left corner from [0,0] in physical
 *           units.
 *
 * Sets the horizontal offset of a two-dimensional data field.
 **/
void
gwy_field_set_xoffset(GwyField *field,
                      gdouble xoffset)
{
    g_return_if_fail(GWY_IS_FIELD(field));
    if (xoffset != field->xoff) {
        field->xoff = xoffset;
        g_object_notify(G_OBJECT(field), "x-offset");
    }
}

/**
 * gwy_field_set_yoffset:
 * @field: A two-dimensional data field.
 * @yoffset: Vertical offset of the top-left corner from [0,0] in physical
 *           units.
 *
 * Sets the vertical offset of a two-dimensional data field.
 **/
void
gwy_field_set_yoffset(GwyField *field,
                      gdouble yoffset)
{
    g_return_if_fail(GWY_IS_FIELD(field));
    if (yoffset != field->yoff) {
        field->yoff = yoffset;
        g_object_notify(G_OBJECT(field), "y-offset");
    }
}

/**
 * gwy_field_get_unit_xy:
 * @field: A two-dimensional data field.
 *
 * Obtains the lateral units of a two-dimensional data field.
 *
 * Returns: The lateral units of @field.
 **/
GwyUnit*
gwy_field_get_unit_xy(GwyField *field)
{
    g_return_val_if_fail(GWY_IS_FIELD(field), NULL);
    Field *priv = field->priv;
    if (!priv->unit_xy)
        priv->unit_xy = gwy_unit_new();
    return priv->unit_xy;
}

/**
 * gwy_field_get_unit_z:
 * @field: A two-dimensional data field.
 *
 * Obtains the value units of a two-dimensional data field.
 *
 * Returns: The value units of @field.
 **/
GwyUnit*
gwy_field_get_unit_z(GwyField *field)
{
    g_return_val_if_fail(GWY_IS_FIELD(field), NULL);
    Field *priv = field->priv;
    if (!priv->unit_z)
        priv->unit_z = gwy_unit_new();
    return priv->unit_z;
}

/**
 * gwy_field_clear:
 * @field: A two-dimensional data field.
 *
 * Fills a field with zeroes.
 **/
void
gwy_field_clear(GwyField *field)
{
    g_return_if_fail(GWY_IS_FIELD(field));
    gwy_memclear(field->data, field->xres * field->yres);
    Field *priv = field->priv;
    priv->cached = (CBIT(MIN) | CBIT(MAX) | CBIT(AVG) | CBIT(RMS)
                    | CBIT(MED) | CBIT(ARF) | CBIT(ART));
    gwy_memclear(priv->cache, GWY_FIELD_CACHE_SIZE);
}

/**
 * gwy_field_fill:
 * @field: A two-dimensional data field.
 * @value: Value to fill @field with.
 *
 * Fills a field with the specified value.
 **/
void
gwy_field_fill(GwyField *field,
               gdouble value)
{
    if (!value) {
        gwy_field_clear(field);
        return;
    }
    g_return_if_fail(GWY_IS_FIELD(field));
    gdouble *p = field->data;
    for (guint i = field->xres * field->yres; i; i--)
        *(p++) = value;
    Field *priv = field->priv;
    priv->cached = (CBIT(MIN) | CBIT(MAX) | CBIT(AVG) | CBIT(RMS)
                    | CBIT(MED) | CBIT(ARF) | CBIT(ART));
    CVAL(priv, MIN) = value;
    CVAL(priv, MAX) = value;
    CVAL(priv, AVG) = value;
    CVAL(priv, RMS) = 0.0;
    CVAL(priv, MED) = value;
    CVAL(priv, ARF) = value;
    CVAL(priv, ART) = value;
}

/**
 * gwy_field_part_clear:
 * @field: A two-dimensional data field.
 * @col: Column index of the upper-left corner of the rectangle.
 * @row: Row index of the upper-left corner of the rectangle.
 * @width: Rectangle width (number of columns).
 * @height: Rectangle height (number of rows).
 *
 * Fills a rectangular part of a field with zeroes.
 **/
void
gwy_field_part_clear(GwyField *field,
                     guint col,
                     guint row,
                     guint width,
                     guint height)
{
    g_return_if_fail(GWY_IS_FIELD(field));
    g_return_if_fail(col + width <= field->xres);
    g_return_if_fail(row + height <= field->yres);

    if (!width || !height)
        return;
    // This is much better because it sets cached statistics
    if (width == field->xres && height == field->yres) {
        gwy_field_clear(field);
        return;
    }

    gdouble *base = field->data + row*field->xres + col;
    for (guint i = 0; i < height; i++)
        gwy_memclear(base + i*field->xres, width);
}

/**
 * gwy_field_part_fill:
 * @field: A two-dimensional data field.
 * @col: Column index of the upper-left corner of the rectangle.
 * @row: Row index of the upper-left corner of the rectangle.
 * @width: Rectangle width (number of columns).
 * @height: Rectangle height (number of rows).
 * @value: Value to fill the rectangle with.
 *
 * Fills a rectangular part of a field with the specified value.
 **/
void
gwy_field_part_fill(GwyField *field,
                    guint col,
                    guint row,
                    guint width,
                    guint height,
                    gdouble value)
{
    if (!value) {
        gwy_field_part_clear(field, col, row, width, height);
        return;
    }

    g_return_if_fail(GWY_IS_FIELD(field));
    g_return_if_fail(col + width <= field->xres);
    g_return_if_fail(row + height <= field->yres);

    if (!width || !height)
        return;
    // This is much better because it sets cached statistics
    if (width == field->xres && height == field->yres) {
        gwy_field_fill(field, value);
        return;
    }

    for (guint i = 0; i < height; i++) {
        gdouble *p = field->data + (i + row)*field->xres + col;
        for (guint j = width; j; j--)
            *(p++) = value;
    }
}

/**
 * gwy_field_get_format_xy:
 * @field: A two-dimensional data field.
 * @style: Output format style.
 * @format: Value format to update or %NULL to create a new format.
 *
 * Finds a suitable format for displaying coordinates in a data field.
 *
 * The returned format will have sufficient precision to represent coordinates
 * of neighbour pixels as different values.
 *
 * Returns: Either @format (with reference count unchanged) or, if it was
 *          %NULL, a newly created #GwyValueFormat.
 **/
GwyValueFormat*
gwy_field_get_format_xy(GwyField *field,
                        GwyValueFormatStyle style,
                        GwyValueFormat *format)
{
    g_return_val_if_fail(GWY_IS_FIELD(field), NULL);
    gdouble max0 = MAX(field->xreal, field->yreal);
    gdouble maxoff = MAX(fabs(field->xreal + field->xoff),
                         fabs(field->yreal + field->yoff));
    gdouble max = MAX(max0, maxoff);
    gdouble unit = MIN(gwy_field_dx(field), gwy_field_dy(field));
    return gwy_unit_format_with_resolution(gwy_field_get_unit_xy(field),
                                           style, max, unit, format);
}

/**
 * gwy_field_get_format_z:
 * @field: A two-dimensional data field.
 * @style: Output format style.
 * @format: Value format to update or %NULL to create a new format.
 *
 * Finds a suitable format for displaying values in a data field.
 *
 * Returns: Either @format (with reference count unchanged) or, if it was
 *          %NULL, a newly created #GwyValueFormat.
 **/
GwyValueFormat*
gwy_field_get_format_z(GwyField *field,
                       GwyValueFormatStyle style,
                       GwyValueFormat *format)
{
    g_return_val_if_fail(GWY_IS_FIELD(field), NULL);
    gdouble min, max;
    gwy_field_min_max(field, &min, &max);
    if (max == min) {
        max = ABS(max);
        min = 0.0;
    }
    return gwy_unit_format_with_digits(gwy_field_get_unit_z(field),
                                       style, max - min, 3, format);
}

#define __LIBGWY_FIELD_C__
#include "libgwy/libgwy-aliases.c"

/**
 * SECTION: field
 * @title: GwyField
 * @short_description: Two-dimensional data in regular grid
 *
 * #GwyField represents two dimensional data in a regular grid.
 *
 * Data are stored in a flat array #GwyField-struct.data of #gdouble values,
 * stored by rows.  This means the column index is the fast index, row index is
 * the slow one, the top left corner indices are (0,0).  The array is
 * contiguous, i.e. there is no padding at the end of each row (and neither
 * beween pixels).  No methods to get and set individual values or rows and
 * columns are provided except gwy_field_index().  The usual mode of operation
 * is to access the data directly, bearing a few things in mind:
 * <itemizedlist>
 *   <listitem>All #GwyField struct fields must be considered read-only. You
 *   may write to #GwyField-struct.data <emphasis>content</emphasis> but you
 *   must not change the field itself.  Use methods such as
 *   gwy_field_set_xreal() to change the field properties.</listitem>
 *   <listitem>For writing, obtain the data with gwy_field_get_data().  This
 *   tells the field that you are going to change the data and invalidates any
 *   caches.  For reading, just access #GwyField-struct.data.</listitem>
 *   <listitem>If you mix direct changes of data with functions that obtain
 *   overall field statistics, you may have to use gwy_field_invalidate() to
 *   induce recalculation of the statistics.</listitem>
 *   <listitem>Always try to access the field data sequentially, as they are
 *   stored in memory.  Even if it means for instance allocating a buffer to
 *   hold a value for each column.  More on that below.</listitem>
 * </itemizedlist>
 *
 * <refsect2 id='GwyField-memory-access-patterns'>
 * <title>Importance of Sequential Memory Access</title>
 * <para>Consider the calculation of sums of all columns in a square field.
 * Denoting @n the field dimension, @data its data, and @sums the array we
 * store the results to, the naïve implementation could be:</para>
 * |[
 * // Column-wise (scattered) data access
 * for (guint j = 0; j < n; j++) {
 *     gdouble s = 0.0;
 *     for (guint i = 0; i < n; i++)
 *         s += data[i*n + j];
 *     sums[j] += s;
 * }
 * ]|
 * <para>Since it always sums the entire column into the local variable @s
 * instead of @sums, it might even seem slightly preferable to sequential
 * processing of data:</para>
 * |[
 * // Row-wise (sequential) data access
 * for (guint i = 0; i < n; i++) {
 *     for (guint j = 0; j < n; j++)
 *         sums[j] += data[i*n + j];
 * }
 * ]|
 * <para>Nothing could be farther from truth.  The following figure compares
 * the processing time per pixel for both memory access patterns (on a Phenom
 * II machine with plenty of RAM):</para>
 * <informalfigure id='GwyField-fig-memory-access-ratio'>
 *   <mediaobject>
 *     <imageobject>
 *       <imagedata fileref='memory-access.png' format='PNG'/>
 *     </imageobject>
 *   </mediaobject>
 * </informalfigure>
 * <para>The column-wise implementation is <emphasis>much</emphasis> slower
 * even for medium-sized fields because of terrible cache-trashing.  Detailed
 * explanation and more examples can be found in Urich Drepper's <emphasis>What
 * Every Programmer Should Know About Memory</emphasis>.</para>
 * </refsect2>
 **/

/**
 * GwyField:
 * @xres: X-resolution, i.e. width in pixels.
 * @yres: Y-resolution, i.e. height in pixels.
 * @xreal: Width in physical units.
 * @yreal: Height in physical units.
 * @xoff: X-offset of the top-left corner from [0,0] in physical units.
 * @yoff: Y-offset of the top-left corner from [0,0] in physical units.
 * @data: Field data.  See the introductory section for details.
 *
 * Object representing two-dimensional data in a regular grid.
 *
 * The #GwyField struct contains some public fields that can be directly
 * accessed for reading.  To set them, you must use the methods such as
 * gwy_field_set_xreal().
 **/

/**
 * GwyFieldClass:
 * @g_object_class: Parent class.
 *
 * Class of two-dimensional data fields.
 **/

/**
 * gwy_field_duplicate:
 * @field: A two-dimensional data field.
 *
 * Duplicates a two-dimensional data field.
 *
 * This is a convenience wrapper of gwy_serializable_duplicate().
 **/

/**
 * gwy_field_assign:
 * @dest: Destination two-dimensional data field.
 * @src: Source two-dimensional data field.
 *
 * Copies the value of a two-dimensional data field.
 *
 * This is a convenience wrapper of gwy_serializable_assign().
 **/

/**
 * gwy_field_index:
 * @field: A two-dimensional data field.
 * @col: Column index in @field.
 * @row: Row index in @field.
 *
 * Accesses a two-dimensional data field pixel.
 *
 * This macro may evaluate its arguments several times.
 * This macro expands to a left-hand side expression.
 *
 * No argument validation is performed.  If you process the data in a loop,
 * you are encouraged to access #GwyField-struct.data directly.
 **/

/**
 * gwy_field_dx:
 * @field: A two-dimensional data field.
 *
 * Calculates the horizontal pixel size in physical units.
 *
 * This macro may evaluate its arguments several times.
 **/

/**
 * gwy_field_dy:
 * @field: A two-dimensional data field.
 *
 * Calculates the vertical pixel size in physical units.
 *
 * This macro may evaluate its arguments several times.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
