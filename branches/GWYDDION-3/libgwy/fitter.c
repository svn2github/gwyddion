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
#include <glib/gi18n-lib.h>
#include "libgwy/macros.h"
#include "libgwy/fitter.h"
#include "libgwy/math.h"
#include "libgwy/libgwy-aliases.h"

#define GWY_FITTER_GET_PRIVATE(o)  \
   (G_TYPE_INSTANCE_GET_PRIVATE((o), GWY_TYPE_FITTER, GwyFitterPrivate))

typedef struct {
    gint dummy;
} GwyFitterPrivate;

static void     gwy_fitter_finalize      (GObject *object);

G_DEFINE_TYPE(GwyFitter, gwy_fitter, G_TYPE_OBJECT)

static void
gwy_fitter_class_init(GwyFitterClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(GwyFitterPrivate));

    gobject_class->finalize = gwy_fitter_finalize;
}

static void
gwy_fitter_init(GwyFitter *fitter)
{
    GwyFitterPrivate *priv = GWY_FITTER_GET_PRIVATE(fitter);
}

void
gwy_fitter_finalize(GObject *object)
{
    GwyFitter *fitter = GWY_FITTER(object);
    GwyFitterPrivate *priv = GWY_FITTER_GET_PRIVATE(fitter);

    G_OBJECT_CLASS(gwy_fitter_parent_class)->finalize(object);
}

/****************************************************************************
 *
 *  High level, public API
 *
 ****************************************************************************/

/**
 * gwy_fitter_new:
 *
 * Creates a new non-linear least-squares fitter.
 *
 * Returns: A new non-linear least-squares fitter.
 **/
GwyFitter*
gwy_fitter_new(void)
{
    return g_object_newv(GWY_TYPE_FITTER, 0, NULL);
}

#define __LIBGWY_FITTER_C__
#include "libgwy/libgwy-aliases.c"

/**
 * SECTION: fitter
 * @title: GwyFitter
 * @short_description: Non-linear least-squares fitting
 **/

/**
 * GwyFitterError:
 *
 * Error codes returned by non-linear least-squares fitting.
 **/

/**
 * GWY_FITTER_ERROR:
 *
 * Error domain for non-linear least-squares fitting. Errors in this domain
 * will be from the #GwyFitterError enumeration. See #GError for information on
 * error domains.
 **/

/**
 * GwyFitter:
 *
 * Object representing non-linear least-squares fitter.
 *
 * The #GwyFitter struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyFitterClass:
 * @g_object_class: Parent class.
 *
 * Class of non-linear least-squares fitters.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
