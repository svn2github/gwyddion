/*
 *  @(#) $Id$
 *  Copyright (C) 2007 David Necas (Yeti), Petr Klapetek.
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
#include <libgwyddion/gwymacros.h>
#include <libprocess/stats.h>
#include <app/data-browser.h>
#include <app/gwymoduleutils-file.h>

/**
 * gwy_app_channel_check_nonsquare:
 * @data: A data container.
 * @id: Data channel id.
 *
 * Sets `realsquare' for a channel highly non-square pixels.
 *
 * The threshold for highly non-square is somewhat arbitrary.
 * Fortunately, most files encoutered in practice have the measure ratio either
 * very close to 1, larger or equal than 2.
 *
 * Returns: %TRUE if the channel was found to have highly non-square pixels
 *          and `realsquare' was set (otherwise it was unset).
 *
 * Since: 2.3
 **/
gboolean
gwy_app_channel_check_nonsquare(GwyContainer *data,
                                gint id)
{
    GwyDataField *dfield;
    gdouble xmeasure, ymeasure, q;
    gboolean nonsquare;
    GQuark quark;
    const gchar *key;
    gchar *s;

    quark = gwy_app_get_data_key_for_id(id);
    dfield = GWY_DATA_FIELD(gwy_container_get_object(data, quark));
    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), FALSE);

    xmeasure = gwy_data_field_get_xmeasure(dfield);
    ymeasure = gwy_data_field_get_ymeasure(dfield);
    q = xmeasure/ymeasure;

    nonsquare = (q > G_SQRT2 || q < 1.0/G_SQRT2);

    key = g_quark_to_string(quark);
    s = g_strconcat(key, "/realsquare", NULL);
    if (nonsquare)
        gwy_container_set_boolean_by_name(data, s, TRUE);
    else
        gwy_container_remove_by_name(data, s);
    g_free(s);

    return nonsquare;
}

/**
 * gwy_app_channel_title_fall_back:
 * @data: A data container.
 * @id: Data channel id.
 *
 * Adds a channel title based on data field units.
 *
 * The guess is very simple, but probably better than `Unknown channel' in
 * most cases.  If there already is a title it is left intact, making use of
 * this function as a fall-back easier.
 *
 * Returns: %TRUE if the title was set (either by this function or before).
 *
 * Since: 2.3
 **/
gboolean
gwy_app_channel_title_fall_back(GwyContainer *data,
                                gint id)
{
    static const struct {
        const gchar *unit;
        const gchar *title;
    }
    map[] = {
        { "m",   "Topography", },
        { "A",   "Current",    },
        { "deg", "Phase",      },
        { "V",   "Voltage",    },
        { "N",   "Force",      },
    };

    GwySIUnit *siunit, *test;
    GwyDataField *dfield;
    const gchar *key, *title;
    GQuark quark;
    guint i;
    gchar *s;

    quark = gwy_app_get_data_key_for_id(id);
    dfield = GWY_DATA_FIELD(gwy_container_get_object(data, quark));
    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), FALSE);

    key = g_quark_to_string(quark);
    s = g_strconcat(key, "/title", NULL);
    quark = g_quark_from_string(s);
    g_free(s);
    if (gwy_container_contains(data, quark))
        return TRUE;

    siunit = gwy_data_field_get_si_unit_z(dfield);
    test = gwy_si_unit_new(NULL);
    title = NULL;

    for (title = NULL, i = 0; i < G_N_ELEMENTS(map) && !title; i++) {
        gwy_si_unit_set_from_string(test, map[i].unit);
        if (gwy_si_unit_equal(siunit, test))
            title = map[i].title;
    }

    g_object_unref(test);

    if (title) {
        gwy_container_set_string(data, quark, g_strdup(title));
        return TRUE;
    }

    return FALSE;
}

/**
 * gwy_app_channel_remove_bad_data:
 * @dfield: A data field.  The values of bad data points are ignored and might
 *          be even left uninitialized.
 * @mfield: A mask field containing 1.0 in place of good data points, 1.0 in
 *          place of bad points.  It will be inverted to become the mask of
 *          bad points.
 *
 * Replaces bad data points with some neutral values.
 *
 * Since Gwyddion has no concept of bad data points, they are usually marked
 * with a mask and replaced with some neutral values upon import, leaving the
 * user to decide how to proceed further.  This helper function performs such
 * replacement, using the average of all good points as the neutral replacement
 * value (at this moment).
 *
 * Returns: The number of bad data points replaced.  If zero is returned, all
 *          points are good and there is no need for masking.
 *
 * Since: 2.14
 **/
guint
gwy_app_channel_remove_bad_data(GwyDataField *dfield, GwyDataField *mfield)
{
    gdouble *data = gwy_data_field_get_data(dfield);
    gdouble *mdata = gwy_data_field_get_data(mfield);
    gdouble *drow, *mrow;
    gdouble avg;
    guint i, j, mcount, xres, yres;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    avg = gwy_data_field_area_get_avg(dfield, mfield, 0, 0, xres, yres);
    mcount = 0;
    for (i = 0; i < yres; i++) {
        mrow = mdata + i*xres;
        drow = data + i*xres;
        for (j = 0; j < xres; j++) {
            if (!mrow[j]) {
                drow[j] = avg;
                mcount++;
            }
            mrow[j] = 1.0 - mrow[j];
        }
    }

    gwy_debug("mcount = %u", mcount);

    return mcount;
}

/************************** Documentation ****************************/

/**
 * SECTION:gwymoduleutils-file
 * @title: file module utils
 * @short_description: Utility functions for file modules
 * @include: app/gwymoduleutils-file.h
 *
 * Functions gwy_app_channel_check_nonsquare() and
 * gwy_app_channel_title_fall_back() perform common tasks improving the
 * imported of channels from foreign data files.  Typically one calls them on
 * all imported channel ids after storing the data fields the the container,
 * if they are useful for a given file type.
 *
 * The group of functions gwy_get_gint16_le(), gwy_get_gint16_be(), etc.
 * is intended to portably read packed binary data structures that are commonly
 * found in SPM files.  They all work identically: the binary data value is
 * read from the buffer, converted if necessary, and the provided
 * buffer pointer is moved to point after the value to faciliate sequential
 * reading.
 *
 * As no buffer size is passed, obviously no buffer size checking is performed.
 * The caller has to ensure the buffer is large enough -- it is expected the
 * caller checks the total buffer size before starting to parse it.
 *
 * For example to portably read the following packed struct stored
 * in big-endian byte order:
 * <informalexample><programlisting>
 * struct {
 *     guint16 xres;
 *     guint16 yres;
 *     gfloat measure;
 * } header;
 * </programlisting></informalexample>
 * one can do (after checking the buffer size):
 * <informalexample><programlisting>
 * const guchar *p = buffer;
 * header.xres    = gwy_get_guint16_be(&amp;p);
 * header.yres    = gwy_get_guint16_be(&amp;p);
 * header.measure = gwy_get_gfloat_be(&amp;p);
 * </programlisting></informalexample>
 * and @p will point after @measure in @buffer after this snippet is finished.
 *
 * The data types used in @header do not matter (provided they are large
 * enough to hold the values), the exact types are determined by the functions
 * used.  Therefore the reading would work identically if @header was defined
 * using common types:
 * <informalexample><programlisting>
 * struct {
 *     gint xres;
 *     gint yres;
 *     gdouble measure;
 * } header;
 * </programlisting></informalexample>
 **/

/**
 * gwy_get_gboolean8:
 * @ppv: Pointer to a pointer to boolean (stored as a signle byte)
 *       in a memory buffer.
 *
 * Reads a boolean value stored as a signle byte from a
 * binary data buffer, moving the buffer pointer to point just after the value.
 *
 * Returns: The #gboolean value read from the buffer.
 *
 * Since: 2.3
 **/

/**
 * gwy_get_gint16_le:
 * @ppv: Pointer to a pointer to a little-endian signed 16bit integer
 *       in a memory buffer.
 *
 * Reads a signed 16bit integer value from a little-endian
 * binary data buffer, moving the buffer pointer to point just after the value.
 *
 * Returns: The #gint16 value read from the buffer.
 *
 * Since: 2.3
 **/

/**
 * gwy_get_gint16_be:
 * @ppv: Pointer to a pointer to a big-endian signed 16bit integer
 *       in a memory buffer.
 *
 * Reads a signed 16bit integer value from a big-endian
 * binary data buffer, moving the buffer pointer to point just after the value.
 *
 * Returns: The #gint16 value read from the buffer.
 *
 * Since: 2.3
 **/

/**
 * gwy_get_guint16_le:
 * @ppv: Pointer to a pointer to a little-endian unsigned 16bit integer
 *       in a memory buffer.
 *
 * Reads an unsigned 16bit integer value from a little-endian
 * binary data buffer, moving the buffer pointer to point just after the value.
 *
 * Returns: The #guint16 value read from the buffer.
 *
 * Since: 2.3
 **/

/**
 * gwy_get_guint16_be:
 * @ppv: Pointer to a pointer to a big-endian unsigned 16bit integer
 *       in a memory buffer.
 *
 * Reads an unsigned 16bit integer value from a big-endian
 * binary data buffer, moving the buffer pointer to point just after the value.
 *
 * Returns: The #guint16 value read from the buffer.
 *
 * Since: 2.3
 **/

/**
 * gwy_get_gint32_le:
 * @ppv: Pointer to a pointer to a little-endian signed 32bit integer
 *       in a memory buffer.
 *
 * Reads a signed 32bit integer value from a little-endian
 * binary data buffer, moving the buffer pointer to point just after the value.
 *
 * Returns: The #gint32 value read from the buffer.
 *
 * Since: 2.3
 **/

/**
 * gwy_get_gint32_be:
 * @ppv: Pointer to a pointer to a big-endian signed 32bit integer
 *       in a memory buffer.
 *
 * Reads a signed 32bit integer value from a big-endian
 * binary data buffer, moving the buffer pointer to point just after the value.
 *
 * Returns: The #gint32 value read from the buffer.
 *
 * Since: 2.3
 **/

/**
 * gwy_get_guint32_le:
 * @ppv: Pointer to a pointer to a little-endian unsigned 32bit integer
 *       in a memory buffer.
 *
 * Reads an unsigned 32bit integer value from a little-endian
 * binary data buffer, moving the buffer pointer to point just after the value.
 *
 * Returns: The #guint32 value read from the buffer.
 *
 * Since: 2.3
 **/

/**
 * gwy_get_guint32_be:
 * @ppv: Pointer to a pointer to a big-endian unsigned 32bit integer
 *       in a memory buffer.
 *
 * Reads an unsigned 32bit integer value from a big-endian
 * binary data buffer, moving the buffer pointer to point just after the value.
 *
 * Returns: The #guint32 value read from the buffer.
 *
 * Since: 2.3
 **/

/**
 * gwy_get_gint64_le:
 * @ppv: Pointer to a pointer to a little-endian signed 64bit integer
 *       in a memory buffer.
 *
 * Reads a signed 64bit integer value from a little-endian
 * binary data buffer, moving the buffer pointer to point just after the value.
 *
 * Returns: The #gint64 value read from the buffer.
 *
 * Since: 2.3
 **/

/**
 * gwy_get_gint64_be:
 * @ppv: Pointer to a pointer to a big-endian signed 64bit integer
 *       in a memory buffer.
 *
 * Reads a signed 64bit integer value from a big-endian
 * binary data buffer, moving the buffer pointer to point just after the value.
 *
 * Returns: The #gint64 value read from the buffer.
 *
 * Since: 2.3
 **/

/**
 * gwy_get_guint64_le:
 * @ppv: Pointer to a pointer to a little-endian unsigned 64bit integer
 *       in a memory buffer.
 *
 * Reads an unsigned 64bit integer value from a little-endian
 * binary data buffer, moving the buffer pointer to point just after the value.
 *
 * Returns: The #guint64 value read from the buffer.
 *
 * Since: 2.3
 **/

/**
 * gwy_get_guint64_be:
 * @ppv: Pointer to a pointer to a big-endian unsigned 64bit integer
 *       in a memory buffer.
 *
 * Reads an unsigned 64bit integer value from a big-endian
 * binary data buffer, moving the buffer pointer to point just after the value.
 *
 * Returns: The #guint64 value read from the buffer.
 *
 * Since: 2.3
 **/

/**
 * gwy_get_gfloat_le:
 * @ppv: Pointer to a pointer to a little-endian single-precision IEEE float
 *       in a memory buffer.
 *
 * Reads a single-precision IEEE float value from a little-endian
 * binary data buffer, moving the buffer pointer to point just after the value.
 *
 * Returns: The #gfloat value read from the buffer.
 *
 * Since: 2.3
 **/

/**
 * gwy_get_gfloat_be:
 * @ppv: Pointer to a pointer to a big-endian single-precision IEEE float
 *       in a memory buffer.
 *
 * Reads a single-precision IEEE float value from a big-endian
 * binary data buffer, moving the buffer pointer to point just after the value.
 *
 * Returns: The #gfloat value read from the buffer.
 *
 * Since: 2.3
 **/

/**
 * gwy_get_gdouble_le:
 * @ppv: Pointer to a pointer to a little-endian double-precision IEEE float
 *       in a memory buffer.
 *
 * Reads a double-precision IEEE float value from a little-endian
 * binary data buffer, moving the buffer pointer to point just after the value.
 *
 * Returns: The #gdouble value read from the buffer.
 *
 * Since: 2.3
 **/

/**
 * gwy_get_gdouble_be:
 * @ppv: Pointer to a pointer to a big-endian double-precision IEEE float
 *       in a memory buffer.
 *
 * Reads a double-precision IEEE float value from a big-endian
 * binary data buffer, moving the buffer pointer to point just after the value.
 *
 * Returns: The #gdouble value read from the buffer.
 *
 * Since: 2.3
 **/

/**
 * gwy_get_pascal_real_le:
 * @ppv: Pointer to a pointer to a little-endian six-byte Pascal Real
 *       in a memory buffer.
 *
 * Reads a six-byte Pascale Real value from a little-endian
 * binary data buffer, moving the buffer pointer to point just after the value.
 *
 * Returns: The floating point value read from the buffer as a #gdouble.
 *
 * Since: 2.3
 **/

/**
 * gwy_get_pascal_real_be:
 * @ppv: Pointer to a pointer to a big-endian six-byte Pascal Real
 *       in a memory buffer.
 *
 * Reads a six-byte Pascale Real value from a big-endian
 * binary data buffer, moving the buffer pointer to point just after the value.
 *
 * Returns: The floating point value read from the buffer as a #gdouble.
 *
 * Since: 2.3
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
