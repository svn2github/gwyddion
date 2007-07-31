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

#ifndef __GWY_PROCESS_LINESTATS_H__
#define __GWY_PROCESS_LINESTATS_H__

#include <libprocess/dataline.h>

G_BEGIN_DECLS

gdouble gwy_data_line_get_max           (GwyDataLine *data_line);
gdouble gwy_data_line_get_min           (GwyDataLine *data_line);
gdouble gwy_data_line_get_avg           (GwyDataLine *data_line);
gdouble gwy_data_line_get_rms           (GwyDataLine *data_line);
gdouble gwy_data_line_get_tan_beta0     (GwyDataLine *data_line);
gdouble gwy_data_line_get_sum           (GwyDataLine *data_line);
gdouble gwy_data_line_part_get_max      (GwyDataLine *data_line,
                                         gint from,
                                         gint to);
gdouble gwy_data_line_part_get_min      (GwyDataLine *data_line,
                                         gint from,
                                         gint to);
gdouble gwy_data_line_part_get_avg      (GwyDataLine *data_line,
                                         gint from,
                                         gint to);
gdouble gwy_data_line_part_get_rms      (GwyDataLine *data_line,
                                         gint from,
                                         gint to);
gdouble gwy_data_line_part_get_tan_beta0(GwyDataLine *data_line,
                                         gint from,
                                         gint to);
gdouble gwy_data_line_part_get_sum      (GwyDataLine *data_line,
                                         gint from,
                                         gint to);
gdouble gwy_data_line_get_modus         (GwyDataLine *data_line,
                                         gint histogram_steps);
gdouble gwy_data_line_part_get_modus    (GwyDataLine *data_line,
                                         gint from,
                                         gint to,
                                         gint histogram_steps);
gdouble gwy_data_line_get_median        (GwyDataLine *data_line);
gdouble gwy_data_line_part_get_median   (GwyDataLine *data_line,
                                         gint from,
                                         gint to);
gdouble gwy_data_line_get_length        (GwyDataLine *data_line);
void    gwy_data_line_distribution      (GwyDataLine *data_line,
                                         GwyDataLine *distribution,
                                         gdouble ymin,
                                         gdouble ymax,
                                         gboolean normalize_to_unity,
                                         gint nstats);
void    gwy_data_line_dh                (GwyDataLine *data_line,
                                         GwyDataLine *target_line,
                                         gdouble ymin,
                                         gdouble ymax,
                                         gint nsteps);
void    gwy_data_line_cdh               (GwyDataLine *data_line,
                                         GwyDataLine *target_line,
                                         gdouble ymin,
                                         gdouble ymax,
                                         gint nsteps);
void    gwy_data_line_da                (GwyDataLine *data_line,
                                         GwyDataLine *target_line,
                                         gdouble ymin,
                                         gdouble ymax,
                                         gint nsteps);
void    gwy_data_line_cda               (GwyDataLine *data_line,
                                         GwyDataLine *target_line,
                                         gdouble ymin,
                                         gdouble ymax,
                                         gint nsteps);
void    gwy_data_line_acf               (GwyDataLine *data_line,
                                         GwyDataLine *target_line);
void    gwy_data_line_hhcf              (GwyDataLine *data_line,
                                         GwyDataLine *target_line);
void    gwy_data_line_psdf              (GwyDataLine *data_line,
                                         GwyDataLine *target_line,
                                         gint windowing,
                                         gint interpolation);

G_END_DECLS

#endif /* __GWY_PROCESS_LINESTATS_H__ */


/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

