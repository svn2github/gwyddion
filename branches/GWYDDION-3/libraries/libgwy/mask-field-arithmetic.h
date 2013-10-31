/*
 *  $Id$
 *  Copyright (C) 2009-2011 David Nečas (Yeti).
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

#ifndef __LIBGWY_MASK_FIELD_ARITHMETIC_H__
#define __LIBGWY_MASK_FIELD_ARITHMETIC_H__

#include <libgwy/mask-field.h>

G_BEGIN_DECLS

typedef enum {
    GWY_LOGICAL_ZERO,
    GWY_LOGICAL_AND,
    GWY_LOGICAL_NIMPL,
    GWY_LOGICAL_A,
    GWY_LOGICAL_NCIMPL,
    GWY_LOGICAL_B,
    GWY_LOGICAL_XOR,
    GWY_LOGICAL_OR,
    GWY_LOGICAL_NOR,
    GWY_LOGICAL_NXOR,
    GWY_LOGICAL_NB,
    GWY_LOGICAL_CIMPL,
    GWY_LOGICAL_NA,
    GWY_LOGICAL_IMPL,
    GWY_LOGICAL_NAND,
    GWY_LOGICAL_ONE,
} GwyLogicalOperator;

void         gwy_mask_field_fill         (GwyMaskField *field,
                                          const GwyFieldPart *fpart,
                                          gboolean value);
void         gwy_mask_field_fill_ellipse (GwyMaskField *field,
                                          const GwyFieldPart *fpart,
                                          gboolean entire_rectangle,
                                          gboolean value);
void         gwy_mask_field_logical      (GwyMaskField *field,
                                          const GwyMaskField *operand,
                                          const GwyMaskField *mask,
                                          GwyLogicalOperator op);
void         gwy_mask_field_part_logical (GwyMaskField *field,
                                          const GwyFieldPart *fpart,
                                          const GwyMaskField *operand,
                                          guint opcol,
                                          guint oprow,
                                          GwyLogicalOperator op);
void         gwy_mask_field_shrink       (GwyMaskField *field,
                                          gboolean from_borders);
void         gwy_mask_field_grow         (GwyMaskField *field,
                                          gboolean separate_grains);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
