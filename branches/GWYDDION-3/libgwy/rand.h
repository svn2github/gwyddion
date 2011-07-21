/*
 *  $Id$
 *  Copyright (C) 2011 David Nečas (Yeti).
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

#ifndef __LIBGWY_RAND_H__
#define __LIBGWY_RAND_H__

#include <glib.h>

G_BEGIN_DECLS

typedef struct _GwyRand GwyRand;

GwyRand* gwy_rand_new                (void)                G_GNUC_MALLOC;
GwyRand* gwy_rand_copy               (const GwyRand *rng)  G_GNUC_MALLOC;
GwyRand* gwy_rand_new_with_seed      (guint64 seed)        G_GNUC_MALLOC;
GwyRand* gwy_rand_new_with_seed_array(const guint64 *seed,
                                      guint seed_length)   G_GNUC_MALLOC;
void     gwy_rand_free               (GwyRand *rng);
void     gwy_rand_set_seed           (GwyRand *rng,
                                      guint64 seed);
void     gwy_rand_set_seed_array     (GwyRand *rng,
                                      const guint64 *seed,
                                      guint seed_length);
guint64  gwy_rand_uint64             (GwyRand *rng);
guint32  gwy_rand_uint32             (GwyRand *rng);
gint64   gwy_rand_int_range          (GwyRand *rng,
                                      gint64 begin,
                                      gint64 end);
gdouble  gwy_rand_double             (GwyRand *rng);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
