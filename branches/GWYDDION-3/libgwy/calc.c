/*
 *  $Id$
 *  Copyright (C) 2012 David Nečas (Yeti).
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
#include "libgwy/strfuncs.h"
#include "libgwy/calc.h"

struct _GwyCalcPrivate {
    int unused;
};

typedef struct _GwyCalcPrivate Calc;

static void     gwy_calc_finalize      (GObject *object);

G_DEFINE_TYPE(GwyCalc, gwy_calc, G_TYPE_OBJECT);

static void
gwy_calc_class_init(GwyCalcClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(Calc));

    gobject_class->finalize = gwy_calc_finalize;
}

static void
gwy_calc_init(GwyCalc *calc)
{
    calc->priv = G_TYPE_INSTANCE_GET_PRIVATE(calc, GWY_TYPE_CALC, Calc);
}

static void
gwy_calc_finalize(GObject *object)
{
    Calc *priv = GWY_CALC(object)->priv;

    G_OBJECT_CLASS(gwy_calc_parent_class)->finalize(object);
}

/**
 * gwy_calc_error_quark:
 *
 * Returns error domain for expression parsing and evaluation.
 *
 * See and use %GWY_CALC_ERROR.
 *
 * Returns: The error domain.
 **/
GQuark
gwy_calc_error_quark(void)
{
    static GQuark error_domain = 0;

    if (!error_domain)
        error_domain = g_quark_from_static_string("gwy-calc-error-quark");

    return error_domain;
}

/****************************************************************************
 *
 *  High level, public API
 *
 ****************************************************************************/

/**
 * gwy_calc_new:
 *
 * Creates a new asynchronous calculation.
 *
 * Returns: A new asynchronous calculation.
 **/
GwyCalc*
gwy_calc_new(void)
{
    return g_object_newv(GWY_TYPE_CALC, 0, NULL);
}

/**
 * SECTION: calc
 * @title: GwyCalc
 * @short_description: Asynchronous calculation.
 *
 * Basic concepts:
 *
 * Two objects are involved: #GwyCalc and #GwyCalcInfo.
 *
 * #GwyCalc represents the abstract interface of a calculation and has methods
 * useful namely to run and control it.
 *
 * #GwyCalcInfo is always owned by one #GwyCalc and contains data private to
 * the code that will perfom the calculation.
 *
 * Three pieces of code are involved: User, provider and executor.
 *
 * User wants to calculate something; it obtains a #GwyCalc from the provider,
 * runs it, possibly queries progress or cancels it, gets informed about
 * termination of the calculation and finalises the #GwyCalc object (i.e.  is
 * the holder of the last reference).
 *
 * Provider can construct calculations; it takes specific data objects,
 * including pre-constructed data structures for the result (as much as
 * possible) from the user and creates both #GwyCalc and #GwyCalcInfo, filling
 * the later with private information for the calculator.
 *
 * Executor actually performs the calculation, reports progress, checks for
 * cancellation and fills the results.  Typically, it will be multi-threaded
 * with a master-workers structure but this is not important for this scheme.
 *
 * Two threads (at least) are involved: main and calculation.
 *
 * Main runs the user and provider parts, then it essentially only interacts
 * using message passing via main loop.  We may also want a synchronous
 * interface that will not involve the GLib main loop for command-line
 * calculations.
 *
 * Calculation runs the executor code.
 *
 * Communication:
 *
 * Data for the task: pre-constructed in main by the provider, then just passed
 * to the calculation thread.  Policy for acceptable data access must be
 * specified in the provider function documentation – typically they continue
 * to be accessible read-only (FIXME: what about silent modifications, e.g.
 * caching of statistics? would need to add locking! an ugly though workable
 * solution is to silently force calculation of all necessary caches in the
 * provider – although it might be time consuming).
 *
 * Start of calculation: start of the calculation thread.
 *
 * Progress reporting in calculation: lock(calc), set_progress(calc),
 * unlock(calc).  Performed occasionally at discretion of the calculation.
 * Contention for this lock should be low.
 *
 * Cancellation by main: lock(calc), cancel(calc), unlock(calc).  This assumes
 * polling in the calculation – considering that the calculation will have a
 * master-workers structure and cannot be expected to cancel immediately
 * anyway, this should be sufficient.
 *
 * Result reporting: directly into main-provided data structures; main is not
 * permitted to access them (content undefined) until calculation reports
 * termination.
 *
 * Termination of calculation: all auxilary data are freed, all worker threads
 * are terminated and only after that termination is signalled.
 *
 * Termination signalling: Not sure.  Two execution modes are probably
 * necessary: with a GLib main loop and without and the calculation needs to
 * know which is in effect.  With main loop, we can submit an idle function
 * to emit a #GwyCalc signal in the main thread; the user then just catches it.
 * Using lock(calc), set_terminated(calc), unlock(calc) assumes someone will
 * poll the state – this can be, in fact, also done by the main loop.  Finally,
 * a `synchronous' signalling can be implemented using a GCond.  Polling and
 * sginalling in the main loop are non-exclusive but synchronous calculation
 * is exclusive with others.
 *
 * Destruction: Nothing special necessary, everything is already shut down when
 * #GwyCalc is being finalised.
 **/

/**
 * GwyCalcError:
 * @GWY_CALC_ERROR_CANCELLED: Calculation was cancelled.
 *
 * Error codes returned by asynchronous calculations.
 **/

/**
 * GWY_CALC_ERROR:
 *
 * Error domain for asynchronous calculations. Errors in this domain
 * will be from the #GwyCalcError enumeration. See #GError for information on
 * error domains.
 **/

/**
 * GwyCalc:
 *
 * Object representing asynchronous calculation.
 *
 * The #GwyCalc struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyCalcClass:
 *
 * Class of asynchronous calculations.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
