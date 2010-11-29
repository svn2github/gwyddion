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
#include "libgwy/field-arithmetic.h"

/* for compatibility checks */
#define EPSILON 1e-6

/**
 * gwy_field_is_incompatible:
 * @field1: A data field.
 * @field2: Another data field.
 * @check: Properties to check for compatibility.
 *
 * Checks whether two fields are compatible.
 *
 * Returns: Zero if all tested properties are compatible.  Flags corresponding
 *          to failed tests if fields are not compatible.
 **/
GwyFieldCompatibilityFlags
gwy_field_is_incompatible(GwyField *field1,
                          GwyField *field2,
                          GwyFieldCompatibilityFlags check)
{

    g_return_val_if_fail(GWY_IS_FIELD(field1), check);
    g_return_val_if_fail(GWY_IS_FIELD(field2), check);

    guint xres1 = field1->xres;
    guint xres2 = field2->xres;
    guint yres1 = field1->yres;
    guint yres2 = field2->yres;
    gdouble xreal1 = field1->xreal;
    gdouble xreal2 = field2->xreal;
    gdouble yreal1 = field1->yreal;
    gdouble yreal2 = field2->yreal;
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
        if (!(fabs(log(xreal1/xreal2)) <= EPSILON))
            result |= GWY_FIELD_COMPATIBLE_XREAL;
    }
    if (check & GWY_FIELD_COMPATIBLE_YREAL) {
        if (!(fabs(log(yreal1/yreal2)) <= EPSILON))
            result |= GWY_FIELD_COMPATIBLE_YREAL;
    }

    /* Measure */
    if (check & GWY_FIELD_COMPATIBLE_DX) {
        if (!(fabs(log(xreal1/xres1*xres2/xreal2))))
            result |= GWY_FIELD_COMPATIBLE_DX;
    }
    if (check & GWY_FIELD_COMPATIBLE_DY) {
        if (!(fabs(log(yreal1/yres1*yres2/yreal2))))
            result |= GWY_FIELD_COMPATIBLE_DY;
    }

    /* Lateral units */
    if (check & GWY_FIELD_COMPATIBLE_LATERAL) {
        /* This can cause instantiation of field units as a side effect */
        GwyUnit *unit1 = gwy_field_get_unit_xy(field1);
        GwyUnit *unit2 = gwy_field_get_unit_xy(field2);
        if (!gwy_unit_equal(unit1, unit2))
            result |= GWY_FIELD_COMPATIBLE_LATERAL;
    }

    /* Value units */
    if (check & GWY_FIELD_COMPATIBLE_VALUE) {
        /* This can cause instantiation of field units as a side effect */
        GwyUnit *unit1 = gwy_field_get_unit_z(field1);
        GwyUnit *unit2 = gwy_field_get_unit_z(field2);
        if (!gwy_unit_equal(unit1, unit2))
            result |= GWY_FIELD_COMPATIBLE_VALUE;
    }

    return result;
}


/**
 * SECTION: field-arithmetic
 * @section_id: GwyField-arithmetic
 * @title: GwyField arithmetic
 * @short_description: Arithmetic operations with fields
 **/

/**
 * GwyFieldCompatibilityFlags:
 * @GWY_FIELD_COMPATIBLE_XRES: X-resolution (width).
 * @GWY_FIELD_COMPATIBLE_YRES: Y-resolution (height)
 * @GWY_FIELD_COMPATIBLE_RES: Both resolutions.
 * @GWY_FIELD_COMPATIBLE_XREAL: Physical x-dimension.
 * @GWY_FIELD_COMPATIBLE_YREAL: Physical y-dimension.
 * @GWY_FIELD_COMPATIBLE_REAL: Both physical dimensions.
 * @GWY_FIELD_COMPATIBLE_DX: Pixel size in x-direction.
 * @GWY_FIELD_COMPATIBLE_DY: Pixel size in y-direction.
 * @GWY_FIELD_COMPATIBLE_DXDY: Pixel dimensions.
 * @GWY_FIELD_COMPATIBLE_LATERAL: Lateral units.
 * @GWY_FIELD_COMPATIBLE_VALUE: Value units.
 * @GWY_FIELD_COMPATIBLE_ALL: All defined properties.
 *
 * Field properties that can be checked for compatibility.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
