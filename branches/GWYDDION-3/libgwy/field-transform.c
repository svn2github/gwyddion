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

#include <string.h>
#include "libgwy/macros.h"
#include "libgwy/math.h"
#include "libgwy/field-transform.h"
#include "libgwy/libgwy-aliases.h"
#include "libgwy/processing-internal.h"

#define DSWAP(x, y) GWY_SWAP(gdouble, x, y)

// For rotations. The largest value before the performance starts deteriorate
// on Phenom II.
enum { BLOCK_SIZE = 64 };

static void
flip_both(GwyField *field, gboolean transform_offsets)
{
    guint n = field->xres * field->yres;
    gdouble *d = field->data, *e = d + n-1;
    for (guint i = n/2; i; i--, d++, e--)
        DSWAP(*d, *e);

    if (transform_offsets) {
        g_object_freeze_notify(G_OBJECT(field));
        gwy_field_set_xoffset(field, -(field->xreal + field->xoff));
        gwy_field_set_yoffset(field, -(field->yreal + field->yoff));
        g_object_thaw_notify(G_OBJECT(field));
    }
}

static void
flip_horizontally(GwyField *field, gboolean transform_offsets)
{
    guint xres = field->xres, yres = field->yres;
    for (guint i = 0; i < yres; i++) {
        gdouble *d = field->data + i*xres, *e = d + xres-1;
        for (guint j = xres/2; j; j--, d++, e--)
            DSWAP(*d, *e);
    }

    if (transform_offsets)
        gwy_field_set_xoffset(field, -(field->xreal + field->xoff));
}

static void
flip_vertically(GwyField *field, gboolean transform_offsets)
{
    guint xres = field->xres, yres = field->yres;
    for (guint i = 0; i < yres/2; i++) {
        gdouble *d = field->data + i*xres,
                *e = field->data + (yres-1 - i)*xres;
        for (guint j = xres; j; j--, d++, e++)
            DSWAP(*d, *e);
    }

    if (transform_offsets)
        gwy_field_set_yoffset(field, -(field->yreal + field->yoff));
}

void
gwy_field_flip(GwyField *field,
               gboolean horizontally,
               gboolean vertically,
               gboolean transform_offsets)
{
    g_return_if_fail(GWY_IS_FIELD(field));

    if (horizontally && vertically)
        flip_both(field, transform_offsets);
    else if (horizontally)
        flip_horizontally(field, transform_offsets);
    else if (vertically)
        flip_vertically(field, transform_offsets);
    // Cached values do not change
}

static void
swap_xy(const GwyField *source,
        GwyField *dest)
{
    guint dxres = dest->xres, dyres = dest->yres;
    guint dxmax = dxres/BLOCK_SIZE * BLOCK_SIZE;
    guint dymax = dyres/BLOCK_SIZE * BLOCK_SIZE;

    for (guint ib = 0; ib < dymax; ib += BLOCK_SIZE) {
        for (guint jb = 0; jb < dxmax; jb += BLOCK_SIZE) {
            const gdouble *sb = source->data + (jb*dyres + ib);
            gdouble *db = dest->data + (ib*dxres + jb);
            for (guint i = 0; i < BLOCK_SIZE; i++) {
                const gdouble *s = sb + i;
                gdouble *d = db + i*dxres;
                for (guint j = BLOCK_SIZE; j; j--, d++, s += dyres)
                    *d = *s;
            }
        }
        if (dxmax != dxres) {
            guint jb = dxmax;
            const gdouble *sb = source->data + (jb*dyres + ib);
            gdouble *db = dest->data + (ib*dxres + jb);
            for (guint i = 0; i < BLOCK_SIZE; i++) {
                const gdouble *s = sb + i;
                gdouble *d = db + i*dxres;
                for (guint j = dxres - dxmax; j; j--, d++, s += dyres)
                    *d = *s;
            }
        }
    }
    if (dymax != dyres) {
        guint ib = dymax;
        for (guint jb = 0; jb < dxmax; jb += BLOCK_SIZE) {
            const gdouble *sb = source->data + (jb*dyres + ib);
            gdouble *db = dest->data + (ib*dxres + jb);
            for (guint i = 0; i < dyres - dymax; i++) {
                const gdouble *s = sb + i;
                gdouble *d = db + i*dxres;
                for (guint j = BLOCK_SIZE; j; j--, d++, s += dyres)
                    *d = *s;
            }
        }
        if (dxmax != dxres) {
            guint jb = dxmax;
            const gdouble *sb = source->data + (jb*dyres + ib);
            gdouble *db = dest->data + (ib*dxres + jb);
            for (guint i = 0; i < dyres - dymax; i++) {
                const gdouble *s = sb + i;
                gdouble *d = db + i*dxres;
                for (guint j = dxres - dxmax; j; j--, d++, s += dyres)
                    *d = *s;
            }
        }
    }
}

static void
rotate_90(const GwyField *source,
          GwyField *dest,
          gboolean transform_offsets)
{
    swap_xy(source, dest);
    flip_vertically(dest, FALSE);
    if (transform_offsets)
        dest->yoff = -(dest->yoff + dest->yreal);
}

static void
rotate_270(const GwyField *source,
           GwyField *dest,
           gboolean transform_offsets)
{
    swap_xy(source, dest);
    flip_horizontally(dest, FALSE);
    if (transform_offsets)
        dest->xoff = -(dest->xoff + dest->xreal);
}

GwyField*
gwy_field_rotate_simple(const GwyField *field,
                        GwySimpleRotation rotation,
                        gboolean transform_offsets)
{
    g_return_val_if_fail(GWY_IS_FIELD(field), NULL);
    g_return_val_if_fail(rotation <= GWY_SIMPLE_ROTATE_CLOCKWISE, NULL);

    if (rotation == GWY_SIMPLE_ROTATE_NONE)
        return gwy_field_duplicate(field);

    if (rotation == GWY_SIMPLE_ROTATE_UPSIDEDOWN) {
        GwyField *newfield = gwy_field_duplicate(field);
        gwy_field_flip(newfield, TRUE, TRUE, transform_offsets);
        return newfield;
    }

    GwyField *newfield = gwy_field_new_alike(field, FALSE);
    GWY_SWAP(guint, newfield->xres, newfield->yres);
    DSWAP(newfield->xreal, newfield->yreal);
    DSWAP(newfield->xoff, newfield->yoff);

    if (rotation == GWY_SIMPLE_ROTATE_COUNTERCLOCKWISE)
        rotate_90(field, newfield, transform_offsets);
    if (rotation == GWY_SIMPLE_ROTATE_CLOCKWISE)
        rotate_270(field, newfield, transform_offsets);

    Field *spriv = field->priv, *dpriv = newfield->priv;
    ASSIGN_UNITS(dpriv->unit_xy, spriv->unit_xy);
    ASSIGN_UNITS(dpriv->unit_z, spriv->unit_z);
    ASSIGN(dpriv->cache, spriv->cache, GWY_FIELD_CACHE_SIZE);
    dpriv->cached = spriv->cached;

    return newfield;
}

#define __LIBGWY_FIELD_TRANSFORM_C__
#include "libgwy/libgwy-aliases.c"

/**
 * SECTION: field-transform
 * @title: GwyField transformations
 * @short_description: Geometrical transformations of fields
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
