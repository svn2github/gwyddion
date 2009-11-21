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

#define SLi gwy_lower_triangular_matrix_index
#define MATRIX_LEN gwy_triangular_matrix_length
#define ASSIGN(p, q, n) memcpy((p), (q), (n)*sizeof(gdouble))

#define GWY_FITTER_GET_PRIVATE(o)  \
   (G_TYPE_INSTANCE_GET_PRIVATE((o), GWY_TYPE_FITTER, GwyFitterPrivate))

typedef struct {
    guint iter_max;
    guint successes_to_get_bored;
    gdouble lambda_start;
    gdouble lambda_max;
    gdouble lambda_increase;
    gdouble lambda_decrease;
    gdouble param_change_min;
    gdouble residuum_change_min;
} GwyFitterSettings;

typedef struct {
    GwyFitterSettings settings;
    GwyFitterStatus status;
    guint nparam;
    guint iter;
    guint nsuccesses;
    gdouble lambda;
    gdouble f;
    gdouble f_best;
    GwyFitterGradientFunc eval_gradient;
    GwyFitterResiduumFunc eval_residuum;
    GwyFitterConstrainFunc constrain;
    /* The real allocation */
    gdouble *workspace;
    /* Things of size nparam */
    gdouble *param;
    gdouble *param_best;
    gdouble *gradient;
    gdouble *scaled_gradient;
    gdouble *diag;
    gdouble *step;
    /* Things of size MATRIX_LEN(nparam) */
    gdouble *hessian;
    gdouble *scaled_hessian;
    gdouble *normal_matrix;
    /* Other memory */
    gboolean *bad_param;
} Fitter;

typedef Fitter GwyFitterPrivate;

static void     gwy_fitter_finalize      (GObject *object);
static void     fitter_set_n_param(Fitter *fitter,
                                   guint nparam);

static const GwyFitterSettings default_settings = {
    .iter_max               = 50,
    .successes_to_get_bored = 5,
    .lambda_start           = 1.0,
    .lambda_max             = 1.0e6,
    .lambda_increase        = 10.0,
    .lambda_decrease        = 4.0,
    .param_change_min       = 1.0e-4,
    .residuum_change_min    = 1.0e-6,
};

G_DEFINE_TYPE(GwyFitter, gwy_fitter, G_TYPE_OBJECT)

static void
gwy_fitter_class_init(GwyFitterClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(GwyFitterPrivate));

    gobject_class->finalize = gwy_fitter_finalize;
}

static void
gwy_fitter_init(GwyFitter *object)
{
    Fitter *fitter = GWY_FITTER_GET_PRIVATE(object);

    fitter->settings = default_settings;
    fitter->lambda = fitter->settings.lambda_start;
}

void
gwy_fitter_finalize(GObject *object)
{
    Fitter *fitter = GWY_FITTER_GET_PRIVATE(object);

    fitter_set_n_param(fitter, 0);

    G_OBJECT_CLASS(gwy_fitter_parent_class)->finalize(object);
}

static void
fitter_set_n_param(Fitter *fitter,
                   guint nparam)
{
    if (fitter->nparam == nparam)
        return;

    guint matrix_len = MATRIX_LEN(nparam);
    fitter->nparam = nparam;

    g_free(fitter->workspace);
    fitter->workspace = nparam ? g_new(gdouble, 6*nparam + 3*matrix_len) : NULL;

    fitter->param = fitter->workspace;
    fitter->param_best = fitter->param + nparam;
    fitter->gradient = fitter->param_best + nparam;
    fitter->scaled_gradient = fitter->gradient + nparam;
    fitter->diag = fitter->scaled_gradient + nparam;
    fitter->step = fitter->diag + nparam;

    fitter->hessian = fitter->step + nparam;
    fitter->scaled_hessian = fitter->hessian + matrix_len;
    fitter->normal_matrix = fitter->scaled_hessian + matrix_len;

    g_free(fitter->bad_param);
    fitter->bad_param = nparam ? g_new(gboolean, nparam) : NULL;
}

static void
fitter_set_param(Fitter *fitter,
                 const gdouble *param)
{
    ASSIGN(fitter->param_best, param, fitter->nparam);
}

/* Paranoid evaluation of residuum and derivatives.
 * If anything fails check for out-of-bounds parameters and possibly change the
 * reported error type, noting which parameter was naughty.
 * Also if nothing seems wrong check if any calculated value is NaN anyway
 * because we do not trust the supplied functions. */
static inline gboolean
eval_residuum_with_check(Fitter *fitter,
                         gpointer user_data)
{
    gboolean ok = TRUE;

    if (!fitter->eval_residuum(fitter->param, &fitter->f, user_data)) {
        fitter->status = GWY_FITTER_STATUS_FUNCTION_FAILURE;
        ok = FALSE;
    }
    else if (isnan(fitter->f)) {
        fitter->status = GWY_FITTER_STATUS_SILENT_FAILURE;
        ok = FALSE;
    }
    if (!ok
        && fitter->constrain
        && !fitter->constrain(fitter->param, fitter->bad_param, user_data))
        fitter->status = GWY_FITTER_STATUS_PARAM_OFF_BOUNDS;
    return ok;
}

static inline gboolean
eval_gradient_with_check(Fitter *fitter,
                         gpointer user_data)
{
    gdouble *h = fitter->hessian, *g = fitter->gradient;
    guint nparam = fitter->nparam;
    gboolean ok = TRUE;

    if (!fitter->eval_gradient(fitter->param, g, h, user_data)) {
        fitter->status = GWY_FITTER_STATUS_GRADIENT_FAILURE;
        ok = FALSE;
    }
    else {
        for (guint i = 0; i < nparam; i++) {
            if (isnan(g[i])) {
                fitter->status = GWY_FITTER_STATUS_SILENT_FAILURE;
                ok = FALSE;
                break;
            }
            for (guint j = 0; j <= i; j++) {
                if (isnan(SLi(h, i, j))) {
                    fitter->status = GWY_FITTER_STATUS_SILENT_FAILURE;
                    ok = FALSE;
                    break;
                }
            }
        }
    }
    if (!ok
        && fitter->constrain
        && !fitter->constrain(fitter->param, fitter->bad_param, user_data))
        fitter->status = GWY_FITTER_STATUS_PARAM_OFF_BOUNDS;
    return ok;
}

static inline void
scale(Fitter *fitter)
{
    guint nparam = fitter->nparam;
    gdouble *h = fitter->hessian, *sh = fitter->scaled_hessian,
            *g = fitter->gradient, *sg = fitter->scaled_gradient,
            *d = fitter->diag;

    for (guint j = 0; j < nparam; j++) {
        gdouble a = SLi(h, j, j);
        d[j] = (a > 0.0) ? sqrt(a) : 1.0;
        sg[j] = g[j]/d[j];
        for (guint k = 0; k < j; k++)
            SLi(sh, j, k) = SLi(h, j, k)/(d[j]*d[k]);
        /* The point of the explicit assigment of 1.0 is not to avoid a
         * division but to make the fitting work with parmeters that have no
         * influence on the function, permitting a simple implementation of
         * fixed parameters. */
        SLi(sh, j, j) = 1.0;
    }
}

static inline void
add_to_diagonal(guint nparam,
                gdouble *hessian,
                gdouble x)
{
    for (guint j = 0; j < nparam; j++)
        SLi(hessian, j, j) += x;
}

static inline gboolean
too_small_param_change(Fitter *fitter)
{
    guint nparam = fitter->nparam;
    /* Not sure how fitter->f_best gets there but otherwise the condition does
     * not scale with fitted function values. */
    gdouble eps = fitter->settings.param_change_min * sqrt(fitter->f_best);

    /* FIXME: If we get here the new parameters have been accepted so it would
     * be really nice if the Hessian was OK too.  What to do if it isn't? */
    if (!gwy_cholesky_invert(fitter->hessian, nparam))
        return FALSE;
    for (guint j = 0; j < nparam; j++) {
        if (fitter->step[j] > eps * sqrt(SLi(fitter->hessian, j, j)))
            return FALSE;
    }
    return TRUE;
}

static inline void
update_param(Fitter *fitter)
{
    guint nparam = fitter->nparam;
    gdouble *p = fitter->param, *pb = fitter->param_best,
            *s = fitter->step, *d = fitter->diag;

    for (guint j = 0; j < nparam; j++) {
        s[j] /= d[j];
        p[j] = pb[j] - s[j];
    }
}

static gboolean
minimize(Fitter *fitter,
         gpointer user_data)
{
    g_return_val_if_fail(fitter->eval_gradient && fitter->eval_residuum, FALSE);
    g_return_val_if_fail(fitter->nparam, FALSE);

    guint nparam = fitter->nparam;
    guint matrix_len = MATRIX_LEN(nparam);
    ASSIGN(fitter->param, fitter->param_best, nparam);
    gwy_memclear(fitter->bad_param, nparam);

    if (!eval_residuum_with_check(fitter, user_data)
        || !eval_gradient_with_check(fitter, user_data))
        return FALSE;

    gdouble f_step = 0.0;
    fitter->f_best = fitter->f;
    fitter->nsuccesses = 0;

    while (fitter->iter++ < fitter->settings.iter_max) {
        scale(fitter);
        while (fitter->lambda <= fitter->settings.lambda_max) {
            fitter->status = GWY_FITTER_STATUS_NONE;
            ASSIGN(fitter->normal_matrix, fitter->scaled_hessian, matrix_len);
            add_to_diagonal(nparam, fitter->normal_matrix, fitter->lambda);
            if (!gwy_cholesky_decompose(fitter->normal_matrix, nparam)) {
                fitter->status = GWY_FITTER_STATUS_CANNOT_STEP;
                goto step_fail;
            }
            ASSIGN(fitter->step, fitter->scaled_gradient, nparam);
            gwy_cholesky_solve(fitter->normal_matrix, fitter->step,
                                    nparam);
            update_param(fitter);
            gwy_memclear(fitter->bad_param, nparam);
            if ((fitter->constrain
                 && !fitter->constrain(fitter->param, fitter->bad_param,
                                       user_data))) {
                fitter->status = GWY_FITTER_STATUS_PARAM_OFF_BOUNDS;
                goto step_fail;
            }
            if (!eval_residuum_with_check(fitter, user_data)
                || fitter->f >= fitter->f_best)
                goto step_fail;
            fitter->nsuccesses++;
            fitter->lambda /= fitter->settings.lambda_decrease;
            f_step = fitter->f_best ? 1.0 - fitter->f/fitter->f_best : 0.0;
            fitter->f_best = fitter->f;
            ASSIGN(fitter->param_best, fitter->param, nparam);
            break;
step_fail:
            fitter->nsuccesses = 0;
            fitter->lambda *= fitter->settings.lambda_increase;
        }
        if (!fitter->nsuccesses) {
            if (fitter->status)
                return FALSE;
            fitter->status = GWY_FITTER_STATUS_LAMBDA_OVERFLOW;
            break;
        }
        else if (fitter->nsuccesses > fitter->settings.successes_to_get_bored) {
            if (f_step < fitter->settings.residuum_change_min
                || too_small_param_change(fitter)) {
                fitter->status = GWY_FITTER_STATUS_TOO_SMALL_CHANGE;
                break;
            }
        }
        gwy_memclear(fitter->bad_param, nparam);
        if (!eval_gradient_with_check(fitter, user_data))
            return FALSE;
        fitter->status = GWY_FITTER_STATUS_NONE;
    }
    if (!fitter->status)
        fitter->status = GWY_FITTER_STATUS_MAX_ITER;

    /* TODO: Calculate errors.  But probably elsewhere.
    if (gwy_cholesky_invert(hessian, gradient, nparam)) {
    }
    */

    return TRUE;
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
 * GwyFitterStatus:
 * @GWY_FITTER_STATUS_NONE: No termination reason.  This occurs only if the
 *                          fitter has not been used for fitting yet.
 * @GWY_FITTER_STATUS_FUNCTION_FAILURE: Function evaluation failed in the
 *                                      calculation of the sum of squares.
 *                                      If the failure is due to out-of-bound
 *                                      parameters
 *                                      %GWY_FITTER_STATUS_PARAM_OFF_BOUNDS
 *                                      is indicated instead.
 * @GWY_FITTER_STATUS_GRADIENT_FAILURE: Derivative evaluation failed in the
 *                                      calculation of gradient and/or Hessian.
 *                                      If the failure is due to out-of-bound
 *                                      parameters
 *                                      %GWY_FITTER_STATUS_PARAM_OFF_BOUNDS
 *                                      is indicated instead.
 * @GWY_FITTER_STATUS_SILENT_FAILURE: Function or derivatives evaluation
 *                                    produced undefined values
 *                                    (not-a-numbers) in spite of indicating
 *                                    success.
 * @GWY_FITTER_STATUS_MAX_ITER: Maximum number of iterations was reached.
 * @GWY_FITTER_STATUS_LAMBDA_OVERFLOW: Maximum value of the Marquardt parameter
 *                                     lambda was exceeded.
 * @GWY_FITTER_STATUS_PARAM_OFF_BOUNDS: A parameter or parameters fell out of
 *                                      bounds.
 * @GWY_FITTER_STATUS_TOO_SMALL_CHANGE: A successful minimization step led to
 *                                      change in parameters and/or sum of
 *                                      squares smaller than given limit.
 * @GWY_FITTER_STATUS_CANNOT_STEP: It was not possible to calculate the
 *                                 parameter changes due to numerical
 *                                 instability.
 * @GWY_FITTER_STATUS_NEGATIVE_HESSIAN: Hessian is not numerically positive
 *                                      definite.  FIXME: unused.
 *
 * Non-linear least-squares fitter status.
 *
 * The status codes represent the reason why the fitting procedure terminated.
 * Some of them indicate numerical or other errors, other indicate reaching
 * some limit that may or may not be considered an error depending on the
 * caller.
 **/

/**
 * GwyFitterResiduumFunc:
 * @param: Values of parameters.
 * @residuum: Location to store the residuum to.
 * @user_data: Data passed to gwy_fitter_minimize().
 *
 * The type of function calculating the sum of squares for #GwyFitter.
 *
 * This is a low-level function type; the function must do all weighting and
 * summing of the contributions of individual data points.
 *
 * Returns: %TRUE if the evaluation succeeded, %FALSE on failure, e.g. due to
 *          a parameter domain error.  If %FALSE is returned @residuum may be
 *          left unset or set to a bogus value.
 **/

/**
 * GwyFitterGradientFunc:
 * @param: Values of parameters.  Note the parameters are not modifiable, if
 *         the function needs a modifiable array of parameters it must make a
 *         copy of @param.
 * @gradient: Array of the same length as @param to store the derivatives of
 *            the residuum by individual parameters to.
 * @hessian: Array to store the Hessian to.  The elements are stored as
 *           described in gwy_lower_triangular_matrix_index().
 * @user_data: Data passed to gwy_fitter_minimize().
 *
 * The type of function calculating the gradient and Hessian for #GwyFitter.
 *
 * This is a low-level function type; the function must do all weighting and
 * summing of the contributions of individual data points.
 *
 * Returns: %TRUE if the evaluation succeeded, %FALSE on failure, e.g. due to
 *          a parameter domain error.  If %FALSE is returned @gradient and
 *          @hessian elements may be left unset or set to bogus values.
 **/

/**
 * GwyFitterConstrainFunc:
 * @param: Values of parameters.
 * @ok: Array of the same length as @param to store the satisfaction of the
 *      constraints to.  This means %FALSE for out-of-bound parameters, %TRUE
 *      for good parameters.  It can be %NULL if the caller does not need this
 *      detailed information.
 * @user_data: Data passed to gwy_fitter_minimize().
 *
 * The type of function checking parameter constraints for #GwyFitter.
 *
 * Returns: %TRUE if all parameters are in prescribed domains, %FALSE if any
 *          is out of bound.
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
