/*
 *  $Id$
 *  Copyright (C) 2009,2011-2013 David Nečas (Yeti).
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

#ifndef __LIBGWY_FIELD_ARITHMETIC_H__
#define __LIBGWY_FIELD_ARITHMETIC_H__

#include <libgwy/math.h>
#include <libgwy/field.h>
#include <libgwy/mask-field.h>

G_BEGIN_DECLS

typedef enum {
    GWY_FIELD_COMPAT_XRES    = 1 << 0,
    GWY_FIELD_COMPAT_YRES    = 1 << 1,
    GWY_FIELD_COMPAT_RES     = GWY_FIELD_COMPAT_XRES | GWY_FIELD_COMPAT_YRES,
    GWY_FIELD_COMPAT_XREAL   = 1 << 2,
    GWY_FIELD_COMPAT_YREAL   = 1 << 3,
    GWY_FIELD_COMPAT_REAL    = GWY_FIELD_COMPAT_XREAL | GWY_FIELD_COMPAT_YREAL,
    GWY_FIELD_COMPAT_DX      = 1 << 4,
    GWY_FIELD_COMPAT_DY      = 1 << 5,
    GWY_FIELD_COMPAT_DXDY    = GWY_FIELD_COMPAT_DX | GWY_FIELD_COMPAT_DY,
    GWY_FIELD_COMPAT_X       = 1 << 6,
    GWY_FIELD_COMPAT_Y       = 1 << 7,
    GWY_FIELD_COMPAT_LATERAL = GWY_FIELD_COMPAT_X | GWY_FIELD_COMPAT_Y,
    GWY_FIELD_COMPAT_VALUE   = 1 << 8,
    GWY_FIELD_COMPAT_UNITS   = GWY_FIELD_COMPAT_LATERAL | GWY_FIELD_COMPAT_VALUE,
    GWY_FIELD_COMPAT_ALL     = 0x01ffu
} GwyFieldCompatFlags;

typedef enum {
    GWY_NORMALIZE_MEAN        = 1 << 0,
    GWY_NORMALIZE_RMS         = 1 << 1,
    GWY_NORMALIZE_ENTIRE_DATA = 1 << 2
} GwyNormalizeFlags;

typedef enum {
    GWY_SCULPTING_UPWARD,
    GWY_SCULPTING_DOWNWARD,
} GwySculpting;

GwyFieldCompatFlags gwy_field_is_incompatible(const GwyField *field1,
                                              const GwyField *field2,
                                              GwyFieldCompatFlags check);

void     gwy_field_clear        (GwyField *field,
                                 const GwyFieldPart *fpart,
                                 const GwyMaskField *mask,
                                 GwyMasking masking);
void     gwy_field_clear_full   (GwyField *field);
void     gwy_field_fill         (GwyField *field,
                                 const GwyFieldPart *fpart,
                                 const GwyMaskField *mask,
                                 GwyMasking masking,
                                 gdouble value);
void     gwy_field_fill_full    (GwyField *field,
                                 gdouble value);
void     gwy_field_add          (GwyField *field,
                                 const GwyFieldPart *fpart,
                                 const GwyMaskField *mask,
                                 GwyMasking masking,
                                 gdouble shift);
void     gwy_field_add_full     (GwyField *field,
                                 gdouble shift);
void     gwy_field_multiply     (GwyField *field,
                                 const GwyFieldPart *fpart,
                                 const GwyMaskField *mask,
                                 GwyMasking masking,
                                 gdouble factor);
void     gwy_field_multiply_full(GwyField *field,
                                 gdouble factor);
void     gwy_field_addmul       (GwyField *field,
                                 const GwyFieldPart *fpart,
                                 const GwyMaskField *mask,
                                 GwyMasking masking,
                                 gdouble factor,
                                 gdouble shift);
void     gwy_field_addmul_full  (GwyField *field,
                                 gdouble shift,
                                 gdouble factor);
guint    gwy_field_clamp        (GwyField *field,
                                 const GwyFieldPart *fpart,
                                 gdouble lower,
                                 gdouble upper);
gboolean gwy_field_normalize    (GwyField *field,
                                 const GwyFieldPart *fpart,
                                 const GwyMaskField *mask,
                                 GwyMasking masking,
                                 gdouble mean,
                                 gdouble rms,
                                 GwyNormalizeFlags flags);
void     gwy_field_sqrt         (GwyField *field,
                                 const GwyFieldPart *fpart,
                                 const GwyMaskField *mask,
                                 GwyMasking masking);
void     gwy_field_sqrt_full    (GwyField *field);
void     gwy_field_apply_func   (GwyField *field,
                                 const GwyFieldPart *fpart,
                                 const GwyMaskField *mask,
                                 GwyMasking masking,
                                 GwyRealFunc function,
                                 gpointer user_data);
void     gwy_field_add_field    (const GwyField *src,
                                 const GwyFieldPart *srcpart,
                                 GwyField *dest,
                                 guint destcol,
                                 guint destrow,
                                 gdouble factor);
void     gwy_field_min_field    (const GwyField *src,
                                 const GwyFieldPart *srcpart,
                                 GwyField *dest,
                                 guint destcol,
                                 guint destrow);
void     gwy_field_max_field    (const GwyField *src,
                                 const GwyFieldPart *srcpart,
                                 GwyField *dest,
                                 guint destcol,
                                 guint destrow);
void     gwy_field_sculpt       (const GwyField *src,
                                 const GwyFieldPart *srcpart,
                                 GwyField *dest,
                                 gint destcol,
                                 gint destrow,
                                 GwySculpting method,
                                 gboolean periodic);
void     gwy_field_hypot_field  (GwyField *field,
                                 const GwyField *operand1,
                                 const GwyField *operand2);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
