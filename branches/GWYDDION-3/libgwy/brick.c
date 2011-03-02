/*
 *  $Id$
 *  Copyright (C) 2011 David Nečas (Yeti).
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
#include "libgwy/brick.h"
#include "libgwy/brick-arithmetic.h"
#include "libgwy/object-internal.h"
#include "libgwy/line-internal.h"
#include "libgwy/brick-internal.h"

#define BASE(brick, col, row, level) \
    (brick->data + ((level)*brick->yres + (row))*brick->xres + (col))

enum { N_ITEMS = 13 };

enum {
    DATA_CHANGED,
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
    PROP_UNIT_XY,
    PROP_UNIT_Z,
    PROP_UNIT_W,
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
    /*00*/ { .name = "xres",    .ctype = GWY_SERIALIZABLE_INT32,        },
    /*01*/ { .name = "yres",    .ctype = GWY_SERIALIZABLE_INT32,        },
    /*02*/ { .name = "zres",    .ctype = GWY_SERIALIZABLE_INT32,        },
    /*03*/ { .name = "xreal",   .ctype = GWY_SERIALIZABLE_DOUBLE,       },
    /*04*/ { .name = "yreal",   .ctype = GWY_SERIALIZABLE_DOUBLE,       },
    /*05*/ { .name = "zreal",   .ctype = GWY_SERIALIZABLE_DOUBLE,       },
    /*06*/ { .name = "xoff",    .ctype = GWY_SERIALIZABLE_DOUBLE,       },
    /*07*/ { .name = "yoff",    .ctype = GWY_SERIALIZABLE_DOUBLE,       },
    /*08*/ { .name = "zoff",    .ctype = GWY_SERIALIZABLE_DOUBLE,       },
    /*09*/ { .name = "unit-xy", .ctype = GWY_SERIALIZABLE_OBJECT,       },
    /*10*/ { .name = "unit-z",  .ctype = GWY_SERIALIZABLE_OBJECT,       },
    /*11*/ { .name = "unit-w",  .ctype = GWY_SERIALIZABLE_OBJECT,       },
    /*12*/ { .name = "data",    .ctype = GWY_SERIALIZABLE_DOUBLE_ARRAY, },
};

static guint brick_signals[N_SIGNALS];
static GParamSpec *brick_pspecs[N_PROPS];

G_DEFINE_TYPE_EXTENDED
    (GwyBrick, gwy_brick, G_TYPE_OBJECT, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_brick_serializable_init))

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

    brick_pspecs[PROP_XRES]
        = g_param_spec_uint("x-res",
                            "X resolution",
                            "Pixel width of the brick.",
                            1, G_MAXUINT, 1,
                            G_PARAM_READABLE | STATICP);

    brick_pspecs[PROP_YRES]
        = g_param_spec_uint("y-res",
                            "Y resolution",
                            "Pixel height of the brick.",
                            1, G_MAXUINT, 1,
                            G_PARAM_READABLE | STATICP);

    brick_pspecs[PROP_ZRES]
        = g_param_spec_uint("z-res",
                            "Z resolution",
                            "Pixel depth of the brick.",
                            1, G_MAXUINT, 1,
                            G_PARAM_READABLE | STATICP);

    brick_pspecs[PROP_XREAL]
        = g_param_spec_double("x-real",
                              "X real size",
                              "Width of the brick in physical units.",
                              G_MINDOUBLE, G_MAXDOUBLE, 1.0,
                              G_PARAM_READWRITE | STATICP);

    brick_pspecs[PROP_YREAL]
        = g_param_spec_double("y-real",
                              "Y real size",
                              "Height of the brick in physical units.",
                              G_MINDOUBLE, G_MAXDOUBLE, 1.0,
                              G_PARAM_READWRITE | STATICP);

    brick_pspecs[PROP_ZREAL]
        = g_param_spec_double("z-real",
                              "Z real size",
                              "Depth of the brick in physical units.",
                              G_MINDOUBLE, G_MAXDOUBLE, 1.0,
                              G_PARAM_READWRITE | STATICP);

    brick_pspecs[PROP_XOFFSET]
        = g_param_spec_double("x-offset",
                              "X offset",
                              "Horizontal offset of the brick top left corner "
                              "in physical units.",
                              -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
                              G_PARAM_READWRITE | STATICP);

    brick_pspecs[PROP_YOFFSET]
        = g_param_spec_double("y-offset",
                              "Y offset",
                              "Vertical offset of the brick top left corner "
                              "in physical units.",
                              -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
                              G_PARAM_READWRITE | STATICP);

    brick_pspecs[PROP_ZOFFSET]
        = g_param_spec_double("z-offset",
                              "Z offset",
                              "Depth offset of the brick top left corner "
                              "in physical units.",
                              -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
                              G_PARAM_READWRITE | STATICP);

    brick_pspecs[PROP_UNIT_XY]
        = g_param_spec_object("unit-xy",
                              "XY unit",
                              "Physical units of lateral dimensions of the "
                              "brick layers.",
                              GWY_TYPE_UNIT,
                              G_PARAM_READABLE | STATICP);

    brick_pspecs[PROP_UNIT_Z]
        = g_param_spec_object("unit-z",
                              "Z unit",
                              "Physical units of brick depth.",
                              GWY_TYPE_UNIT,
                              G_PARAM_READABLE | STATICP);

    brick_pspecs[PROP_UNIT_W]
        = g_param_spec_object("unit-w",
                              "W unit",
                              "Physical units of brick values.",
                              GWY_TYPE_UNIT,
                              G_PARAM_READABLE | STATICP);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, brick_pspecs[i]);

    /**
     * GwyBrick::data-changed:
     * @gwybrick: The #GwyBrick which received the signal.
     *
     * The ::data-changed signal is emitted whenever brick data changes.
     **/
    brick_signals[DATA_CHANGED]
        = g_signal_new_class_handler("data-changed",
                                     G_OBJECT_CLASS_TYPE(klass),
                                     G_SIGNAL_RUN_FIRST,
                                     NULL, NULL, NULL,
                                     g_cclosure_marshal_VOID__VOID,
                                     G_TYPE_NONE, 0);
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
    free_data(brick);
    G_OBJECT_CLASS(gwy_brick_parent_class)->finalize(object);
}

static void
gwy_brick_dispose(GObject *object)
{
    GwyBrick *brick = GWY_BRICK(object);
    GWY_OBJECT_UNREF(brick->priv->unit_xy);
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
    if (priv->unit_xy)
       n += gwy_serializable_n_items(GWY_SERIALIZABLE(priv->unit_xy));
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

    it = serialize_items[3];
    it.value.v_double = brick->xreal;
    items->items[items->n++] = it;
    n++;

    it = serialize_items[4];
    it.value.v_double = brick->yreal;
    items->items[items->n++] = it;
    n++;

    it = serialize_items[5];
    it.value.v_double = brick->zreal;
    items->items[items->n++] = it;
    n++;

    if (brick->xoff) {
        it = serialize_items[6];
        it.value.v_double = brick->xoff;
        items->items[items->n++] = it;
        n++;
    }

    if (brick->yoff) {
        it = serialize_items[7];
        it.value.v_double = brick->yoff;
        items->items[items->n++] = it;
        n++;
    }

    if (brick->zoff) {
        it = serialize_items[8];
        it.value.v_double = brick->zoff;
        items->items[items->n++] = it;
        n++;
    }

    if (priv->unit_xy) {
        g_return_val_if_fail(items->len - items->n, 0);
        it = serialize_items[9];
        it.value.v_object = (GObject*)priv->unit_xy;
        items->items[items->n++] = it;
        gwy_serializable_itemize(GWY_SERIALIZABLE(priv->unit_xy), items);
        n++;
    }

    if (priv->unit_z) {
        g_return_val_if_fail(items->len - items->n, 0);
        it = serialize_items[10];
        it.value.v_object = (GObject*)priv->unit_z;
        items->items[items->n++] = it;
        gwy_serializable_itemize(GWY_SERIALIZABLE(priv->unit_z), items);
        n++;
    }

    if (priv->unit_z) {
        g_return_val_if_fail(items->len - items->n, 0);
        it = serialize_items[11];
        it.value.v_object = (GObject*)priv->unit_w;
        items->items[items->n++] = it;
        gwy_serializable_itemize(GWY_SERIALIZABLE(priv->unit_w), items);
        n++;
    }

    g_return_val_if_fail(items->len - items->n, 0);
    it = serialize_items[12];
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
    // Set defaults
    its[3].value.v_double = brick->xreal;
    its[4].value.v_double = brick->yreal;
    its[5].value.v_double = brick->zreal;
    its[6].value.v_double = brick->xoff;
    its[7].value.v_double = brick->yoff;
    its[8].value.v_double = brick->zoff;
    gwy_deserialize_filter_items(its, N_ITEMS, items, NULL,
                                 "GwyBrick", error_list);

    if (G_UNLIKELY(!its[0].value.v_uint32
                   || !its[1].value.v_uint32
                   || !its[2].value.v_uint32)) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_INVALID,
                           _("Brick dimensions %u×%u×%u are invalid."),
                           its[0].value.v_uint32,
                           its[1].value.v_uint32,
                           its[2].value.v_uint32);
        goto fail;
    }

    if (G_UNLIKELY(its[0].value.v_uint32 * its[1].value.v_uint32
                   * its[2].value.v_uint32
                   != its[12].array_size)) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_INVALID,
                           _("Brick dimensions %u×%u×%u do not match data "
                             "size %lu."),
                           its[0].value.v_uint32,
                           its[1].value.v_uint32,
                           its[2].value.v_uint32,
                           (gulong)its[12].array_size);
        goto fail;
    }

    if (!_gwy_check_object_component(its + 9, brick, GWY_TYPE_UNIT, error_list))
        goto fail;
    if (!_gwy_check_object_component(its + 10, brick, GWY_TYPE_UNIT, error_list))
        goto fail;
    if (!_gwy_check_object_component(its + 11, brick, GWY_TYPE_UNIT, error_list))
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
    priv->unit_xy = (GwyUnit*)its[9].value.v_object;
    priv->unit_z = (GwyUnit*)its[10].value.v_object;
    priv->unit_w = (GwyUnit*)its[11].value.v_object;
    free_data(brick);
    brick->data = its[12].value.v_double_array;
    priv->allocated = TRUE;

    return TRUE;

fail:
    GWY_OBJECT_UNREF(its[9].value.v_object);
    GWY_OBJECT_UNREF(its[10].value.v_object);
    GWY_OBJECT_UNREF(its[11].value.v_object);
    GWY_FREE(its[12].value.v_double_array);
    return FALSE;
}

static GObject*
gwy_brick_duplicate_impl(GwySerializable *serializable)
{
    GwyBrick *brick = GWY_BRICK(serializable);
    GwyBrick *duplicate = gwy_brick_new_alike(brick, FALSE);

    gwy_assign(duplicate->data, brick->data,
               brick->xres*brick->yres*brick->zres);
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
    ASSIGN_UNITS(dpriv->unit_xy, spriv->unit_xy);
    ASSIGN_UNITS(dpriv->unit_z, spriv->unit_z);
    ASSIGN_UNITS(dpriv->unit_w, spriv->unit_w);
}

static void
gwy_brick_assign_impl(GwySerializable *destination,
                      GwySerializable *source)
{
    GwyBrick *dest = GWY_BRICK(destination);
    GwyBrick *src = GWY_BRICK(source);
    Brick *dpriv = dest->priv;

    GParamSpec *notify[N_PROPS];
    guint nn = 0;
    if (dest->xres != src->xres)
        notify[nn++] = brick_pspecs[PROP_XRES];
    if (dest->yres != src->yres)
        notify[nn++] = brick_pspecs[PROP_YRES];
    if (dest->zres != src->zres)
        notify[nn++] = brick_pspecs[PROP_ZRES];
    if (dest->xreal != src->xreal)
        notify[nn++] = brick_pspecs[PROP_XREAL];
    if (dest->yreal != src->yreal)
        notify[nn++] = brick_pspecs[PROP_YREAL];
    if (dest->zreal != src->zreal)
        notify[nn++] = brick_pspecs[PROP_ZREAL];
    if (dest->xoff != src->xoff)
        notify[nn++] = brick_pspecs[PROP_XOFFSET];
    if (dest->yoff != src->yoff)
        notify[nn++] = brick_pspecs[PROP_YOFFSET];
    if (dest->zoff != src->zoff)
        notify[nn++] = brick_pspecs[PROP_ZOFFSET];

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

        case PROP_UNIT_W:
        // Instantiate the units to be consistent with the direct interface
        // that never admits the units are NULL.
        if (!priv->unit_w)
            priv->unit_w = gwy_unit_new();
        g_value_set_object(value, priv->unit_w);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

/**
 * gwy_brick_new:
 *
 * Creates a new two-dimensional data brick.
 *
 * The brick dimensions will be 1×1×1 and it will be zero-filled.  This
 * parameterless constructor exists mainly for language bindings,
 * gwy_brick_new_sized() and gwy_brick_new_alike() are usually more useful.
 *
 * Returns: A new two-dimensional data brick.
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
 * Creates a new two-dimensional data brick of specified dimensions.
 *
 * Returns: A new two-dimensional data brick.
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
 * @model: A two-dimensional data brick to use as the template.
 * @clear: %TRUE to fill the new brick data with zeroes, %FALSE to leave it
 *         uninitialised.
 *
 * Creates a new two-dimensional data brick similar to another brick.
 *
 * All properties of the newly created brick will be identical to @model,
 * except the data that will be either zeroes or uninitialised.  Use
 * gwy_brick_duplicate() to completely duplicate a brick including data.
 *
 * Returns: A new two-dimensional data brick.
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
 * @brick: A two-dimensional data brick.
 * @bpart: (allow-none):
 *             Area in @brick to extract to the new brick.  Passing %NULL
 *             creates an identical copy of @brick, similarly to
 *             gwy_brick_duplicate() (though with @keep_offsets set to %FALSE
 *             the offsets are reset).
 * @keep_offsets: %TRUE to set the X and Y offsets of the new brick
 *                using @bpart and @brick offsets.  %FALSE to set offsets
 *                of the new brick to zeroes.
 *
 * Creates a new two-dimensional brick as a rectangular part of another brick.
 *
 * The rectangle specified by @bpart must be entirely contained in @brick.
 * Both dimensions must be non-zero.
 *
 * Data are physically copied, i.e. changing the new brick data does not change
 * @brick's data and vice versa.  Physical dimensions of the new brick are
 * calculated to correspond to the extracted part.
 *
 * Returns: A new two-dimensional data brick.
 **/
GwyBrick*
gwy_brick_new_part(const GwyBrick *brick,
                   const GwyBrickPart *bpart,
                   gboolean keep_offsets)
{
    guint col, row, level, width, height, depth;
    if (!_gwy_brick_check_part(brick, bpart,
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
    ASSIGN_UNITS(dpriv->unit_xy, spriv->unit_xy);
    ASSIGN_UNITS(dpriv->unit_z, spriv->unit_z);
    ASSIGN_UNITS(dpriv->unit_w, spriv->unit_w);
    if (keep_offsets) {
        part->xoff = brick->xoff + col*gwy_brick_dx(brick);
        part->yoff = brick->yoff + row*gwy_brick_dy(brick);
        part->zoff = brick->zoff + level*gwy_brick_dz(brick);
    }

    return part;
}

/**
 * gwy_brick_set_size:
 * @brick: A two-dimensional data brick.
 * @xres: Desired X resolution.
 * @yres: Desired Y resolution.
 * @zres: Desired Z resolution.
 * @clear: %TRUE to fill the new brick data with zeroes, %FALSE to leave it
 *         uninitialised.
 *
 * Resizes a two-dimensional data brick.
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
        notify[nn++] = brick_pspecs[PROP_XRES];
    if (brick->yres != yres)
        notify[nn++] = brick_pspecs[PROP_YRES];
    if (brick->zres != zres)
        notify[nn++] = brick_pspecs[PROP_ZRES];

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
 * @brick: A two-dimensional data brick.
 *
 * Emits signal GwyBrick::data-changed on a brick.
 **/
void
gwy_brick_data_changed(GwyBrick *brick)
{
    g_return_if_fail(GWY_IS_BRICK(brick));
    g_signal_emit(brick, brick_signals[DATA_CHANGED], 0);
}

/**
 * gwy_brick_copy:
 * @src: Source two-dimensional data data brick.
 * @srcpart: Area in brick @src to copy.  Pass %NULL to copy entire @src.
 * @dest: Destination two-dimensional data brick.
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
 * the part of the rectangle that is corrsponds to data inside @src and @dest
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
    if (!_gwy_brick_limit_parts(src, srcpart, dest, destcol, destrow, destlevel,
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
 * @src: Source two-dimensional data brick.
 * @dest: Destination two-dimensional data brick.
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
 * @brick: A two-dimensional data brick.
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
 * @brick: A two-dimensional data brick.
 * @xreal: Width in physical units.
 *
 * Sets the physical width of a two-dimensional data brick.
 **/
void
gwy_brick_set_xreal(GwyBrick *brick,
                    gdouble xreal)
{
    g_return_if_fail(GWY_IS_BRICK(brick));
    g_return_if_fail(xreal > 0.0);
    if (xreal != brick->xreal) {
        brick->xreal = xreal;
        g_object_notify_by_pspec(G_OBJECT(brick), brick_pspecs[PROP_XREAL]);
    }
}

/**
 * gwy_brick_set_yreal:
 * @brick: A two-dimensional data brick.
 * @yreal: Width in physical units.
 *
 * Sets the physical height of a two-dimensional data brick.
 **/
void
gwy_brick_set_yreal(GwyBrick *brick,
                    gdouble yreal)
{
    g_return_if_fail(GWY_IS_BRICK(brick));
    g_return_if_fail(yreal > 0.0);
    if (yreal != brick->yreal) {
        brick->yreal = yreal;
        g_object_notify_by_pspec(G_OBJECT(brick), brick_pspecs[PROP_YREAL]);
    }
}

/**
 * gwy_brick_set_zreal:
 * @brick: A two-dimensional data brick.
 * @zreal: Width in physical units.
 *
 * Sets the physical height of a two-dimensional data brick.
 **/
void
gwy_brick_set_zreal(GwyBrick *brick,
                    gdouble zreal)
{
    g_return_if_fail(GWY_IS_BRICK(brick));
    g_return_if_fail(zreal > 0.0);
    if (zreal != brick->zreal) {
        brick->zreal = zreal;
        g_object_notify_by_pspec(G_OBJECT(brick), brick_pspecs[PROP_ZREAL]);
    }
}

/**
 * gwy_brick_set_xoffset:
 * @brick: A two-dimensional data brick.
 * @xoffset: Horizontal offset of the top-left corner from [0,0,0] in physical
 *           units.
 *
 * Sets the horizontal offset of a two-dimensional data brick.
 **/
void
gwy_brick_set_xoffset(GwyBrick *brick,
                      gdouble xoffset)
{
    g_return_if_fail(GWY_IS_BRICK(brick));
    if (xoffset != brick->xoff) {
        brick->xoff = xoffset;
        g_object_notify_by_pspec(G_OBJECT(brick), brick_pspecs[PROP_XOFFSET]);
    }
}

/**
 * gwy_brick_set_yoffset:
 * @brick: A two-dimensional data brick.
 * @yoffset: Vertical offset of the top-left corner from [0,0,0] in physical
 *           units.
 *
 * Sets the vertical offset of a two-dimensional data brick.
 **/
void
gwy_brick_set_yoffset(GwyBrick *brick,
                      gdouble yoffset)
{
    g_return_if_fail(GWY_IS_BRICK(brick));
    if (yoffset != brick->yoff) {
        brick->yoff = yoffset;
        g_object_notify_by_pspec(G_OBJECT(brick), brick_pspecs[PROP_YOFFSET]);
    }
}

/**
 * gwy_brick_set_zoffset:
 * @brick: A two-dimensional data brick.
 * @zoffset: Depth offset of the top-left corner from [0,0,0] in physical
 *           units.
 *
 * Sets the vertical offset of a two-dimensional data brick.
 **/
void
gwy_brick_set_zoffset(GwyBrick *brick,
                      gdouble zoffset)
{
    g_return_if_fail(GWY_IS_BRICK(brick));
    if (zoffset != brick->zoff) {
        brick->zoff = zoffset;
        g_object_notify_by_pspec(G_OBJECT(brick), brick_pspecs[PROP_ZOFFSET]);
    }
}

/**
 * gwy_brick_get_unit_xy:
 * @brick: A two-dimensional data brick.
 *
 * Obtains the lateral units of a two-dimensional data brick.
 *
 * Returns: (transfer none):
 *          The lateral units of @brick.
 **/
GwyUnit*
gwy_brick_get_unit_xy(const GwyBrick *brick)
{
    g_return_val_if_fail(GWY_IS_BRICK(brick), NULL);
    Brick *priv = brick->priv;
    if (!priv->unit_xy)
        priv->unit_xy = gwy_unit_new();
    return priv->unit_xy;
}

/**
 * gwy_brick_get_unit_z:
 * @brick: A two-dimensional data brick.
 *
 * Obtains the depth units of a two-dimensional data brick.
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
 * @brick: A two-dimensional data brick.
 *
 * Obtains the value units of a two-dimensional data brick.
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

gboolean
_gwy_brick_check_part(const GwyBrick *brick,
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
        g_return_val_if_fail(bpart->width <= brick->xres - bpart->col,
                             FALSE);
        g_return_val_if_fail(bpart->row < brick->yres, FALSE);
        g_return_val_if_fail(bpart->height <= brick->yres - bpart->row,
                             FALSE);
        g_return_val_if_fail(bpart->level < brick->zres, FALSE);
        g_return_val_if_fail(bpart->depth <= brick->zres - bpart->level,
                             FALSE);
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

gboolean
_gwy_brick_check_plane_part(const GwyBrick *brick,
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
        g_return_val_if_fail(fpart->width <= brick->xres - fpart->col,
                             FALSE);
        g_return_val_if_fail(fpart->row < brick->yres, FALSE);
        g_return_val_if_fail(fpart->height <= brick->yres - fpart->row,
                             FALSE);
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

gboolean
_gwy_brick_limit_parts(const GwyBrick *src,
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
        if (OVERLAPPING(*col, *width, destcol, *width)
            && OVERLAPPING(*row, *height, destrow, *height)
            && OVERLAPPING(*level, *depth, destlevel, *depth)) {
            g_warning("Source and destination blocks overlap.  "
                      "Data corruption is imminent.");
        }
    }

    return *width && *height && *depth;
}

gboolean
_gwy_brick_check_target(const GwyBrick *brick,
                        const GwyBrick *target,
                        guint col,
                        guint row,
                        guint level,
                        guint width,
                        guint height,
                        guint depth,
                        guint *targetcol,
                        guint *targetrow,
                        guint *targetlevel)
{
    g_return_val_if_fail(GWY_IS_BRICK(brick), FALSE);
    g_return_val_if_fail(GWY_IS_BRICK(target), FALSE);

    if (target->xres == brick->xres
        && target->yres == brick->yres
        && target->zres == brick->zres) {
        *targetcol = col;
        *targetrow = row;
        *targetlevel = level;
        return TRUE;
    }
    if (target->xres == width
        && target->yres == height
        && target->zres == depth) {
        *targetcol = *targetrow = *targetlevel = 0;
        return TRUE;
    }

    g_critical("Target brick dimensions match neither source brick nor the "
               "part.");
    return FALSE;
}

/**
 * gwy_brick_format_xy:
 * @brick: A two-dimensional data brick.
 * @style: Output format style.
 *
 * Finds a suitable format for displaying lateral coordinates in a data brick.
 *
 * The created format has a sufficient precision to represent coordinates
 * of neighbour pixels as different values.
 *
 * Returns: A newly created value format.
 **/
GwyValueFormat*
gwy_brick_format_xy(const GwyBrick *brick,
                    GwyValueFormatStyle style)
{
    g_return_val_if_fail(GWY_IS_BRICK(brick), NULL);
    gdouble max0 = MAX(brick->xreal, brick->yreal);
    gdouble maxoff = MAX(fabs(brick->xreal + brick->xoff),
                         fabs(brick->yreal + brick->yoff));
    gdouble max = MAX(max0, maxoff);
    gdouble unit = MIN(gwy_brick_dx(brick), gwy_brick_dy(brick));
    return gwy_unit_format_with_resolution(gwy_brick_get_unit_xy(brick),
                                           style, max, unit);
}

/**
 * gwy_brick_format_z:
 * @brick: A two-dimensional data brick.
 * @style: Output format style.
 *
 * Finds a suitable format for displaying depth coordinates in a data brick.
 *
 * The created format has a sufficient precision to represent coordinates
 * of neighbour pixels as different values.
 *
 * Returns: A newly created value format.
 **/
GwyValueFormat*
gwy_brick_format_z(const GwyBrick *brick,
                   GwyValueFormatStyle style)
{
    g_return_val_if_fail(GWY_IS_BRICK(brick), NULL);
    gdouble max = MAX(brick->zreal, fabs(brick->zreal + brick->zoff));
    gdouble unit = gwy_brick_dz(brick);
    return gwy_unit_format_with_resolution(gwy_brick_get_unit_z(brick),
                                           style, max, unit);
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
 * The #GwyBrick struct contains some public bricks that can be directly
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
