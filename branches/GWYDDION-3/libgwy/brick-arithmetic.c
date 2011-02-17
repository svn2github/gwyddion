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

#include <string.h>
#include <math.h>
#include "libgwy/macros.h"
#include "libgwy/brick.h"
#include "libgwy/brick-arithmetic.h"

// For compatibility checks.
#define EPSILON 1e-6

/**
 * gwy_brick_is_incompatible:
 * @brick1: A data brick.
 * @brick2: Another data brick.
 * @check: Properties to check for compatibility.
 *
 * Checks whether two bricks are compatible.
 *
 * Returns: Zero if all tested properties are compatible.  Flags corresponding
 *          to failed tests if bricks are not compatible.
 **/
GwyBrickCompatibilityFlags
gwy_brick_is_incompatible(GwyBrick *brick1,
                          GwyBrick *brick2,
                          GwyBrickCompatibilityFlags check)
{

    g_return_val_if_fail(GWY_IS_BRICK(brick1), check);
    g_return_val_if_fail(GWY_IS_BRICK(brick2), check);

    guint xres1 = brick1->xres;
    guint xres2 = brick2->xres;
    guint yres1 = brick1->yres;
    guint yres2 = brick2->yres;
    guint zres1 = brick1->zres;
    guint zres2 = brick2->zres;
    gdouble xreal1 = brick1->xreal;
    gdouble xreal2 = brick2->xreal;
    gdouble yreal1 = brick1->yreal;
    gdouble yreal2 = brick2->yreal;
    gdouble zreal1 = brick1->zreal;
    gdouble zreal2 = brick2->zreal;
    GwyBrickCompatibilityFlags result = 0;

    /* Resolution */
    if (check & GWY_BRICK_COMPATIBLE_XRES) {
        if (xres1 != xres2)
            result |= GWY_BRICK_COMPATIBLE_XRES;
    }
    if (check & GWY_BRICK_COMPATIBLE_YRES) {
        if (yres1 != yres2)
            result |= GWY_BRICK_COMPATIBLE_YRES;
    }
    if (check & GWY_BRICK_COMPATIBLE_ZRES) {
        if (zres1 != zres2)
            result |= GWY_BRICK_COMPATIBLE_ZRES;
    }

    /* Real size */
    /* Keeps the conditions for real numbers in negative form to catch NaNs and
     * odd values as incompatible. */
    if (check & GWY_BRICK_COMPATIBLE_XREAL) {
        if (!(fabs(log(xreal1/xreal2)) <= EPSILON))
            result |= GWY_BRICK_COMPATIBLE_XREAL;
    }
    if (check & GWY_BRICK_COMPATIBLE_YREAL) {
        if (!(fabs(log(yreal1/yreal2)) <= EPSILON))
            result |= GWY_BRICK_COMPATIBLE_YREAL;
    }
    if (check & GWY_BRICK_COMPATIBLE_ZREAL) {
        if (!(fabs(log(zreal1/zreal2)) <= EPSILON))
            result |= GWY_BRICK_COMPATIBLE_ZREAL;
    }

    /* Measure */
    if (check & GWY_BRICK_COMPATIBLE_DX) {
        if (!(fabs(log(xreal1/xres1*xres2/xreal2)) <= EPSILON))
            result |= GWY_BRICK_COMPATIBLE_DX;
    }
    if (check & GWY_BRICK_COMPATIBLE_DY) {
        if (!(fabs(log(yreal1/yres1*yres2/yreal2)) <= EPSILON))
            result |= GWY_BRICK_COMPATIBLE_DY;
    }
    if (check & GWY_BRICK_COMPATIBLE_DZ) {
        if (!(fabs(log(zreal1/zres1*zres2/zreal2)) <= EPSILON))
            result |= GWY_BRICK_COMPATIBLE_DZ;
    }

    /* Lateral units */
    if (check & GWY_BRICK_COMPATIBLE_LATERAL) {
        /* This can cause instantiation of brick units as a side effect */
        GwyUnit *unit1 = gwy_brick_get_unit_xy(brick1);
        GwyUnit *unit2 = gwy_brick_get_unit_xy(brick2);
        if (!gwy_unit_equal(unit1, unit2))
            result |= GWY_BRICK_COMPATIBLE_LATERAL;
    }

    /* Depth units */
    if (check & GWY_BRICK_COMPATIBLE_DEPTH) {
        /* This can cause instantiation of brick units as a side effect */
        GwyUnit *unit1 = gwy_brick_get_unit_z(brick1);
        GwyUnit *unit2 = gwy_brick_get_unit_z(brick2);
        if (!gwy_unit_equal(unit1, unit2))
            result |= GWY_BRICK_COMPATIBLE_DEPTH;
    }

    /* Value units */
    if (check & GWY_BRICK_COMPATIBLE_VALUE) {
        /* This can cause instantiation of brick units as a side effect */
        GwyUnit *unit1 = gwy_brick_get_unit_w(brick1);
        GwyUnit *unit2 = gwy_brick_get_unit_w(brick2);
        if (!gwy_unit_equal(unit1, unit2))
            result |= GWY_BRICK_COMPATIBLE_VALUE;
    }

    return result;
}

/**
 * gwy_brick_clear_full:
 * @brick: A two-dimensional data brick.
 *
 * Fills an entire brick with zeroes.
 **/
void
gwy_brick_clear_full(GwyBrick *brick)
{
    g_return_if_fail(GWY_IS_BRICK(brick));
    gwy_clear(brick->data, brick->xres*brick->yres*brick->zres);
}

/**
 * SECTION: brick-arithmetic
 * @section_id: GwyBrick-arithmetic
 * @title: GwyBrick arithmetic
 * @short_description: Arithmetic operations with bricks
 **/

/**
 * GwyBrickCompatibilityFlags:
 * @GWY_BRICK_COMPATIBLE_XRES: X-resolution (width).
 * @GWY_BRICK_COMPATIBLE_YRES: Y-resolution (height)
 * @GWY_BRICK_COMPATIBLE_ZRES: Z-resolution (depth)
 * @GWY_BRICK_COMPATIBLE_RES: All resolutions.
 * @GWY_BRICK_COMPATIBLE_XREAL: Physical x-dimension.
 * @GWY_BRICK_COMPATIBLE_YREAL: Physical y-dimension.
 * @GWY_BRICK_COMPATIBLE_ZREAL: Physical z-dimension.
 * @GWY_BRICK_COMPATIBLE_REAL: All physical dimensions.
 * @GWY_BRICK_COMPATIBLE_DX: Pixel size in x-direction.
 * @GWY_BRICK_COMPATIBLE_DY: Pixel size in y-direction.
 * @GWY_BRICK_COMPATIBLE_DXDY: Lateral pixel dimensions.
 * @GWY_BRICK_COMPATIBLE_DZ: Pixel size in z-direction.
 * @GWY_BRICK_COMPATIBLE_DXDYDZ: All pixel dimensions.
 * @GWY_BRICK_COMPATIBLE_LATERAL: Lateral units.
 * @GWY_BRICK_COMPATIBLE_DEPTH: Depth units.
 * @GWY_BRICK_COMPATIBLE_VALUE: Value units.
 * @GWY_BRICK_COMPATIBLE_ALL: All defined properties.
 *
 * Brick properties that can be checked for compatibility.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
