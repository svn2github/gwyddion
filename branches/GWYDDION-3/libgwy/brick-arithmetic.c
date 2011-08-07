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
#include "libgwy/math-internal.h"
#include "libgwy/object-internal.h"
#include "libgwy/line-internal.h"
#include "libgwy/field-internal.h"
#include "libgwy/brick-internal.h"

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
gwy_brick_is_incompatible(const GwyBrick *brick1,
                          const GwyBrick *brick2,
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
        if (!(fabs(log(xreal1/xreal2)) <= COMPAT_EPSILON))
            result |= GWY_BRICK_COMPATIBLE_XREAL;
    }
    if (check & GWY_BRICK_COMPATIBLE_YREAL) {
        if (!(fabs(log(yreal1/yreal2)) <= COMPAT_EPSILON))
            result |= GWY_BRICK_COMPATIBLE_YREAL;
    }
    if (check & GWY_BRICK_COMPATIBLE_ZREAL) {
        if (!(fabs(log(zreal1/zreal2)) <= COMPAT_EPSILON))
            result |= GWY_BRICK_COMPATIBLE_ZREAL;
    }

    /* Measure */
    if (check & GWY_BRICK_COMPATIBLE_DX) {
        if (!(fabs(log(xreal1/xres1*xres2/xreal2)) <= COMPAT_EPSILON))
            result |= GWY_BRICK_COMPATIBLE_DX;
    }
    if (check & GWY_BRICK_COMPATIBLE_DY) {
        if (!(fabs(log(yreal1/yres1*yres2/yreal2)) <= COMPAT_EPSILON))
            result |= GWY_BRICK_COMPATIBLE_DY;
    }
    if (check & GWY_BRICK_COMPATIBLE_DZ) {
        if (!(fabs(log(zreal1/zres1*zres2/zreal2)) <= COMPAT_EPSILON))
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
 * gwy_brick_is_incompatible_with_field:
 * @brick: A data brick.
 * @field: A data field.
 * @check: Properties to check for compatibility.  The properties are from
 *         the #GwyFieldCompatibilityFlags enum as the field cannot be
 *         incompatible in dimensions it does not have.
 *
 * Checks whether a brick and a field are compatible.
 *
 * The field is checked for compatibility with planes in the brick.  This means
 * X- and Y-quantities in both objects correspond, while Z of @field
 * corresponds to W of @brick.  Z of @brick does not correspond to anything
 * in @field and is not compared.
 *
 * Returns: Zero if all tested properties are compatible.  Flags corresponding
 *          to failed tests if fields are not compatible.
 **/
GwyFieldCompatibilityFlags
gwy_brick_is_incompatible_with_field(const GwyBrick *brick,
                                     const GwyField *field,
                                     GwyFieldCompatibilityFlags check)
{
    g_return_val_if_fail(GWY_IS_BRICK(brick), check);
    g_return_val_if_fail(GWY_IS_FIELD(field), check);

    guint xres1 = brick->xres;
    guint xres2 = field->xres;
    guint yres1 = brick->yres;
    guint yres2 = field->yres;
    gdouble xreal1 = brick->xreal;
    gdouble xreal2 = field->xreal;
    gdouble yreal1 = brick->yreal;
    gdouble yreal2 = field->yreal;
    GwyFieldCompatibilityFlags result = 0;

    /* Resolution */
    if (check & GWY_FIELD_COMPATIBLE_XRES) {
        if (xres1 != xres2)
            result |= GWY_FIELD_COMPATIBLE_XRES;
    }
    if (check & GWY_FIELD_COMPATIBLE_YRES) {
        if (yres1 != yres2)
            result |= GWY_FIELD_COMPATIBLE_YRES;
    }

    /* Real size */
    /* Keeps the conditions for real numbers in negative form to catch NaNs and
     * odd values as incompatible. */
    if (check & GWY_FIELD_COMPATIBLE_XREAL) {
        if (!(fabs(log(xreal1/xreal2)) <= COMPAT_EPSILON))
            result |= GWY_FIELD_COMPATIBLE_XREAL;
    }
    if (check & GWY_FIELD_COMPATIBLE_YREAL) {
        if (!(fabs(log(yreal1/yreal2)) <= COMPAT_EPSILON))
            result |= GWY_FIELD_COMPATIBLE_YREAL;
    }

    /* Measure */
    if (check & GWY_FIELD_COMPATIBLE_DX) {
        if (!(fabs(log(xreal1/xres1*xres2/xreal2)) <= COMPAT_EPSILON))
            result |= GWY_FIELD_COMPATIBLE_DX;
    }
    if (check & GWY_FIELD_COMPATIBLE_DY) {
        if (!(fabs(log(yreal1/yres1*yres2/yreal2)) <= COMPAT_EPSILON))
            result |= GWY_FIELD_COMPATIBLE_DY;
    }

    /* Lateral units */
    if (check & GWY_FIELD_COMPATIBLE_LATERAL) {
        /* This can cause instantiation of field units as a side effect */
        GwyUnit *unit1 = gwy_brick_get_unit_xy(brick);
        GwyUnit *unit2 = gwy_field_get_unit_xy(field);
        if (!gwy_unit_equal(unit1, unit2))
            result |= GWY_FIELD_COMPATIBLE_LATERAL;
    }

    /* Value units */
    if (check & GWY_FIELD_COMPATIBLE_VALUE) {
        /* This can cause instantiation of field units as a side effect */
        GwyUnit *unit1 = gwy_brick_get_unit_w(brick);
        GwyUnit *unit2 = gwy_field_get_unit_z(field);
        if (!gwy_unit_equal(unit1, unit2))
            result |= GWY_FIELD_COMPATIBLE_VALUE;
    }

    return result;
}

/**
 * gwy_brick_is_incompatible_with_line:
 * @brick: A data brick.
 * @line: A data line.
 * @check: Properties to check for compatibility.  The properties are from
 *         the #GwyLineCompatibilityFlags enum as the line cannot be
 *         incompatible in dimensions it does not have.
 *
 * Checks whether a brick and a line are compatible.
 *
 * The line is checked for compatibility with lines along Z in the brick.  This
 * means X of @line corresponds to Z of @brick while Y of @line corresponds to
 * W of @brick.  X and Y of @brick do not correspond to anything in @line and
 * are not compared.
 *
 * Returns: Zero if all tested properties are compatible.  Flags corresponding
 *          to failed tests if lines are not compatible.
 **/
GwyLineCompatibilityFlags
gwy_brick_is_incompatible_with_line(const GwyBrick *brick,
                                    const GwyLine *line,
                                    GwyLineCompatibilityFlags check)
{
    g_return_val_if_fail(GWY_IS_BRICK(brick), check);
    g_return_val_if_fail(GWY_IS_LINE(line), check);

    guint res1 = brick->zres;
    guint res2 = line->res;
    gdouble real1 = brick->zreal;
    gdouble real2 = line->real;
    GwyLineCompatibilityFlags result = 0;

    /* Resolution */
    if (check & GWY_LINE_COMPATIBLE_RES) {
        if (res1 != res2)
            result |= GWY_LINE_COMPATIBLE_RES;
    }

    /* Real size */
    /* Keeps the conditions for real numbers in negative form to catch NaNs and
     * odd values as incompatible. */
    if (check & GWY_LINE_COMPATIBLE_REAL) {
        if (!(fabs(log(real1/real2)) <= COMPAT_EPSILON))
            result |= GWY_LINE_COMPATIBLE_REAL;
    }

    /* Measure */
    if (check & GWY_LINE_COMPATIBLE_DX) {
        if (!(fabs(log(real1/res1*res2/real2)) <= COMPAT_EPSILON))
            result |= GWY_LINE_COMPATIBLE_DX;
    }

    /* Lateral units */
    if (check & GWY_LINE_COMPATIBLE_LATERAL) {
        /* This can cause instantiation of line units as a side effect */
        GwyUnit *unit1 = gwy_brick_get_unit_z(brick);
        GwyUnit *unit2 = gwy_line_get_unit_x(line);
        if (!gwy_unit_equal(unit1, unit2))
            result |= GWY_LINE_COMPATIBLE_LATERAL;
    }

    /* Value units */
    if (check & GWY_LINE_COMPATIBLE_VALUE) {
        /* This can cause instantiation of line units as a side effect */
        GwyUnit *unit1 = gwy_brick_get_unit_w(brick);
        GwyUnit *unit2 = gwy_line_get_unit_y(line);
        if (!gwy_unit_equal(unit1, unit2))
            result |= GWY_LINE_COMPATIBLE_VALUE;
    }

    return result;
}

/**
 * gwy_brick_extract_plane:
 * @brick: A data brick.
 * @target: A two-dimensional data field where the result will be placed.
 *          Its dimensions may match either @brick planes or @fpart.  In the
 *          former case the placement of result is determined by @fpart; in the
 *          latter case the result fills the entire @target.
 * @fpart: (allow-none):
 *         Area in the @brick plane to extract to the field.  Passing %NULL
 *         extracts the full plane.
 * @level: Level index in @brick.
 * @keep_offsets: %TRUE to set the X and Y offsets of the field
 *                using @fpart and @brick offsets.  %FALSE to set offsets
 *                of the field to zeroes.
 *
 * Extracts a brick plane to a field.
 **/
void
gwy_brick_extract_plane(const GwyBrick *brick,
                        GwyField *target,
                        const GwyFieldPart *fpart,
                        guint level,
                        gboolean keep_offsets)
{
    guint fcol, frow, fwidth, fheight, bcol, brow, bwidth, bheight;
    if (!_gwy_brick_check_plane_part(brick, fpart,
                                     &bcol, &brow, level, &bwidth, &bheight)
        || !gwy_field_check_target_part(target, fpart,
                                        brick->xres, brick->yres,
                                        &fcol, &frow, &fwidth, &fheight))
        return;

    // Can happen with NULL @fpart and incompatible objects.
    g_return_if_fail(fwidth == bwidth && fheight == bheight);

    const gdouble *bbase = brick->data + (level*brick->yres + brow)*brick->xres
                           + bcol;
    gdouble *fbase = target->data + frow*target->xres + fcol;

    if (bwidth == brick->xres && fwidth == target->xres) {
        g_assert(bcol == 0 && fcol == 0);
        gwy_assign(fbase, bbase, fwidth*fheight);
    }
    else {
        for (guint i = 0; i < fheight; i++)
            gwy_assign(fbase + i*target->xres, bbase + i*brick->xres, fwidth);
    }

    gwy_field_set_xreal(target, target->xres*gwy_brick_dx(brick));
    gwy_field_set_yreal(target, target->yres*gwy_brick_dy(brick));
    if (keep_offsets) {
        // XXX: unsigned arithmetic would break if bcol < fcol, brow < frow.
        gwy_field_set_xoffset(target,
                              brick->xoff + (bcol - fcol)*gwy_brick_dx(brick));
        gwy_field_set_yoffset(target,
                              brick->yoff + (brow - frow)*gwy_brick_dy(brick));
    }
    else {
        gwy_field_set_xoffset(target, 0.0);
        gwy_field_set_yoffset(target, 0.0);
    }
    ASSIGN_UNITS(target->priv->unit_xy, brick->priv->unit_xy);
    ASSIGN_UNITS(target->priv->unit_z, brick->priv->unit_w);
    gwy_field_invalidate(target);
}

/**
 * gwy_brick_extract_line:
 * @brick: A data brick.
 * @target: A one-dimensional data line where the result will be placed.
 *          Its dimensions may match either @brick lines or @lpart.  In the
 *          former case the placement of result is determined by @lpart; in the
 *          latter case the result fills the entire @target.
 * @lpart: (allow-none):
 *         Part of the @brick line to extract to the line.  Passing %NULL
 *         extracts the full line.
 * @col: Column index in @brick.
 * @row: Row index in @brick.
 * @keep_offsets: %TRUE to set the X offset of the line using @lpart and @brick
 *                offsets.  %FALSE to set offset of the line to zero.
 *
 * Extracts a vertical profile in a brick to a line.
 *
 * Since the brick is organised in memory by planes this method has to access
 * memory in a scattered manner.  Hence it is suitable for the extraction of
 * a single line, however, if you process many lines at once it is better to
 * process the data by plane.
 **/
void
gwy_brick_extract_line(const GwyBrick *brick,
                       GwyLine *target,
                       const GwyLinePart *lpart,
                       guint col, guint row,
                       gboolean keep_offsets)
{
    guint pos, len, level, depth;
    if (!_gwy_brick_check_line_part(brick, lpart, col, row, &level, &depth)
        || !_gwy_line_check_target_part(target, lpart, brick->zres, &pos, &len))
        return;

    // Can happen with NULL @lpart and incompatible objects.
    g_return_if_fail(len == depth);

    const gdouble *bbase = brick->data + (level*brick->yres + row)*brick->xres
                           + col;
    gdouble *lbase = target->data + pos;
    guint stride = brick->xres * brick->yres;

    for (guint i = 0; i < len; i++)
        lbase[i] = bbase[i*stride];

    gwy_line_set_real(target, target->res*gwy_brick_dz(brick));
    if (keep_offsets) {
        // XXX: unsigned arithmetic would break if level < pos.
        gwy_line_set_offset(target,
                            brick->zoff + (level - pos)*gwy_brick_dz(brick));
    }
    else {
        gwy_line_set_offset(target, 0.0);
    }
    ASSIGN_UNITS(target->priv->unit_x, brick->priv->unit_z);
    ASSIGN_UNITS(target->priv->unit_y, brick->priv->unit_w);
    //gwy_line_invalidate(target);
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
 * @GWY_BRICK_COMPATIBLE_UNITS: All units.
 * @GWY_BRICK_COMPATIBLE_ALL: All defined properties.
 *
 * Brick properties that can be checked for compatibility.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
