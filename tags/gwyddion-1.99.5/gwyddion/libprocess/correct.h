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

#ifndef __GWY_PROCESS_CORRECT_H__
#define __GWY_PROCESS_CORRECT_H__

#include <libprocess/datafield.h>

G_BEGIN_DECLS

void gwy_data_field_correct_laplace_iteration (GwyDataField *data_field,
                                               GwyDataField *mask_field,
                                               GwyDataField *buffer_field,
                                               gdouble *error,
                                               gdouble *corfactor);
void gwy_data_field_correct_average           (GwyDataField *data_field,
                                               GwyDataField *mask_field);
void gwy_data_field_mask_outliers             (GwyDataField *data_field,
                                               GwyDataField *mask_field,
                                               gdouble thresh);

GwyPlaneSymmetry gwy_data_field_unrotate_find_corrections(GwyDataLine *derdist,
                                                          gdouble *correction);

void gwy_data_field_get_drift_from_isotropy(GwyDataField *data_field,
                                      GwyDataLine *drift,
                                      gint window_size,
                                      gdouble smoothing);

void gwy_data_field_get_drift_from_correlation(GwyDataField *data_field,
                                      GwyDataLine *drift,
                                      gint skip_tolerance,
                                      gdouble smoothing);

void gwy_data_field_get_drift_from_sample(GwyDataField *data_field,
                                          GwyDataField *object_centers,
                                      GwyDataLine *drift,
                                      GwyInterpolationType interpolation,
                                      gdouble smoothing);

void gwy_data_field_correct_drift(GwyDataField *data_field,
                                  GwyDataLine *drift,
                                  gboolean crop);


G_END_DECLS

#endif /*__GWY_PROCESS_CORRECT__*/

