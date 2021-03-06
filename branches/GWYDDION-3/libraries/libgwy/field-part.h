/*
 *  $Id$
 *  Copyright (C) 2010-2011 David Nečas (Yeti).
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

#ifndef __LIBGWY_FIELD_PART_H__
#define __LIBGWY_FIELD_PART_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct {
    guint col;
    guint row;
    guint width;
    guint height;
} GwyFieldPart;

#define GWY_TYPE_FIELD_PART (gwy_field_part_get_type())

GType         gwy_field_part_get_type (void)                           G_GNUC_CONST;
GwyFieldPart* gwy_field_part_copy     (const GwyFieldPart *fpart)      G_GNUC_MALLOC;
void          gwy_field_part_free     (GwyFieldPart *fpart);
gboolean      gwy_field_part_intersect(GwyFieldPart *fpart,
                                       const GwyFieldPart *otherpart);
void          gwy_field_part_union    (GwyFieldPart *fpart,
                                       const GwyFieldPart *otherpart);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
