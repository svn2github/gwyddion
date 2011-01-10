/*
 *  $Id$
 *  Copyright (C) 2007,2009 David Necas (Yeti).
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

#ifndef __LIBGWY_FFT_H__
#define __LIBGWY_FFT_H__

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
    GWY_WINDOWING_NONE       = 0,
    GWY_WINDOWING_HANN       = 1,
    GWY_WINDOWING_HAMMING    = 2,
    GWY_WINDOWING_BLACKMANN  = 3,
    GWY_WINDOWING_LANCZOS    = 4,
    GWY_WINDOWING_WELCH      = 5,
    GWY_WINDOWING_RECT       = 6,
    GWY_WINDOWING_NUTTALL    = 7,
    GWY_WINDOWING_FLAT_TOP   = 8,
    GWY_WINDOWING_KAISER25   = 9
} GwyWindowingType;

guint gwy_fft_nice_transform_size(guint size)                 G_GNUC_CONST;
void  gwy_fft_window_sample      (gdouble *data,
                                  guint n,
                                  GwyWindowingType windowing,
                                  guint normalize);
void  gwy_fft_load_wisdom        (void);
void  gwy_fft_save_wisdom        (void);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
