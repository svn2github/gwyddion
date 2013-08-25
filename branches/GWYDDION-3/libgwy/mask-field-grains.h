/*
 *  $Id$
 *  Copyright (C) 2011-2013 David Nečas (Yeti).
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

#ifndef __LIBGWY_MASK_FIELD_GRAINS_H__
#define __LIBGWY_MASK_FIELD_GRAINS_H__

#include <libgwy/math.h>
#include <libgwy/field.h>
#include <libgwy/mask-field.h>

G_BEGIN_DECLS

guint               gwy_mask_field_n_grains            (const GwyMaskField *field);
const guint*        gwy_mask_field_grain_numbers       (const GwyMaskField *field);
const guint*        gwy_mask_field_distance_transform  (const GwyMaskField *field);
const guint*        gwy_mask_field_grain_sizes         (const GwyMaskField *field);
const GwyFieldPart* gwy_mask_field_grain_bounding_boxes(const GwyMaskField *field);
const GwyXY*        gwy_mask_field_grain_positions     (const GwyMaskField *field);
void                gwy_mask_field_remove_grain        (GwyMaskField *field,
                                                        guint grain_id);
void                gwy_mask_field_remove_grains       (GwyMaskField *field,
                                                        const guint *grain_ids,
                                                        guint nids);
void                gwy_mask_field_extract_grain       (const GwyMaskField *field,
                                                        GwyMaskField *target,
                                                        guint grain_id,
                                                        guint border_width);
void                gwy_mask_field_mark_extrema        (GwyMaskField *field,
                                                        const GwyField *heightfield,
                                                        gboolean maxima);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
