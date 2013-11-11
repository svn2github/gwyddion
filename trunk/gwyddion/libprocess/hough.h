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

#ifndef __GWY_PROCESS_HOUGH_H__
#define __GWY_PROCESS_HOUGH_H__

#include <libprocess/datafield.h>

G_BEGIN_DECLS

void gwy_data_field_hough_line(GwyDataField *dfield,
                               GwyDataField *x_gradient,
                               GwyDataField *y_gradient,
                               GwyDataField *result,
                               gint hwidth,
                               gboolean overlapping);

void gwy_data_field_hough_circle(GwyDataField *dfield,
                                 GwyDataField *x_gradient,
                                 GwyDataField *y_gradient,
                                 GwyDataField *result,
                                 gdouble radius);


void gwy_data_field_hough_line_strenghten(GwyDataField *dfield,
                               GwyDataField *x_gradient,
                               GwyDataField *y_gradient,
                               gint hwidth,
                               gdouble threshold);

void gwy_data_field_hough_circle_strenghten(GwyDataField *dfield,
                               GwyDataField *x_gradient,
                               GwyDataField *y_gradient,
                               gdouble radius,
                               gdouble threshold);


gint gwy_data_field_get_local_maxima_list(GwyDataField *dfield,
                                          gdouble *xdata,
                                          gdouble *ydata,
                                          gdouble *zdata,
                                          gint ndata,
                                          gint skip,
                                          gdouble threshold,
                                          gboolean subpixel);

void gwy_data_field_hough_polar_line_to_datafield(GwyDataField *dfield,
                                                  gdouble rho,
                                                  gdouble theta,
                                                  gint *px1,
                                                  gint *px2,
                                                  gint *py1,
                                                  gint *py2);

void gwy_data_field_hough_datafield_line_to_polar(gint px1,
                                                  gint px2,
                                                  gint py1,
                                                  gint py2,
                                                  gdouble *rho,
                                                  gdouble *theta);


G_END_DECLS

#endif /* __GWY_PROCESS_INTTRANS_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
