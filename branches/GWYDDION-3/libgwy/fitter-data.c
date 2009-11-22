/*
 *  $Id$
 *  Copyright (C) 2009 David Necas (Yeti).
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

#include "config.h"
#include <string.h>
#include <glib.h>
#include "libgwy/macros.h"
#include "libgwy/fitter-data.h"
#include "libgwy/math.h"
#include "libgwy/types.h"
#include "libgwy/libgwy-aliases.h"

#define SLi gwy_lower_triangular_matrix_index
#define MATRIX_LEN gwy_triangular_matrix_length
#define ASSIGN(p, q, n) memcpy((p), (q), (n)*sizeof(gdouble))

#define GWY_FITTER_DATA_GET_PRIVATE(o)  \
   (G_TYPE_INSTANCE_GET_PRIVATE((o), GWY_TYPE_FITTER_DATA, GwyFitterDataPrivate))

typedef struct {
    gint dummy;
} FitterData;

typedef FitterData GwyFitterDataPrivate;

static void gwy_fitter_data_finalize    (GObject *object);

G_DEFINE_TYPE(GwyFitterData, gwy_fitter_data, G_TYPE_OBJECT)

static void
gwy_fitter_data_class_init(GwyFitterDataClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(GwyFitterDataPrivate));

    gobject_class->finalize = gwy_fitter_data_finalize;
}

static void
gwy_fitter_data_init(GwyFitterData *object)
{
    FitterData *fitter_data = GWY_FITTER_DATA_GET_PRIVATE(object);
}

void
gwy_fitter_data_finalize(GObject *object)
{
    FitterData *fitter_data = GWY_FITTER_DATA_GET_PRIVATE(object);

    G_OBJECT_CLASS(gwy_fitter_data_parent_class)->finalize(object);
}

#define __LIBGWY_FITTER_DATA_C__
#include "libgwy/libgwy-aliases.c"

/**
 * SECTION: fitter-data
 * @title: GwyFitterData
 * @short_description: Data wrapper for non-linear least-squares fitter
 **/

/**
 * GwyFitterData:
 *
 * Object representing non-linear least-squares fitter data.
 *
 * The #GwyFitterData struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyFitterDataClass:
 * @g_object_class: Parent class.
 *
 * Class of non-linear least-squares fitter data.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
