/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
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

#include "config.h"
#include <string.h>

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwydebugobjects.h>
#include <libprocess/linestats.h>
#include <libprocess/interpolation.h>

#define GWY_DATA_LINE_TYPE_NAME "GwyDataLine"

/* INTERPOLATION: FIXME, gwy_data_line_rotate() does `something'. */

enum {
    DATA_CHANGED,
    LAST_SIGNAL
};

static void        gwy_data_line_finalize         (GObject *object);
static void        gwy_data_line_serializable_init(GwySerializableIface *iface);
static GByteArray* gwy_data_line_serialize        (GObject *obj,
                                                   GByteArray *buffer);
static gsize       gwy_data_line_get_size         (GObject *obj);
static GObject*    gwy_data_line_deserialize      (const guchar *buffer,
                                                   gsize size,
                                                   gsize *position);
static GObject*    gwy_data_line_duplicate_real   (GObject *object);
static void        gwy_data_line_clone_real       (GObject *source,
                                                   GObject *copy);

static guint data_line_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_EXTENDED
    (GwyDataLine, gwy_data_line, G_TYPE_OBJECT, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_data_line_serializable_init))

static void
gwy_data_line_serializable_init(GwySerializableIface *iface)
{
    iface->serialize = gwy_data_line_serialize;
    iface->deserialize = gwy_data_line_deserialize;
    iface->get_size = gwy_data_line_get_size;
    iface->duplicate = gwy_data_line_duplicate_real;
    iface->clone = gwy_data_line_clone_real;
}

static void
gwy_data_line_class_init(GwyDataLineClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_data_line_finalize;

/**
 * GwyDataLine::data-changed:
 * @gwydataline: The #GwyDataLine which received the signal.
 *
 * The ::data-changed signal is never emitted by data line itself.  It
 * is intended as a means to notify others data line users they should
 * update themselves.
 */
    data_line_signals[DATA_CHANGED]
        = g_signal_new("data-changed",
                       G_OBJECT_CLASS_TYPE(gobject_class),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET(GwyDataLineClass, data_changed),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);
}

static void
gwy_data_line_init(GwyDataLine *data_line)
{
    gwy_debug_objects_creation(G_OBJECT(data_line));
}

static void
gwy_data_line_finalize(GObject *object)
{
    GwyDataLine *data_line = (GwyDataLine*)object;

    gwy_object_unref(data_line->si_unit_x);
    gwy_object_unref(data_line->si_unit_y);
    g_free(data_line->data);

    G_OBJECT_CLASS(gwy_data_line_parent_class)->finalize(object);
}

/**
 * gwy_data_line_new:
 * @res: Resolution, i.e., the number of samples.
 * @real: Real physical dimension.
 * @nullme: Whether the data line should be initialized to zeroes. If %FALSE,
 *          the data will not be initialized.
 *
 * Creates a new data line.
 *
 * Returns: A newly created data line.
 **/
GwyDataLine*
gwy_data_line_new(gint res, gdouble real, gboolean nullme)
{
    GwyDataLine *data_line;

    gwy_debug("");
    data_line = g_object_new(GWY_TYPE_DATA_LINE, NULL);

    data_line->res = res;
    data_line->real = real;
    if (nullme)
        data_line->data = g_new0(gdouble, data_line->res);
    else
        data_line->data = g_new(gdouble, data_line->res);

    return data_line;
}

/**
 * gwy_data_line_new_alike:
 * @model: A data line to take resolutions and units from.
 * @nullme: Whether the data line should be initialized to zeroes. If %FALSE,
 *          the data will not be initialized.
 *
 * Creates a new data line similar to an existing one.
 *
 * Use gwy_data_line_duplicate() if you want to copy a data line including
 * data.
 *
 * Returns: A newly created data line.
 **/
GwyDataLine*
gwy_data_line_new_alike(GwyDataLine *model,
                        gboolean nullme)
{
    GwyDataLine *data_line;

    g_return_val_if_fail(GWY_IS_DATA_LINE(model), NULL);
    data_line = g_object_new(GWY_TYPE_DATA_LINE, NULL);

    data_line->res = model->res;
    data_line->real = model->real;
    if (nullme)
        data_line->data = g_new0(gdouble, data_line->res);
    else
        data_line->data = g_new(gdouble, data_line->res);

    if (model->si_unit_x)
        data_line->si_unit_x = gwy_si_unit_duplicate(model->si_unit_x);
    if (model->si_unit_y)
        data_line->si_unit_y = gwy_si_unit_duplicate(model->si_unit_y);

    return data_line;
}

/**
 * gwy_data_line_new_resampled:
 * @data_line: A data line.
 * @res: Desired resolution.
 * @interpolation: Interpolation method to use.
 *
 * Creates a new data line by resampling an existing one.
 *
 * This method is equivalent to gwy_data_line_duplicate() followed by
 * gwy_data_line_resample(), but it is more efficient.
 *
 * Returns: A newly created data line.
 *
 * Since: 2.1
 **/
GwyDataLine*
gwy_data_line_new_resampled(GwyDataLine *data_line,
                            gint res,
                            GwyInterpolationType interpolation)
{
    GwyDataLine *result;

    g_return_val_if_fail(GWY_IS_DATA_LINE(data_line), NULL);
    if (data_line->res == res)
        return gwy_data_line_duplicate(data_line);

    g_return_val_if_fail(res > 0, NULL);

    result = gwy_data_line_new(res, data_line->real, FALSE);
    result->off = data_line->off;
    if (data_line->si_unit_x)
        result->si_unit_x = gwy_si_unit_duplicate(data_line->si_unit_x);
    if (data_line->si_unit_y)
        result->si_unit_y = gwy_si_unit_duplicate(data_line->si_unit_y);

    gwy_interpolation_resample_block_1d(data_line->res, data_line->data,
                                        result->res, result->data,
                                        interpolation, TRUE);

    return result;
}

static GByteArray*
gwy_data_line_serialize(GObject *obj,
                        GByteArray *buffer)
{
    GwyDataLine *data_line;
    gdouble *poff;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_DATA_LINE(obj), NULL);

    data_line = GWY_DATA_LINE(obj);
    if (!data_line->si_unit_x)
        data_line->si_unit_x = gwy_si_unit_new("");
    if (!data_line->si_unit_y)
        data_line->si_unit_y = gwy_si_unit_new("");
    poff = data_line->off ? &data_line->off : NULL;
    {
        GwySerializeSpec spec[] = {
            { 'i', "res", &data_line->res, NULL, },
            { 'd', "real", &data_line->real, NULL, },
            { 'd', "off", poff, NULL, },
            { 'o', "si_unit_x", &data_line->si_unit_x, NULL, },
            { 'o', "si_unit_y", &data_line->si_unit_y, NULL, },
            { 'D', "data", &data_line->data, &data_line->res, },
        };

        return gwy_serialize_pack_object_struct(buffer,
                                                GWY_DATA_LINE_TYPE_NAME,
                                                G_N_ELEMENTS(spec), spec);
    }
}

static gsize
gwy_data_line_get_size(GObject *obj)
{
    GwyDataLine *data_line;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_DATA_LINE(obj), 0);

    data_line = GWY_DATA_LINE(obj);
    if (!data_line->si_unit_x)
        data_line->si_unit_x = gwy_si_unit_new("");
    if (!data_line->si_unit_y)
        data_line->si_unit_y = gwy_si_unit_new("");
    {
        GwySerializeSpec spec[] = {
            { 'i', "res", &data_line->res, NULL, },
            { 'd', "real", &data_line->real, NULL, },
            { 'd', "off", &data_line->off, NULL, },
            { 'o', "si_unit_x", &data_line->si_unit_x, NULL, },
            { 'o', "si_unit_y", &data_line->si_unit_y, NULL, },
            { 'D', "data", &data_line->data, &data_line->res, },
        };

        return gwy_serialize_get_struct_size(GWY_DATA_LINE_TYPE_NAME,
                                             G_N_ELEMENTS(spec), spec);
    }
}

static GObject*
gwy_data_line_deserialize(const guchar *buffer,
                          gsize size,
                          gsize *position)
{
    guint32 fsize;
    gint res;
    gdouble real, off = 0.0, *data = NULL;
    GwySIUnit *si_unit_x = NULL, *si_unit_y = NULL;
    GwyDataLine *data_line;
    GwySerializeSpec spec[] = {
      { 'i', "res", &res, NULL, },
      { 'd', "real", &real, NULL, },
      { 'd', "off", &off, NULL, },
      { 'o', "si_unit_x", &si_unit_x, NULL, },
      { 'o', "si_unit_y", &si_unit_y, NULL, },
      { 'D', "data", &data, &fsize, },
    };

    gwy_debug("");
    g_return_val_if_fail(buffer, NULL);

    if (!gwy_serialize_unpack_object_struct(buffer, size, position,
                                            GWY_DATA_LINE_TYPE_NAME,
                                            G_N_ELEMENTS(spec), spec)) {
        g_free(data);
        gwy_object_unref(si_unit_x);
        gwy_object_unref(si_unit_y);
        return NULL;
    }
    if (fsize != (guint)res) {
        g_critical("Serialized %s size mismatch %u != %u",
              GWY_DATA_LINE_TYPE_NAME, fsize, res);
        g_free(data);
        gwy_object_unref(si_unit_x);
        gwy_object_unref(si_unit_y);
        return NULL;
    }

    /* don't allocate large amount of memory just to immediately free it */
    data_line = gwy_data_line_new(1, real, FALSE);
    g_free(data_line->data);
    data_line->res = res;
    data_line->off = off;
    data_line->data = data;
    if (si_unit_y) {
        gwy_object_unref(data_line->si_unit_y);
        data_line->si_unit_y = si_unit_y;
    }
    if (si_unit_x) {
        gwy_object_unref(data_line->si_unit_x);
        data_line->si_unit_x = si_unit_x;
    }

    return (GObject*)data_line;
}

static GObject*
gwy_data_line_duplicate_real(GObject *object)
{
    GwyDataLine *data_line, *duplicate;

    g_return_val_if_fail(GWY_IS_DATA_LINE(object), NULL);
    data_line = GWY_DATA_LINE(object);
    duplicate = gwy_data_line_new_alike(data_line, FALSE);
    memcpy(duplicate->data, data_line->data, data_line->res*sizeof(gdouble));

    return (GObject*)duplicate;
}

static void
gwy_data_line_clone_real(GObject *source, GObject *copy)
{
    GwyDataLine *data_line, *clone;

    g_return_if_fail(GWY_IS_DATA_LINE(source));
    g_return_if_fail(GWY_IS_DATA_LINE(copy));

    data_line = GWY_DATA_LINE(source);
    clone = GWY_DATA_LINE(copy);

    if (clone->res != data_line->res) {
        clone->res = data_line->res;
        clone->data = g_renew(gdouble, clone->data, clone->res);
    }
    clone->real = data_line->real;
    memcpy(clone->data, data_line->data, data_line->res*sizeof(gdouble));

    /* SI Units can be NULL */
    if (data_line->si_unit_x && clone->si_unit_x)
        gwy_serializable_clone(G_OBJECT(data_line->si_unit_x),
                               G_OBJECT(clone->si_unit_x));
    else if (data_line->si_unit_x && !clone->si_unit_x)
        clone->si_unit_x = gwy_si_unit_duplicate(data_line->si_unit_x);
    else if (!data_line->si_unit_x && clone->si_unit_x)
        gwy_object_unref(clone->si_unit_x);

    if (data_line->si_unit_y && clone->si_unit_y)
        gwy_serializable_clone(G_OBJECT(data_line->si_unit_y),
                               G_OBJECT(clone->si_unit_y));
    else if (data_line->si_unit_y && !clone->si_unit_y)
        clone->si_unit_y = gwy_si_unit_duplicate(data_line->si_unit_y);
    else if (!data_line->si_unit_y && clone->si_unit_y)
        gwy_object_unref(clone->si_unit_y);
}

/**
 * gwy_data_line_data_changed:
 * @data_line: A data line.
 *
 * Emits signal "data_changed" on a data line.
 **/
void
gwy_data_line_data_changed(GwyDataLine *data_line)
{
    g_signal_emit(data_line, data_line_signals[DATA_CHANGED], 0);
}

/**
 * gwy_data_line_resample:
 * @data_line: A data line.
 * @res: Desired resolution.
 * @interpolation: Interpolation method to use.
 *
 * Resamples a data line.
 *
 * In other words changes the size of one dimensional field related with data
 * line. The original values are used for resampling using a requested
 * interpolation alorithm.
 **/
void
gwy_data_line_resample(GwyDataLine *data_line,
                       gint res,
                       GwyInterpolationType interpolation)
{
    gdouble *bdata;

    g_return_if_fail(GWY_IS_DATA_LINE(data_line));
    if (res == data_line->res)
        return;
    g_return_if_fail(res > 1);

    if (interpolation == GWY_INTERPOLATION_NONE) {
        data_line->res = res;
        data_line->data = g_renew(gdouble, data_line->data, data_line->res);
        return;
    }

    bdata = g_new(gdouble, res);
    gwy_interpolation_resample_block_1d(data_line->res, data_line->data,
                                        res, bdata,
                                        interpolation, FALSE);
    g_free(data_line->data);
    data_line->data = bdata;
    data_line->res = res;
}

/**
 * gwy_data_line_resize:
 * @data_line: A data line.
 * @from: Where to start.
 * @to: Where to finish + 1.
 *
 * Resizes (crops) a data line.
 *
 * Extracts a part of data line in range @from..(@to-1), recomputing real
 * sizes.
 **/
void
gwy_data_line_resize(GwyDataLine *a, gint from, gint to)
{
    g_return_if_fail(GWY_IS_DATA_LINE(a));
    if (to < from)
        GWY_SWAP(gint, from, to);
    g_return_if_fail(from >= 0 && to <= a->res);
    a->real *= (to - from)/((double)a->res);
    a->res = to - from;
    memmove(a->data, a->data + from, a->res*sizeof(gdouble));
    a->data = g_renew(gdouble, a->data, a->res*sizeof(gdouble));
}

/**
 * gwy_data_line_part_extract:
 * @data_line: A data line.
 * @from: Where to start.
 * @len: Length of extracted segment.
 *
 * Extracts a part of a data line to a new data line.
 *
 * Returns: The extracted area as a newly created data line.
 **/
GwyDataLine*
gwy_data_line_part_extract(GwyDataLine *data_line,
                           gint from,
                           gint len)
{
    GwyDataLine *result;

    g_return_val_if_fail(GWY_IS_DATA_LINE(data_line), NULL);
    g_return_val_if_fail(from >= 0
                         && len > 0
                         && from + len <= data_line->res, NULL);

    if (from == 0 && len == data_line->res)
        return gwy_data_line_duplicate(data_line);

    result = gwy_data_line_new(len, data_line->real*len/data_line->res, FALSE);
    memcpy(result->data, data_line->data + from, len*sizeof(gdouble));

    if (data_line->si_unit_x)
        result->si_unit_x = gwy_si_unit_duplicate(data_line->si_unit_x);
    if (data_line->si_unit_y)
        result->si_unit_y = gwy_si_unit_duplicate(data_line->si_unit_y);

    return result;
}

/**
 * gwy_data_line_copy:
 * @data_line: Source data line.
 * @b: Destination data line.
 *
 * Copies the contents of a data line to another already allocated data line
 * of the same size.
 *
 * <warning>Semantic of method differs from gwy_data_field_copy(), it copies
 * only data.  It will be probably changed.</warning>
 **/
void
gwy_data_line_copy(GwyDataLine *a, GwyDataLine *b)
{
    g_return_if_fail(a->res == b->res);

    memcpy(b->data, a->data, a->res*sizeof(gdouble));
}

/**
 * gwy_data_line_get_dval:
 * @data_line: A data line.
 * @x: Position in data line in range [0, resolution].  If the value is outside
 *     this range, the nearest border value is returned.
 * @interpolation: Interpolation method to use.
 *
 * Gets interpolated value at arbitrary data line point indexed by pixel
 * coordinates.
 *
 * Note pixel values are centered in intervals [@j, @j+1], so to get the same
 * value as gwy_data_line_get_val(@data_line, @j) returns,
 * it's necessary to add 0.5:
 * gwy_data_line_get_dval(@data_line, @j+0.5, @interpolation).
 *
 * See also gwy_data_line_get_dval_real() that does the same, but takes
 * real coordinates.
 *
 * Returns: Value interpolated in the data line.
 **/
gdouble
gwy_data_line_get_dval(GwyDataLine *a, gdouble x, gint interpolation)
{
    gint l;
    gdouble rest;
    gdouble intline[4];

    g_return_val_if_fail(GWY_IS_DATA_LINE(a), 0.0);

    if (G_UNLIKELY(interpolation == GWY_INTERPOLATION_NONE))
        return 0.0;

    x -= 0.5;    /* To centered pixel value */
    l = floor(x);
    if (G_UNLIKELY(l < 0))
        return a->data[0];
    if (G_UNLIKELY(l >= a->res - 1))
        return a->data[a->res - 1];

    rest = x - l;
    /*simple (and fast) methods*/
    switch (interpolation) {
        case GWY_INTERPOLATION_ROUND:
        return a->data[l];

        case GWY_INTERPOLATION_LINEAR:
        return (1.0 - rest)*a->data[l] + rest*a->data[l+1];

        default:
        /* use linear in border intervals */
        if (l < 1 || l >= (a->res - 2))
            return (1.0 - rest)*a->data[l] + rest*a->data[l+1];

        /* other 4point methods are very similar: */
        intline[0] = a->data[l-1];
        intline[1] = a->data[l];
        intline[2] = a->data[l+1];
        intline[3] = a->data[l+2];
        return gwy_interpolation_get_dval_of_equidists(rest, intline,
                                                       interpolation);
        break;
    }
}

/**
 * gwy_data_line_get_data:
 * @data_line: A data line.
 *
 * Gets the raw data buffer of a data line.
 *
 * The returned buffer is not guaranteed to be valid through whole data
 * line life time.  Some function may change it, most notably
 * gwy_data_line_resize() and gwy_data_line_resample().
 *
 * This function invalidates any cached information, use
 * gwy_data_line_get_data_const() if you are not going to change the data.
 *
 * Returns: The data as an array of doubles of length gwy_data_line_get_res().
 **/
gdouble*
gwy_data_line_get_data(GwyDataLine *data_line)
{
    g_return_val_if_fail(GWY_IS_DATA_LINE(data_line), NULL);
    return data_line->data;
}

/**
 * gwy_data_line_get_data_const:
 * @data_line: A data line.
 *
 * Gets the raw data buffer of a data line, read-only.
 *
 * The returned buffer is not guaranteed to be valid through whole data
 * line life time.  Some function may change it, most notably
 * gwy_data_line_resize() and gwy_data_line_resample().
 *
 * Use gwy_data_line_get_data() if you want to change the data.
 *
 * Returns: The data as an array of doubles of length gwy_data_line_get_res().
 **/
const gdouble*
gwy_data_line_get_data_const(GwyDataLine *data_line)
{
    g_return_val_if_fail(GWY_IS_DATA_LINE(data_line), NULL);
    return (const gdouble*)data_line->data;
}

/**
 * gwy_data_line_get_res:
 * @data_line: A data line.
 *
 * Gets the number of data points in a data line.
 *
 * Returns: Resolution (number of data points).
 **/
gint
gwy_data_line_get_res(GwyDataLine *data_line)
{
    g_return_val_if_fail(GWY_IS_DATA_LINE(data_line), 0);
    return data_line->res;
}

/**
 * gwy_data_line_get_real:
 * @data_line: A data line.
 *
 * Gets the physical size of a data line.
 *
 * Returns: Real size of data line.
 **/
gdouble
gwy_data_line_get_real(GwyDataLine *data_line)
{
    g_return_val_if_fail(GWY_IS_DATA_LINE(data_line), 0.0);
    return data_line->real;
}

/**
 * gwy_data_line_set_real:
 * @data_line: A data line.
 * @real: value to be set
 *
 * Sets the real data line size.
 **/
void
gwy_data_line_set_real(GwyDataLine *data_line, gdouble real)
{
    g_return_if_fail(GWY_IS_DATA_LINE(data_line));
    data_line->real = real;
}

/**
 * gwy_data_line_get_offset:
 * @data_line: A data line.
 *
 * Gets the offset of data line origin.
 *
 * Returns: Offset value.
 **/
gdouble
gwy_data_line_get_offset(GwyDataLine *data_line)
{
    g_return_val_if_fail(GWY_IS_DATA_LINE(data_line), 0.0);
    return data_line->off;
}

/**
 * gwy_data_line_set_offset:
 * @data_line: A data line.
 * @offset: New offset value.
 *
 * Sets the offset of a data line origin.
 *
 * Note offsets don't affect any calculation, nor functions like
 * gwy_data_line_rtoi().
 **/
void
gwy_data_line_set_offset(GwyDataLine *data_line,
                         gdouble offset)
{
    g_return_if_fail(GWY_IS_DATA_LINE(data_line));
    data_line->off = offset;
}

/**
 * gwy_data_line_get_si_unit_x:
 * @data_line: A data line.
 *
 * Returns lateral SI unit of a data line.
 *
 * Returns: SI unit corresponding to the lateral (X) dimension of the data
 *          line.  Its reference count is not incremented.
 **/
GwySIUnit*
gwy_data_line_get_si_unit_x(GwyDataLine *data_line)
{
    g_return_val_if_fail(GWY_IS_DATA_LINE(data_line), NULL);

    if (!data_line->si_unit_x)
        data_line->si_unit_x = gwy_si_unit_new("");

    return data_line->si_unit_x;
}

/**
 * gwy_data_line_get_si_unit_y:
 * @data_line: A data line.
 *
 * Returns value SI unit of a data line.
 *
 * Returns: SI unit corresponding to the "height" (Z) dimension of the data
 *          line.  Its reference count is not incremented.
 **/
GwySIUnit*
gwy_data_line_get_si_unit_y(GwyDataLine *data_line)
{
    g_return_val_if_fail(GWY_IS_DATA_LINE(data_line), NULL);

    if (!data_line->si_unit_y)
        data_line->si_unit_y = gwy_si_unit_new("");

    return data_line->si_unit_y;
}

/**
 * gwy_data_line_set_si_unit_x:
 * @data_line: A data line.
 * @si_unit: SI unit to be set.
 *
 * Sets the SI unit corresponding to the lateral (X) dimension of a data
 * line.
 *
 * It does not assume a reference on @si_unit, instead it adds its own
 * reference.
 **/
void
gwy_data_line_set_si_unit_x(GwyDataLine *data_line,
                            GwySIUnit *si_unit)
{
    g_return_if_fail(GWY_IS_DATA_LINE(data_line));
    g_return_if_fail(GWY_IS_SI_UNIT(si_unit));
    if (data_line->si_unit_x == si_unit)
        return;

    gwy_object_unref(data_line->si_unit_x);
    g_object_ref(si_unit);
    data_line->si_unit_x = si_unit;
}

/**
 * gwy_data_line_set_si_unit_y:
 * @data_line: A data line.
 * @si_unit: SI unit to be set.
 *
 * Sets the SI unit corresponding to the "height" (Z) dimension of a data
 * line.
 *
 * It does not assume a reference on @si_unit, instead it adds its own
 * reference.
 **/
void
gwy_data_line_set_si_unit_y(GwyDataLine *data_line,
                            GwySIUnit *si_unit)
{
    g_return_if_fail(GWY_IS_DATA_LINE(data_line));
    g_return_if_fail(GWY_IS_SI_UNIT(si_unit));
    if (data_line->si_unit_y == si_unit)
        return;

    gwy_object_unref(data_line->si_unit_y);
    g_object_ref(si_unit);
    data_line->si_unit_y = si_unit;
}

/**
 * gwy_data_line_get_value_format_x:
 * @data_line: A data line.
 * @style: Unit format style.
 * @format: A SI value format to modify, or %NULL to allocate a new one.
 *
 * Finds value format good for displaying coordinates of a data line.
 *
 * Returns: The value format.  If @format is %NULL, a newly allocated format
 *          is returned, otherwise (modified) @format itself is returned.
 **/
GwySIValueFormat*
gwy_data_line_get_value_format_x(GwyDataLine *data_line,
                                 GwySIUnitFormatStyle style,
                                 GwySIValueFormat *format)
{
    gdouble max, unit;

    g_return_val_if_fail(GWY_IS_DATA_LINE(data_line), NULL);

    max = data_line->real;
    unit = data_line->real/data_line->res;
    return gwy_si_unit_get_format_with_resolution
                                   (gwy_data_line_get_si_unit_x(data_line),
                                    style, max, unit, format);
}

/**
 * gwy_data_line_get_value_format_y:
 * @data_line: A data line.
 * @style: Unit format style.
 * @format: A SI value format to modify, or %NULL to allocate a new one.
 *
 * Finds value format good for displaying values of a data line.
 *
 * Note this functions searches for minimum and maximum value in @data_line,
 * therefore it's relatively slow.
 *
 * Returns: The value format.  If @format is %NULL, a newly allocated format
 *          is returned, otherwise (modified) @format itself is returned.
 **/
GwySIValueFormat*
gwy_data_line_get_value_format_y(GwyDataLine *data_line,
                                 GwySIUnitFormatStyle style,
                                 GwySIValueFormat *format)
{
    gdouble max, min;

    g_return_val_if_fail(GWY_IS_DATA_LINE(data_line), NULL);

    max = gwy_data_line_get_max(data_line);
    min = gwy_data_line_get_min(data_line);
    if (max == min) {
        max = ABS(max);
        min = 0.0;
    }

    return gwy_si_unit_get_format(gwy_data_line_get_si_unit_y(data_line),
                                  style, max - min, format);
}

/**
 * gwy_data_line_itor:
 * @data_line: A data line.
 * @pixpos: Pixel coordinate.
 *
 * Transforms pixel coordinate to real (physical) coordinate.
 *
 * That is it maps range [0..resolution] to range [0..real-size].  It is not
 * suitable for conversion of matrix indices to physical coordinates, you
 * have to use gwy_data_line_itor(@data_line, @pixpos + 0.5) for that.
 *
 * Returns: @pixpos in real coordinates.
 **/
gdouble
gwy_data_line_itor(GwyDataLine *data_line, gdouble pixpos)
{
    return pixpos * data_line->real/data_line->res;
}

/**
 * gwy_data_line_rtoi:
 * @data_line: A data line.
 * @realpos: Real coordinate.
 *
 * Transforms real (physical) coordinate to pixel coordinate.
 *
 * That is it maps range [0..real-size] to range [0..resolution].
 *
 * Returns: @realpos in pixel coordinates.
 **/
gdouble
gwy_data_line_rtoi(GwyDataLine *data_line, gdouble realpos)
{
    return realpos * data_line->res/data_line->real;
}

/**
 * gwy_data_line_get_val:
 * @data_line: A data line.
 * @i: Position in the line (index).
 *
 * Gets value at given position in a data line.
 *
 * Do not access data with this function inside inner loops, it's slow.
 * Get raw data buffer with gwy_data_line_get_data_const() and access it
 * directly instead.
 *
 * Returns: Value at given index.
 **/
gdouble
gwy_data_line_get_val(GwyDataLine *data_line,
                      gint i)
{
    g_return_val_if_fail(i >= 0 && i < data_line->res, 0.0);

    return data_line->data[i];
}

/**
 * gwy_data_line_set_val:
 * @data_line: A data line.
 * @i: Position in the line (index).
 * @value: Value to set.
 *
 * Sets the value at given position in a data line.
 *
 * Do not set data with this function inside inner loops, it's slow.  Get raw
 * data buffer with gwy_data_line_get_data() and write to it directly instead.
 **/
void
gwy_data_line_set_val(GwyDataLine *data_line,
                      gint i,
                      gdouble value)
{
    g_return_if_fail(i >= 0 && i < data_line->res);

    data_line->data[i] = value;
}


/**
 * gwy_data_line_get_dval_real:
 * @data_line: A data line.
 * @x: real coordinates position
 * @interpolation: interpolation method used
 *
 * Gets interpolated value at arbitrary data line point indexed by real
 * coordinates.
 *
 * See also gwy_data_line_get_dval() for interpolation explanation.
 *
 * Returns: Value interpolated in the data line.
 **/
gdouble
gwy_data_line_get_dval_real(GwyDataLine *a, gdouble x, gint interpolation)
{
    return gwy_data_line_get_dval(a, gwy_data_line_rtoi(a, x), interpolation);
}

/**
 * gwy_data_line_invert:
 * @data_line: A data line.
 * @x: Whether to invert data point order.
 * @z: Whether to invert in Z direction (i.e., invert values).
 *
 * Reflects amd/or inverts a data line.
 *
 * In the case of value reflection, it's inverted about mean value.
 **/
void
gwy_data_line_invert(GwyDataLine *data_line,
                     gboolean x,
                     gboolean z)
{
    gint i;
    gdouble avg;
    gdouble *data;

    g_return_if_fail(GWY_IS_DATA_LINE(data_line));
    data = data_line->data;
    if (x) {
        for (i = 0; i < data_line->res/2; i++)
            GWY_SWAP(gdouble, data[i], data[data_line->res-1 - i]);
    }

    if (z) {
        avg = gwy_data_line_get_avg(data_line);
        for (i = 0; i < data_line->res; i++)
            data[i] = 2*avg - data[i];
    }
}

/**
 * gwy_data_line_fill:
 * @data_line: A data line.
 * @value: Value to fill data line with.
 *
 * Fills a data line with specified value.
 **/
void
gwy_data_line_fill(GwyDataLine *data_line,
                   gdouble value)
{
    gint i;

    g_return_if_fail(GWY_IS_DATA_LINE(data_line));
    for (i = 0; i < data_line->res; i++)
        data_line->data[i] = value;
}

/**
 * gwy_data_line_clear:
 * @data_line: A data line.
 *
 * Fills a data line with zeroes.
 **/
void
gwy_data_line_clear(GwyDataLine *data_line)
{
    g_return_if_fail(GWY_IS_DATA_LINE(data_line));
    memset(data_line->data, 0, data_line->res*sizeof(gdouble));
}

/**
 * gwy_data_line_add:
 * @data_line: A data line.
 * @value: Value to be added.
 *
 * Adds a specified value to all values in a data line.
 **/
void
gwy_data_line_add(GwyDataLine *data_line,
                  gdouble value)
{
    gint i;

    g_return_if_fail(GWY_IS_DATA_LINE(data_line));
    for (i = 0; i < data_line->res; i++)
        data_line->data[i] += value;
}

/**
 * gwy_data_line_multiply:
 * @data_line: A data line.
 * @value: Value to multiply data line with.
 *
 * Multiplies all values in a data line with a specified value.
 **/
void
gwy_data_line_multiply(GwyDataLine *data_line,
                       gdouble value)
{
    gint i;

    g_return_if_fail(GWY_IS_DATA_LINE(data_line));
    for (i = 0; i < data_line->res; i++)
        data_line->data[i] *= value;
}

/**
 * gwy_data_line_part_fill:
 * @data_line: A data line.
 * @from: Index the line part starts at.
 * @to: Index the line part ends at + 1.
 * @value: Value to fill data line part with.
 *
 * Fills specified part of data line with specified number
 **/
void
gwy_data_line_part_fill(GwyDataLine *data_line,
                        gint from, gint to,
                        gdouble value)
{
    gint i;

    g_return_if_fail(GWY_IS_DATA_LINE(data_line));
    if (to < from)
        GWY_SWAP(gint, from, to);

    g_return_if_fail(from >= 0 && to <= data_line->res);

    for (i = from; i < to; i++)
        data_line->data[i] = value;
}

/**
 * gwy_data_line_part_clear:
 * @data_line: A data line.
 * @from: Index the line part starts at.
 * @to: Index the line part ends at + 1.
 *
 * Fills a data line part with zeroes.
 **/
void
gwy_data_line_part_clear(GwyDataLine *data_line,
                         gint from, gint to)
{
    g_return_if_fail(GWY_IS_DATA_LINE(data_line));
    if (to < from)
        GWY_SWAP(gint, from, to);

    g_return_if_fail(from >= 0 && to <= data_line->res);

    memset(data_line->data + from, 0, (to - from)*sizeof(gdouble));
}

/**
 * gwy_data_line_part_add:
 * @data_line: A data line.
 * @from: Index the line part starts at.
 * @to: Index the line part ends at + 1.
 * @value: Value to be added
 *
 * Adds specified value to all values in a part of a data line.
 **/
void
gwy_data_line_part_add(GwyDataLine *data_line,
                       gint from, gint to,
                       gdouble value)
{
    gint i;

    g_return_if_fail(GWY_IS_DATA_LINE(data_line));
    if (to < from)
        GWY_SWAP(gint, from, to);

    g_return_if_fail(from >= 0 && to <= data_line->res);

    for (i = from; i < to; i++)
        data_line->data[i] += value;
}

/**
 * gwy_data_line_part_multiply:
 * @data_line: A data line.
 * @from: Index the line part starts at.
 * @to: Index the line part ends at + 1.
 * @value: Value multiply data line part with.
 *
 * Multiplies all values in a part of data line by specified value.
 **/
void
gwy_data_line_part_multiply(GwyDataLine *data_line,
                            gint from, gint to,
                            gdouble value)
{
    gint i;

    g_return_if_fail(GWY_IS_DATA_LINE(data_line));
    if (to < from)
        GWY_SWAP(gint, from, to);

    g_return_if_fail(from >= 0 && to <= data_line->res);

    for (i = from; i < to; i++)
        data_line->data[i] *= value;
}

/**
 * gwy_data_line_threshold:
 * @data_line: A data line.
 * @threshval: Threshold value.
 * @bottom: Lower replacement value.
 * @top: Upper replacement value.
 *
 * Sets all the values to @bottom or @top value
 * depending on whether the original values are
 * below or above @threshold value
 *
 * Returns: total number of values above threshold
 **/
gint
gwy_data_line_threshold(GwyDataLine *a,
                        gdouble threshval, gdouble bottom, gdouble top)
{
    gint i, tot = 0;

    g_return_val_if_fail(GWY_IS_DATA_LINE(a), 0);

    for (i = 0; i < a->res; i++) {
        if (a->data[i] < threshval)
            a->data[i] = bottom;
        else {
            a->data[i] = top;
            tot++;
        }
    }
    return tot;
}

/**
 * gwy_data_line_part_threshold:
 * @data_line: A data line.
 * @from: Index the line part starts at.
 * @to: Index the line part ends at + 1.
 * @threshval: Threshold value.
 * @bottom: Lower replacement value.
 * @top: Upper replacement value.
 *
 * Sets all the values within interval to @bottom or @top value
 * depending on whether the original values are
 * below or above @threshold value.
 *
 * Returns: total number of values above threshold within interval
 **/
gint
gwy_data_line_part_threshold(GwyDataLine *a,
                             gint from, gint to,
                             gdouble threshval, gdouble bottom, gdouble top)
{
    gint i, tot = 0;

    g_return_val_if_fail(GWY_IS_DATA_LINE(a), 0);
    if (to < from)
        GWY_SWAP(gint, from, to);

    g_return_val_if_fail(from >= 0 && to <= a->res, 0);

    for (i = from; i < to; i++) {
        if (a->data[i] < threshval)
            a->data[i] = bottom;
        else {
            a->data[i] = top;
            tot++;
        }
    }
    return tot;
}

/**
 * gwy_data_line_get_line_coeffs:
 * @data_line: A data line.
 * @av: Height coefficient.
 * @bv: Slope coeficient.
 *
 * Finds line leveling coefficients.
 *
 * The coefficients can be used for line leveling using relation
 * data[i] := data[i] - (av + bv*i);
 **/
void
gwy_data_line_get_line_coeffs(GwyDataLine *a, gdouble *av, gdouble *bv)
{
    gdouble sumxi, sumxixi;
    gdouble sumsixi = 0.0;
    gdouble sumsi = 0.0;
    gdouble n = a->res;
    gint i;

    g_return_if_fail(GWY_IS_DATA_LINE(a));

    /* These are already averages, not sums */
    sumxi = (n-1)/2.0;
    sumxixi = (2*n-1)*(n-1)/6.0;

    for (i = 0; i < a->res; i++) {
        sumsi += a->data[i];
        sumsixi += a->data[i] * i;
    }
    sumsi /= n;
    sumsixi /= n;

    if (bv)
        *bv = (sumsixi - sumsi*sumxi)/(sumxixi - sumxi*sumxi);
    if (av)
        *av = (sumsi*sumxixi - sumxi*sumsixi)/(sumxixi - sumxi*sumxi);
}

/**
 * gwy_data_line_line_level:
 * @data_line: A data line.
 * @av: Height coefficient.
 * @bv: Slope coefficient.
 *
 * Performs line leveling.
 *
 * See gwy_data_line_get_line_coeffs() for deails.
 **/
void
gwy_data_line_line_level(GwyDataLine *a, gdouble av, gdouble bv)
{
    gint i;

    g_return_if_fail(GWY_IS_DATA_LINE(a));

    for (i = 0; i < a->res; i++)
        a->data[i] -= av + bv*i;
}

/**
 * gwy_data_line_line_rotate:
 * @data_line: A data line.
 * @angle: Angle of rotation (in radians), counterclockwise.
 * @interpolation: Interpolation method to use (can be only of two-point type).
 *
 * Performs line rotation.
 *
 * This is operation similar to leveling, but it does not change the angles
 * between line segments.
 **/
void
gwy_data_line_line_rotate(GwyDataLine *a,
                          gdouble angle,
                          gint interpolation)
{
    gint i, k, maxi, res;
    gdouble ratio, x, as, radius, xl1, xl2, yl1, yl2;
    gdouble *dx, *dy;

    g_return_if_fail(GWY_IS_DATA_LINE(a));
    if (angle == 0)
        return;

    /* INTERPOLATION: not checked, I'm not sure how this all relates to
     * interpolation */
    res = a->res;
    dx = g_new(gdouble, a->res);
    dy = g_new(gdouble, a->res);

    ratio = a->real/a->res;
    dx[0] = 0;
    dy[0] = a->data[0];
    for (i = 1; i < a->res; i++) {
        as = atan2(a->data[i], i*ratio);
        radius = hypot(i*ratio, a->data[i]);
        dx[i] = radius*cos(as + angle);
        dy[i] = radius*sin(as + angle);
    }


    k = 0;
    maxi = 0;
    for (i = 1; i < a->res; i++) {
        x = i*ratio;
        k = 0;
        do {
            k++;
        } while (dx[k] < x && k < a->res);

        if (k >= a->res-1) {
            maxi = i;
            break;
        }

        xl1 = dx[k-1];
        xl2 = dx[k];
        yl1 = dy[k-1];
        yl2 = dy[k];

        if (interpolation == GWY_INTERPOLATION_ROUND
            || interpolation == GWY_INTERPOLATION_LINEAR) {
            a->data[i] = gwy_interpolation_get_dval(x, xl1, yl1, xl2, yl2,
                                                    interpolation);
        }
        else
            g_warning("Interpolation not implemented yet.\n");
    }
    if (maxi != 0) {
        a->real *= maxi/((double)a->res);
        a->res = maxi;
        a->data = g_renew(gdouble, a->data, a->res*sizeof(gdouble));
    }

    if (a->res != res)
        gwy_data_line_resample(a, res, interpolation);


    g_free(dx);
    g_free(dy);
}

/**
 * gwy_data_line_get_der:
 * @data_line: A data line.
 * @i: Pixel coordinate.
 *
 * Computes central derivaltion at given index in a data line.
 *
 * Returns: Derivation at given position.
 **/
gdouble
gwy_data_line_get_der(GwyDataLine *a, gint i)
{
    g_return_val_if_fail(i >= 0 && i < a->res, 0.0);

    if (i == 0)
        return (a->data[1] - a->data[0])*a->res/a->real;
    if (i == (a->res-1))
        return (a->data[i] - a->data[i-1])*a->res/a->real;
    return (a->data[i+1] - a->data[i-1])*a->res/a->real/2;
}

/**
 * gwy_data_line_part_fit_polynom:
 * @data_line: A data line.
 * @n: Polynom degree.
 * @coeffs: An array of size @n+1 to store the coefficients to, or %NULL
 *          (a fresh array is allocated then).
 * @from: Index the line part starts at.
 * @to: Index the line part ends at + 1.
 *
 * Fits a polynomial through a part of a data line.
 *
 * Please see gwy_data_line_fit_polynom() for more details.
 *
 * Returns: The coefficients of the polynomial (@coeffs when it was not %NULL,
 *          otherwise a newly allocated array).
 **/
gdouble*
gwy_data_line_part_fit_polynom(GwyDataLine *data_line,
                               gint n, gdouble *coeffs,
                               gint from, gint to)
{
    gdouble *sumx, *m;
    gint i, j;
    gdouble *data;

    g_return_val_if_fail(GWY_IS_DATA_LINE(data_line), NULL);
    g_return_val_if_fail(n >= 0, NULL);
    data = data_line->data;

    if (to < from)
        GWY_SWAP(gint, from, to);

    sumx = g_new0(gdouble, 2*n+1);
    if (!coeffs)
        coeffs = g_new0(gdouble, n+1);
    else
        memset(coeffs, 0, (n+1)*sizeof(gdouble));

    for (i = from; i < to; i++) {
        gdouble x = i;
        gdouble y = data[i];
        gdouble xp;

        xp = 1.0;
        for (j = 0; j <= n; j++) {
            sumx[j] += xp;
            coeffs[j] += xp*y;
            xp *= x;
        }
        for (j = n+1; j <= 2*n; j++) {
            sumx[j] += xp;
            xp *= x;
        }
    }

    m = g_new(gdouble, (n+1)*(n+2)/2);
    for (i = 0; i <= n; i++) {
        gdouble *row = m + i*(i+1)/2;

        for (j = 0; j <= i; j++)
            row[j] = sumx[i+j];
    }
    if (!gwy_math_choleski_decompose(n+1, m)) {
        g_warning("Line polynomial fit failed");
        memset(coeffs, 0, (n+1)*sizeof(gdouble));
    }
    else
        gwy_math_choleski_solve(n+1, m, coeffs);

    g_free(m);
    g_free(sumx);

    return coeffs;
}

/**
 * gwy_data_line_fit_polynom:
 * @data_line: A data line.
 * @n: Polynom degree.
 * @coeffs: An array of size @n+1 to store the coefficients to, or %NULL
 *          (a fresh array is allocated then).
 *
 * Fits a polynomial through a data line.
 *
 * Note @n is polynomial degree, so the size of @coeffs is @n+1.  X-values
 * are indices in the data line.
 *
 * For polynomials of degree 0 and 1 it's better to use gwy_data_line_get_avg()
 * and gwy_data_line_line_coeffs() because they are faster.
 *
 * Returns: The coefficients of the polynomial (@coeffs when it was not %NULL,
 *          otherwise a newly allocated array).
 **/
gdouble*
gwy_data_line_fit_polynom(GwyDataLine *data_line,
                          gint n, gdouble *coeffs)
{
    return gwy_data_line_part_fit_polynom(data_line, n, coeffs,
                                          0, gwy_data_line_get_res(data_line));
}


/**
 * gwy_data_line_part_subtract_polynom:
 * @data_line: A data line.
 * @n: Polynom degree.
 * @coeffs: An array of size @n+1 with polynomial coefficients to.
 * @from: Index the line part starts at.
 * @to: Index the line part ends at + 1.
 *
 * Subtracts a polynomial from a part of a data line.
 **/
void
gwy_data_line_part_subtract_polynom(GwyDataLine *data_line,
                                    gint n,
                                    const gdouble *coeffs,
                                    gint from, gint to)
{
    gint i, j;
    gdouble val;

    g_return_if_fail(GWY_IS_DATA_LINE(data_line));
    g_return_if_fail(coeffs);
    g_return_if_fail(n >= 0);

    if (to < from)
        GWY_SWAP(gint, from, to);

    for (i = from; i < to; i++) {
        val = 0.0;
        for (j = n; j; j--) {
            val += coeffs[j];
            val *= i;
        }
        val += coeffs[0];

        data_line->data[i] -= val;
    }

}

/**
 * gwy_data_line_subtract_polynom:
 * @data_line: A data line.
 * @n: Polynom degree.
 * @coeffs: An array of size @n+1 with polynomial coefficients to.
 *
 * Subtracts a polynomial from a data line.
 **/
void
gwy_data_line_subtract_polynom(GwyDataLine *data_line,
                               gint n,
                               const gdouble *coeffs)
{
    gwy_data_line_part_subtract_polynom(data_line, n, coeffs,
                                        0, gwy_data_line_get_res(data_line));
}

/**
 * gwy_data_line_cumulate:
 * @data_line: A data line.
 *
 * Transforms a distribution in a data line to cummulative distribution.
 *
 * Each element becomes sum of all previous elements in the line, including
 * self.
 **/
void
gwy_data_line_cumulate(GwyDataLine *data_line)
{
    gdouble sum;
    gdouble *data;
    gint i;

    g_return_if_fail(GWY_IS_DATA_LINE(data_line));

    data = data_line->data;
    sum = 0.0;
    for (i = 0; i < data_line->res; i++) {
        sum += data[i];
        data[i] = sum;
    }
}


/************************** Documentation ****************************/

/**
 * SECTION:dataline
 * @title: GwyDataLine
 * @short_description: One-dimensional data representation
 *
 * #GwyDataLine represents 1D data arrays in Gwyddion. It is used for most of
 * the data processing functions connected with 1D data, graphs, etc.
 **/

/**
 * GwyDataLine:
 *
 * The #GwyDataLine struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * gwy_data_line_duplicate:
 * @data_line: A data line to duplicate.
 *
 * Convenience macro doing gwy_serializable_duplicate() with all the necessary
 * typecasting.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
