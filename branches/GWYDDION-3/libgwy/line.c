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
#include "libgwy/line.h"
#include "libgwy/line-statistics.h"
#include "libgwy/libgwy-aliases.h"
#include "libgwy/processing-internal.h"

enum { N_ITEMS = 5 };

enum {
    DATA_CHANGED,
    N_SIGNALS
};

enum {
    PROP_0,
    PROP_RES,
    PROP_REAL,
    PROP_OFFSET,
    PROP_UNIT_X,
    PROP_UNIT_Y,
    N_PROPS
};

static void     gwy_line_finalize         (GObject *object);
static void     gwy_line_dispose          (GObject *object);
static void     gwy_line_serializable_init(GwySerializableInterface *iface);
static gsize    gwy_line_n_items          (GwySerializable *serializable);
static gsize    gwy_line_itemize          (GwySerializable *serializable,
                                            GwySerializableItems *items);
static gboolean gwy_line_construct        (GwySerializable *serializable,
                                            GwySerializableItems *items,
                                            GwyErrorList **error_list);
static GObject* gwy_line_duplicate_impl   (GwySerializable *serializable);
static void     gwy_line_assign_impl      (GwySerializable *destination,
                                            GwySerializable *source);
static void     gwy_line_set_property     (GObject *object,
                                            guint prop_id,
                                            const GValue *value,
                                            GParamSpec *pspec);
static void     gwy_line_get_property     (GObject *object,
                                            guint prop_id,
                                            GValue *value,
                                            GParamSpec *pspec);

static const GwySerializableItem serialize_items[N_ITEMS] = {
    /*0*/ { .name = "real",   .ctype = GWY_SERIALIZABLE_DOUBLE,       },
    /*1*/ { .name = "off",    .ctype = GWY_SERIALIZABLE_DOUBLE,       },
    /*2*/ { .name = "unit-x", .ctype = GWY_SERIALIZABLE_OBJECT,       },
    /*3*/ { .name = "unit-y", .ctype = GWY_SERIALIZABLE_OBJECT,       },
    /*4*/ { .name = "data",   .ctype = GWY_SERIALIZABLE_DOUBLE_ARRAY, },
};

static guint line_signals[N_SIGNALS];

G_DEFINE_TYPE_EXTENDED
    (GwyLine, gwy_line, G_TYPE_OBJECT, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_line_serializable_init))

static void
gwy_line_serializable_init(GwySerializableInterface *iface)
{
    iface->n_items   = gwy_line_n_items;
    iface->itemize   = gwy_line_itemize;
    iface->construct = gwy_line_construct;
    iface->duplicate = gwy_line_duplicate_impl;
    iface->assign    = gwy_line_assign_impl;
}

static void
gwy_line_class_init(GwyLineClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(Line));

    gobject_class->dispose = gwy_line_dispose;
    gobject_class->finalize = gwy_line_finalize;
    gobject_class->get_property = gwy_line_get_property;
    gobject_class->set_property = gwy_line_set_property;

    g_object_class_install_property
        (gobject_class,
         PROP_RES,
         g_param_spec_uint("res",
                           "Resolution",
                           "Pixel length of the line.",
                           1, G_MAXUINT, 1,
                           G_PARAM_READABLE | STATICP));

    g_object_class_install_property
        (gobject_class,
         PROP_REAL,
         g_param_spec_double("real",
                             "Real size",
                             "Length of the line in physical units.",
                             G_MINDOUBLE, G_MAXDOUBLE, 1.0,
                             G_PARAM_READWRITE | STATICP));

    g_object_class_install_property
        (gobject_class,
         PROP_OFFSET,
         g_param_spec_double("offset",
                             "Offset",
                             "Offset of the line start in physical units.",
                             -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
                             G_PARAM_READWRITE | STATICP));

    g_object_class_install_property
        (gobject_class,
         PROP_UNIT_X,
         g_param_spec_object("unit-x",
                             "X unit",
                             "Physical units of lateral dimension of the "
                             "line.",
                             GWY_TYPE_UNIT,
                             G_PARAM_READABLE | STATICP));

    g_object_class_install_property
        (gobject_class,
         PROP_UNIT_Y,
         g_param_spec_object("unit-y",
                             "Y unit",
                             "Physical units of line values.",
                             GWY_TYPE_UNIT,
                             G_PARAM_READABLE | STATICP));

    /**
     * GwyLine::data-changed:
     * @gwyline: The #GwyLine which received the signal.
     *
     * The ::data-changed signal is emitted whenever line data changes.
     **/
    line_signals[DATA_CHANGED]
        = g_signal_new_class_handler("data-changed",
                                     G_OBJECT_CLASS_TYPE(klass),
                                     G_SIGNAL_RUN_FIRST,
                                     NULL, NULL, NULL,
                                     g_cclosure_marshal_VOID__VOID,
                                     G_TYPE_NONE, 0);
}

static void
gwy_line_init(GwyLine *line)
{
    line->priv = G_TYPE_INSTANCE_GET_PRIVATE(line, GWY_TYPE_LINE, Line);
    line->res = 1;
    line->real = 1.0;
    line->data = g_new0(gdouble, 1);
}

static void
gwy_line_finalize(GObject *object)
{
    GwyLine *line = GWY_LINE(object);
    GWY_FREE(line->data);
    G_OBJECT_CLASS(gwy_line_parent_class)->finalize(object);
}

static void
gwy_line_dispose(GObject *object)
{
    GwyLine *line = GWY_LINE(object);
    GWY_OBJECT_UNREF(line->priv->unit_x);
    GWY_OBJECT_UNREF(line->priv->unit_y);
    G_OBJECT_CLASS(gwy_line_parent_class)->dispose(object);
}

static gsize
gwy_line_n_items(GwySerializable *serializable)
{
    GwyLine *line = GWY_LINE(serializable);
    Line *priv = line->priv;
    gsize n = N_ITEMS;
    if (priv->unit_x)
       n += gwy_serializable_n_items(GWY_SERIALIZABLE(priv->unit_x));
    if (priv->unit_y)
       n += gwy_serializable_n_items(GWY_SERIALIZABLE(priv->unit_y));
    return n;
}

static gsize
gwy_line_itemize(GwySerializable *serializable,
                 GwySerializableItems *items)
{
    GwyLine *line = GWY_LINE(serializable);
    Line *priv = line->priv;
    GwySerializableItem it;
    guint n = 0;

    g_return_val_if_fail(items->len - items->n >= N_ITEMS, 0);

    it = serialize_items[0];
    it.value.v_double = line->real;
    items->items[items->n++] = it;
    n++;

    if (line->off) {
        it = serialize_items[1];
        it.value.v_double = line->off;
        items->items[items->n++] = it;
        n++;
    }

    if (priv->unit_x) {
        g_return_val_if_fail(items->len - items->n, 0);
        it = serialize_items[2];
        it.value.v_object = (GObject*)priv->unit_x;
        items->items[items->n++] = it;
        gwy_serializable_itemize(GWY_SERIALIZABLE(priv->unit_x), items);
        n++;
    }

    if (priv->unit_y) {
        g_return_val_if_fail(items->len - items->n, 0);
        it = serialize_items[3];
        it.value.v_object = (GObject*)priv->unit_y;
        items->items[items->n++] = it;
        gwy_serializable_itemize(GWY_SERIALIZABLE(priv->unit_y), items);
        n++;
    }

    g_return_val_if_fail(items->len - items->n, 0);
    it = serialize_items[4];
    it.value.v_double_array = line->data;
    it.array_size = line->res;
    items->items[items->n++] = it;
    n++;

    return n;
}

static gboolean
gwy_line_construct(GwySerializable *serializable,
                   GwySerializableItems *items,
                   GwyErrorList **error_list)
{
    GwySerializableItem its[N_ITEMS];
    memcpy(its, serialize_items, sizeof(serialize_items));
    its[0].value.v_double = 1.0;
    gwy_deserialize_filter_items(its, N_ITEMS, items, "GwyLine", error_list);

    GwyLine *line = GWY_LINE(serializable);
    Line *priv = line->priv;

    if (G_UNLIKELY(!its[4].array_size)) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_INVALID,
                           _("Line contains no data."));
        return FALSE;
    }

    if (G_UNLIKELY(its[2].value.v_object
                   && !GWY_IS_UNIT(its[2].value.v_object))) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_INVALID,
                           _("Line x units are of type %s "
                             "instead of GwyUnit."),
                           G_OBJECT_TYPE_NAME(its[2].value.v_object));
        return FALSE;
    }

    if (G_UNLIKELY(its[3].value.v_object
                   && !GWY_IS_UNIT(its[3].value.v_object))) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_INVALID,
                           _("Line y units are of type %s "
                             "instead of GwyUnit."),
                           G_OBJECT_TYPE_NAME(its[3].value.v_object));
        return FALSE;
    }

    line->res = its[4].array_size;
    // FIXME: Catch near-zero and near-infinity values.
    line->real = CLAMP(its[0].value.v_double, G_MINDOUBLE, G_MAXDOUBLE);
    line->off = CLAMP(its[1].value.v_double, -G_MAXDOUBLE, G_MAXDOUBLE);
    priv->unit_x = (GwyUnit*)its[2].value.v_object;
    its[2].value.v_object = NULL;
    priv->unit_y = (GwyUnit*)its[3].value.v_object;
    its[3].value.v_object = NULL;
    g_free(line->data);
    line->data = its[4].value.v_double_array;
    its[4].value.v_double_array = NULL;
    its[4].array_size = 0;

    return TRUE;
}

static GObject*
gwy_line_duplicate_impl(GwySerializable *serializable)
{
    GwyLine *line = GWY_LINE(serializable);
    GwyLine *duplicate = gwy_line_new_alike(line, FALSE);
    ASSIGN(duplicate->data, line->data, line->res);
    return G_OBJECT(duplicate);
}

static void
copy_info(GwyLine *dest,
          const GwyLine *src)
{
    dest->res = src->res;
    dest->real = src->real;
    dest->off = src->off;
    Line *dpriv = dest->priv, *spriv = src->priv;
    ASSIGN_UNITS(dpriv->unit_x, spriv->unit_x);
    ASSIGN_UNITS(dpriv->unit_y, spriv->unit_y);
}

static void
gwy_line_assign_impl(GwySerializable *destination,
                     GwySerializable *source)
{
    GwyLine *dest = GWY_LINE(destination);
    GwyLine *src = GWY_LINE(source);

    const gchar *notify[N_PROPS];
    guint nn = 0;
    if (dest->res != src->res)
        notify[nn++] = "res";
    if (dest->real != src->real)
        notify[nn++] = "real";
    if (dest->off != src->off)
        notify[nn++] = "offset";

    if (dest->res != src->res) {
        g_free(dest->data);
        dest->data = g_new(gdouble, src->res);
    }
    ASSIGN(dest->data, src->data, src->res);
    copy_info(dest, src);

    GObject *object = G_OBJECT(dest);
    g_object_freeze_notify(object);
    for (guint i = 0; i < nn; i++)
        g_object_notify(object, notify[i]);
    g_object_thaw_notify(object);
}

static void
gwy_line_set_property(GObject *object,
                      guint prop_id,
                      const GValue *value,
                      GParamSpec *pspec)
{
    GwyLine *line = GWY_LINE(object);

    switch (prop_id) {
        case PROP_REAL:
        line->real = g_value_get_double(value);
        break;

        case PROP_OFFSET:
        line->off = g_value_get_double(value);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_line_get_property(GObject *object,
                      guint prop_id,
                      GValue *value,
                      GParamSpec *pspec)
{
    GwyLine *line = GWY_LINE(object);
    Line *priv = line->priv;

    switch (prop_id) {
        case PROP_RES:
        g_value_set_uint(value, line->res);
        break;

        case PROP_REAL:
        g_value_set_double(value, line->real);
        break;

        case PROP_OFFSET:
        g_value_set_double(value, line->off);
        break;

        case PROP_UNIT_X:
        // Instantiate the units to be consistent with the direct interface
        // that never admits the units are NULL.
        if (!priv->unit_x)
            priv->unit_x = gwy_unit_new();
        g_value_set_object(value, priv->unit_x);
        break;

        case PROP_UNIT_Y:
        // Instantiate the units to be consistent with the direct interface
        // that never admits the units are NULL.
        if (!priv->unit_y)
            priv->unit_y = gwy_unit_new();
        g_value_set_object(value, priv->unit_y);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

/**
 * gwy_line_new:
 *
 * Creates a new one-dimensional data line.
 *
 * The line dimensions will be 1×1 and it will be zero-filled.  This
 * paremterless constructor exists mainly for language bindings,
 * gwy_line_new_sized() and gwy_line_new_alike() are usually more useful.
 *
 * Returns: A new one-dimensional data line.
 **/
GwyLine*
gwy_line_new(void)
{
    return g_object_newv(GWY_TYPE_LINE, 0, NULL);
}

/**
 * gwy_line_new_sized:
 * @res: Line resolution (width).
 * @clear: %TRUE to fill the new line data with zeroes, %FALSE to leave it
 *         unitialized.
 *
 * Creates a new one-dimensional data line of specified dimension.
 *
 * Returns: A new one-dimensional data line.
 **/
GwyLine*
gwy_line_new_sized(guint res,
                   gboolean clear)
{
    g_return_val_if_fail(res, NULL);

    GwyLine *line = g_object_newv(GWY_TYPE_LINE, 0, NULL);
    g_free(line->data);
    line->res = res;
    if (clear)
        line->data = g_new0(gdouble, line->res);
    else
        line->data = g_new(gdouble, line->res);
    return line;
}

/**
 * gwy_line_new_alike:
 * @model: A one-dimensional data line to use as the template.
 * @clear: %TRUE to fill the new line data with zeroes, %FALSE to leave it
 *         unitialized.
 *
 * Creates a new one-dimensional data line similar to another line.
 *
 * All properties of the newly created line will be identical to @model,
 * except the data that will be either zeroes or unitialized.  Use
 * gwy_line_duplicate() to completely duplicate a line including data.
 *
 * Returns: A new one-dimensional data line.
 **/
GwyLine*
gwy_line_new_alike(const GwyLine *model,
                   gboolean clear)
{
    g_return_val_if_fail(GWY_IS_LINE(model), NULL);
    GwyLine *line = gwy_line_new_sized(model->res, clear);
    copy_info(line, model);
    return line;
}

/**
 * gwy_line_new_part:
 * @line: A one-dimensional data line.
 * @pos: Position of the line part start.
 * @len: Part length (number of items).
 * @keep_offset: %TRUE to set the offset of the new line
 *                using @pos and @line offsets.  %FALSE to set the offset
 *                of the new line to zero.
 *
 * Creates a new one-dimensional line as a part of another line.
 *
 * The block of length @len starting at @pos must be entirely contained in
 * @line.  The dimension must be non-zero.
 *
 * Data are physically copied, i.e. changing the new line data does not change
 * @line's data and vice versa.  Physical dimensions of the new line are
 * calculated to correspond to the extracted part.
 *
 * Returns: A new one-dimensional data line.
 **/
GwyLine*
gwy_line_new_part(const GwyLine *line,
                  guint pos,
                  guint len,
                  gboolean keep_offset)
{
    g_return_val_if_fail(GWY_IS_LINE(line), NULL);
    g_return_val_if_fail(len, NULL);
    g_return_val_if_fail(pos + len <= line->res, NULL);

    GwyLine *part;

    if (len == line->res) {
        part = gwy_line_duplicate(line);
        if (!keep_offset)
            part->off = 0.0;
        return part;
    }

    part = gwy_line_new_sized(len, FALSE);
    gwy_line_part_copy(line, pos, len, part, 0);
    part->real = line->real*len/line->res;
    ASSIGN_UNITS(part->priv->unit_x, line->priv->unit_x);
    ASSIGN_UNITS(part->priv->unit_y, line->priv->unit_y);
    if (keep_offset)
        part->off = line->off + line->real*pos/line->res;
    return part;
}

/**
 * gwy_line_new_resampled:
 * @line: A one-dimensional data line.
 * @res: Desired resolution.
 * @interpolation: Interpolation method to use.
 *
 * Creates a new one-dimensional data line by resampling another line.
 *
 * Returns: A new one-dimensional data line.
 **/
GwyLine*
gwy_line_new_resampled(const GwyLine *line,
                       guint res,
                       GwyInterpolationType interpolation)
{
    g_return_val_if_fail(GWY_IS_LINE(line), NULL);
    g_return_val_if_fail(res, NULL);
    if (res == line->res)
        return gwy_line_duplicate(line);

    GwyLine *dest;
    dest = gwy_line_new_sized(res, FALSE);
    copy_info(dest, line);
    dest->res = res;  // Undo
    gwy_interpolation_resample_block_1d(line->res, line->data,
                                        dest->res, dest->data,
                                        interpolation, TRUE);

    return dest;
}

/**
 * gwy_line_data_changed:
 * @line: A one-dimensional data line.
 *
 * Emits signal GwyLine::data-changed on a line.
 **/
void
gwy_line_data_changed(GwyLine *line)
{
    g_return_if_fail(GWY_IS_LINE(line));
    g_signal_emit(line, line_signals[DATA_CHANGED], 0);
}

/**
 * gwy_line_copy:
 * @src: Source one-dimensional data line.
 * @dest: Destination one-dimensional data line.
 *
 * Copies the data of a line to another line of the same dimensions.
 *
 * Only the data are copied.  To make a line completely identical to another,
 * including units, offsets and change of dimensions, you can use
 * gwy_line_assign().
 **/
void
gwy_line_copy(const GwyLine *src,
              GwyLine *dest)
{
    g_return_if_fail(GWY_IS_LINE(src));
    g_return_if_fail(GWY_IS_LINE(dest));
    g_return_if_fail(dest->res == src->res);
    ASSIGN(dest->data, src->data, src->res);
}

/**
 * gwy_line_part_copy:
 * @src: Source one-dimensional data data line.
 * @pos: Position of the line part start.
 * @len: Part length (number of items).
 * @dest: Destination one-dimensional data line.
 * @destpos: Destination position in @dest.
 *
 * Copies a part from one line to another.
 *
 * The copied block starts at @pos in @src and its lenght is @len.  It is
 * copied to @dest starting from @destpos.
 *
 * There are no limitations on the indices or dimensions.  Only the part of the
 * block that is corrsponds to data inside @src and @dest is copied.  This can
 * also mean nothing is copied at all.
 *
 * If @src is equal to @dest, the areas may not overlap.
 **/
void
gwy_line_part_copy(const GwyLine *src,
                   guint pos,
                   guint len,
                   GwyLine *dest,
                   guint destpos)
{
    g_return_if_fail(GWY_IS_LINE(src));
    g_return_if_fail(GWY_IS_LINE(dest));

    if (pos >= src->res || destpos >= dest->res)
        return;

    len = MIN(len, src->res - pos);
    len = MIN(len, dest->res - destpos);
    if (!len)
        return;

    ASSIGN(dest->data + destpos, src->data + pos, len);
}

/**
 * gwy_line_set_real:
 * @line: A one-dimensional data line.
 * @real: Length in physical units.
 *
 * Sets the physical lenght of a one-dimensional data line.
 **/
void
gwy_line_set_real(GwyLine *line,
                  gdouble real)
{
    g_return_if_fail(GWY_IS_LINE(line));
    g_return_if_fail(real > 0.0);
    if (real != line->real) {
        line->real = real;
        g_object_notify(G_OBJECT(line), "real");
    }
}

/**
 * gwy_line_set_offset:
 * @line: A one-dimensional data line.
 * @off: Offset of the line start from 0 in physical units.
 *
 * Sets the offset of a one-dimensional data line.
 **/
void
gwy_line_set_offset(GwyLine *line,
                    gdouble offset)
{
    g_return_if_fail(GWY_IS_LINE(line));
    if (offset != line->off) {
        line->off = offset;
        g_object_notify(G_OBJECT(line), "offset");
    }
}

/**
 * gwy_line_get_unit_x:
 * @line: A one-dimensional data line.
 *
 * Obtains the lateral units of a one-dimensional data line.
 *
 * Returns: The lateral units of @line.
 **/
GwyUnit*
gwy_line_get_unit_x(GwyLine *line)
{
    g_return_val_if_fail(GWY_IS_LINE(line), NULL);
    Line *priv = line->priv;
    if (!priv->unit_x)
        priv->unit_x = gwy_unit_new();
    return priv->unit_x;
}

/**
 * gwy_line_get_unit_y:
 * @line: A one-dimensional data line.
 *
 * Obtains the value units of a one-dimensional data line.
 *
 * Returns: The value units of @line.
 **/
GwyUnit*
gwy_line_get_unit_y(GwyLine *line)
{
    g_return_val_if_fail(GWY_IS_LINE(line), NULL);
    Line *priv = line->priv;
    if (!priv->unit_y)
        priv->unit_y = gwy_unit_new();
    return priv->unit_y;
}

/**
 * gwy_line_clear:
 * @line: A one-dimensional data line.
 *
 * Fills a line with zeroes.
 **/
void
gwy_line_clear(GwyLine *line)
{
    g_return_if_fail(GWY_IS_LINE(line));
    gwy_memclear(line->data, line->res);
}

/**
 * gwy_line_fill:
 * @line: A one-dimensional data line.
 * @value: Value to fill @line with.
 *
 * Fills a line with the specified value.
 **/
void
gwy_line_fill(GwyLine *line,
              gdouble value)
{
    if (!value) {
        gwy_line_clear(line);
        return;
    }
    g_return_if_fail(GWY_IS_LINE(line));
    gwy_line_part_fill(line, 0, line->res, value);
}

/**
 * gwy_line_part_clear:
 * @line: A one-dimensional data line.
 * @pos: Position of the line part start.
 * @len: Part length (number of items).
 *
 * Fills a part of a line with zeroes.
 **/
void
gwy_line_part_clear(GwyLine *line,
                    guint pos,
                    guint len)
{
    g_return_if_fail(GWY_IS_LINE(line));
    g_return_if_fail(pos + len <= line->res);

    if (!len)
        return;

    gwy_memclear(line->data + pos, len);
}

/**
 * gwy_line_part_fill:
 * @line: A one-dimensional data line.
 * @pos: Position of the line part start.
 * @len: Part length (number of items).
 * @value: Value to fill the rectangle with.
 *
 * Fills a part of a line with the specified value.
 **/
void
gwy_line_part_fill(GwyLine *line,
                   guint pos,
                   guint len,
                   gdouble value)
{
    if (!value) {
        gwy_line_part_clear(line, pos, len);
        return;
    }

    g_return_if_fail(GWY_IS_LINE(line));
    g_return_if_fail(pos + len <= line->res);

    if (!len)
        return;

    gdouble *p = line->data + pos;
    for (guint j = len; j; j--)
        *(p++) = value;
}

/**
 * gwy_line_get_format_x:
 * @line: A one-dimensional data line.
 * @style: Output format style.
 * @format: Value format to update or %NULL to create a new format.
 *
 * Finds a suitable format for displaying coordinates in a data line.
 *
 * The returned format will have sufficient precision to represent coordinates
 * of neighbour pixels as different values.
 *
 * Returns: Either @format (with reference count unchanged) or, if it was
 *          %NULL, a newly created #GwyValueFormat.
 **/
GwyValueFormat*
gwy_line_get_format_x(GwyLine *line,
                      GwyValueFormatStyle style,
                      GwyValueFormat *format)
{
    g_return_val_if_fail(GWY_IS_LINE(line), NULL);
    gdouble max = MAX(line->real, fabs(line->real + line->off));
    gdouble unit = gwy_line_dx(line);
    return gwy_unit_format_with_resolution(gwy_line_get_unit_x(line),
                                           style, max, unit, format);
}

/**
 * gwy_line_get_format_y:
 * @line: A one-dimensional data line.
 * @style: Output format style.
 * @format: Value format to update or %NULL to create a new format.
 *
 * Finds a suitable format for displaying values in a data line.
 *
 * Returns: Either @format (with reference count unchanged) or, if it was
 *          %NULL, a newly created #GwyValueFormat.
 **/
GwyValueFormat*
gwy_line_get_format_y(GwyLine *line,
                      GwyValueFormatStyle style,
                      GwyValueFormat *format)
{
    g_return_val_if_fail(GWY_IS_LINE(line), NULL);
    gdouble min, max;
    gwy_line_min_max(line, &min, &max);
    if (max == min) {
        max = ABS(max);
        min = 0.0;
    }
    return gwy_unit_format_with_digits(gwy_line_get_unit_y(line),
                                       style, max - min, 3, format);
}

#define __LIBGWY_LINE_C__
#include "libgwy/libgwy-aliases.c"

/**
 * SECTION: line
 * @title: GwyLine
 * @short_description: One-dimensional data in regular grid
 *
 * #GwyLine represents one dimensional data in a regular grid.
 *
 * Data are stored in a flat array #GwyLine-struct.data of #gdouble values.
 **/

/**
 * GwyLine:
 * @res: X-resolution, i.e. width in pixels.
 * @real: Width in physical units.
 * @off: X-offset of the line start from 0 in physical units.
 * @data: Line data.  See the introductory section for details.
 *
 * Object representing one-dimensional data in a regular grid.
 *
 * The #GwyLine struct contains some public lines that can be directly
 * accessed for reading.  To set them, you must use the methods such as
 * gwy_line_set_real().
 **/

/**
 * GwyLineClass:
 * @g_object_class: Parent class.
 *
 * Class of one-dimensional data lines.
 **/

/**
 * gwy_line_duplicate:
 * @line: A one-dimensional data line.
 *
 * Duplicates a one-dimensional data line.
 *
 * This is a convenience wrapper of gwy_serializable_duplicate().
 **/

/**
 * gwy_line_assign:
 * @dest: Destination one-dimensional data line.
 * @src: Source one-dimensional data line.
 *
 * Copies the value of a one-dimensional data line.
 *
 * This is a convenience wrapper of gwy_serializable_assign().
 **/

/**
 * gwy_line_index:
 * @line: A one-dimensional data line.
 * @pos: Position in @line.
 *
 * Accesses a one-dimensional data line pixel.
 *
 * This macro may evaluate its arguments several times.
 * This macro expands to a left-hand side expression.
 *
 * No argument validation is performed.  If you process the data in a loop,
 * you are encouraged to access #GwyLine-struct.data directly.
 **/

/**
 * gwy_line_dx:
 * @line: A one-dimensional data line.
 *
 * Calculates the pixel size in physical units.
 *
 * This macro may evaluate its arguments several times.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
