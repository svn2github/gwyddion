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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

/*< private_header >*/

#ifndef __GWYPROCESS_INTERNAL_H__
#define __GWYPROCESS_INTERNAL_H__

#include <libprocess/gwyprocessenums.h>
#include <libprocess/datafield.h>

G_BEGIN_DECLS

/* Must include fftw3.h to get it actually defined */
#define _GWY_FFTW_PATIENCE FFTW_ESTIMATE

/* Cache operations */
#define CVAL(datafield, b)  ((datafield)->cache[GWY_DATA_FIELD_CACHE_##b])
#define CBIT(b)             (1 << GWY_DATA_FIELD_CACHE_##b)
#define CTEST(datafield, b) ((datafield)->cached & CBIT(b))

typedef struct {
    guint col;
    guint row;
    guint width;
    guint height;
} GwyFieldPart;

/* XXX: Maybe this protoype is too detailed.  Including the mask and area
 * arguments explicitly will permit certain optimistations, namely limiting
 * the working area. */
typedef void (*GwyDataFieldScalarFunc)(GwyDataField *field,
                                       GwyDataField *mask,
                                       GwyMaskingType mode,
                                       const GwyFieldPart *fpart,
                                       gpointer params,
                                       gdouble *results);

G_GNUC_INTERNAL
void _gwy_cdline_class_setup_presets(void);

G_GNUC_INTERNAL
void _gwy_grain_value_class_setup_presets(void);

G_GNUC_INTERNAL
void _gwy_calibration_class_setup_presets(void);

G_END_DECLS

#endif /* __GWYPROCESS_INTERNAL_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

