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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#ifndef __GWY_PROCESS_LEVEL_H__
#define __GWY_PROCESS_LEVEL_H__

#include <libprocess/datafield.h>

G_BEGIN_DECLS

void gwy_data_field_area_fit_plane        (GwyDataField *data_field,
                                           gint col,
                                           gint row,
                                           gint width,
                                           gint height,
                                           gdouble *pa,
                                           gdouble *pbx,
                                           gdouble *pby);

void gwy_data_field_fit_plane             (GwyDataField *data_field,
                                           GwyDataField *exclusion_mask,
                                           gdouble *pa,
                                           gdouble *pbx,
                                           gdouble *pby);

void gwy_data_field_plane_level           (GwyDataField *data_field,
                                           gdouble a,
                                           gdouble bx,
                                           gdouble by);

void gwy_data_field_plane_rotate          (GwyDataField *data_field,
                                           gdouble xangle,
                                           gdouble yangle,
                                           GwyInterpolationType interpolation);

gdouble* gwy_data_field_fit_polynom       (GwyDataField *data_field,
                                           gint col_degree,
                                           gint row_degree,
                                           gdouble *coeffs);
gdouble* gwy_data_field_area_fit_polynom  (GwyDataField *data_field,
                                           gint col,
                                           gint row,
                                           gint width,
                                           gint height,
                                           gint col_degree,
                                           gint row_degree,
                                           gdouble *coeffs);

void gwy_data_field_subtract_polynom      (GwyDataField *data_field,
                                           gint col_degree,
                                           gint row_degree,
                                           const gdouble *coeffs);
void gwy_data_field_area_subtract_polynom (GwyDataField *data_field,
                                           gint col,
                                           gint row,
                                           gint width,
                                           gint height,
                                           gint col_degree,
                                           gint row_degree,
                                           const gdouble *coeffs);

gdouble* gwy_data_field_fit_legendre      (GwyDataField *data_field,
                                           gint col_degree,
                                           gint row_degree,
                                           gdouble *coeffs);
gdouble* gwy_data_field_area_fit_legendre (GwyDataField *data_field,
                                           gint col,
                                           gint row,
                                           gint width,
                                           gint height,
                                           gint col_degree,
                                           gint row_degree,
                                           gdouble *coeffs);

void gwy_data_field_subtract_legendre     (GwyDataField *data_field,
                                           gint col_degree,
                                           gint row_degree,
                                           const gdouble *coeffs);
void gwy_data_field_area_subtract_legendre(GwyDataField *data_field,
                                           gint col,
                                           gint row,
                                           gint width,
                                           gint height,
                                           gint col_degree,
                                           gint row_degree,
                                           const gdouble *coeffs);

gdouble* gwy_data_field_fit_poly_max      (GwyDataField *data_field,
                                           gint max_degree,
                                           gdouble *coeffs);
gdouble* gwy_data_field_area_fit_poly_max (GwyDataField *data_field,
                                           gint col, gint row,
                                           gint width, gint height,
                                           gint max_degree,
                                           gdouble *coeffs);

void gwy_data_field_subtract_poly_max     (GwyDataField *data_field,
                                           gint max_degree,
                                           const gdouble *coeffs);
void gwy_data_field_area_subtract_poly_max(GwyDataField *data_field,
                                           gint col, gint row,
                                           gint width, gint height,
                                           gint max_degree,
                                           const gdouble *coeffs);

GwyDataField** gwy_data_field_area_fit_local_planes(GwyDataField *data_field,
                                                    gint size,
                                                    gint col, gint row,
                                                    gint width, gint height,
                                                    gint nresults,
                                                    const GwyPlaneFitQuantity *types,
                                                    GwyDataField **results);
GwyDataField* gwy_data_field_area_local_plane_quantity(GwyDataField *data_field,
                                                       gint size,
                                                       gint col, gint row,
                                                       gint width, gint height,
                                                       GwyPlaneFitQuantity type,
                                                       GwyDataField *result);
GwyDataField** gwy_data_field_fit_local_planes(GwyDataField *data_field,
                                               gint size,
                                               gint nresults,
                                               const GwyPlaneFitQuantity *types,
                                               GwyDataField **results);
GwyDataField* gwy_data_field_local_plane_quantity(GwyDataField *data_field,
                                                  gint size,
                                                  GwyPlaneFitQuantity type,
                                                  GwyDataField *result);

G_END_DECLS

#endif /* __GWY_PROCESS_LEVEL_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

