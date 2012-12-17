/*
 *  $Id$
 *  Copyright (C) 2009-2012 David Nečas (Yeti).
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
#include "libgwy/object-utils.h"
#include "libgwy/math.h"
#include "libgwy/serialize.h"
#include "libgwy/field.h"
#include "libgwy/field-arithmetic.h"
#include "libgwy/field-statistics.h"
#include "libgwy/object-internal.h"
#include "libgwy/field-internal.h"

#define CVAL GWY_FIELD_CVAL
#define CBIT GWY_FIELD_CBIT
#define CTEST GWY_FIELD_CTEST

enum { N_ITEMS = 11 };

enum {
    SGNL_DATA_CHANGED,
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
    PROP_UNIT_X,
    PROP_UNIT_Y,
    PROP_UNIT_Z,
    PROP_NAME,
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
    /*00*/ { .name = "xres",   .ctype = GWY_SERIALIZABLE_INT32,        .value.v_uint32 = 1   },
    /*01*/ { .name = "yres",   .ctype = GWY_SERIALIZABLE_INT32,        .value.v_uint32 = 1   },
    /*02*/ { .name = "xreal",  .ctype = GWY_SERIALIZABLE_DOUBLE,       .value.v_double = 1.0 },
    /*03*/ { .name = "yreal",  .ctype = GWY_SERIALIZABLE_DOUBLE,       .value.v_double = 1.0 },
    /*04*/ { .name = "xoff",   .ctype = GWY_SERIALIZABLE_DOUBLE,       },
    /*05*/ { .name = "yoff",   .ctype = GWY_SERIALIZABLE_DOUBLE,       },
    /*06*/ { .name = "unit-x", .ctype = GWY_SERIALIZABLE_OBJECT,       },
    /*07*/ { .name = "unit-y", .ctype = GWY_SERIALIZABLE_OBJECT,       },
    /*08*/ { .name = "unit-z", .ctype = GWY_SERIALIZABLE_OBJECT,       },
    /*09*/ { .name = "name",   .ctype = GWY_SERIALIZABLE_STRING,       },
    /*10*/ { .name = "data",   .ctype = GWY_SERIALIZABLE_DOUBLE_ARRAY, },
};

static guint signals[N_SIGNALS];
static GParamSpec *properties[N_PROPS];

G_DEFINE_TYPE_EXTENDED
    (GwyField, gwy_field, G_TYPE_OBJECT, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_field_serializable_init));

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

    properties[PROP_XRES]
        = g_param_spec_uint("x-res",
                            "X resolution",
                            "Pixel width of the field.",
                            1, G_MAXUINT, 1,
                            G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_YRES]
        = g_param_spec_uint("y-res",
                            "Y resolution",
                            "Pixel height of the field.",
                            1, G_MAXUINT, 1,
                            G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_XREAL]
        = g_param_spec_double("x-real",
                              "X real size",
                              "Width of the field in physical units.",
                              G_MINDOUBLE, G_MAXDOUBLE, 1.0,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_YREAL]
        = g_param_spec_double("y-real",
                              "Y real size",
                              "Height of the field in physical units.",
                              G_MINDOUBLE, G_MAXDOUBLE, 1.0,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_XOFFSET]
        = g_param_spec_double("x-offset",
                              "X offset",
                              "Horizontal offset of the field top left corner "
                              "in physical units.",
                              -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_YOFFSET]
        = g_param_spec_double("y-offset",
                              "Y offset",
                              "Vertical offset of the field top left corner "
                              "in physical units.",
                              -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_UNIT_X]
        = g_param_spec_object("unit-x",
                              "X unit",
                              "Physical units of horizontal lateral dimensions "
                              "of the field.",
                              GWY_TYPE_UNIT,
                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_UNIT_Y]
        = g_param_spec_object("unit-y",
                              "Y unit",
                              "Physical units of vertical lateral dimensions "
                              "of the field.",
                              GWY_TYPE_UNIT,
                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_UNIT_Z]
        = g_param_spec_object("unit-z",
                              "Z unit",
                              "Physical units of field values.",
                              GWY_TYPE_UNIT,
                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_NAME]
        = g_param_spec_string("name",
                              "Name",
                              "Name of the field.",
                              NULL,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, properties[i]);

    /**
     * GwyField::data-changed:
     * @gwyfield: The #GwyField which received the signal.
     * @arg1: (allow-none):
     *        Area in @gwyfield that has changed.
     *        It may be %NULL, meaning the entire field.
     *
     * The ::data-changed signal is emitted when field data changes.
     * More precisely, #GwyField itself never emits this signal.  You can emit
     * it explicitly with gwy_field_data_changed() to notify anything that
     * displays (or otherwise uses) the field.
     **/
    signals[SGNL_DATA_CHANGED]
        = g_signal_new_class_handler("data-changed",
                                     G_OBJECT_CLASS_TYPE(klass),
                                     G_SIGNAL_RUN_FIRST,
                                     NULL, NULL, NULL,
                                     g_cclosure_marshal_VOID__BOXED,
                                     G_TYPE_NONE, 1,
                                     GWY_TYPE_FIELD_PART);
}

static void
gwy_field_init(GwyField *field)
{
    field->priv = G_TYPE_INSTANCE_GET_PRIVATE(field, GWY_TYPE_FIELD, Field);
    field->xres = field->yres = 1;
    field->xreal = field->yreal = 1.0;
    field->data = &field->priv->storage;
}

void
_gwy_field_set_cache_for_flat(GwyField *field,
                              gdouble value)
{
    Field *priv = field->priv;
    priv->cached = (CBIT(MIN) | CBIT(MAX) | CBIT(AVG) | CBIT(RMS) | CBIT(MSQ)
                    | CBIT(MED) | CBIT(ARE));
    CVAL(priv, MIN) = value;
    CVAL(priv, MAX) = value;
    CVAL(priv, AVG) = value;
    CVAL(priv, RMS) = 0.0;
    CVAL(priv, MSQ) = value*value;
    CVAL(priv, MED) = value;
    CVAL(priv, ARE) = field->xreal * field->yreal;
}

static void
alloc_data(GwyField *field,
           gboolean clear)
{
    if (clear) {
        field->data = g_new0(gdouble, field->xres * field->yres);
        _gwy_field_set_cache_for_flat(field, 0.0);
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
        field->data = NULL;
}

static void
gwy_field_finalize(GObject *object)
{
    GwyField *field = GWY_FIELD(object);
    GWY_FREE(field->priv->name);
    free_data(field);
    G_OBJECT_CLASS(gwy_field_parent_class)->finalize(object);
}

static void
gwy_field_dispose(GObject *object)
{
    GwyField *field = GWY_FIELD(object);
    GWY_OBJECT_UNREF(field->priv->unit_y);
    GWY_OBJECT_UNREF(field->priv->unit_x);
    GWY_OBJECT_UNREF(field->priv->unit_z);
    G_OBJECT_CLASS(gwy_field_parent_class)->dispose(object);
}

static gsize
gwy_field_n_items(GwySerializable *serializable)
{
    GwyField *field = GWY_FIELD(serializable);
    Field *priv = field->priv;
    gsize n = N_ITEMS;
    if (priv->unit_x)
       n += gwy_serializable_n_items(GWY_SERIALIZABLE(priv->unit_x));
    if (priv->unit_y)
       n += gwy_serializable_n_items(GWY_SERIALIZABLE(priv->unit_y));
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

    _gwy_serialize_double(field->xreal, serialize_items + 2, items, &n);
    _gwy_serialize_double(field->yreal, serialize_items + 3, items, &n);
    _gwy_serialize_double(field->xoff, serialize_items + 4, items, &n);
    _gwy_serialize_double(field->yoff, serialize_items + 5, items, &n);
    _gwy_serialize_unit(priv->unit_x, serialize_items + 6, items, &n);
    _gwy_serialize_unit(priv->unit_y, serialize_items + 7, items, &n);
    _gwy_serialize_unit(priv->unit_z, serialize_items + 8, items, &n);
    _gwy_serialize_string(priv->name, serialize_items + 9, items, &n);

    g_return_val_if_fail(items->len - items->n, 0);
    it = serialize_items[10];
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
    GwyField *field = GWY_FIELD(serializable);
    Field *priv = field->priv;

    GwySerializableItem its[N_ITEMS];
    memcpy(its, serialize_items, sizeof(serialize_items));
    gwy_deserialize_filter_items(its, N_ITEMS, items, NULL,
                                 "GwyField", error_list);

    if (!_gwy_check_data_dimension(error_list, "GwyField", 2,
                                   its[0].value.v_uint32,
                                   its[1].value.v_uint32))
        goto fail;

    if (G_UNLIKELY(its[0].value.v_uint32 * its[1].value.v_uint32
                   != its[10].array_size)) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_INVALID,
                           // TRANSLATORS: Error message.
                           _("GwyField dimensions %u×%u do not match data size "
                             "%lu."),
                           its[0].value.v_uint32, its[1].value.v_uint32,
                           (gulong)its[10].array_size);
        goto fail;
    }

    if (!_gwy_check_object_component(its + 6, field, GWY_TYPE_UNIT, error_list))
        goto fail;
    if (!_gwy_check_object_component(its + 7, field, GWY_TYPE_UNIT, error_list))
        goto fail;
    if (!_gwy_check_object_component(its + 8, field, GWY_TYPE_UNIT, error_list))
        goto fail;

    field->xres = its[0].value.v_uint32;
    field->yres = its[1].value.v_uint32;
    // FIXME: Catch near-zero and near-infinity values.
    field->xreal = CLAMP(its[2].value.v_double, G_MINDOUBLE, G_MAXDOUBLE);
    field->yreal = CLAMP(its[3].value.v_double, G_MINDOUBLE, G_MAXDOUBLE);
    field->xoff = CLAMP(its[4].value.v_double, -G_MAXDOUBLE, G_MAXDOUBLE);
    field->yoff = CLAMP(its[5].value.v_double, -G_MAXDOUBLE, G_MAXDOUBLE);
    priv->unit_x = (GwyUnit*)its[6].value.v_object;
    priv->unit_y = (GwyUnit*)its[7].value.v_object;
    priv->unit_z = (GwyUnit*)its[8].value.v_object;
    free_data(field);
    priv->name = its[9].value.v_string;
    field->data = its[10].value.v_double_array;
    priv->allocated = TRUE;

    return TRUE;

fail:
    GWY_OBJECT_UNREF(its[6].value.v_object);
    GWY_OBJECT_UNREF(its[7].value.v_object);
    GWY_OBJECT_UNREF(its[8].value.v_object);
    GWY_FREE(its[9].value.v_string);
    GWY_FREE(its[10].value.v_double_array);
    return FALSE;
}

static GObject*
gwy_field_duplicate_impl(GwySerializable *serializable)
{
    GwyField *field = GWY_FIELD(serializable);
    Field *priv = field->priv;

    GwyField *duplicate = gwy_field_new_alike(field, FALSE);
    Field *dpriv = duplicate->priv;

    gwy_assign(duplicate->data, field->data, field->xres*field->yres);
    gwy_assign(dpriv->cache, priv->cache, GWY_FIELD_CACHE_SIZE);
    gwy_assign_string(&dpriv->name, priv->name);
    dpriv->cached = priv->cached;

    return G_OBJECT(duplicate);
}

// Does NOT copy name for use in new-alike-type functions.
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
    _gwy_assign_unit(&dpriv->unit_x, spriv->unit_x);
    _gwy_assign_unit(&dpriv->unit_y, spriv->unit_y);
    _gwy_assign_unit(&dpriv->unit_z, spriv->unit_z);
}

static void
gwy_field_assign_impl(GwySerializable *destination,
                      GwySerializable *source)
{
    GwyField *dest = GWY_FIELD(destination);
    GwyField *src = GWY_FIELD(source);
    Field *spriv = src->priv, *dpriv = dest->priv;

    GParamSpec *notify[N_PROPS];
    guint nn = 0;
    if (dest->xres != src->xres)
        notify[nn++] = properties[PROP_XRES];
    if (dest->yres != src->yres)
        notify[nn++] = properties[PROP_YRES];
    if (dest->xreal != src->xreal)
        notify[nn++] = properties[PROP_XREAL];
    if (dest->yreal != src->yreal)
        notify[nn++] = properties[PROP_YREAL];
    if (dest->xoff != src->xoff)
        notify[nn++] = properties[PROP_XOFFSET];
    if (dest->yoff != src->yoff)
        notify[nn++] = properties[PROP_YOFFSET];
    if (gwy_assign_string(&dpriv->name, spriv->name))
        notify[nn++] = properties[PROP_NAME];

    if (dest->xres * dest->yres != src->xres * src->yres) {
        free_data(dest);
        dest->data = g_new(gdouble, src->xres * src->yres);
        dpriv->allocated = TRUE;
    }
    gwy_assign(dest->data, src->data, src->xres * src->yres);
    copy_info(dest, src);
    gwy_assign(dpriv->cache, spriv->cache, GWY_FIELD_CACHE_SIZE);
    dpriv->cached = spriv->cached;
    _gwy_notify_properties_by_pspec(G_OBJECT(dest), notify, nn);
}

static void
gwy_field_set_property(GObject *object,
                       guint prop_id,
                       const GValue *value,
                       GParamSpec *pspec)
{
    GwyField *field = GWY_FIELD(object);
    gdouble dblvalue;

    switch (prop_id) {
        case PROP_XREAL:
        dblvalue = g_value_get_double(value);
        if (field->xreal != dblvalue) {
            field->priv->cached &= ~CBIT(ARE);
            field->xreal = dblvalue;
        }
        break;

        case PROP_YREAL:
        dblvalue = g_value_get_double(value);
        if (field->yreal != dblvalue) {
            field->priv->cached &= ~CBIT(ARE);
            field->yreal = dblvalue;
        }
        break;

        case PROP_XOFFSET:
        field->xoff = g_value_get_double(value);
        break;

        case PROP_YOFFSET:
        field->yoff = g_value_get_double(value);
        break;

        case PROP_NAME:
        gwy_assign_string(&field->priv->name, g_value_get_string(value));
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

        // Instantiate the units to be consistent with the direct interface
        // that never admits the units are NULL.
        case PROP_UNIT_X:
        if (!priv->unit_x)
            priv->unit_x = gwy_unit_new();
        g_value_set_object(value, priv->unit_x);
        break;

        case PROP_UNIT_Y:
        if (!priv->unit_y)
            priv->unit_y = gwy_unit_new();
        g_value_set_object(value, priv->unit_y);
        break;

        case PROP_UNIT_Z:
        if (!priv->unit_z)
            priv->unit_z = gwy_unit_new();
        g_value_set_object(value, priv->unit_z);
        break;

        case PROP_NAME:
        g_value_set_string(value, priv->name);
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
 * parameterless constructor exists mainly for language bindings,
 * gwy_field_new_sized() and gwy_field_new_alike() are usually more useful.
 *
 * Returns: (transfer full):
 *          A new two-dimensional data field.
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
 *         uninitialised.
 *
 * Creates a new two-dimensional data field of specified dimensions.
 *
 * Returns: (transfer full):
 *          A new two-dimensional data field.
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
 *         uninitialised.
 *
 * Creates a new two-dimensional data field similar to another field.
 *
 * All properties of the newly created field will be identical to @model,
 * except the data that will be either zeroes or uninitialised, and name which
 * will be unset.  Use gwy_field_duplicate() to completely duplicate a field
 * including data.
 *
 * Returns: (transfer full):
 *          A new two-dimensional data field.
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
 * @fpart: (allow-none):
 *         Area in @field to extract to the new field.  Passing %NULL
 *         creates an identical copy of @field, similarly to
 *         gwy_field_duplicate() (though with @keep_offsets set to %FALSE
 *         the offsets are reset).
 * @keep_offsets: %TRUE to set the X and Y offsets of the new field
 *                using @fpart and @field offsets.  %FALSE to set offsets
 *                of the new field to zeroes.
 *
 * Creates a new two-dimensional field as a part of another field.
 *
 * The part specified by @fpart must be entirely contained in @field.
 * Both dimensions must be non-zero.
 *
 * Data are physically copied, i.e. changing the new field data does not change
 * @field's data and vice versa.  Physical dimensions of the new field are
 * calculated to correspond to the extracted part.
 *
 * Returns: (transfer full):
 *          A new two-dimensional data field.
 **/
GwyField*
gwy_field_new_part(const GwyField *field,
                   const GwyFieldPart *fpart,
                   gboolean keep_offsets)
{
    guint col, row, width, height;
    if (!gwy_field_check_part(field, fpart, &col, &row, &width, &height))
        return NULL;

    if (width == field->xres && height == field->yres) {
        GwyField *part = gwy_field_duplicate(field);
        if (!keep_offsets)
            part->xoff = part->yoff = 0.0;
        return part;
    }

    GwyField *part = gwy_field_new_sized(width, height, FALSE);
    gwy_field_copy(field, fpart, part, 0, 0);
    part->xreal = width*gwy_field_dx(field);
    part->yreal = height*gwy_field_dy(field);

    Field *spriv = field->priv, *dpriv = part->priv;
    _gwy_assign_unit(&dpriv->unit_x, spriv->unit_x);
    _gwy_assign_unit(&dpriv->unit_y, spriv->unit_y);
    _gwy_assign_unit(&dpriv->unit_z, spriv->unit_z);
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
 * Returns: (transfer full):
 *          A new two-dimensional data field.
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
    gwy_interpolation_resample_block_2d(field->data,
                                        field->xres, field->yres, field->xres,
                                        dest->data,
                                        dest->xres, dest->yres, dest->xres,
                                        interpolation, TRUE);

    return dest;
}

/**
 * gwy_field_new_from_mask:
 * @mask: Two-dimensional mask field used as a template for the created field.
 * @zero: Field value that will correspond to zeroes in the mask.
 * @one: Field value that will correspond to ones in the mask.
 *
 * Creates a new two-dimensional data field from a mask field.
 *
 * Returns: (transfer full):
 *          A new two-dimensional data field.
 **/
GwyField*
gwy_field_new_from_mask(const GwyMaskField *mask,
                        gdouble zero,
                        gdouble one)
{
    g_return_val_if_fail(GWY_IS_MASK_FIELD(mask), NULL);

    guint xres = mask->xres, yres = mask->yres;
    GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
    GwyMaskIter iter;
    for (guint i = 0; i < yres; i++) {
        gdouble *d = field->data + i*xres;
        gwy_mask_field_iter_init(mask, iter, 0, i);
        for (guint j = xres; j; j--, d++) {
            *d = gwy_mask_iter_get(iter) ? one : zero;
            gwy_mask_iter_next(iter);
        }
    }

    return field;
}

/**
 * gwy_field_set_size:
 * @field: A two-dimensional data field.
 * @xres: Desired X resolution.
 * @yres: Desired Y resolution.
 * @clear: %TRUE to fill the new field data with zeroes, %FALSE to leave it
 *         uninitialised.
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

    GParamSpec *notify[2];
    guint nn = 0;
    if (field->xres != xres)
        notify[nn++] = properties[PROP_XRES];
    if (field->yres != yres)
        notify[nn++] = properties[PROP_YRES];

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
            gwy_field_clear_full(field);
        else
            gwy_field_invalidate(field);
    }
    _gwy_notify_properties_by_pspec(G_OBJECT(field), notify, nn);
}

/**
 * gwy_field_data_changed:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *         Area in @field that has changed.  Passing %NULL means the entire
 *         field.
 *
 * Emits signal GwyField::data-changed on a field.
 **/
void
gwy_field_data_changed(GwyField *field,
                       GwyFieldPart *fpart)
{
    g_return_if_fail(GWY_IS_FIELD(field));
    g_signal_emit(field, signals[SGNL_DATA_CHANGED], 0, fpart);
}

/**
 * gwy_field_copy:
 * @src: Source two-dimensional data data field.
 * @srcpart: Area in field @src to copy.  Pass %NULL to copy entire @src.
 * @dest: Destination two-dimensional data field.
 * @destcol: Destination column in @dest.
 * @destrow: Destination row in @dest.
 *
 * Copies data from one field to another.
 *
 * The copied rectangle is defined by @srcpart and it is copied to @dest
 * starting from (@destcol, @destrow).
 *
 * There are no limitations on the row and column indices or dimensions.  Only
 * the part of the rectangle that corresponds to data inside @src and @dest
 * is copied.  This can also mean no data are copied at all.
 *
 * If @src is equal to @dest the areas may <emphasis>not</emphasis> overlap.
 **/
void
gwy_field_copy(const GwyField *src,
               const GwyFieldPart *srcpart,
               GwyField *dest,
               guint destcol,
               guint destrow)
{
    guint col, row, width, height;
    if (!gwy_field_limit_parts(src, srcpart, dest, destcol, destrow,
                               FALSE, &col, &row, &width, &height))
        return;

    if (width == src->xres && width == dest->xres) {
        g_assert(col == 0 && destcol == 0);
        gwy_assign(dest->data + width*destrow, src->data + width*row,
                   width*height);
        if (height == src->yres && height == dest->yres) {
            gwy_assign(dest->priv->cache, src->priv->cache,
                       GWY_FIELD_CACHE_SIZE);
            dest->priv->cached = src->priv->cached;
            if (dest->xreal != src->xreal || dest->yreal != src->yreal)
                dest->priv->cached &= ~CBIT(ARE);
        }
        else
            gwy_field_invalidate(dest);
    }
    else {
        const gdouble *src0 = src->data + src->xres*row + col;
        gdouble *dest0 = dest->data + dest->xres*destrow + destcol;
        for (guint i = 0; i < height; i++)
            gwy_assign(dest0 + dest->xres*i, src0 + src->xres*i, width);
        gwy_field_invalidate(dest);
    }
}

/**
 * gwy_field_copy_full:
 * @src: Source two-dimensional data field.
 * @dest: Destination two-dimensional data field.
 *
 * Copies the entire data from one field to another.
 *
 * The two fields must be of identical dimensions.
 *
 * Only the data are copied.  To make a field completely identical to another,
 * including units, offsets and change of dimensions, you can use
 * gwy_field_assign().
 **/
void
gwy_field_copy_full(const GwyField *src,
                    GwyField *dest)
{
    g_return_if_fail(GWY_IS_FIELD(src));
    g_return_if_fail(GWY_IS_FIELD(dest));
    // This is a sanity check as gwy_field_copy() can handle anything.
    g_return_if_fail(src->xres == dest->xres && src->yres == dest->yres);
    gwy_field_copy(src, NULL, dest, 0, 0);
}

/**
 * gwy_field_invalidate:
 * @field: A two-dimensional data field.
 *
 * Invalidates cached field statistics.
 *
 * All #GwyField methods invalidate (or, in some cases, recalculate) cached
 * statistics if they modify the data.
 *
 * If you write to @field's data directly and namely mix writing to the field
 * data with calls to methods providing overall field characteristics (minimum,
 * maximum, mean value, etc.) you may have to explicitly invalidate the cached
 * values as the methods have no means of knowing whether you changed the data
 * meanwhile or not:
 * |[
 * gdouble *data = field->data;
 *
 * for (i = 0; i < xres*yres; i++) {
 *     // Change data.
 * }
 * gwy_field_invalidate(data);          // Invalidate data as we changed it.
 * med = gwy_field_median_full(field);  // This is OK, cache was invalidated.
 *                                      // But now the new median is cached.
 * for (i = 0; i < xres*yres; i++) {
 *     // Change data more.
 * }
 * gwy_field_invalidate(field);         // Forget the cached median value.
 * med = gwy_field_median_full(field);  // OK again, median is recalculated.
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
        Field *priv = field->priv;
        priv->cached &= ~CBIT(ARE);
        field->xreal = xreal;
        g_object_notify_by_pspec(G_OBJECT(field), properties[PROP_XREAL]);
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
        Field *priv = field->priv;
        priv->cached &= ~CBIT(ARE);
        field->yreal = yreal;
        g_object_notify_by_pspec(G_OBJECT(field), properties[PROP_YREAL]);
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
        g_object_notify_by_pspec(G_OBJECT(field), properties[PROP_XOFFSET]);
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
        g_object_notify_by_pspec(G_OBJECT(field), properties[PROP_YOFFSET]);
    }
}

/**
 * gwy_field_clear_offsets:
 * @field: A two-dimensional data field.
 *
 * Sets the offsets of a two-dimensional data field to zeroes.
 **/
void
gwy_field_clear_offsets(GwyField *field)
{
    g_return_if_fail(GWY_IS_FIELD(field));

    GParamSpec *notify[2];
    guint nn = 0;
    if (field->xoff) {
        field->xoff = 0.0;
        notify[nn++] = properties[PROP_XOFFSET];
    }
    if (field->yoff) {
        field->yoff = 0.0;
        notify[nn++] = properties[PROP_YOFFSET];
    }
    _gwy_notify_properties_by_pspec(G_OBJECT(field), notify, nn);
}

/**
 * gwy_field_get_unit_x:
 * @field: A two-dimensional data field.
 *
 * Obtains the horizontal lateral units of a two-dimensional data field.
 *
 * Returns: (transfer none):
 *          The horizontal lateral units of @field.
 **/
GwyUnit*
gwy_field_get_unit_x(const GwyField *field)
{
    g_return_val_if_fail(GWY_IS_FIELD(field), NULL);
    Field *priv = field->priv;
    if (!priv->unit_x)
        priv->unit_x = gwy_unit_new();
    return priv->unit_x;
}

/**
 * gwy_field_get_unit_y:
 * @field: A two-dimensional data field.
 *
 * Obtains the vertical lateral units of a two-dimensional data field.
 *
 * Returns: (transfer none):
 *          The vertical lateral units of @field.
 **/
GwyUnit*
gwy_field_get_unit_y(const GwyField *field)
{
    g_return_val_if_fail(GWY_IS_FIELD(field), NULL);
    Field *priv = field->priv;
    if (!priv->unit_y)
        priv->unit_y = gwy_unit_new();
    return priv->unit_y;
}

/**
 * gwy_field_get_unit_z:
 * @field: A two-dimensional data field.
 *
 * Obtains the value units of a two-dimensional data field.
 *
 * Returns: (transfer none):
 *          The value units of @field.
 **/
GwyUnit*
gwy_field_get_unit_z(const GwyField *field)
{
    g_return_val_if_fail(GWY_IS_FIELD(field), NULL);
    Field *priv = field->priv;
    if (!priv->unit_z)
        priv->unit_z = gwy_unit_new();
    return priv->unit_z;
}

/**
 * gwy_field_xy_units_match:
 * @field: A two-dimensional data field.
 *
 * Checks whether a two-dimensional data field has the same lateral units in
 * both coordinates.
 *
 * Returns: %TRUE if @x and @y units of @field are equal, %FALSE if they
 *          differ.
 **/
gboolean
gwy_field_xy_units_match(const GwyField *field)
{
    g_return_val_if_fail(GWY_IS_FIELD(field), FALSE);
    Field *priv = field->priv;
    return gwy_unit_equal(priv->unit_x, priv->unit_y);
}

/**
 * gwy_field_xyz_units_match:
 * @field: A two-dimensional data field.
 *
 * Checks whether a two-dimensional data field has the same both lateral units
 * and value units.
 *
 * Returns: %TRUE if @x, @y and @z units of @field are equal, %FALSE if any
 *          two differ.
 **/
gboolean
gwy_field_xyz_units_match(const GwyField *field)
{
    g_return_val_if_fail(GWY_IS_FIELD(field), FALSE);
    Field *priv = field->priv;
    return (gwy_unit_equal(priv->unit_x, priv->unit_y)
            && gwy_unit_equal(priv->unit_x, priv->unit_z));
}

/**
 * gwy_field_check_part:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *         Area in @field, possibly %NULL.
 * @col: Location to store the actual column index of the upper-left corner
 *       of the part.
 * @row: Location to store the actual row index of the upper-left corner
 *       of the part.
 * @width: Location to store the actual width (number of columns)
 *         of the part.
 * @height: Location to store the actual height (number of rows)
 *          of the part.
 *
 * Validates the position and dimensions of a field part.
 *
 * If @fpart is %NULL entire @field is to be used.  Otherwise @fpart must be
 * contained in @field.
 *
 * If the position and dimensions are valid @col, @row, @width and @height are
 * set to the actual rectangular part in @field.  If the function returns
 * %FALSE their values are undefined.
 *
 * This function is typically used in functions that operate on a part of a
 * field but do not work with masks.  See gwy_field_check_mask() for checking
 * of part and mask together.  Example (note gwy_field_new_congruent() can
 * create transposed, rotated and flipped fields):
 * |[
 * GwyField*
 * transpose_field(const GwyField *field,
 *                 const GwyFieldPart *fpart)
 * {
 *     guint col, row, width, height;
 *     if (!gwy_field_check_part(field, fpart,
 *                               &col, &row, &width, &height))
 *         return NULL;
 *
 *     // Perform the transposition of @field part given by @col, @row,
 *     // @width and @height...
 * }
 * ]|
 *
 * Returns: %TRUE if the position and dimensions are valid and the caller
 *          should proceed.  %FALSE if the caller should not proceed, either
 *          because @field is not a #GwyField instance or the position or
 *          dimensions is invalid (a critical error is emitted in these cases)
 *          or the actual part is zero-sized.
 **/
gboolean
gwy_field_check_part(const GwyField *field,
                     const GwyFieldPart *fpart,
                     guint *col, guint *row,
                     guint *width, guint *height)
{
    g_return_val_if_fail(GWY_IS_FIELD(field), FALSE);
    if (fpart) {
        if (!fpart->width || !fpart->height)
            return FALSE;
        // The two separate conditions are to catch integer overflows.
        g_return_val_if_fail(fpart->col < field->xres, FALSE);
        g_return_val_if_fail(fpart->width <= field->xres - fpart->col,
                             FALSE);
        g_return_val_if_fail(fpart->row < field->yres, FALSE);
        g_return_val_if_fail(fpart->height <= field->yres - fpart->row,
                             FALSE);
        *col = fpart->col;
        *row = fpart->row;
        *width = fpart->width;
        *height = fpart->height;
    }
    else {
        *col = *row = 0;
        *width = field->xres;
        *height = field->yres;
    }

    return TRUE;
}

/**
 * gwy_field_check_target_part:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *         Target area in @field, possibly %NULL.
 * @width_full: Number of columns of the entire source.
 * @height_full: Number of rows of the entire source.
 * @col: Location to store the actual column index of the upper-left corner
 *       of the target part.
 * @row: Location to store the actual row index of the upper-left corner
 *       of the target part.
 * @width: Location to store the actual width (number of columns)
 *         of the target part.
 * @height: Location to store the actual height (number of rows)
 *          of the target part.
 *
 * Validates the position and dimensions of a target field part for extraction.
 *
 * If @fpart is %NULL the dimensions of @field must match the entire source,
 * i.e. (@width_full,@height_full).  Otherwise @fpart must be contained in
 * @field, except if its dimensions match the entire field in which case the
 * offsets can be arbitrary as they pertain to the source only.
 *
 * If the position and dimensions are valid @col, @row, @width and @height are
 * set to the actual rectangular part in @field.  If the function returns
 * %FALSE their values are undefined.
 *
 * This function is typically used if a field is extracted from a larger data.
 * Example (note gwy_brick_extract_plane() extracts planes from a brick):
 * |[
 * void
 * extract_brick_plane(const GwyBrick *brick,
 *                     GwyField *target,
 *                     const GwyFieldPart *fpart,
 *                     guint level)
 * {
 *     guint col, row, width, height;
 *     if (!gwy_field_check_target_part(target, fpart,
 *                                      brick->xres, brick->yres,
 *                                      &col, &row, &width, &height))
 *         return;
 *
 *     // Check @brick and perform the extraction of its plane to rectangle
 *     // given by @col, @row, @width and @height in @field...
 * }
 * ]|
 *
 * Returns: %TRUE if the position and dimensions are valid and the caller
 *          should proceed.  %FALSE if the caller should not proceed, either
 *          because @field is not a #GwyField instance or the position or
 *          dimensions is invalid (a critical error is emitted in these cases)
 *          or the actual part is zero-sized.
 **/
gboolean
gwy_field_check_target_part(const GwyField *field,
                            const GwyFieldPart *fpart,
                            guint width_full, guint height_full,
                            guint *col, guint *row,
                            guint *width, guint *height)
{
    g_return_val_if_fail(GWY_IS_FIELD(field), FALSE);
    if (fpart) {
        if (!fpart->width || !fpart->height)
            return FALSE;

        if (fpart->width == field->xres && fpart->height == field->yres) {
            // The part dimensions may correspond to the entire target field.
            // @fpart->col, @fpart->row are then not relevant for the target.
            *col = *row = 0;
            *width = fpart->width;
            *height = fpart->height;
        }
        else {
            // The two separate conditions are to catch integer overflows.
            g_return_val_if_fail(fpart->col < field->xres, FALSE);
            g_return_val_if_fail(fpart->width <= field->xres - fpart->col,
                                 FALSE);
            g_return_val_if_fail(fpart->row < field->yres, FALSE);
            g_return_val_if_fail(fpart->height <= field->yres - fpart->row,
                                 FALSE);
            *col = fpart->col;
            *row = fpart->row;
            *width = fpart->width;
            *height = fpart->height;
        }
    }
    else {
        g_return_val_if_fail(field->xres == width_full, FALSE);
        g_return_val_if_fail(field->yres == height_full, FALSE);
        *col = *row = 0;
        *width = field->xres;
        *height = field->yres;
    }

    return TRUE;
}

/**
 * gwy_field_limit_parts:
 * @src: A source two-dimensional data field.
 * @srcpart: (allow-none):
 *           Area in @src, possibly %NULL.
 * @dest: A destination two-dimensional data field.
 * @destcol: Column index for the upper-left corner of the part in @dest.
 * @destrow: Row index for the upper-left corner of the part in @dest.
 * @transpose: %TRUE to assume the area is transposed (rotated by 90 degrees)
 *             in the destination.
 * @col: Location to store the actual column index of the upper-left corner
 *       of the source part.
 * @row: Location to store the actual row index of the upper-left corner
 *       of the source part.
 * @width: Location to store the actual width (number of columns)
 *         of the source part.
 * @height: Location to store the actual height (number of rows)
 *          of the source part.
 *
 * Limits the dimensions of a field part for copying.
 *
 * The area is limited to make it contained both in @src and @dest and @col,
 * @row, @width and @height are set to the actual position and dimensions in
 * @src.  If the function returns %FALSE their values are undefined.
 *
 * If @src and @dest are the same field the source and destination parts should
 * not overlap.
 *
 * This function is typically used in copy-like functions that transfer a part
 * of a field into another field.
 * Example (note gwy_field_copy() copies field parts):
 * |[
 * void
 * copy_field(const GwyField *src,
 *            const GwyFieldPart *srcpart,
 *            GwyField *dest,
 *            guint destcol, guint destrow)
 * {
 *     guint col, row, width, height;
 *     if (!gwy_field_limit_parts(src, srcpart, dest, destcol, destrow,
 *                                FALSE, &col, &row, &width, &height))
 *         return;
 *
 *     // Copy area of size @width, @height at @col, @row in @src to an
 *     // equally-sized area at @destcol, @destrow in @dest...
 * }
 * ]|
 *
 * Returns: %TRUE if the caller should proceed.  %FALSE if the caller should
 *          not proceed, either because @field or @target is not a #GwyField
 *          instance (a critical error is emitted in these cases) or the actual
 *          part is zero-sized.
 **/
gboolean
gwy_field_limit_parts(const GwyField *src,
                      const GwyFieldPart *srcpart,
                      const GwyField *dest,
                      guint destcol, guint destrow,
                      gboolean transpose,
                      guint *col, guint *row,
                      guint *width, guint *height)
{
    g_return_val_if_fail(GWY_IS_FIELD(src), FALSE);
    g_return_val_if_fail(GWY_IS_FIELD(dest), FALSE);

    if (srcpart) {
        *col = srcpart->col;
        *row = srcpart->row;
        *width = srcpart->width;
        *height = srcpart->height;
        if (*col >= src->xres || *row >= src->yres)
            return FALSE;
        *width = MIN(*width, src->xres - *col);
        *height = MIN(*height, src->yres - *row);
    }
    else {
        *col = *row = 0;
        *width = src->xres;
        *height = src->yres;
    }

    if (destcol >= dest->xres || destrow >= dest->yres)
        return FALSE;

    if (transpose) {
        *width = MIN(*width, dest->yres - destrow);
        *height = MIN(*height, dest->xres - destcol);
    }
    else {
        *width = MIN(*width, dest->xres - destcol);
        *height = MIN(*height, dest->yres - destrow);
    }

    if (src == dest) {
        if ((transpose
             && gwy_overlapping(*col, *width, destcol, *height)
             && gwy_overlapping(*row, *height, destrow, *width))
            || (gwy_overlapping(*col, *width, destcol, *width)
                && gwy_overlapping(*row, *height, destrow, *height))) {
            g_warning("Source and destination blocks overlap.  "
                      "Data corruption is imminent.");
        }
    }

    return *width && *height;
}

/**
 * gwy_field_check_target:
 * @field: A two-dimensional data field.
 * @target: A two-dimensional data field.
 * @fpart: (allow-none):
 *         Area in @field and/or @target, possibly %NULL which means entire
 *         @field.
 * @targetcol: Location to store the actual column index of the upper-left
 *             corner of the part in the target.
 * @targetrow: Location to store the actual row index of the upper-left corner
 *             of the part in the target.
 *
 * Validates the position and dimensions of a target field.
 *
 * Dimensions of @target must match either @field or @fpart.  In the first case
 * the rectangular part is the same in @field and @target.  In the second case
 * the target corresponds only to the field part.
 *
 * If the position and dimensions are valid @targetcol and @targetrow are set
 * to the actual position in @target.  If the function returns %FALSE their
 * values are undefined.
 *
 * This function is typically used in filters and similar functions that
 * operate on a part of a field and produce data of the size of this part.
 * Example (note gwy_field_filter_standard() can apply the Sobel filter):
 * |[
 * void
 * sobel_filter(const GwyField *field,
 *              const GwyFieldPart *fpart,
 *              GwyField *target)
 * {
 *     guint col, row, width, height;
 *     if (!gwy_field_check_part(field, fpart,
 *                               &col, &row, &width, &height))
 *         return;
 *
 *     guint targetcol, targetrow;
 *     if !gwy_field_check_target(field, target,
 *                                &(GwyFieldPart){ col, row, width, height },
 *                                &targetcol, &targetrow))
 *         return;
 *
 *     // Apply the filter of @field part given by @col, @row, @width and
 *     // @height and put the result into an equally-sized area in @target at
 *     // @targetcol, @targetrow...
 * }
 * ]|
 *
 * Returns: %TRUE if the position and dimensions are valid and the caller
 *          should proceed.  %FALSE if the caller should not proceed, either
 *          because @field or @target is not a #GwyField instance or the
 *          position or dimensions is invalid (a critical error is emitted in
 *          these cases) or the actual part is zero-sized.
 **/
gboolean
gwy_field_check_target(const GwyField *field,
                       const GwyField *target,
                       const GwyFieldPart *fpart,
                       guint *targetcol,
                       guint *targetrow)
{
    g_return_val_if_fail(GWY_IS_FIELD(field), FALSE);
    g_return_val_if_fail(GWY_IS_FIELD(target), FALSE);

    // We normally always pass non-NULL @fpart but permit also NULL.
    if (!fpart)
        fpart = &(GwyFieldPart){ 0, 0, field->xres, field->yres };
    else if (!fpart->width || !fpart->height)
        return FALSE;

    if (target->xres == field->xres && target->yres == field->yres) {
        *targetcol = fpart->col;
        *targetrow = fpart->row;
        return TRUE;
    }
    if (target->xres == fpart->width && target->yres == fpart->height) {
        *targetcol = *targetrow = 0;
        return TRUE;
    }

    g_critical("Target field dimensions match neither source field nor the "
               "part.");
    return FALSE;
}

/**
 * gwy_field_check_mask:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *         Area in @field, possibly %NULL.
 * @mask: (allow-none):
 *        A two-dimensional mask field.
 * @masking: (inout):
 *           Masking mode.  If it is %GWY_MASK_IGNORE the mask is completely
 *           ignored.  If, on the other hand, @mask is %NULL the mode is
 *           <emphasis>set</emphasis> to %GWY_MASK_IGNORE.
 * @col: Location to store the actual column index of the upper-left corner
 *       of the field part.
 * @row: Location to store the actual row index of the upper-left corner
 *       of the field part.
 * @width: Location to store the actual width (number of columns)
 *         of the part.
 * @height: Location to store the actual height (number of rows)
 *          of the part.
 * @maskcol: Location to store the actual column index of the upper-left corner
 *           of the mask part.
 * @maskrow: Location to store the actual row index of the upper-left corner
 *           of the mask part.
 *
 * Validates the position and dimensions of a masked field part.
 *
 * If @fpart is %NULL entire @field is to be used.  Otherwise @fpart must be
 * contained in @field.
 *
 * The dimensions of @mask, if non-%NULL, must match either @field or @fpart.
 * In the first case the rectangular part is the same in @field and @mask.  In
 * the second case the mask covers only the field part.
 *
 * If the position and dimensions are valid @col, @row, @width, @height,
 * @maskcol and @maskrow are set to the actual rectangular part in @field.  If
 * the function returns %FALSE their values are undefined.
 *
 * This function is typically used in functions that operate on a part of a
 * field and allow masking.  See gwy_field_check_part() for checking of field
 * parts only.  Example (note gwy_field_rms() calculates the rms):
 * |[
 * gdouble
 * calculate_rms(const GwyField *field,
 *               const GwyFieldPart *fpart,
 *               const GwyMaskField *mask,
 *               GwyMaskingType masking)
 * {
 *     guint col, row, width, height, maskcol, maskrow;
 *     if (!gwy_field_check_mask(field, fpart, mask, &masking,
 *                               &col, &row, &width, &height,
 *                               &maskcol, &maskrow))
 *         return 0.0;
 *
 *     // Calculate rms of area given by @col, @row, @width and @height in
 *     // @field using @mask part given by @maskcol, @maskrow, @width and
 *     // @height if @masking is not GWY_MASK_IGNORE...
 * }
 * ]|
 *
 * Returns: %TRUE if the position and dimensions are valid and the caller
 *          should proceed.  %FALSE if the caller should not proceed, either
 *          because @field is not a #GwyField instance, @mask is not a
 *          #GwyMaskField instance or the position or dimensions is invalid (a
 *          critical error is emitted in these cases) or the actual part is
 *          zero-sized.
 **/
gboolean
gwy_field_check_mask(const GwyField *field,
                     const GwyFieldPart *fpart,
                     const GwyMaskField *mask,
                     GwyMaskingType *masking,
                     guint *col, guint *row,
                     guint *width, guint *height,
                     guint *maskcol, guint *maskrow)
{
    if (!gwy_field_check_part(field, fpart, col, row, width, height))
        return FALSE;
    if (mask && (*masking == GWY_MASK_INCLUDE
                 || *masking == GWY_MASK_EXCLUDE)) {
        g_return_val_if_fail(GWY_IS_MASK_FIELD(mask), FALSE);
        if (mask->xres == field->xres && mask->yres == field->yres) {
            *maskcol = *col;
            *maskrow = *row;
        }
        else if (mask->xres == *width && mask->yres == *height)
            *maskcol = *maskrow = 0;
        else {
            g_critical("Mask dimensions match neither the entire field "
                       "nor the part.");
            return FALSE;
        }
    }
    else {
        if (*masking != GWY_MASK_INCLUDE
            && *masking != GWY_MASK_EXCLUDE
            && *masking != GWY_MASK_IGNORE)
            g_critical("Invalid masking mode %u.", *masking);
        *masking = GWY_MASK_IGNORE;
        *maskcol = *maskrow = 0;
    }

    return TRUE;
}

/**
 * gwy_field_format_xy:
 * @field: A two-dimensional data field.
 * @style: Output format style.
 *
 * Finds a suitable format for displaying coordinates in a data field.
 *
 * The created format has a sufficient precision to represent coordinates
 * of neighbour pixels as different values.
 *
 * This function can be used only if units of @x and @y are identical which is
 * common but does not hold universally.  If it holds this function is usually
 * preferable to separate gwy_field_format_x() and gwy_field_format_y() because
 * the same format is then used for both coordinates. The separate functions do
 * not guarantee this for non-square fields even if @x and @y units are the
 * same.
 *
 * Returns: (transfer full):
 *          A newly created value format.
 **/
GwyValueFormat*
gwy_field_format_xy(const GwyField *field,
                    GwyValueFormatStyle style)
{
    g_return_val_if_fail(GWY_IS_FIELD(field), NULL);
    if (!gwy_unit_equal(field->priv->unit_x, field->priv->unit_y)) {
        g_warning("X and Y units of field do not match.");
        return gwy_field_format_x(field, style);
    }
    gdouble max0 = fmax(field->xreal, field->yreal);
    gdouble maxoff = fmax(fabs(field->xreal + field->xoff),
                         fabs(field->yreal + field->yoff));
    gdouble max = fmax(max0, maxoff);
    gdouble unit = fmin(gwy_field_dx(field), gwy_field_dy(field));
    return gwy_unit_format_with_resolution(gwy_field_get_unit_x(field),
                                           style, max, unit);
}

/**
 * gwy_field_format_x:
 * @field: A two-dimensional data field.
 * @style: Output format style.
 *
 * Finds a suitable format for displaying horizontal coordinates in a data
 * field.
 *
 * The created format has a sufficient precision to represent coordinates
 * of neighbour pixels as different values.  See gwy_field_format_xy() if
 * you prefer a common format for @x and @y coordinates.
 *
 * Returns: (transfer full):
 *          A newly created value format.
 **/
GwyValueFormat*
gwy_field_format_x(const GwyField *field,
                   GwyValueFormatStyle style)
{
    g_return_val_if_fail(GWY_IS_FIELD(field), NULL);
    gdouble max = fmax(field->xreal, fabs(field->xreal + field->xoff));
    gdouble unit = gwy_field_dx(field);
    return gwy_unit_format_with_resolution(gwy_field_get_unit_x(field),
                                           style, max, unit);
}

/**
 * gwy_field_format_y:
 * @field: A two-dimensional data field.
 * @style: Output format style.
 *
 * Finds a suitable format for displaying horizontal coordinates in a data
 * field.
 *
 * The created format has a sufficient precision to represent coordinates
 * of neighbour pixels as different values.  See gwy_field_format_xy() if
 * you prefer a common format for @x and @y coordinates.
 *
 * Returns: (transfer full):
 *          A newly created value format.
 **/
GwyValueFormat*
gwy_field_format_y(const GwyField *field,
                   GwyValueFormatStyle style)
{
    g_return_val_if_fail(GWY_IS_FIELD(field), NULL);
    gdouble max = fmax(field->yreal, fabs(field->yreal + field->yoff));
    gdouble unit = gwy_field_dy(field);
    return gwy_unit_format_with_resolution(gwy_field_get_unit_y(field),
                                           style, max, unit);
}

/**
 * gwy_field_format_z:
 * @field: A two-dimensional data field.
 * @style: Output format style.
 *
 * Finds a suitable format for displaying values in a data field.
 *
 * Returns: (transfer full):
 *          A newly created value format.
 **/
GwyValueFormat*
gwy_field_format_z(const GwyField *field,
                   GwyValueFormatStyle style)
{
    g_return_val_if_fail(GWY_IS_FIELD(field), NULL);
    gdouble min, max;
    gwy_field_min_max(field, NULL, NULL, GWY_MASK_IGNORE, &min, &max);
    if (max == min) {
        max = fabs(max);
        min = 0.0;
    }
    return gwy_unit_format_with_digits(gwy_field_get_unit_z(field),
                                       style, max - min, 3);
}

/**
 * gwy_field_set_name:
 * @field: A two-dimensional data field.
 * @name: (allow-none):
 *        New field name.
 *
 * Sets the name of a two-dimensional data field.
 **/
void
gwy_field_set_name(GwyField *field,
                   const gchar *name)
{
    g_return_if_fail(GWY_IS_FIELD(field));
    if (!gwy_assign_string(&field->priv->name, name))
        return;

    g_object_notify_by_pspec(G_OBJECT(field), properties[PROP_NAME]);
}

/**
 * gwy_field_get_name:
 * @field: A two-dimensional data field.
 *
 * Gets the name of a two-dimensional data field.
 *
 * Returns: (allow-none):
 *          Field name, owned by @field.
 **/
const gchar*
gwy_field_get_name(const GwyField *field)
{
    g_return_val_if_fail(GWY_IS_FIELD(field), NULL);
    return field->priv->name;
}

/**
 * gwy_field_get:
 * @field: A two-dimensional data field.
 * @col: Column index in @field.
 * @row: Row index in @field.
 *
 * Obtains a single field value.
 *
 * This function exists <emphasis>only for language bindings</emphasis> as it
 * is very slow compared to gwy_field_index() or simply accessing @data in
 * #GwyField directly in C.  See also gwy_field_value() and similar fucntions
 * for smarter ways to obtain a single value from a #GwyField.
 *
 * Returns: The value at (@col,@row).
 **/
gdouble
gwy_field_get(const GwyField *field,
              guint col,
              guint row)
{
    g_return_val_if_fail(GWY_IS_FIELD(field), NAN);
    g_return_val_if_fail(col < field->xres, NAN);
    g_return_val_if_fail(row < field->yres, NAN);
    return gwy_field_index(field, col, row);
}

/**
 * gwy_field_set:
 * @field: A two-dimensional data field.
 * @col: Column index in @field.
 * @row: Row index in @field.
 * @value: Value to store at given position.
 *
 * Sets a single field value.
 *
 * This function exists <emphasis>only for language bindings</emphasis> as it
 * is very slow compared to gwy_field_index() or simply accessing @data in
 * #GwyField directly in C.
 **/
void
gwy_field_set(const GwyField *field,
              guint col,
              guint row,
              gdouble value)
{
    g_return_if_fail(GWY_IS_FIELD(field));
    g_return_if_fail(col < field->xres);
    g_return_if_fail(row < field->yres);
    gwy_field_index(field, col, row) = value;
    // Invalidate.
    field->priv->cached = 0;
}

/**
 * SECTION: field
 * @title: GwyField
 * @short_description: Two-dimensional data in regular grid
 *
 * #GwyField represents two-dimensional data in a regular grid.
 *
 * Data are stored in a flat array @data in #GwyField-struct as #gdouble
 * values, stored by rows.  This means the column index is the fast index, row
 * index is the slow one, the top left corner indices are (0,0).  The array is
 * contiguous, i.e. there is no padding at the end of each row (and neither
 * beween pixels).  No methods to get and set individual values or rows and
 * columns are provided except gwy_field_index().  The usual mode of operation
 * is to access the data directly, bearing a few things in mind:
 * <itemizedlist>
 *   <listitem>All #GwyField-struct struct fields must be considered
 *   read-only.</listitem>
 *   <listitem>For reading, access @data in #GwyField-struct
 *   directly.</listitem>
 *   <listitem>For writing, you can write to @data array
 *   <emphasis>content</emphasis>
 *   but you must not change the field itself.  Use methods such as
 *   gwy_field_set_xreal() to change the field properties.</listitem>
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
 * you are encouraged to access @data in #GwyField-struct directly.
 * |[
 * // Read a field value.
 * gdouble value = gwy_field_index(field, 1, 2);
 *
 * // Write it elsewhere.
 * gwy_field_index(field, 3, 4) = value;
 * ]|
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
