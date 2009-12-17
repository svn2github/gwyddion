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
#include "libgwy/serialize.h"
#include "libgwy/field.h"
#include "libgwy/libgwy-aliases.h"

#define ASSIGN(p, q, n) memcpy((p), (q), (n)*sizeof(gdouble))

enum { N_ITEMS = 9 };

enum {
    DATA_CHANGED,
    N_SIGNALS
};

typedef enum {
    GWY_FIELD_CACHE_MIN = 0,
    GWY_FIELD_CACHE_MAX,
    GWY_FIELD_CACHE_SUM,
    GWY_FIELD_CACHE_RMS,
    GWY_FIELD_CACHE_MED,
    GWY_FIELD_CACHE_ARF,
    GWY_FIELD_CACHE_ART,
    GWY_FIELD_CACHE_ARE,
    GWY_FIELD_CACHE_SIZE
} GwyFieldCached;

struct _GwyFieldPrivate {
    /* FIXME: Consider permitting x-units != y-units. */
    GwyUnit *unit_xy;
    GwyUnit *unit_z;
    guint32 cached;
    gdouble cache[GWY_FIELD_CACHE_SIZE];
};

typedef struct _GwyFieldPrivate Field;

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
    field->data = g_new0(gdouble, 1);
}

static void
gwy_field_finalize(GObject *object)
{
    GwyField *field = GWY_FIELD(object);
    GWY_FREE(field->data);
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
    it.value.v_uint32 = field->xres;
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

    if (!its[0].value.v_uint32 || !its[1].value.v_uint32) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_INVALID,
                           _("Field dimensions %u×%u are invalid."),
                           its[0].value.v_uint32, its[1].value.v_uint32);
        return FALSE;
    }

    if (its[0].value.v_uint32 * its[1].value.v_uint32
        != its[8].array_size) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_INVALID,
                           _("Field dimensions %u×%u do not match data size "
                             "%lu."),
                           its[0].value.v_uint32, its[1].value.v_uint32,
                           (gulong)its[8].array_size);
        return FALSE;
    }

    if (its[6].value.v_object && !GWY_IS_UNIT(its[6].value.v_object)) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_INVALID,
                           _("Field xy units are of type %s "
                             "instead of GwyUnit."),
                           G_OBJECT_TYPE_NAME(its[6].value.v_object));
        return FALSE;
    }

    if (its[7].value.v_object && !GWY_IS_UNIT(its[7].value.v_object)) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_INVALID,
                           _("Field z units are of type %s "
                             "instead of GwyUnit."),
                           G_OBJECT_TYPE_NAME(its[7].value.v_object));
        return FALSE;
    }

    g_free(field->data);
    field->xres = its[0].value.v_uint32;
    field->yres = its[1].value.v_uint32;
    // FIXME: Catch near-zero and near-infinity values.
    field->xreal = CLAMP(its[2].value.v_double, G_MINDOUBLE, G_MAXDOUBLE);
    field->xreal = CLAMP(its[3].value.v_double, G_MINDOUBLE, G_MAXDOUBLE);
    field->xoff = CLAMP(its[4].value.v_double, -G_MAXDOUBLE, G_MAXDOUBLE);
    field->yoff = CLAMP(its[5].value.v_double, -G_MAXDOUBLE, G_MAXDOUBLE);
    priv->unit_xy = (GwyUnit*)its[6].value.v_object;
    its[6].value.v_object = NULL;
    priv->unit_z = (GwyUnit*)its[7].value.v_object;
    its[7].value.v_object = NULL;
    field->data = its[8].value.v_double_array;
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
}

#define ASSIGN_UNITS(dest, src) \
    do { \
        if (src && dest) \
            gwy_unit_assign(dest, src); \
        else if (dest) \
            GWY_OBJECT_UNREF(dest); \
        else if (src) \
            dest = gwy_unit_duplicate(src); \
    } while (0)

static void
gwy_field_assign_impl(GwySerializable *destination,
                      GwySerializable *source)
{
    GwyField *dest = GWY_FIELD(destination);
    GwyField *src = GWY_FIELD(source);
    Field *spriv = src->priv, *dpriv = dest->priv;

    if (dest->xres * dest->yres != src->xres * src->yres) {
        g_free(dest->data);
        dest->data = g_new(gdouble, src->xres * src->yres);
    }
    ASSIGN(dest->data, src->data, src->xres * src->yres);
    copy_info(dest, src);
    ASSIGN(dpriv->cache, spriv->cache, GWY_FIELD_CACHE_SIZE);
    dpriv->cached = spriv->cached;
    ASSIGN_UNITS(dpriv->unit_xy, spriv->unit_xy);
    ASSIGN_UNITS(dpriv->unit_z, spriv->unit_z);

    gwy_field_data_changed(dest);
}

/**
 * gwy_field_new:
 *
 * Creates a new two-dimensional data field.
 *
 * Returns: A new two-dimensional data field.
 **/
GwyField*
gwy_field_new(void)
{
    return g_object_newv(GWY_TYPE_FIELD, 0, NULL);
}

GwyField*
gwy_field_new_sized(guint xres,
                    guint yres,
                    gboolean clear)
{
    g_return_val_if_fail(xres && yres, NULL);

    GwyField *field = g_object_newv(GWY_TYPE_FIELD, 0, NULL);
    g_free(field->data);
    field->xres = xres;
    field->yres = yres;
    if (clear)
        field->data = g_new0(gdouble, field->xres * field->yres);
    else
        field->data = g_new(gdouble, field->xres * field->yres);
    return field;
}

GwyField*
gwy_field_new_alike(GwyField *model,
                    gboolean clear)
{
    g_return_val_if_fail(GWY_IS_FIELD(model), NULL);

    GwyField *field = gwy_field_new_sized(model->xres, model->yres, clear);
    copy_info(field, model);
    Field *mpriv = model->priv, *priv = field->priv;
    ASSIGN_UNITS(priv->unit_xy, mpriv->unit_xy);
    ASSIGN_UNITS(priv->unit_z, mpriv->unit_z);
    return field;
}

GwyField*
gwy_field_new_part(GwyField *field,
                   guint col,
                   guint row,
                   guint width,
                   guint height)
{
}

GwyField*
gwy_field_new_resampled(GwyField *field,
                        guint xres,
                        guint yres,
                        GwyInterpolationType interpolation)
{
}

void
gwy_field_data_changed(GwyField *field)
{
    g_return_if_fail(GWY_IS_FIELD(field));
    g_signal_emit(field, field_signals[DATA_CHANGED], 0);
}

void
gwy_field_copy(GwyField *src,
               GwyField *dest)
{
    g_return_if_fail(GWY_IS_FIELD(src));
    g_return_if_fail(GWY_IS_FIELD(dest));
}

void
gwy_field_part_copy(GwyField *src,
                    GwyField *dest,
                    guint col,
                    guint row,
                    guint width,
                    guint height,
                    guint destcol,
                    guint destrow)
{
    g_return_if_fail(GWY_IS_FIELD(src));
    g_return_if_fail(GWY_IS_FIELD(dest));
}

gdouble*
gwy_field_get_data(GwyField *field)
{
    g_return_val_if_fail(GWY_IS_FIELD(field), NULL);
    // FIXME: gwy_data_field_invalidate(field);
    return field->data;
}

void
gwy_field_set_xreal(GwyField *field,
                    gdouble xreal)
{
    g_return_if_fail(GWY_IS_FIELD(field));
}

void
gwy_field_set_yreal(GwyField *field,
                    gdouble yreal)
{
    g_return_if_fail(GWY_IS_FIELD(field));
}

void
gwy_field_set_xoffset(GwyField *field,
                      gdouble xoff)
{
    g_return_if_fail(GWY_IS_FIELD(field));
}

void
gwy_field_set_yoffset(GwyField *field,
                      gdouble yoff)
{
    g_return_if_fail(GWY_IS_FIELD(field));
}

GwyUnit*
gwy_field_get_unit_xy(GwyField *field)
{
    g_return_val_if_fail(GWY_IS_FIELD(field), NULL);
    Field *priv = field->priv;
    if (!priv->unit_xy)
        priv->unit_xy = gwy_unit_new();
    return priv->unit_xy;
}

GwyUnit*
gwy_field_get_unit_z(GwyField *field)
{
    g_return_val_if_fail(GWY_IS_FIELD(field), NULL);
    Field *priv = field->priv;
    if (!priv->unit_z)
        priv->unit_z = gwy_unit_new();
    return priv->unit_z;
}

void
gwy_field_clear(GwyField *field)
{
    g_return_if_fail(GWY_IS_FIELD(field));
}

void
gwy_field_fill(GwyField *field,
               gdouble value)
{
    g_return_if_fail(GWY_IS_FIELD(field));
}

void
gwy_field_part_clear(GwyField *field,
                     guint col,
                     guint row,
                     guint width,
                     guint height)
{
    g_return_if_fail(GWY_IS_FIELD(field));
}

void
gwy_field_part_fill(GwyField *field,
                    guint col,
                    guint row,
                    guint width,
                    guint height,
                    gdouble value)
{
}

#define __LIBGWY_FIELD_C__
#include "libgwy/libgwy-aliases.c"

/**
 * SECTION: field
 * @title: GwyField
 * @short_description: Two-dimensional data in regular grid
 *
 * #GwyField represents two dimensional data in a regular grid...
 **/

/**
 * GwyField:
 * @xres: X-resolution, i.e. width in pixels.
 * @yres: Y-resolution, i.e. height in pixels.
 * @xreal: Width in physical units.
 * @yreal: Height in physical units.
 * @xoff: X-offset of the top-left corner from [0,0] in physical units.
 * @yoff: Y-offset of the top-left corner from [0,0] in physical units.
 * @data: FIXME FIXME FIXME
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
 * Class of physical fields objects.
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

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
