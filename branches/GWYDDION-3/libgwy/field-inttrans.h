/*
 *  $Id$
 *  Copyright (C) 2013 David Nečas (Yeti).
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

#ifndef __LIBGWY_FIELD_INTTRANS_H__
#define __LIBGWY_FIELD_INTTRANS_H__

#include <libgwy/field.h>
#include <libgwy/fft.h>

G_BEGIN_DECLS

void gwy_field_row_fft           (const GwyField *rin,
                                  const GwyField *iin,
                                  GwyField *rout,
                                  GwyField *iout,
                                  GwyWindowingType windowing,
                                  gboolean preserverms,
                                  guint level);
void gwy_field_row_fft_raw       (GwyField *rin,
                                  GwyField *iin,
                                  GwyField *rout,
                                  GwyField *iout,
                                  GwyTransformDirection direction,
                                  gboolean preserveinput);
void gwy_field_fft               (const GwyField *rin,
                                  const GwyField *iin,
                                  GwyField *rout,
                                  GwyField *iout,
                                  GwyWindowingType windowing,
                                  gboolean preserverms,
                                  gint level);
void gwy_field_fft_raw           (GwyField *rin,
                                  GwyField *iin,
                                  GwyField *rout,
                                  GwyField *iout,
                                  GwyTransformDirection direction,
                                  gboolean preserveinput);
void gwy_field_fft_humanize      (GwyField *field);
void gwy_field_fft_dehumanize    (GwyField *field);
void gwy_field_row_fft_humanize  (GwyField *field);
void gwy_field_row_fft_dehumanize(GwyField *field);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
