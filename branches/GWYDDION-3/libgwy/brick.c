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

#include <stdlib.h>
#include <glib/gi18n-lib.h>
#include "libgwy/object-utils.h"
#include "libgwy/math.h"
#include "libgwy/serialize.h"
#include "libgwy/brick.h"
#include "libgwy/brick-arithmetic.h"
#include "libgwy/object-internal.h"
#include "libgwy/brick-internal.h"

#define BASE(brick, col, row, level) \
    (brick->data + ((level)*brick->yres + (row))*brick->xres + (col))

enum { N_ITEMS = 15 };

enum {
    SGNL_DATA_CHANGED,
    N_SIGNALS
};

enum {
    PROP_0,
    PROP_XRES,
    PROP_YRES,
    PROP_ZRES,
    PROP_XREAL,
    PROP_YREAL,
    PROP_ZREAL,
    PROP_XOFFSET,
    PROP_YOFFSET,
    PROP_ZOFFSET,
    PROP_UNIT_X,
    PROP_UNIT_Y,
    PROP_UNIT_Z,
    PROP_UNIT_W,
    PROP_NAME,
    N_PROPS
};

static void     gwy_brick_finalize         (GObject *object);
static void     gwy_brick_dispose          (GObject *object);
static void     gwy_brick_serializable_init(GwySerializableInterface *iface);
static gsize    gwy_brick_n_items          (GwySerializable *serializable);
static gsize    gwy_brick_itemize          (GwySerializable *serializable,
                                            GwySerializableItems *items);
static gboolean gwy_brick_construct        (GwySerializable *serializable,
                                            GwySerializableItems *items,
                                            GwyErrorList **error_list);
static GObject* gwy_brick_duplicate_impl   (GwySerializable *serializable);
static void     gwy_brick_assign_impl      (GwySerializable *destination,
                                            GwySerializable *source);
static void     gwy_brick_set_property     (GObject *object,
                                            guint prop_id,
                                            const GValue *value,
                                            GParamSpec *pspec);
static void     gwy_brick_get_property     (GObject *object,
                                            guint prop_id,
                                            GValue *value,
                                            GParamSpec *pspec);

static const GwySerializableItem serialize_items[N_ITEMS] = {
    /*00*/ { .name = "xres",   .ctype = GWY_SERIALIZABLE_INT32,        .value.v_uint32 = 1   },
    /*01*/ { .name = "yres",   .ctype = GWY_SERIALIZABLE_INT32,        .value.v_uint32 = 1   },
    /*02*/ { .name = "zres",   .ctype = GWY_SERIALIZABLE_INT32,        .value.v_uint32 = 1   },
    /*03*/ { .name = "xreal",  .ctype = GWY_SERIALIZABLE_DOUBLE,       .value.v_double = 1.0 },
    /*04*/ { .name = "yreal",  .ctype = GWY_SERIALIZABLE_DOUBLE,       .value.v_double = 1.0 },
    /*05*/ { .name = "zreal",  .ctype = GWY_SERIALIZABLE_DOUBLE,       .value.v_double = 1.0 },
    /*06*/ { .name = "xoff",   .ctype = GWY_SERIALIZABLE_DOUBLE,       },
    /*07*/ { .name = "yoff",   .ctype = GWY_SERIALIZABLE_DOUBLE,       },
    /*08*/ { .name = "zoff",   .ctype = GWY_SERIALIZABLE_DOUBLE,       },
    /*09*/ { .name = "unit-x", .ctype = GWY_SERIALIZABLE_OBJECT,       },
    /*10*/ { .name = "unit-y", .ctype = GWY_SERIALIZABLE_OBJECT,       },
    /*11*/ { .name = "unit-z", .ctype = GWY_SERIALIZABLE_OBJECT,       },
    /*12*/ { .name = "unit-w", .ctype = GWY_SERIALIZABLE_OBJECT,       },
    /*13*/ { .name = "name",   .ctype = GWY_SERIALIZABLE_STRING,       },
    /*14*/ { .name = "data",   .ctype = GWY_SERIALIZABLE_DOUBLE_ARRAY, },
};

static guint signals[N_SIGNALS];
static GParamSpec *properties[N_PROPS];

G_DEFINE_TYPE_EXTENDED
    (GwyBrick, gwy_brick, G_TYPE_OBJECT, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_brick_serializable_init));

static void
gwy_brick_serializable_init(GwySerializableInterface *iface)
{
    iface->n_items   = gwy_brick_n_items;
    iface->itemize   = gwy_brick_itemize;
    iface->construct = gwy_brick_construct;
    iface->duplicate = gwy_brick_duplicate_impl;
    iface->assign    = gwy_brick_assign_impl;
}

static void
gwy_brick_class_init(GwyBrickClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(Brick));

    gobject_class->dispose = gwy_brick_dispose;
    gobject_class->finalize = gwy_brick_finalize;
    gobject_class->get_property = gwy_brick_get_property;
    gobject_class->set_property = gwy_brick_set_property;

    properties[PROP_XRES]
        = g_param_spec_uint("x-res",
                            "X resolution",
                            "Pixel width of the brick.",
                            1, G_MAXUINT, 1,
                            G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_YRES]
        = g_param_spec_uint("y-res",
                            "Y resolution",
                            "Pixel height of the brick.",
                            1, G_MAXUINT, 1,
                            G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_ZRES]
        = g_param_spec_uint("z-res",
                            "Z resolution",
                            "Pixel depth of the brick.",
                            1, G_MAXUINT, 1,
                            G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_XREAL]
        = g_param_spec_double("x-real",
                              "X real size",
                              "Width of the brick in physical units.",
                              G_MINDOUBLE, G_MAXDOUBLE, 1.0,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_YREAL]
        = g_param_spec_double("y-real",
                              "Y real size",
                              "Height of the brick in physical units.",
                              G_MINDOUBLE, G_MAXDOUBLE, 1.0,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_ZREAL]
        = g_param_spec_double("z-real",
                              "Z real size",
                              "Depth of the brick in physical units.",
                              G_MINDOUBLE, G_MAXDOUBLE, 1.0,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_XOFFSET]
        = g_param_spec_double("x-offset",
                              "X offset",
                              "Horizontal offset of the brick top left corner "
                              "in physical units.",
                              -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_YOFFSET]
        = g_param_spec_double("y-offset",
                              "Y offset",
                              "Vertical offset of the brick top left corner "
                              "in physical units.",
                              -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_ZOFFSET]
        = g_param_spec_double("z-offset",
                              "Z offset",
                              "Depth offset of the brick top left corner "
                              "in physical units.",
                              -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_UNIT_X]
        = g_param_spec_object("unit-x",
                              "X unit",
                              "Physical units of horizontal lateral dimensions "
                              "of the brick layers.",
                              GWY_TYPE_UNIT,
                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_UNIT_Y]
        = g_param_spec_object("unit-y",
                              "Y unit",
                              "Physical units of vertical lateral dimensions "
                              "of the brick layers.",
                              GWY_TYPE_UNIT,
                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_UNIT_Z]
        = g_param_spec_object("unit-z",
                              "Z unit",
                              "Physical units of brick depth.",
                              GWY_TYPE_UNIT,
                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_UNIT_W]
        = g_param_spec_object("unit-w",
                              "W unit",
                              "Physical units of brick values.",
                              GWY_TYPE_UNIT,
                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_NAME]
        = g_param_spec_string("name",
                              "Name",
                              "Name of the brick.",
                              NULL,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, properties[i]);

    /**
     * GwyBrick::data-changed:
     * @gwybrick: The #GwyBrick which received the signal.
     * @arg1: (allow-none):
     *        Part of @gwybrick that has changed.
     *        It may be %NULL, meaning the entire brick.
     *
     * The ::data-changed signal is emitted when brick data changes.
     * More precisely, #GwyBrick itself never emits this signal.  You can emit
     * it explicitly with gwy_brick_data_changed() to notify anything that
     * displays (or otherwise uses) the brick.
     **/
    signals[SGNL_DATA_CHANGED]
        = g_signal_new_class_handler("data-changed",
                                     G_OBJECT_CLASS_TYPE(klass),
                                     G_SIGNAL_RUN_FIRST,
                                     NULL, NULL, NULL,
                                     g_cclosure_marshal_VOID__BOXED,
                                     G_TYPE_NONE, 1,
                                     GWY_TYPE_BRICK_PART);
}

static void
gwy_brick_init(GwyBrick *brick)
{
    brick->priv = G_TYPE_INSTANCE_GET_PRIVATE(brick, GWY_TYPE_BRICK, Brick);
    brick->xres = brick->yres = brick->zres = 1;
    brick->xreal = brick->yreal = brick->zreal = 1.0;
    brick->data = &brick->priv->storage;
}

static void
alloc_data(GwyBrick *brick,
           gboolean clear)
{
    if (clear) {
        brick->data = g_new0(gdouble, brick->xres * brick->yres * brick->zres);
    }
    else {
        brick->data = g_new(gdouble, brick->xres * brick->yres * brick->zres);
        gwy_brick_invalidate(brick);
    }
    brick->priv->allocated = TRUE;
}

static void
free_data(GwyBrick *brick)
{
    if (brick->priv->allocated)
        GWY_FREE(brick->data);
    else
        brick->data = NULL;
}

static void
gwy_brick_finalize(GObject *object)
{
    GwyBrick *brick = GWY_BRICK(object);
    GWY_FREE(brick->priv->name);
    free_data(brick);
    G_OBJECT_CLASS(gwy_brick_parent_class)->finalize(object);
}

static void
gwy_brick_dispose(GObject *object)
{
    GwyBrick *brick = GWY_BRICK(object);
    GWY_OBJECT_UNREF(brick->priv->unit_x);
    GWY_OBJECT_UNREF(brick->priv->unit_y);
    GWY_OBJECT_UNREF(brick->priv->unit_z);
    GWY_OBJECT_UNREF(brick->priv->unit_w);
    G_OBJECT_CLASS(gwy_brick_parent_class)->dispose(object);
}

static gsize
gwy_brick_n_items(GwySerializable *serializable)
{
    GwyBrick *brick = GWY_BRICK(serializable);
    Brick *priv = brick->priv;
    gsize n = N_ITEMS;
    if (priv->unit_x)
       n += gwy_serializable_n_items(GWY_SERIALIZABLE(priv->unit_x));
    if (priv->unit_y)
       n += gwy_serializable_n_items(GWY_SERIALIZABLE(priv->unit_y));
    if (priv->unit_z)
       n += gwy_serializable_n_items(GWY_SERIALIZABLE(priv->unit_z));
    if (priv->unit_w)
       n += gwy_serializable_n_items(GWY_SERIALIZABLE(priv->unit_w));
    return n;
}

static gsize
gwy_brick_itemize(GwySerializable *serializable,
                  GwySerializableItems *items)
{
    GwyBrick *brick = GWY_BRICK(serializable);
    Brick *priv = brick->priv;
    GwySerializableItem it;
    guint n = 0;

    g_return_val_if_fail(items->len - items->n >= N_ITEMS, 0);

    it = serialize_items[0];
    it.value.v_uint32 = brick->xres;
    items->items[items->n++] = it;
    n++;

    it = serialize_items[1];
    it.value.v_uint32 = brick->yres;
    items->items[items->n++] = it;
    n++;

    it = serialize_items[2];
    it.value.v_uint32 = brick->zres;
    items->items[items->n++] = it;
    n++;

    _gwy_serialize_double(brick->xreal, serialize_items + 3, items, &n);
    _gwy_serialize_double(brick->yreal, serialize_items + 4, items, &n);
    _gwy_serialize_double(brick->zreal, serialize_items + 5, items, &n);
    _gwy_serialize_double(brick->xoff, serialize_items + 6, items, &n);
    _gwy_serialize_double(brick->yoff, serialize_items + 7, items, &n);
    _gwy_serialize_double(brick->zoff, serialize_items + 8, items, &n);
    _gwy_serialize_unit(priv->unit_x, serialize_items + 9, items, &n);
    _gwy_serialize_unit(priv->unit_y, serialize_items + 10, items, &n);
    _gwy_serialize_unit(priv->unit_z, serialize_items + 11, items, &n);
    _gwy_serialize_unit(priv->unit_w, serialize_items + 12, items, &n);
    _gwy_serialize_string(priv->name, serialize_items + 13, items, &n);

    g_return_val_if_fail(items->len - items->n, 0);
    it = serialize_items[14];
    it.value.v_double_array = brick->data;
    it.array_size = brick->xres * brick->yres * brick->zres;
    items->items[items->n++] = it;
    n++;

    return n;
}

static gboolean
gwy_brick_construct(GwySerializable *serializable,
                    GwySerializableItems *items,
                    GwyErrorList **error_list)
{
    GwyBrick *brick = GWY_BRICK(serializable);
    Brick *priv = brick->priv;

    GwySerializableItem its[N_ITEMS];
    memcpy(its, serialize_items, sizeof(serialize_items));
    gwy_deserialize_filter_items(its, N_ITEMS, items, NULL,
                                 "GwyBrick", error_list);

    if (G_UNLIKELY(!its[0].value.v_uint32
                   || !its[1].value.v_uint32
                   || !its[2].value.v_uint32)) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_INVALID,
                           // TRANSLATORS: Error message.
                           _("GwyBrick dimensions %u×%u×%u are invalid."),
                           its[0].value.v_uint32,
                           its[1].value.v_uint32,
                           its[2].value.v_uint32);
        goto fail;
    }

    if (G_UNLIKELY(its[0].value.v_uint32 * its[1].value.v_uint32
                   * its[2].value.v_uint32
                   != its[14].array_size)) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_INVALID,
                           // TRANSLATORS: Error message.
                           _("GwyBrick dimensions %u×%u×%u do not match data "
                             "size %lu."),
                           its[0].value.v_uint32,
                           its[1].value.v_uint32,
                           its[2].value.v_uint32,
                           (gulong)its[14].array_size);
        goto fail;
    }

    if (!_gwy_check_object_component(its + 9, brick, GWY_TYPE_UNIT, error_list))
        goto fail;
    if (!_gwy_check_object_component(its + 10, brick, GWY_TYPE_UNIT, error_list))
        goto fail;
    if (!_gwy_check_object_component(its + 11, brick, GWY_TYPE_UNIT, error_list))
        goto fail;
    if (!_gwy_check_object_component(its + 12, brick, GWY_TYPE_UNIT, error_list))
        goto fail;

    brick->xres = its[0].value.v_uint32;
    brick->yres = its[1].value.v_uint32;
    brick->zres = its[2].value.v_uint32;
    // FIXME: Catch near-zero and near-infinity values.
    brick->xreal = CLAMP(its[3].value.v_double, G_MINDOUBLE, G_MAXDOUBLE);
    brick->yreal = CLAMP(its[4].value.v_double, G_MINDOUBLE, G_MAXDOUBLE);
    brick->zreal = CLAMP(its[5].value.v_double, G_MINDOUBLE, G_MAXDOUBLE);
    brick->xoff = CLAMP(its[6].value.v_double, -G_MAXDOUBLE, G_MAXDOUBLE);
    brick->yoff = CLAMP(its[7].value.v_double, -G_MAXDOUBLE, G_MAXDOUBLE);
    brick->zoff = CLAMP(its[8].value.v_double, -G_MAXDOUBLE, G_MAXDOUBLE);
    priv->unit_x = (GwyUnit*)its[9].value.v_object;
    priv->unit_y = (GwyUnit*)its[10].value.v_object;
    priv->unit_z = (GwyUnit*)its[11].value.v_object;
    priv->unit_w = (GwyUnit*)its[12].value.v_object;
    free_data(brick);
    priv->name = its[13].value.v_string;
    brick->data = its[14].value.v_double_array;
    priv->allocated = TRUE;

    return TRUE;

fail:
    GWY_OBJECT_UNREF(its[9].value.v_object);
    GWY_OBJECT_UNREF(its[10].value.v_object);
    GWY_OBJECT_UNREF(its[11].value.v_object);
    GWY_OBJECT_UNREF(its[12].value.v_object);
    GWY_FREE(its[13].value.v_string);
    GWY_FREE(its[14].value.v_double_array);
    return FALSE;
}

static GObject*
gwy_brick_duplicate_impl(GwySerializable *serializable)
{
    GwyBrick *brick = GWY_BRICK(serializable);
    GwyBrick *duplicate = gwy_brick_new_alike(brick, FALSE);

    gwy_assign(duplicate->data, brick->data,
               brick->xres*brick->yres*brick->zres);
    gwy_assign_string(&duplicate->priv->name, brick->priv->name);
    // TODO: Caches, if any

    return G_OBJECT(duplicate);
}

static void
copy_info(GwyBrick *dest,
          const GwyBrick *src)
{
    dest->xres = src->xres;
    dest->yres = src->yres;
    dest->zres = src->zres;
    dest->xreal = src->xreal;
    dest->yreal = src->yreal;
    dest->zreal = src->zreal;
    dest->xoff = src->xoff;
    dest->yoff = src->yoff;
    dest->zoff = src->zoff;
    Brick *dpriv = dest->priv, *spriv = src->priv;
    _gwy_assign_unit(&dpriv->unit_x, spriv->unit_x);
    _gwy_assign_unit(&dpriv->unit_y, spriv->unit_y);
    _gwy_assign_unit(&dpriv->unit_z, spriv->unit_z);
    _gwy_assign_unit(&dpriv->unit_w, spriv->unit_w);
}

static void
gwy_brick_assign_impl(GwySerializable *destination,
                      GwySerializable *source)
{
    GwyBrick *dest = GWY_BRICK(destination);
    GwyBrick *src = GWY_BRICK(source);
    Brick *dpriv = dest->priv, *spriv = src->priv;

    GParamSpec *notify[N_PROPS];
    guint nn = 0;
    if (dest->xres != src->xres)
        notify[nn++] = properties[PROP_XRES];
    if (dest->yres != src->yres)
        notify[nn++] = properties[PROP_YRES];
    if (dest->zres != src->zres)
        notify[nn++] = properties[PROP_ZRES];
    if (dest->xreal != src->xreal)
        notify[nn++] = properties[PROP_XREAL];
    if (dest->yreal != src->yreal)
        notify[nn++] = properties[PROP_YREAL];
    if (dest->zreal != src->zreal)
        notify[nn++] = properties[PROP_ZREAL];
    if (dest->xoff != src->xoff)
        notify[nn++] = properties[PROP_XOFFSET];
    if (dest->yoff != src->yoff)
        notify[nn++] = properties[PROP_YOFFSET];
    if (dest->zoff != src->zoff)
        notify[nn++] = properties[PROP_ZOFFSET];
    if (gwy_assign_string(&dpriv->name, spriv->name))
        notify[nn++] = properties[PROP_NAME];

    if (dest->xres * dest->yres * dest->zres
        != src->xres * src->yres * src->zres) {
        free_data(dest);
        dest->data = g_new(gdouble, src->xres * src->yres * src->zres);
        dpriv->allocated = TRUE;
    }
    gwy_assign(dest->data, src->data, src->xres * src->yres * src->zres);
    copy_info(dest, src);
    // TODO: Caches, if any
    _gwy_notify_properties_by_pspec(G_OBJECT(dest), notify, nn);
}

static void
gwy_brick_set_property(GObject *object,
                       guint prop_id,
                       const GValue *value,
                       GParamSpec *pspec)
{
    GwyBrick *brick = GWY_BRICK(object);

    switch (prop_id) {
        case PROP_XREAL:
        brick->xreal = g_value_get_double(value);
        break;

        case PROP_YREAL:
        brick->yreal = g_value_get_double(value);
        break;

        case PROP_ZREAL:
        brick->zreal = g_value_get_double(value);
        break;

        case PROP_XOFFSET:
        brick->xoff = g_value_get_double(value);
        break;

        case PROP_YOFFSET:
        brick->yoff = g_value_get_double(value);
        break;

        case PROP_ZOFFSET:
        brick->zoff = g_value_get_double(value);
        break;

        case PROP_NAME:
        gwy_assign_string(&brick->priv->name, g_value_get_string(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_brick_get_property(GObject *object,
                       guint prop_id,
                       GValue *value,
                       GParamSpec *pspec)
{
    GwyBrick *brick = GWY_BRICK(object);
    Brick *priv = brick->priv;

    switch (prop_id) {
        case PROP_XRES:
        g_value_set_uint(value, brick->xres);
        break;

        case PROP_YRES:
        g_value_set_uint(value, brick->yres);
        break;

        case PROP_ZRES:
        g_value_set_uint(value, brick->zres);
        break;

        case PROP_XREAL:
        g_value_set_double(value, brick->xreal);
        break;

        case PROP_YREAL:
        g_value_set_double(value, brick->yreal);
        break;

        case PROP_ZREAL:
        g_value_set_double(value, brick->zreal);
        break;

        case PROP_XOFFSET:
        g_value_set_double(value, brick->xoff);
        break;

        case PROP_YOFFSET:
        g_value_set_double(value, brick->yoff);
        break;

        case PROP_ZOFFSET:
        g_value_set_double(value, brick->zoff);
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

        case PROP_UNIT_W:
        if (!priv->unit_w)
            priv->unit_w = gwy_unit_new();
        g_value_set_object(value, priv->unit_w);
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
 * gwy_brick_new:
 *
 * Creates a new three-dimensional data brick.
 *
 * The brick dimensions will be 1×1×1 and it will be zero-filled.  This
 * parameterless constructor exists mainly for language bindings,
 * gwy_brick_new_sized() and gwy_brick_new_alike() are usually more useful.
 *
 * Returns: (transfer full):
 *          A new three-dimensional data brick.
 **/
GwyBrick*
gwy_brick_new(void)
{
    return g_object_newv(GWY_TYPE_BRICK, 0, NULL);
}

/**
 * gwy_brick_new_sized:
 * @xres: Horizontal resolution (width).
 * @yres: Vertical resolution (height).
 * @zres: Number of layers (depth).
 * @clear: %TRUE to fill the new brick data with zeroes, %FALSE to leave it
 *         uninitialised.
 *
 * Creates a new three-dimensional data brick of specified dimensions.
 *
 * Returns: (transfer full):
 *          A new three-dimensional data brick.
 **/
GwyBrick*
gwy_brick_new_sized(guint xres,
                    guint yres,
                    guint zres,
                    gboolean clear)
{
    g_return_val_if_fail(xres && yres, NULL);

    GwyBrick *brick = g_object_newv(GWY_TYPE_BRICK, 0, NULL);
    free_data(brick);
    brick->xres = xres;
    brick->yres = yres;
    brick->zres = zres;
    alloc_data(brick, clear);
    return brick;
}

/**
 * gwy_brick_new_alike:
 * @model: A three-dimensional data brick to use as the template.
 * @clear: %TRUE to fill the new brick data with zeroes, %FALSE to leave it
 *         uninitialised.
 *
 * Creates a new three-dimensional data brick similar to another brick.
 *
 * All properties of the newly created brick will be identical to @model,
 * except the data that will be either zeroes or uninitialised, and name which
 * will be unset.  Use gwy_brick_duplicate() to completely duplicate a brick
 * including data.
 *
 * Returns: (transfer full):
 *          A new three-dimensional data brick.
 **/
GwyBrick*
gwy_brick_new_alike(const GwyBrick *model,
                    gboolean clear)
{
    g_return_val_if_fail(GWY_IS_BRICK(model), NULL);

    GwyBrick *brick = gwy_brick_new_sized(model->xres, model->yres, model->zres,
                                          clear);
    copy_info(brick, model);
    return brick;
}

/**
 * gwy_brick_new_part:
 * @brick: A three-dimensional data brick.
 * @bpart: (allow-none):
 *         Part of @brick to extract to the new brick.  Passing %NULL
 *         creates an identical copy of @brick, similarly to
 *         gwy_brick_duplicate() (though with @keep_offsets set to %FALSE
 *         the offsets are reset).
 * @keep_offsets: %TRUE to set the X and Y offsets of the new brick
 *                using @bpart and @brick offsets.  %FALSE to set offsets
 *                of the new brick to zeroes.
 *
 * Creates a new three-dimensional brick as a rectangular part of another
 * brick.
 *
 * The box specified by @bpart must be entirely contained in @brick.
 * Both dimensions must be non-zero.
 *
 * Data are physically copied, i.e. changing the new brick data does not change
 * @brick's data and vice versa.  Physical dimensions of the new brick are
 * calculated to correspond to the extracted part.
 *
 * Returns: (transfer full):
 *          A new three-dimensional data brick.
 **/
GwyBrick*
gwy_brick_new_part(const GwyBrick *brick,
                   const GwyBrickPart *bpart,
                   gboolean keep_offsets)
{
    guint col, row, level, width, height, depth;
    if (!gwy_brick_check_part(brick, bpart,
                              &col, &row, &level, &width, &height, &depth))
        return NULL;

    if (width == brick->xres && height == brick->yres && depth == brick->zres) {
        GwyBrick *part = gwy_brick_duplicate(brick);
        if (!keep_offsets)
            part->xoff = part->yoff = part->zoff = 0.0;
        return part;
    }

    GwyBrick *part = gwy_brick_new_sized(width, height, depth, FALSE);
    gwy_brick_copy(brick, bpart, part, 0, 0, 0);
    part->xreal = width*gwy_brick_dx(brick);
    part->yreal = height*gwy_brick_dy(brick);
    part->zreal = depth*gwy_brick_dz(brick);

    Brick *spriv = brick->priv, *dpriv = part->priv;
    _gwy_assign_unit(&dpriv->unit_x, spriv->unit_x);
    _gwy_assign_unit(&dpriv->unit_y, spriv->unit_y);
    _gwy_assign_unit(&dpriv->unit_z, spriv->unit_z);
    _gwy_assign_unit(&dpriv->unit_w, spriv->unit_w);
    if (keep_offsets) {
        part->xoff = brick->xoff + col*gwy_brick_dx(brick);
        part->yoff = brick->yoff + row*gwy_brick_dy(brick);
        part->zoff = brick->zoff + level*gwy_brick_dz(brick);
    }

    return part;
}

/**
 * gwy_brick_set_size:
 * @brick: A three-dimensional data brick.
 * @xres: Desired X resolution.
 * @yres: Desired Y resolution.
 * @zres: Desired Z resolution.
 * @clear: %TRUE to fill the new brick data with zeroes, %FALSE to leave it
 *         uninitialised.
 *
 * Resizes a three-dimensional data brick.
 *
 * If the new data size differs from the old data size this method is only
 * marginally more efficient than destroying the old brick and creating a new
 * one.
 *
 * In no case the original data are preserved, not even if @xres, @yres and
 * @zres are equal to the current brick dimensions.  Use gwy_brick_new_part()
 * to extract a part of a brick into a new brick.  Only the dimensions are
 * changed; all other properies, such as physical dimensions, offsets and
 * units, are kept.
 **/
void
gwy_brick_set_size(GwyBrick *brick,
                   guint xres,
                   guint yres,
                   guint zres,
                   gboolean clear)
{
    g_return_if_fail(GWY_IS_BRICK(brick));
    g_return_if_fail(xres && yres && zres);

    GParamSpec *notify[3];
    guint nn = 0;
    if (brick->xres != xres)
        notify[nn++] = properties[PROP_XRES];
    if (brick->yres != yres)
        notify[nn++] = properties[PROP_YRES];
    if (brick->zres != zres)
        notify[nn++] = properties[PROP_ZRES];

    if (brick->xres*brick->yres*brick->zres != xres*yres*zres) {
        free_data(brick);
        brick->xres = xres;
        brick->yres = yres;
        brick->zres = zres;
        alloc_data(brick, clear);
    }
    else {
        brick->xres = xres;
        brick->yres = yres;
        brick->zres = zres;
        if (clear)
            gwy_brick_clear_full(brick);
        else
            gwy_brick_invalidate(brick);
    }
    _gwy_notify_properties_by_pspec(G_OBJECT(brick), notify, nn);
}

/**
 * gwy_brick_data_changed:
 * @brick: A three-dimensional data brick.
 * @bpart: (allow-none):
 *         Part of @brick that has changed.  Passing %NULL means the entire
 *         brick.
 *
 * Emits signal GwyBrick::data-changed on a brick.
 **/
void
gwy_brick_data_changed(GwyBrick *brick,
                       GwyBrickPart *bpart)
{
    g_return_if_fail(GWY_IS_BRICK(brick));
    g_signal_emit(brick, signals[SGNL_DATA_CHANGED], 0, bpart);
}

/**
 * gwy_brick_copy:
 * @src: Source three-dimensional data data brick.
 * @srcpart: Area in brick @src to copy.  Pass %NULL to copy entire @src.
 * @dest: Destination three-dimensional data brick.
 * @destcol: Destination column in @dest.
 * @destrow: Destination row in @dest.
 * @destlevel: Destination level in @dest.
 *
 * Copies data from one brick to another.
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
gwy_brick_copy(const GwyBrick *src,
               const GwyBrickPart *srcpart,
               GwyBrick *dest,
               guint destcol,
               guint destrow,
               guint destlevel)
{
    guint col, row, level, width, height, depth;
    if (!gwy_brick_limit_parts(src, srcpart, dest, destcol, destrow, destlevel,
                               &col, &row, &level, &width, &height, &depth))
        return;

    // Entire brick part
    if (width == src->xres && width == dest->xres
        && height == src->yres && height == dest->yres) {
        g_assert(col == 0 && destcol == 0 && row == 0 && destrow == 0);
        gwy_assign(dest->data + width*height*destlevel,
                   src->data + width*height*level,
                   width*height*depth);
        // XXX: Caches for full brick
        gwy_brick_invalidate(dest);
    }
    else {
        const gdouble *src0 = BASE(src, col, row, level);
        gdouble *dest0 = BASE(dest, destcol, destrow, destlevel);
        for (guint l = 0; l < depth; l++) {
            const gdouble *src1 = src0 + l*src->xres*src->yres;
            gdouble *dest1 = dest0 + l*dest->xres*dest->yres;
            for (guint i = 0; i < height; i++)
                gwy_assign(dest1 + dest->xres*i, src1 + src->xres*i, width);
        }
        gwy_brick_invalidate(dest);
    }
}

/**
 * gwy_brick_copy_full:
 * @src: Source three-dimensional data brick.
 * @dest: Destination three-dimensional data brick.
 *
 * Copies the entire data from one brick to another.
 *
 * The two bricks must be of identical dimensions.
 *
 * Only the data are copied.  To make a brick completely identical to another,
 * including units, offsets and change of dimensions, you can use
 * gwy_brick_assign().
 **/
void
gwy_brick_copy_full(const GwyBrick *src,
                    GwyBrick *dest)
{
    g_return_if_fail(GWY_IS_BRICK(src));
    g_return_if_fail(GWY_IS_BRICK(dest));
    // This is a sanity check as gwy_brick_copy() can handle anything.
    g_return_if_fail(src->xres == dest->xres
                     && src->yres == dest->yres
                     && src->zres == dest->zres);
    gwy_brick_copy(src, NULL, dest, 0, 0, 0);
}

/**
 * gwy_brick_invalidate:
 * @brick: A three-dimensional data brick.
 *
 * Invalidates cached brick statistics.
 *
 * All #GwyBrick methods invalidate (or, in some cases, recalculate) cached
 * statistics if they modify the data.
 *
 * If you write to @brick's data directly and namely mix writing to the brick
 * data with calls to methods providing overall brick characteristics (minimum,
 * maximum, mean value, etc.) you may have to explicitly invalidate the cached
 * values as the methods have no means of knowing whether you changed the data
 * meanwhile or not:
 * |[
 * gdouble *data = brick->data;
 *
 * for (i = 0; i < xres*yres*zres; i++) {
 *     // Change data.
 * }
 * gwy_brick_invalidate(data);          // Invalidate data as we changed it.
 * med = gwy_brick_median_full(brick);  // This is OK, cache was invalidated.
 *                                      // But now the new median is cached.
 * for (i = 0; i < xres*yres*zres; i++) {
 *     // Change data more.
 * }
 * gwy_brick_invalidate(brick);         // Forget the cached median value.
 * med = gwy_brick_median_full(brick);  // OK again, median is recalculated.
 * ]|
 **/
void
gwy_brick_invalidate(GwyBrick *brick)
{
    g_return_if_fail(GWY_IS_BRICK(brick));
}

/**
 * gwy_brick_set_xreal:
 * @brick: A three-dimensional data brick.
 * @xreal: Width in physical units.
 *
 * Sets the physical width of a three-dimensional data brick.
 **/
void
gwy_brick_set_xreal(GwyBrick *brick,
                    gdouble xreal)
{
    g_return_if_fail(GWY_IS_BRICK(brick));
    g_return_if_fail(xreal > 0.0);
    if (xreal != brick->xreal) {
        brick->xreal = xreal;
        g_object_notify_by_pspec(G_OBJECT(brick), properties[PROP_XREAL]);
    }
}

/**
 * gwy_brick_set_yreal:
 * @brick: A three-dimensional data brick.
 * @yreal: Width in physical units.
 *
 * Sets the physical height of a three-dimensional data brick.
 **/
void
gwy_brick_set_yreal(GwyBrick *brick,
                    gdouble yreal)
{
    g_return_if_fail(GWY_IS_BRICK(brick));
    g_return_if_fail(yreal > 0.0);
    if (yreal != brick->yreal) {
        brick->yreal = yreal;
        g_object_notify_by_pspec(G_OBJECT(brick), properties[PROP_YREAL]);
    }
}

/**
 * gwy_brick_set_zreal:
 * @brick: A three-dimensional data brick.
 * @zreal: Width in physical units.
 *
 * Sets the physical height of a three-dimensional data brick.
 **/
void
gwy_brick_set_zreal(GwyBrick *brick,
                    gdouble zreal)
{
    g_return_if_fail(GWY_IS_BRICK(brick));
    g_return_if_fail(zreal > 0.0);
    if (zreal != brick->zreal) {
        brick->zreal = zreal;
        g_object_notify_by_pspec(G_OBJECT(brick), properties[PROP_ZREAL]);
    }
}

/**
 * gwy_brick_set_xoffset:
 * @brick: A three-dimensional data brick.
 * @xoffset: Horizontal offset of the top-left corner from [0,0,0] in physical
 *           units.
 *
 * Sets the horizontal offset of a three-dimensional data brick.
 **/
void
gwy_brick_set_xoffset(GwyBrick *brick,
                      gdouble xoffset)
{
    g_return_if_fail(GWY_IS_BRICK(brick));
    if (xoffset != brick->xoff) {
        brick->xoff = xoffset;
        g_object_notify_by_pspec(G_OBJECT(brick), properties[PROP_XOFFSET]);
    }
}

/**
 * gwy_brick_set_yoffset:
 * @brick: A three-dimensional data brick.
 * @yoffset: Vertical offset of the top-left corner from [0,0,0] in physical
 *           units.
 *
 * Sets the vertical offset of a three-dimensional data brick.
 **/
void
gwy_brick_set_yoffset(GwyBrick *brick,
                      gdouble yoffset)
{
    g_return_if_fail(GWY_IS_BRICK(brick));
    if (yoffset != brick->yoff) {
        brick->yoff = yoffset;
        g_object_notify_by_pspec(G_OBJECT(brick), properties[PROP_YOFFSET]);
    }
}

/**
 * gwy_brick_set_zoffset:
 * @brick: A three-dimensional data brick.
 * @zoffset: Depth offset of the top-left corner from [0,0,0] in physical
 *           units.
 *
 * Sets the vertical offset of a three-dimensional data brick.
 **/
void
gwy_brick_set_zoffset(GwyBrick *brick,
                      gdouble zoffset)
{
    g_return_if_fail(GWY_IS_BRICK(brick));
    if (zoffset != brick->zoff) {
        brick->zoff = zoffset;
        g_object_notify_by_pspec(G_OBJECT(brick), properties[PROP_ZOFFSET]);
    }
}

/**
 * gwy_brick_get_unit_x:
 * @brick: A three-dimensional data brick.
 *
 * Obtains the horizotnal lateral units of a three-dimensional data brick.
 *
 * Returns: (transfer none):
 *          The horizotnal lateral units of @brick.
 **/
GwyUnit*
gwy_brick_get_unit_x(const GwyBrick *brick)
{
    g_return_val_if_fail(GWY_IS_BRICK(brick), NULL);
    Brick *priv = brick->priv;
    if (!priv->unit_x)
        priv->unit_x = gwy_unit_new();
    return priv->unit_x;
}

/**
 * gwy_brick_get_unit_y:
 * @brick: A three-dimensional data brick.
 *
 * Obtains the vertical lateral units of a three-dimensional data brick.
 *
 * Returns: (transfer none):
 *          The vertical lateral units of @brick.
 **/
GwyUnit*
gwy_brick_get_unit_y(const GwyBrick *brick)
{
    g_return_val_if_fail(GWY_IS_BRICK(brick), NULL);
    Brick *priv = brick->priv;
    if (!priv->unit_y)
        priv->unit_y = gwy_unit_new();
    return priv->unit_y;
}

/**
 * gwy_brick_get_unit_z:
 * @brick: A three-dimensional data brick.
 *
 * Obtains the depth units of a three-dimensional data brick.
 *
 * Returns: (transfer none):
 *          The depth units of @brick.
 **/
GwyUnit*
gwy_brick_get_unit_z(const GwyBrick *brick)
{
    g_return_val_if_fail(GWY_IS_BRICK(brick), NULL);
    Brick *priv = brick->priv;
    if (!priv->unit_z)
        priv->unit_z = gwy_unit_new();
    return priv->unit_z;
}

/**
 * gwy_brick_get_unit_w:
 * @brick: A three-dimensional data brick.
 *
 * Obtains the value units of a three-dimensional data brick.
 *
 * Returns: (transfer none):
 *          The value units of @brick.
 **/
GwyUnit*
gwy_brick_get_unit_w(const GwyBrick *brick)
{
    g_return_val_if_fail(GWY_IS_BRICK(brick), NULL);
    Brick *priv = brick->priv;
    if (!priv->unit_w)
        priv->unit_w = gwy_unit_new();
    return priv->unit_w;
}

/**
 * gwy_brick_xy_units_match:
 * @brick: A three-dimensional data brick.
 *
 * Checks whether a three-dimensional data brick has the same lateral units in
 * both coordinates.
 *
 * Returns: %TRUE if @x and @y units of @brick are equal, %FALSE if they
 *          differ.
 **/
gboolean
gwy_brick_xy_units_match(const GwyBrick *brick)
{
    g_return_val_if_fail(GWY_IS_BRICK(brick), FALSE);
    Brick *priv = brick->priv;
    return gwy_unit_equal(priv->unit_x, priv->unit_y);
}

/**
 * gwy_brick_xyz_units_match:
 * @brick: A three-dimensional data brick.
 *
 * Checks whether a three-dimensional data brick has all coordinate units
 * identical.
 *
 * Returns: %TRUE if @x, @y and @z units of @brick are equal, %FALSE if any
 *          two differ.
 **/
gboolean
gwy_brick_xyz_units_match(const GwyBrick *brick)
{
    g_return_val_if_fail(GWY_IS_BRICK(brick), FALSE);
    Brick *priv = brick->priv;
    return (gwy_unit_equal(priv->unit_x, priv->unit_y)
            && gwy_unit_equal(priv->unit_x, priv->unit_z));
}

/**
 * gwy_brick_check_part:
 * @brick: A three-dimensional data brick.
 * @bpart: (allow-none):
 *         Part of @brick, possibly %NULL.
 * @col: Location to store the actual column index of the top upper-left corner
 *       of the part.
 * @row: Location to store the actual row index of the top upper-left corner
 *       of the part.
 * @level: Location to store the actual level index of the top upper-left
 *         corner of the part.
 * @width: Location to store the actual width (number of columns)
 *         of the part.
 * @height: Location to store the actual height (number of rows)
 *          of the part.
 * @depth: Location to store the actual depth (number of levels)
 *         of the part.
 *
 * Validates the position and dimensions of a brick part.
 *
 * If @bpart is %NULL entire @brick is to be used.  Otherwise @bpart must be
 * contained in @brick.
 *
 * If the position and dimensions are valid @col, @row, @level, @width, @height
 * and @depth are set to the actual rectangular part in @brick.  If the
 * function returns %FALSE their values are undefined.
 *
 * This function is typically used in functions that operate on a part of a
 * brick.
 *
 * Returns: %TRUE if the position and dimensions are valid and the caller
 *          should proceed.  %FALSE if the caller should not proceed, either
 *          because @brick is not a #GwyBrick instance or the position or
 *          dimensions is invalid (a critical error is emitted in these cases)
 *          or the actual part is zero-sized.
 **/
gboolean
gwy_brick_check_part(const GwyBrick *brick,
                     const GwyBrickPart *bpart,
                     guint *col, guint *row, guint *level,
                     guint *width, guint *height, guint *depth)
{
    g_return_val_if_fail(GWY_IS_BRICK(brick), FALSE);
    if (bpart) {
        if (!bpart->width || !bpart->height || !bpart->depth)
            return FALSE;
        // The two separate conditions are to catch integer overflows.
        g_return_val_if_fail(bpart->col < brick->xres, FALSE);
        g_return_val_if_fail(bpart->width <= brick->xres - bpart->col, FALSE);
        g_return_val_if_fail(bpart->row < brick->yres, FALSE);
        g_return_val_if_fail(bpart->height <= brick->yres - bpart->row, FALSE);
        g_return_val_if_fail(bpart->level < brick->zres, FALSE);
        g_return_val_if_fail(bpart->depth <= brick->zres - bpart->level, FALSE);
        *col = bpart->col;
        *row = bpart->row;
        *level = bpart->level;
        *width = bpart->width;
        *height = bpart->height;
        *depth = bpart->depth;
    }
    else {
        *col = *row = *level = 0;
        *width = brick->xres;
        *height = brick->yres;
        *depth = brick->zres;
    }

    return TRUE;
}

/**
 * gwy_brick_check_plane_part:
 * @brick: A three-dimensional data brick.
 * @fpart: (allow-none):
 *         Part of @brick plane, possibly %NULL.
 * @col: Location to store the actual column index of the top upper-left corner
 *       of the part.
 * @row: Location to store the actual row index of the top upper-left corner
 *       of the part.
 * @level: Level index in @brick.
 * @width: Location to store the actual width (number of columns)
 *         of the part.
 * @height: Location to store the actual height (number of rows)
 *          of the part.
 *
 * Validates the position and dimensions of a planar brick part.
 *
 * If @fpart is %NULL entire @brick plane is to be used.  Otherwise @fpart must
 * be contained in a @brick plane; @level must be a valid level index in any
 * case.
 *
 * If the position and dimensions are valid @col, @row, @width and @height
 * are set to the actual rectangular part in a @brick plane.  If the function
 * returns %FALSE their values are undefined.
 *
 * This function is typically used in functions that extract a plane of a brick
 * into a field.
 *
 * Returns: %TRUE if the position and dimensions are valid and the caller
 *          should proceed.  %FALSE if the caller should not proceed, either
 *          because @brick is not a #GwyBrick instance or the position or
 *          dimensions is invalid (a critical error is emitted in these cases)
 *          or the actual part is zero-sized.
 **/
gboolean
gwy_brick_check_plane_part(const GwyBrick *brick,
                           const GwyFieldPart *fpart,
                           guint *col, guint *row, guint level,
                           guint *width, guint *height)
{
    g_return_val_if_fail(GWY_IS_BRICK(brick), FALSE);
    g_return_val_if_fail(level < brick->zres, FALSE);
    if (fpart) {
        if (!fpart->width || !fpart->height)
            return FALSE;
        // The two separate conditions are to catch integer overflows.
        g_return_val_if_fail(fpart->col < brick->xres, FALSE);
        g_return_val_if_fail(fpart->width <= brick->xres - fpart->col, FALSE);
        g_return_val_if_fail(fpart->row < brick->yres, FALSE);
        g_return_val_if_fail(fpart->height <= brick->yres - fpart->row, FALSE);
        *col = fpart->col;
        *row = fpart->row;
        *width = fpart->width;
        *height = fpart->height;
    }
    else {
        *col = *row = 0;
        *width = brick->xres;
        *height = brick->yres;
    }

    return TRUE;
}

/**
 * gwy_brick_check_line_part:
 * @brick: A three-dimensional data brick.
 * @lpart: (allow-none):
 *         Part of @brick section, possibly %NULL.
 * @col: Column index in @brick.
 * @row: Row index in @brick.
 * @level: Location to store the actual level index of the top upper-left
 *         corner of the part.
 * @depth: Location to store the actual depth (number of levels)
 *         of the part.
 *
 * Validates the position and dimensions of a line brick part.
 *
 * If @lpart is %NULL entire @brick section is to be used.  Otherwise @lpart
 * must be contained in a @brick section; @col and @row must be a valid column
 * and row indices in any case.
 *
 * If the position and dimensions are valid @level and @depth are set to the
 * actual segment in a @brick section.  If the function returns %FALSE their
 * values are undefined.
 *
 * This function is typically used in functions that extract a section of a
 * brick into a line.
 *
 * Returns: %TRUE if the position and dimensions are valid and the caller
 *          should proceed.  %FALSE if the caller should not proceed, either
 *          because @brick is not a #GwyBrick instance or the position or
 *          dimensions is invalid (a critical error is emitted in these cases)
 *          or the actual part is zero-sized.
 **/
gboolean
gwy_brick_check_line_part(const GwyBrick *brick,
                          const GwyLinePart *lpart,
                          guint col, guint row,
                          guint *level, guint *depth)
{
    g_return_val_if_fail(GWY_IS_BRICK(brick), FALSE);
    g_return_val_if_fail(col < brick->xres, FALSE);
    g_return_val_if_fail(row < brick->yres, FALSE);
    if (lpart) {
        if (!lpart->len)
            return FALSE;
        // The two separate conditions are to catch integer overflows.
        g_return_val_if_fail(lpart->pos < brick->zres, FALSE);
        g_return_val_if_fail(lpart->len <= brick->zres - lpart->pos, FALSE);
        *level = lpart->pos;
        *depth = lpart->len;
    }
    else {
        *level = 0;
        *depth = brick->zres;
    }

    return TRUE;
}

/**
 * gwy_brick_limit_parts:
 * @src: A source three-dimensional data brick.
 * @srcpart: (allow-none):
 *           Area in @src, possibly %NULL.
 * @dest: A destination three-dimensional data brick.
 * @destcol: Column index for the top upper-left corner of the part in @dest.
 * @destrow: Row index for the top upper-left corner of the part in @dest.
 * @destlevel: Level index for the top upper-left corner of the part in @dest.
 * @col: Location to store the actual column index of the top upper-left corner
 *       of the source part.
 * @row: Location to store the actual row index of the top upper-left corner
 *       of the source part.
 * @level: Location to store the actual level index of the top upper-left
 *         corner of the source part.
 * @width: Location to store the actual width (number of columns)
 *         of the source part.
 * @height: Location to store the actual height (number of rows)
 *          of the source part.
 * @depth: Location to store the actual depth (number of levels)
 *         of the source part.
 *
 * Limits the dimensions of a brick part for copying.
 *
 * The part is limited to make it contained both in @src and @dest and @col,
 * @row, @level, @width, @height and @depth are set to the actual position and
 * dimensions in @src.  If the function returns %FALSE their values are
 * undefined.
 *
 * If @src and @dest are the same brick the source and destination parts should
 * not overlap.
 *
 * This function is typically used in copy-like functions that transfer a part
 * of a brick into another brick.
 *
 * Returns: %TRUE if the caller should proceed.  %FALSE if the caller should
 *          not proceed, either because @brick or @target is not a #GwyBrick
 *          instance (a critical error is emitted in these cases) or the actual
 *          part is zero-sized.
 **/
gboolean
gwy_brick_limit_parts(const GwyBrick *src,
                      const GwyBrickPart *srcpart,
                      const GwyBrick *dest,
                      guint destcol, guint destrow, guint destlevel,
                      guint *col, guint *row, guint *level,
                      guint *width, guint *height, guint *depth)
{
    g_return_val_if_fail(GWY_IS_BRICK(src), FALSE);
    g_return_val_if_fail(GWY_IS_BRICK(dest), FALSE);

    if (srcpart) {
        *col = srcpart->col;
        *row = srcpart->row;
        *level = srcpart->level;
        *width = srcpart->width;
        *height = srcpart->height;
        *depth = srcpart->depth;
        if (*col >= src->xres || *row >= src->yres || *level >= src->zres)
            return FALSE;
        *width = MIN(*width, src->xres - *col);
        *height = MIN(*height, src->yres - *row);
        *depth = MIN(*depth, src->zres - *level);
    }
    else {
        *col = *row = *level = 0;
        *width = src->xres;
        *height = src->yres;
        *depth = src->zres;
    }

    if (destcol >= dest->xres || destrow >= dest->yres
        || destlevel >= dest->zres)
        return FALSE;

    *width = MIN(*width, dest->xres - destcol);
    *height = MIN(*height, dest->yres - destrow);
    *depth = MIN(*depth, dest->zres - destlevel);

    if (src == dest) {
        if (gwy_overlapping(*col, *width, destcol, *width)
            && gwy_overlapping(*row, *height, destrow, *height)
            && gwy_overlapping(*level, *depth, destlevel, *depth)) {
            g_warning("Source and destination blocks overlap.  "
                      "Data corruption is imminent.");
        }
    }

    return *width && *height && *depth;
}

/**
 * gwy_brick_check_target:
 * @brick: A three-dimensional data brick.
 * @target: A three-dimensional data brick.
 * @bpart: (allow-none):
 *         Area in @brick and/or @target, possibly %NULL which means entire
 *         @brick.
 * @targetcol: Location to store the actual column index of the top upper-left
 *             corner of the part in the target.
 * @targetrow: Location to store the actual row index of the top upper-left
 *             corner of the part in the target.
 * @targetlevel: Location to store the actual level index of the top upper-left
 *               corner of the part in the target.
 *
 * Validates the position and dimensions of a target brick.
 *
 * Dimensions of @target must match either @brick or @bpart.  In the first case
 * the rectangular part is the same in @brick and @target.  In the second case
 * the target corresponds only to the brick part.
 *
 * If the position and dimensions are valid @targetcol, @targetrow and
 * @targetlevel are set to the actual position in @target.  If the function
 * returns %FALSE their values are undefined.
 *
 * This function is typically used in functions that operate on a part of a
 * brick and produce data of the size of this part.
 *
 * Returns: %TRUE if the position and dimensions are valid and the caller
 *          should proceed.  %FALSE if the caller should not proceed, either
 *          because @brick or @target is not a #GwyBrick instance or the
 *          position or dimensions is invalid (a critical error is emitted in
 *          these cases) or the actual part is zero-sized.
 **/
gboolean
gwy_brick_check_target(const GwyBrick *brick,
                       const GwyBrick *target,
                       const GwyBrickPart *bpart,
                       guint *targetcol,
                       guint *targetrow,
                       guint *targetlevel)
{
    g_return_val_if_fail(GWY_IS_BRICK(brick), FALSE);
    g_return_val_if_fail(GWY_IS_BRICK(target), FALSE);

    // We normally always pass non-NULL @bpart but permit also NULL.
    if (!bpart)
        bpart = &(GwyBrickPart){ 0, 0, 0, brick->xres, brick->yres, brick->zres };
    else if (!bpart->width || !bpart->height || !bpart->depth)
        return FALSE;

    if (target->xres == brick->xres
        && target->yres == brick->yres
        && target->zres == brick->zres) {
        *targetcol = bpart->col;
        *targetrow = bpart->row;
        *targetlevel = bpart->level;
        return TRUE;
    }
    if (target->xres == bpart->width
        && target->yres == bpart->height
        && target->zres == bpart->depth) {
        *targetcol = *targetrow = *targetlevel = 0;
        return TRUE;
    }

    g_critical("Target brick dimensions match neither source brick nor the "
               "part.");
    return FALSE;
}

/**
 * gwy_brick_format_x:
 * @brick: A three-dimensional data brick.
 * @style: Output format style.
 *
 * Finds a suitable format for displaying horizontal lateral coordinates in a
 * data brick.
 *
 * The created format has a sufficient precision to represent coordinates
 * of neighbour pixels as different values.
 *
 * Returns: (transfer full):
 *          A newly created value format.
 **/
GwyValueFormat*
gwy_brick_format_x(const GwyBrick *brick,
                   GwyValueFormatStyle style)
{
    g_return_val_if_fail(GWY_IS_BRICK(brick), NULL);
    gdouble max = fmax(brick->xreal, fabs(brick->xreal + brick->xoff));
    gdouble unit = gwy_brick_dx(brick);
    return gwy_unit_format_with_resolution(gwy_brick_get_unit_x(brick),
                                           style, max, unit);
}

/**
 * gwy_brick_format_y:
 * @brick: A three-dimensional data brick.
 * @style: Output format style.
 *
 * Finds a suitable format for displaying vertical lateral coordinates in a
 * data brick.
 *
 * The created format has a sufficient precision to represent coordinates
 * of neighbour pixels as different values.
 *
 * Returns: (transfer full):
 *          A newly created value format.
 **/
GwyValueFormat*
gwy_brick_format_y(const GwyBrick *brick,
                   GwyValueFormatStyle style)
{
    g_return_val_if_fail(GWY_IS_BRICK(brick), NULL);
    gdouble max = fmax(brick->yreal, fabs(brick->yreal + brick->yoff));
    gdouble unit = gwy_brick_dy(brick);
    return gwy_unit_format_with_resolution(gwy_brick_get_unit_y(brick),
                                           style, max, unit);
}

/**
 * gwy_brick_format_xy:
 * @brick: A three-dimensional data brick.
 * @style: Output format style.
 *
 * Finds a suitable format for displaying lateral coordinates in a data brick.
 *
 * The created format has a sufficient precision to represent coordinates
 * of neighbour pixels as different values.
 *
 * This function can be used only if units of @x and @y are identical which is
 * common but does not hold universally.  If it holds this function is usually
 * preferable to separate gwy_brick_format_x() and gwy_brick_format_y() because
 * the same format is then used for both coordinates. The separate functions do
 * not guarantee this for bricks with non-square bases even if @x and @y units
 * are the same.
 *
 * Returns: (transfer full):
 *          A newly created value format.
 **/
GwyValueFormat*
gwy_brick_format_xy(const GwyBrick *brick,
                    GwyValueFormatStyle style)
{
    g_return_val_if_fail(GWY_IS_BRICK(brick), NULL);
    if (!gwy_unit_equal(brick->priv->unit_x, brick->priv->unit_y)) {
        g_warning("X and Y units of brick do not match.");
        return gwy_brick_format_x(brick, style);
    }
    gdouble max0 = fmax(brick->xreal, brick->yreal);
    gdouble maxoff = fmax(fabs(brick->xreal + brick->xoff),
                          fabs(brick->yreal + brick->yoff));
    gdouble max = fmax(max0, maxoff);
    gdouble unit = fmin(gwy_brick_dx(brick), gwy_brick_dy(brick));
    return gwy_unit_format_with_resolution(gwy_brick_get_unit_x(brick),
                                           style, max, unit);
}

/**
 * gwy_brick_format_z:
 * @brick: A three-dimensional data brick.
 * @style: Output format style.
 *
 * Finds a suitable format for displaying depth coordinates in a data brick.
 *
 * The created format has a sufficient precision to represent coordinates
 * of neighbour pixels as different values.
 *
 * Returns: (transfer full):
 *          A newly created value format.
 **/
GwyValueFormat*
gwy_brick_format_z(const GwyBrick *brick,
                   GwyValueFormatStyle style)
{
    g_return_val_if_fail(GWY_IS_BRICK(brick), NULL);
    gdouble max = fmax(brick->zreal, fabs(brick->zreal + brick->zoff));
    gdouble unit = gwy_brick_dz(brick);
    return gwy_unit_format_with_resolution(gwy_brick_get_unit_z(brick),
                                           style, max, unit);
}

/**
 * gwy_brick_set_name:
 * @brick: A three-dimensional data brick.
 * @name: (allow-none):
 *        New brick name.
 *
 * Sets the name of a three-dimensional data brick.
 **/
void
gwy_brick_set_name(GwyBrick *brick,
                   const gchar *name)
{
    g_return_if_fail(GWY_IS_BRICK(brick));
    if (!gwy_assign_string(&brick->priv->name, name))
        return;

    g_object_notify_by_pspec(G_OBJECT(brick), properties[PROP_NAME]);
}

/**
 * gwy_brick_get_name:
 * @brick: A three-dimensional data brick.
 *
 * Gets the name of a three-dimensional data brick.
 *
 * Returns: (allow-none):
 *          Brick name, owned by @brick.
 **/
const gchar*
gwy_brick_get_name(const GwyBrick *brick)
{
    g_return_val_if_fail(GWY_IS_BRICK(brick), NULL);
    return brick->priv->name;
}

/**
 * gwy_brick_get:
 * @brick: A three-dimensional data brick.
 * @col: Column index in @brick.
 * @row: Row index in @brick.
 * @level: Level index in @brick.
 *
 * Obtains a single brick value.
 *
 * This function exists <emphasis>only for language bindings</emphasis> as it
 * is very slow compared to gwy_brick_index() or simply accessing @data in
 * #GwyBrick directly in C.
 *
 * Returns: The value at (@col,@row,@level).
 **/
gdouble
gwy_brick_get(const GwyBrick *brick,
              guint col,
              guint row,
              guint level)
{
    g_return_val_if_fail(GWY_IS_BRICK(brick), NAN);
    g_return_val_if_fail(col < brick->xres, NAN);
    g_return_val_if_fail(row < brick->yres, NAN);
    g_return_val_if_fail(level < brick->zres, NAN);
    return gwy_brick_index(brick, col, row, level);
}

/**
 * gwy_brick_set:
 * @brick: A three-dimensional data brick.
 * @col: Column index in @brick.
 * @row: Row index in @brick.
 * @level: Level index in @brick.
 * @value: Value to store at given position.
 *
 * Sets a single brick value.
 *
 * This function exists <emphasis>only for language bindings</emphasis> as it
 * is very slow compared to gwy_brick_index() or simply accessing @data in
 * #GwyBrick directly in C.
 **/
void
gwy_brick_set(const GwyBrick *brick,
              guint col,
              guint row,
              guint level,
              gdouble value)
{
    g_return_if_fail(GWY_IS_BRICK(brick));
    g_return_if_fail(col < brick->xres);
    g_return_if_fail(row < brick->yres);
    g_return_if_fail(level < brick->zres);
    gwy_brick_index(brick, col, row, level) = value;
    // Invalidate.
    //brick->priv->cached = 0;
}

/**
 * SECTION: brick
 * @title: GwyBrick
 * @short_description: Three-dimensional data in regular grid
 *
 * #GwyBrick represents three-dimensional data in a regular grid.
 *
 * Data are stored in a flat array @data in #GwyBrick-struct as #gdouble
 * values, stored by levels, then by rows.  This means the column index is the
 * fastest index, row index is the meidum index and level index is the slow
 * one, the top left corner indices are (0,0,0).  The array is contiguous, i.e.
 * there is no padding at the end of each row or level (and neither beween
 * pixels).  No methods to get and set individual values or rows and columns
 * are provided except gwy_brick_index().  The usual mode of operation is to
 * access the data directly, bearing a few things in mind:
 * <itemizedlist>
 *   <listitem>All #GwyBrick-struct struct bricks must be considered
 *   read-only.</listitem>
 *   <listitem>For reading, access @data in #GwyBrick-struct
 *   directly.</listitem>
 *   <listitem>For writing, you can write to @data <emphasis>content</emphasis>
 *   but you must not change the brick itself.  Use methods such as
 *   gwy_brick_set_xreal() to change the brick properties.</listitem>
 *   <listitem>If you mix direct changes of data with functions that obtain
 *   overall brick statistics, you may have to use gwy_brick_invalidate() to
 *   induce recalculation of the statistics.</listitem>
 *   <listitem>Always try to access the brick data sequentially, as they are
 *   stored in memory.  Even if it means for instance allocating a buffer to
 *   hold a value for each column.  More on that below.</listitem>
 * </itemizedlist>
 **/

/**
 * GwyBrick:
 * @xres: X-resolution, i.e. width in pixels.
 * @yres: Y-resolution, i.e. height in pixels.
 * @zres: Z-resolution, i.e. depth in pixels.
 * @xreal: Width in physical units.
 * @yreal: Height in physical units.
 * @zreal: Depth in physical units.
 * @xoff: X-offset of the top-upper-left corner from [0,0,0] in physical units.
 * @yoff: Y-offset of the top-upper-left corner from [0,0,0] in physical units.
 * @zoff: Z-offset of the top-upper-left corner from [0,0,0] in physical units.
 * @data: Brick data.  See the introductory section for details.
 *
 * Object representing three-dimensional data in a regular grid.
 *
 * The #GwyBrick struct contains some public fields that can be directly
 * accessed for reading.  To set them, you must use the methods such as
 * gwy_brick_set_xreal().
 **/

/**
 * GwyBrickClass:
 *
 * Class of three-dimensional data bricks.
 **/

/**
 * gwy_brick_duplicate:
 * @brick: A three-dimensional data brick.
 *
 * Duplicates a three-dimensional data brick.
 *
 * This is a convenience wrapper of gwy_serializable_duplicate().
 **/

/**
 * gwy_brick_assign:
 * @dest: Destination three-dimensional data brick.
 * @src: Source three-dimensional data brick.
 *
 * Copies the value of a three-dimensional data brick.
 *
 * This is a convenience wrapper of gwy_serializable_assign().
 **/

/**
 * gwy_brick_index:
 * @brick: A three-dimensional data brick.
 * @col: Column index in @brick.
 * @row: Row index in @brick.
 * @level: Level index in @brick.
 *
 * Accesses a three-dimensional data brick pixel.
 *
 * This macro may evaluate its arguments several times.
 * This macro expands to a left-hand side expression.
 *
 * No argument validation is performed.  If you process the data in a loop,
 * you are encouraged to access @data in #GwyBrick-struct directly.
 * |[
 * // Read a brick value.
 * gdouble value = gwy_brick_index(brick, 1, 2, 3);
 *
 * // Write it elsewhere.
 * gwy_brick_index(brick, 4, 5, 6) = value;
 * ]|
 **/

/**
 * gwy_brick_dx:
 * @brick: A three-dimensional data brick.
 *
 * Calculates the horizontal pixel size in physical units.
 *
 * This macro may evaluate its arguments several times.
 **/

/**
 * gwy_brick_dy:
 * @brick: A three-dimensional data brick.
 *
 * Calculates the vertical pixel size in physical units.
 *
 * This macro may evaluate its arguments several times.
 **/

/**
 * gwy_brick_dz:
 * @brick: A three-dimensional data brick.
 *
 * Calculates the depth-ward pixel size in physical units.
 *
 * This macro may evaluate its arguments several times.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
