/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2012 David Necas (Yeti), Petr Klapetek.
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

#ifndef __GWY_PROCESS_GRAINS_H__
#define __GWY_PROCESS_GRAINS_H__

#include <libprocess/datafield.h>
#include <libprocess/gwyprocesstypes.h>

G_BEGIN_DECLS

void gwy_data_field_grains_mark_curvature(GwyDataField *data_field,
                                          GwyDataField *grain_field,
                                          gdouble threshval,
                                          gboolean below);

void gwy_data_field_grains_mark_watershed(GwyDataField *data_field,
                                          GwyDataField *grain_field,
                                          gint locate_steps,
                                          gint locate_thresh,
                                          gdouble locate_dropsize,
                                          gint wshed_steps,
                                          gdouble wshed_dropsize,
                                          gboolean prefilter,
                                          gboolean below);

gboolean gwy_data_field_grains_remove_grain(GwyDataField *grain_field,
                                            gint col,
                                            gint row);
gboolean gwy_data_field_grains_extract_grain(GwyDataField *grain_field,
                                             gint col,
                                             gint row);

void gwy_data_field_grains_remove_by_number(GwyDataField *grain_field,
                                            gint number);

void gwy_data_field_grains_remove_by_size(GwyDataField *grain_field,
                                          gint size);

void gwy_data_field_grains_remove_by_height(GwyDataField *data_field,
                                            GwyDataField *grain_field,
                                            gdouble threshval,
                                            gboolean below);
void gwy_data_field_grains_remove_touching_border(GwyDataField *grain_field);

GwyComputationState* gwy_data_field_grains_watershed_init(GwyDataField *data_field,
                                                          GwyDataField *grain_field,
                                                          gint locate_steps,
                                                          gint locate_thresh,
                                                          gdouble locate_dropsize,
                                                          gint wshed_steps,
                                                          gdouble wshed_dropsize,
                                                          gboolean prefilter,
                                                          gboolean below);
void gwy_data_field_grains_watershed_iteration(GwyComputationState *state);
void gwy_data_field_grains_watershed_finalize(GwyComputationState *state);

void gwy_data_field_grains_mark_height(GwyDataField *data_field,
                                       GwyDataField *grain_field,
                                       gdouble threshval,
                                       gboolean below);

void gwy_data_field_grains_mark_slope(GwyDataField *data_field,
                                      GwyDataField *grain_field,
                                      gdouble threshval,
                                      gboolean below);

void gwy_data_field_grains_add(GwyDataField *grain_field,
                              GwyDataField *add_field);

void gwy_data_field_grains_intersect(GwyDataField *grain_field,
                                     GwyDataField *intersect_field);

gint gwy_data_field_number_grains(GwyDataField *mask_field,
                                  gint *grains);
gint* gwy_data_field_get_grain_bounding_boxes(GwyDataField *mask_field,
                                              gint ngrains,
                                              const gint *grains,
                                              gint *bboxes);

GwyDataLine* gwy_data_field_grains_get_distribution(GwyDataField *data_field,
                                                    GwyDataField *grain_field,
                                                    GwyDataLine *distribution,
                                                    gint ngrains,
                                                    const gint *grains,
                                                    GwyGrainQuantity quantity,
                                                    gint nstats);
gdouble*     gwy_data_field_grains_get_values      (GwyDataField *data_field,
                                                    gdouble *values,
                                                    gint ngrains,
                                                    const gint *grains,
                                                    GwyGrainQuantity quantity);
gdouble**    gwy_data_field_grains_get_quantities  (GwyDataField *data_field,
                                                    gdouble **values,
                                                    const GwyGrainQuantity *quantities,
                                                    guint nquantities,
                                                    guint ngrains,
                                                    const gint *grains);
gboolean     gwy_grain_quantity_needs_same_units   (GwyGrainQuantity quantity);
GwySIUnit*   gwy_grain_quantity_get_units          (GwyGrainQuantity quantity,
                                                    GwySIUnit *siunitxy,
                                                    GwySIUnit *siunitz,
                                                    GwySIUnit *result);

void gwy_data_field_area_grains_tgnd(GwyDataField *data_field,
                                     GwyDataLine *target_line,
                                     gint col,
                                     gint row,
                                     gint width,
                                     gint height,
                                     gboolean below,
                                     gint nstats);
void gwy_data_field_area_grains_tgnd_range(GwyDataField *data_field,
                                           GwyDataLine *target_line,
                                           gint col, gint row,
                                           gint width, gint height,
                                           gdouble min, gdouble max,
                                           gboolean below,
                                           gint nstats);

void gwy_data_field_grains_splash_water(GwyDataField *data_field,
                                        GwyDataField *minima,
                                        gint locate_steps,
                                        gdouble locate_dropsize);

void     gwy_data_field_grain_distance_transform(GwyDataField *data_field);
gboolean gwy_data_field_fill_voids              (GwyDataField *data_field,
                                                 gboolean nonsimple);
gint     gwy_data_field_waterpour               (GwyDataField *data_field,
                                                 GwyDataField *result,
                                                 gint *grains);
void     gwy_data_field_mark_extrema            (GwyDataField *dfield,
                                                 GwyDataField *extrema,
                                                 gboolean maxima);

G_END_DECLS

#endif /* __GWY_PROCESS_GRAINS__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
