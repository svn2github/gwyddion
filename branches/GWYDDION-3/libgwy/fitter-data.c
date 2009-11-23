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

typedef enum {
    NONE = 0,
    POINT_VARARG,
    VECTOR_VARARG,
    VECTOR_ARRAY
} GwyFitterInterfaceType;

typedef struct {
    GwyFitterInterfaceType type;
    guint nparam;
    guint ndata;
    /* Point interface */
    GwyFitterPointFunc point_func;
    GwyFitterPointWeightFunc point_weight;
    GwyPointXY *point_data;
    /* Vector interface */
    GwyFitterVectorFunc vector_func;
    GwyFitterVectorVFunc vector_vfunc;
    GwyFitterVectorDFunc vector_dfunc;
    gpointer vector_data;
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

/* The values would not confuse us when using the current interface but we
 * do not want any leftovers if someone switches interfaces back and forth. */
static void
invalidate_point_interface(FitterData *fitterdata)
{
    fitterdata->point_func = NULL;
    fitterdata->point_weight = NULL;
    if (fitterdata->point_data) {
        fitterdata->ndata = 0;
        fitterdata->point_data = NULL;
    }
}

static void
invalidate_vector_interface(FitterData *fitterdata)
{
    fitterdata->vector_func = NULL;
    fitterdata->vector_vfunc = NULL;
    fitterdata->vector_dfunc = NULL;
    if (fitterdata->vector_data) {
        fitterdata->ndata = 0;
        fitterdata->vector_data = NULL;
    }
}

void
gwy_fitter_data_set_point_function(GwyFitterData *object,
                                   guint nparams,
                                   GwyFitterPointFunc function)
{
    g_return_if_fail(GWY_IS_FITTER_DATA(object));
    FitterData *fitterdata = GWY_FITTER_DATA_GET_PRIVATE(object);

    invalidate_vector_interface(fitterdata);
    fitterdata->type = POINT_VARARG;
    fitterdata->nparam = nparams;
    fitterdata->point_func = function;
}

void
gwy_fitter_data_set_point_weight(GwyFitterData *object,
                                 GwyFitterPointWeightFunc weight)
{
    g_return_if_fail(GWY_IS_FITTER_DATA(object));
    FitterData *fitterdata = GWY_FITTER_DATA_GET_PRIVATE(object);

    invalidate_vector_interface(fitterdata);
    g_return_if_fail(fitterdata->type == POINT_VARARG);
    fitterdata->point_weight = weight;
}

void
gwy_fitter_data_set_point_data(GwyFitterData *object,
                               GwyPointXY *data,
                               guint ndata)
{
    g_return_if_fail(GWY_IS_FITTER_DATA(object));
    FitterData *fitterdata = GWY_FITTER_DATA_GET_PRIVATE(object);

    invalidate_vector_interface(fitterdata);
    g_return_if_fail(fitterdata->type == POINT_VARARG);
    fitterdata->ndata = ndata;
    fitterdata->point_data = data;
}

void
gwy_fitter_data_set_vector_function(GwyFitterData *object,
                                    guint nparams,
                                    GwyFitterVectorFunc function)
{
    g_return_if_fail(GWY_IS_FITTER_DATA(object));
    FitterData *fitterdata = GWY_FITTER_DATA_GET_PRIVATE(object);

    invalidate_point_interface(fitterdata);
    fitterdata->vector_vfunc = NULL;
    fitterdata->vector_dfunc = NULL;
    fitterdata->type = VECTOR_VARARG;
    fitterdata->nparam = nparams;
    fitterdata->vector_func = function;
}

void
gwy_fitter_data_set_vector_vfunction(GwyFitterData *object,
                                     guint nparams,
                                     GwyFitterVectorVFunc function,
                                     GwyFitterVectorDFunc derivative)
{
    g_return_if_fail(GWY_IS_FITTER_DATA(object));
    FitterData *fitterdata = GWY_FITTER_DATA_GET_PRIVATE(object);

    invalidate_point_interface(fitterdata);
    fitterdata->vector_func = NULL;
    fitterdata->type = VECTOR_ARRAY;
    fitterdata->nparam = nparams;
    fitterdata->vector_vfunc = function;
    fitterdata->vector_dfunc = derivative;
}

void
gwy_fitter_data_set_vector_data(GwyFitterData *object,
                                guint ndata,
                                gpointer user_data)
{
    g_return_if_fail(GWY_IS_FITTER_DATA(object));
    FitterData *fitterdata = GWY_FITTER_DATA_GET_PRIVATE(object);

    invalidate_point_interface(fitterdata);
    g_return_if_fail(fitterdata->type == VECTOR_VARARG
                     || fitterdata->type == VECTOR_ARRAY);
    fitterdata->ndata = ndata;
    fitterdata->vector_data = user_data;
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
