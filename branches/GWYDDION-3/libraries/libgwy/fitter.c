/*
 *  $Id$
 *  Copyright (C) 2009,2011,2014 David Nečas (Yeti).
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

#include <string.h>
#include "libgwy/macros.h"
#include "libgwy/serialize.h"
#include "libgwy/math.h"
#include "libgwy/fitter.h"
#include "libgwy/types.h"
#include "libgwy/math-internal.h"
#include "libgwy/object-internal.h"

enum {
    PROP_0,
    PROP_N_PARAMS,
    PROP_ITER_MAX,
    PROP_SUCCESSES_TO_GET_BORED,
    PROP_LAMBDA_MAX,
    PROP_LAMBDA_START,
    PROP_LAMBDA_INCREASE,
    PROP_LAMBDA_DECREASE,
    PROP_PARAM_CHANGE_MIN,
    PROP_RESIDUUM_CHANGE_MIN,
    N_PROPS
};

/* This is ordered, higher value means all the lower are available too. */
typedef enum {
    VALID_NOTHING = 0,
    VALID_PARAMS,
    VALID_FUNCTION,
    VALID_HESSIAN,
    VALID_INV_HESSIAN_DIAG,
    VALID_INV_HESSIAN,
} GwyFitterValid;

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

struct _GwyFitterPrivate {
    GwyFitterSettings settings;
    GwyFitterStatus status;
    GwyFitterValid valid;
    guint nparam;
    guint iter;
    guint nsuccesses;
    gboolean fitting;
    gdouble lambda;
    gdouble f;
    gdouble f_best;
    GwyFitterGradientFunc eval_gradient;
    GwyFitterMatrixGradientFunc eval_gradient_matrix;
    GwyFitterResiduumFunc eval_residuum;
    GwyFitterConstrainFunc constrain;
    /* Things of size nparam */
    gdouble *workspace1;    /* The real allocation */
    gdouble *param;
    gdouble *param_best;
    gdouble *gradient;
    gdouble *diag;
    gdouble *step;
    gdouble *inv_diag;
    /* Things of size MATRIX_LEN(nparam) */
    gdouble *workspace2;    /* The real allocation */
    gdouble *hessian;
    gdouble *inv_hessian;
    /* The abstract matrix interface */
    GwyMatrix *matrix_hessian;
};

typedef struct _GwyFitterPrivate Fitter;

static void     gwy_fitter_finalize    (GObject *object);
static void     gwy_fitter_set_property(GObject *object,
                                        guint prop_id,
                                        const GValue *value,
                                        GParamSpec *pspec);
static void     gwy_fitter_get_property(GObject *object,
                                        guint prop_id,
                                        GValue *value,
                                        GParamSpec *pspec);
static void     fitter_set_n_param     (Fitter *fitter,
                                        guint nparam);
static gboolean fitter_invert_hessian  (Fitter *fitter);

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

static GParamSpec *properties[N_PROPS];

G_DEFINE_TYPE(GwyFitter, gwy_fitter, G_TYPE_OBJECT);

static void
gwy_fitter_class_init(GwyFitterClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(Fitter));

    gobject_class->finalize = gwy_fitter_finalize;
    gobject_class->get_property = gwy_fitter_get_property;
    gobject_class->set_property = gwy_fitter_set_property;

    properties[PROP_N_PARAMS]
        = g_param_spec_uint("n-params",
                            "Number of params",
                            "Number of fitting parameters.",
                            0, 1024, 0,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_ITER_MAX]
        = g_param_spec_uint("max-iters",
                            "Maximum iters",
                            "Maximum number of iterations.",
                            1, G_MAXUINT, default_settings.iter_max,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_SUCCESSES_TO_GET_BORED]
        = g_param_spec_uint("successes-to-get-bored",
                            "Successes to get bored",
                            "The number of consecutive successful steps after "
                            "which to start checking the minimum required "
                            "change of lambda and residuum.",
                            1, G_MAXUINT,
                            default_settings.successes_to_get_bored,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_LAMBDA_MAX]
        = g_param_spec_double("lambda-max",
                              "Maximum lambda",
                              "Maximum value of Marquardt parameter lambda.",
                              0.0, G_MAXDOUBLE, default_settings.lambda_max,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_LAMBDA_START]
        = g_param_spec_double("lambda-start",
                              "Lambda start",
                              "Value to set the Marquardt parameter lambda "
                              "to at the start of each fitting.",
                              G_MINDOUBLE, G_MAXDOUBLE,
                              default_settings.lambda_start,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_LAMBDA_INCREASE]
        = g_param_spec_double("lambda-increase",
                              "Lambda increase",
                              "Factor to multiply Marquardt parameter lambda "
                              "with after an unsuccessful step.",
                              1.0, G_MAXDOUBLE, default_settings.lambda_increase,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_LAMBDA_DECREASE]
        = g_param_spec_double("lambda-decrease",
                              "Lambda decrease",
                              "Factor to divide Marquardt parameter lambda "
                              "with after a successful step.",
                              1.0, G_MAXDOUBLE, default_settings.lambda_decrease,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_PARAM_CHANGE_MIN]
        = g_param_spec_double("min-param-change",
                              "Minimum param change",
                              "Minimum relative change of at least one "
                              "parameter in a successful step.",
                              0.0, G_MAXDOUBLE,
                              default_settings.param_change_min,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_RESIDUUM_CHANGE_MIN]
        = g_param_spec_double("min-residuum-change",
                              "Minimum residuum change",
                              "Minimum relative decrease of the residuum "
                              "in a successful step.",
                              0.0, G_MAXDOUBLE,
                              default_settings.residuum_change_min,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, properties[i]);

}

static void
gwy_fitter_init(GwyFitter *fitter)
{
    fitter->priv = G_TYPE_INSTANCE_GET_PRIVATE(fitter, GWY_TYPE_FITTER, Fitter);
    fitter->priv->settings = default_settings;
}

static void
gwy_fitter_finalize(GObject *object)
{
    GwyFitter *fitter = GWY_FITTER(object);
    if (fitter->priv->matrix_hessian) {
        gwy_matrix_unref(fitter->priv->matrix_hessian);
        fitter->priv->matrix_hessian = NULL;
    }
    fitter_set_n_param(fitter->priv, 0);
    G_OBJECT_CLASS(gwy_fitter_parent_class)->finalize(object);
}

static void
gwy_fitter_set_property(GObject *object,
                        guint prop_id,
                        const GValue *value,
                        GParamSpec *pspec)
{
    GwyFitter *fitter = GWY_FITTER(object);
    GwyFitterSettings *settings = &fitter->priv->settings;
    switch (prop_id) {
        case PROP_N_PARAMS:
        fitter_set_n_param(fitter->priv, g_value_get_uint(value));
        break;

        case PROP_ITER_MAX:
        settings->iter_max = g_value_get_uint(value);
        break;

        case PROP_SUCCESSES_TO_GET_BORED:
        settings->successes_to_get_bored = g_value_get_uint(value);
        break;

        case PROP_LAMBDA_MAX:
        settings->lambda_max = g_value_get_double(value);
        break;

        case PROP_LAMBDA_START:
        settings->lambda_start = g_value_get_double(value);
        break;

        case PROP_LAMBDA_INCREASE:
        settings->lambda_increase = g_value_get_double(value);
        break;

        case PROP_LAMBDA_DECREASE:
        settings->lambda_decrease = g_value_get_double(value);
        break;

        case PROP_PARAM_CHANGE_MIN:
        settings->param_change_min = g_value_get_double(value);
        break;

        case PROP_RESIDUUM_CHANGE_MIN:
        settings->residuum_change_min = g_value_get_double(value);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_fitter_get_property(GObject *object,
                        guint prop_id,
                        GValue *value,
                        GParamSpec *pspec)
{
    GwyFitter *fitter = GWY_FITTER(object);
    GwyFitterSettings *settings = &fitter->priv->settings;
    switch (prop_id) {
        case PROP_N_PARAMS:
        g_value_set_uint(value, fitter->priv->nparam);
        break;

        case PROP_ITER_MAX:
        g_value_set_uint(value, settings->iter_max);
        break;

        case PROP_SUCCESSES_TO_GET_BORED:
        g_value_set_uint(value, settings->successes_to_get_bored);
        break;

        case PROP_LAMBDA_MAX:
        g_value_set_double(value, settings->lambda_max);
        break;

        case PROP_LAMBDA_START:
        g_value_set_double(value, settings->lambda_start);
        break;

        case PROP_LAMBDA_INCREASE:
        g_value_set_double(value, settings->lambda_increase);
        break;

        case PROP_LAMBDA_DECREASE:
        g_value_set_double(value, settings->lambda_decrease);
        break;

        case PROP_PARAM_CHANGE_MIN:
        g_value_set_double(value, settings->param_change_min);
        break;

        case PROP_RESIDUUM_CHANGE_MIN:
        g_value_set_double(value, settings->residuum_change_min);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
fitter_set_n_param(Fitter *fitter,
                   guint nparam)
{
    fitter->valid = 0;
    fitter->status = GWY_FITTER_STATUS_NONE;

    if (fitter->nparam == nparam)
        return;

    if (fitter->fitting)
        g_critical("Number of parameters changed during an iteration.");

    fitter->nparam = nparam;

    g_free(fitter->workspace1);
    fitter->workspace1 = nparam ? g_new(gdouble, 6*nparam) : NULL;
    fitter->param = fitter->workspace1;
    fitter->param_best = fitter->param + nparam;
    fitter->gradient = fitter->param_best + nparam;
    fitter->diag = fitter->gradient + nparam;
    fitter->step = fitter->diag + nparam;
    fitter->inv_diag = fitter->step + nparam;

    GWY_FREE(fitter->workspace2);
    if (!fitter->matrix_hessian) {
        guint matrix_len = MATRIX_LEN(nparam);
        fitter->workspace2 = nparam ? g_new(gdouble, 2*matrix_len) : NULL;
        fitter->hessian = fitter->workspace2;
        fitter->inv_hessian = fitter->hessian + matrix_len;
    }
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

    if (ok)
        fitter->valid = MAX(fitter->valid, VALID_FUNCTION);
    else if (fitter->constrain
             && !fitter->constrain(fitter->param, NULL, user_data))
        fitter->status = GWY_FITTER_STATUS_PARAM_OFF_BOUNDS;

    return ok;
}

static inline gboolean
eval_gradient_with_check(Fitter *fitter,
                         gpointer user_data)
{
    gdouble *h = fitter->hessian, *g = fitter->gradient;
    guint nparam = fitter->nparam;
    gboolean ok;

    fitter->status = GWY_FITTER_STATUS_GRADIENT_FAILURE;
    if (fitter->eval_gradient_matrix) {
        ok = fitter->eval_gradient_matrix(fitter->param,
                                          g, fitter->matrix_hessian, user_data);
    }
    else if (fitter->eval_gradient) {
        ok = fitter->eval_gradient(fitter->param, g, h, user_data);
    }
    else {
        g_return_val_if_reached(FALSE);
    }

    if (ok) {
        for (guint i = 0; i < nparam; i++) {
            if (isnan(g[i])) {
                fitter->status = GWY_FITTER_STATUS_SILENT_FAILURE;
                ok = FALSE;
                break;
            }
            // XXX: We cannot detect silent failure for abstract matrix
            // operations.  Must trust the matrix implementation.
            if (!fitter->eval_gradient_matrix) {
                for (guint j = 0; j <= i; j++) {
                    if (isnan(SLi(h, i, j))) {
                        fitter->status = GWY_FITTER_STATUS_SILENT_FAILURE;
                        ok = FALSE;
                        break;
                    }
                }
            }
        }
    }

    if (!ok) {
        if (fitter->constrain
                 && !fitter->constrain(fitter->param, NULL, user_data))
            fitter->status = GWY_FITTER_STATUS_PARAM_OFF_BOUNDS;
        return FALSE;
    }

    fitter->valid = MAX(fitter->valid, VALID_HESSIAN);
    return TRUE;
}

static inline gboolean
extract_hessian_diagonal(Fitter *fitter)
{
    guint nparam = fitter->nparam;
    gdouble *d = fitter->diag;

    if (fitter->matrix_hessian) {
        if (gwy_matrix_get_diagonal(fitter->matrix_hessian, d))
            return TRUE;
        fitter->status = GWY_FITTER_STATUS_MATRIX_FAILURE;
        return FALSE;
    }

    const gdouble *h = fitter->hessian;
    for (guint j = 0; j < nparam; j++)
        d[j] = SLi(h, j, j);

    return TRUE;
}

static inline gboolean
set_diagonal_from_array(Fitter *fitter, const gdouble *a)
{
    guint nparam = fitter->nparam;

    if (fitter->matrix_hessian) {
        if (gwy_matrix_set_diagonal(fitter->matrix_hessian, a))
            return TRUE;
        fitter->status = GWY_FITTER_STATUS_MATRIX_FAILURE;
        return FALSE;
    }

    gdouble *h = fitter->hessian;
    for (guint j = 0; j < nparam; j++)
        SLi(h, j, j) = a[j];

    return TRUE;
}

// Do the replacement with zeroes at the diagonal 1.0 directly here so that
// diag[] remembers the original Hessian diagonal and we can always restore
// it.
// XXX: Reuses step[] as a scratch buffer
static inline gboolean
add_to_diagonal(Fitter *fitter)
{
    guint nparam = fitter->nparam;
    const gdouble *d = fitter->diag;
    gdouble *buf = fitter->step;
    gdouble lambda = fitter->lambda;

    for (guint j = 0; j < nparam; j++) {
        gdouble dj = d[j];
        if (!(dj > 0.0))
            dj = 1.0;

        buf[j] = d[j] + lambda*dj;
    }

    return set_diagonal_from_array(fitter, buf);
}

static inline gboolean
restore_diagonal(Fitter *fitter)
{
    return set_diagonal_from_array(fitter, fitter->diag);
}

static inline gboolean
too_small_param_change(Fitter *fitter)
{
    guint nparam = fitter->nparam;
    /* Not sure how fitter->f_best gets there but otherwise the condition does
     * not scale with fitted function values. */
    gdouble eps = fitter->settings.param_change_min * sqrt(fitter->f_best);
    gdouble *buf = fitter->inv_diag;

    /* FIXME: If we get here the new parameters have been accepted so it would
     * be really nice if the Hessian was OK too.  What to do if it isn't?
     * At present we pretend the change was not too small.  */
    if (fitter->eval_gradient_matrix) {
        if (!gwy_matrix_get_inv_diagonal(fitter->matrix_hessian, buf))
            return FALSE;
    }
    else {
        if (!fitter_invert_hessian(fitter))
            return FALSE;

        for (guint j = 0; j < nparam; j++)
            buf[j] = SLi(fitter->inv_hessian, j, j);
    }

    fitter->valid = MAX(fitter->valid, VALID_INV_HESSIAN_DIAG);
    for (guint j = 0; j < nparam; j++) {
        gdouble step_min = eps * sqrt(fmax(buf[j], 0.0));
        if (fabs(fitter->step[j]) > step_min)
            return FALSE;
    }
    return TRUE;
}

static inline void
update_param(Fitter *fitter)
{
    guint nparam = fitter->nparam;
    const gdouble *pb = fitter->param_best, *s = fitter->step;
    gdouble *p = fitter->param;

    for (guint j = 0; j < nparam; j++)
        p[j] = pb[j] - s[j];
}

static inline gboolean
solve_step(Fitter *fitter)
{
    if (fitter->eval_gradient_matrix) {
        return gwy_matrix_inv_multiply(fitter->matrix_hessian,
                                       fitter->gradient, fitter->step);
    }

    guint nparam = fitter->nparam;

    gwy_assign(fitter->inv_hessian, fitter->hessian, MATRIX_LEN(nparam));
    if (!gwy_cholesky_decompose(fitter->inv_hessian, nparam)) {
        return FALSE;
    }
    gwy_assign(fitter->step, fitter->gradient, nparam);
    gwy_cholesky_solve(fitter->inv_hessian, fitter->step, nparam);
    return TRUE;
}

static gboolean
fitter_minimize(Fitter *fitter,
                gpointer user_data)
{
    guint nparam = fitter->nparam;

    fitter->iter = 0;
    fitter->lambda = fitter->settings.lambda_start;
    fitter->valid = MIN(fitter->valid, VALID_PARAMS);
    gwy_assign(fitter->param, fitter->param_best, nparam);

    if (!eval_residuum_with_check(fitter, user_data)
        || !eval_gradient_with_check(fitter, user_data))
        return FALSE;

    gdouble f_step = 0.0;
    fitter->f_best = fitter->f;
    fitter->nsuccesses = 0;

    while (fitter->iter++ < fitter->settings.iter_max) {
        if (!extract_hessian_diagonal(fitter))
            return FALSE;
        while (fitter->lambda <= fitter->settings.lambda_max) {
            fitter->status = GWY_FITTER_STATUS_NONE;
            if (!add_to_diagonal(fitter))
                return FALSE;
            if (!solve_step(fitter)) {
                fitter->status = GWY_FITTER_STATUS_CANNOT_STEP;
                goto step_fail;
            }
            update_param(fitter);
            if ((fitter->constrain
                 && !fitter->constrain(fitter->param, NULL, user_data))) {
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
            gwy_assign(fitter->param_best, fitter->param, nparam);
            fitter->valid = MIN(fitter->valid, VALID_FUNCTION);
            break;
step_fail:
            fitter->nsuccesses = 0;
            fitter->lambda *= fitter->settings.lambda_increase;
            if (!restore_diagonal(fitter))
                return FALSE;
            // FIXME: If we get here with GWY_FITTER_STATUS_PARAM_OFF_BOUNDS,
            // we should call constrain() to find out which parameters are
            // off bounds and temporarily treat them as fixed (i.e. set
            // the corresponding gradient elements to 0.0).  Probably in
            // addition to the lamba increase.  Or just hard-fix them...
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
        if (!eval_gradient_with_check(fitter, user_data))
            return FALSE;
        fitter->status = GWY_FITTER_STATUS_NONE;
    }
    if (fitter->valid < VALID_HESSIAN)
        eval_gradient_with_check(fitter, user_data);
    if (!fitter->status)
        fitter->status = GWY_FITTER_STATUS_MAX_ITER;

    return TRUE;
}

static gboolean
fitter_invert_hessian(Fitter *fitter)
{
    if (fitter->eval_gradient_matrix)
        return FALSE;

    guint nparam = fitter->nparam;
    guint matrix_len = MATRIX_LEN(nparam);
    gwy_assign(fitter->inv_hessian, fitter->hessian, matrix_len);
    /* Make possible inversion of fitter with naively fixed parameters. */
    gboolean zero[nparam];
    gwy_clear(zero, nparam);
    for (guint i = 0; i < nparam; i++) {
        if (fitter->diag[i] == 0.0) {
            SLi(fitter->inv_hessian, i, i) = 1.0;
            zero[i] = TRUE;
        }
    }
    if (gwy_cholesky_invert(fitter->inv_hessian, fitter->nparam)) {
        fitter->valid = MAX(fitter->valid, VALID_INV_HESSIAN);
        for (guint i = 0; i < nparam; i++) {
            if (zero[i])
                SLi(fitter->inv_hessian, i, i) = 0.0;
        }
        return TRUE;
    }
    return FALSE;
}

/****************************************************************************
 *
 *  High level, public API
 *
 ****************************************************************************/

/**
 * gwy_fitter_new: (constructor)
 *
 * Creates a new non-linear least-squares fitter.
 *
 * Returns: (transfer full):
 *          A new non-linear least-squares fitter.
 **/
GwyFitter*
gwy_fitter_new(void)
{
    return g_object_newv(GWY_TYPE_FITTER, 0, NULL);
}

/**
 * gwy_fitter_status:
 * @fitter: A non-linear least-squares fitter.
 *
 * Obtains the status of the last fitting performed with a fitter.
 *
 * The status is reset to %GWY_FITTER_STATUS_NONE if the fitter is 
 * reconfigured.
 *
 * Returns: The fitting status.
 **/
guint
gwy_fitter_status(GwyFitter *fitter)
{
    g_return_val_if_fail(GWY_IS_FITTER(fitter), GWY_FITTER_STATUS_NONE);
    return fitter->priv->status;
}

/**
 * gwy_fitter_set_n_params:
 * @fitter: A non-linear least-squares fitter.
 * @nparams: New number of parameters.
 *
 * Sets the number of parameters in the model of a fitter.
 *
 * This method invalidates the parameters and sets the fitting status to
 * %GWY_FITTER_STATUS_NONE.
 **/
void
gwy_fitter_set_n_params(GwyFitter *fitter,
                        guint nparams)
{
    g_return_if_fail(GWY_IS_FITTER(fitter));
    fitter_set_n_param(fitter->priv, nparams);
    g_object_notify_by_pspec(G_OBJECT(fitter), properties[PROP_N_PARAMS]);
}

/**
 * gwy_fitter_get_n_params:
 * @fitter: A non-linear least-squares fitter.
 *
 * Gets the number of parameters in the model of a fitter.
 *
 * Returns: The number of model parameters.
 **/
guint
gwy_fitter_get_n_params(GwyFitter *fitter)
{
    g_return_val_if_fail(GWY_IS_FITTER(fitter), 0);
    return fitter->priv->nparam;
}

/**
 * gwy_fitter_set_params:
 * @fitter: A non-linear least-squares fitter.
 * @params: Array with new parameter values.
 *
 * Sets the values of parameters in the model of a fitter.
 **/
void
gwy_fitter_set_params(GwyFitter *fitter,
                      const gdouble *params)
{
    g_return_if_fail(GWY_IS_FITTER(fitter));
    Fitter *priv = fitter->priv;
    g_return_if_fail(params || !priv->nparam);
    gwy_assign(priv->param_best, params, priv->nparam);
    priv->valid = VALID_PARAMS;
}

/**
 * gwy_fitter_get_params:
 * @fitter: A non-linear least-squares fitter.
 * @params: Array to store the parameter values to, it can be %NULL.
 *
 * Gets the values of parameters in the model of a fitter.
 *
 * Returns: %TRUE if the parameters were set, i.e. the fitter has a valid set
 *          of parameters.
 **/
gboolean
gwy_fitter_get_params(GwyFitter *fitter,
                      gdouble *params)
{
    g_return_val_if_fail(GWY_IS_FITTER(fitter), FALSE);
    Fitter *priv = fitter->priv;
    if (priv->valid < VALID_PARAMS)
        return FALSE;
    if (params && priv->nparam)
        gwy_assign(params, priv->param_best, priv->nparam);
    return TRUE;
}

/**
 * gwy_fitter_get_lambda:
 * @fitter: A non-linear least-squares fitter.
 *
 * Gets the value of Marquardt parameter lambda of a fitter.
 *
 * Returns: The value of Marquardt parameter lambda.
 **/
gdouble
gwy_fitter_get_lambda(GwyFitter *fitter)
{
    g_return_val_if_fail(GWY_IS_FITTER(fitter), -1.0);
    return fitter->priv->lambda;
}

/**
 * gwy_fitter_iter_number:
 * @fitter: A non-linear least-squares fitter.
 *
 * Gets the number of iterations performed in the last fit.
 *
 * Returns: The number of iterations.
 **/
guint
gwy_fitter_iter_number(GwyFitter *fitter)
{
    g_return_val_if_fail(GWY_IS_FITTER(fitter), 0);
    return fitter->priv->iter;
}

/**
 * gwy_fitter_set_funcs:
 * @fitter: A non-linear least-squares fitter.
 * @eval_residuum: Function to calculate the sum of squares.
 * @eval_gradient: Function to calculate the gradient and Hessian.
 *
 * Sets the model functions of a fitter.
 *
 * This is the low-level interface.  In most cases you do not want to calculate
 * Hessian yourself, instead, you want to provide just the function to fit and
 * the data to fit.  See #GwyFitTask for the high-level interface.
 **/
void
gwy_fitter_set_funcs(GwyFitter *fitter,
                     GwyFitterResiduumFunc eval_residuum,
                     GwyFitterGradientFunc eval_gradient)

{
    g_return_if_fail(GWY_IS_FITTER(fitter));
    g_return_if_fail(eval_residuum && eval_gradient);
    Fitter *priv = fitter->priv;
    priv->status = GWY_FITTER_STATUS_NONE;
    priv->eval_residuum = eval_residuum;
    priv->eval_gradient = eval_gradient;
    priv->eval_gradient_matrix = NULL;
}

/**
 * gwy_fitter_set_matrix_funcs:
 * @fitter: A non-linear least-squares fitter.
 * @eval_residuum: Function to calculate the sum of squares.
 * @eval_gradient: Function to calculate the gradient and Hessian.
 *
 * Sets the model functions of a fitter.
 *
 * This is the abstract low-level interface.  In most cases you do not want to
 * calculate Hessian yourself, instead, you want to provide just the function
 * to fit and the data to fit.  See #GwyFitTask for the high-level interface.
 *
 * Even if you want to calculate Hessian yourself, you can usually represent
 * it as an array of doubles.  However, if the Hessian is for instance a huge
 * sparse matrix or it is actually stored in the GPU, #GwyFitter can work with
 * that and does not need to access the matrix elements directly.
 *
 * The abstract matrix must be square and support the following functions
 * to be usable with #GwyFitter:
 * gwy_matrix_n_cols(), gwy_matrix_n_rows(), gwy_matrix_inv_multiply(),
 * gwy_matrix_get_diagonal(), gwy_matrix_set_diagonal().
 **/
void
gwy_fitter_set_matrix_funcs(GwyFitter *fitter,
                            GwyFitterResiduumFunc eval_residuum,
                            GwyFitterMatrixGradientFunc eval_gradient)
{
    g_return_if_fail(GWY_IS_FITTER(fitter));
    g_return_if_fail(eval_residuum && eval_gradient);
    Fitter *priv = fitter->priv;
    priv->status = GWY_FITTER_STATUS_NONE;
    priv->eval_residuum = eval_residuum;
    priv->eval_gradient = NULL;
    priv->eval_gradient_matrix = eval_gradient;
}

/**
 * gwy_fitter_set_matrix:
 * @fitter: A non-linear least-squares fitter.
 * @matrix: (allow-none):
 *          Abstract matrix to use for the Hessian and normal matrix.
 *
 * Sets the abstract matrix used with the low-level interface.
 *
 * The fitter takes a reference to the matrix.  This reference is only released
 * by calling this method with a different matrix (possibly %NULL) or
 * destroying the fitter.
 *
 * Setting the matrix immediately sets the number of parameters to the matrix
 * order.  If the matrix is set to %NULL, the number of parameters is set to
 * zero.  While the low-level matrix functions are used, the number of
 * parameters must not be changed.
 *
 * See gwy_fitter_set_matrix_funcs() for more discussion.
 **/
void
gwy_fitter_set_matrix(GwyFitter *fitter,
                      GwyMatrix *matrix)
{
    g_return_if_fail(GWY_IS_FITTER(fitter));
    Fitter *priv = fitter->priv;
    if (priv->matrix_hessian == matrix)
        return;

    guint nparam = 0;
    if (matrix) {
        nparam = gwy_matrix_n_cols(matrix);
        g_return_if_fail(gwy_matrix_n_rows(matrix) == nparam);
        gwy_matrix_ref(matrix);
    }
    if (priv->matrix_hessian)
        gwy_matrix_unref(priv->matrix_hessian);

    priv->matrix_hessian = matrix;
    fitter_set_n_param(fitter->priv, nparam);
}

/**
 * gwy_fitter_get_matrix:
 * @fitter: A non-linear least-squares fitter.
 *
 * Sets the abstract matrix used with the low-level interface.
 *
 * See gwy_fitter_set_matrix() and gwy_fitter_set_matrix_funcs() for a
 * discussion.
 *
 * Returns: (allow-none):
 *          The matrix used by the low level-interface, if any.
 **/
GwyMatrix*
gwy_fitter_get_matrix(const GwyFitter *fitter)
{
    g_return_val_if_fail(GWY_IS_FITTER(fitter), NULL);
    return fitter->priv->matrix_hessian;
}

/**
 * gwy_fitter_set_constraint:
 * @fitter: A non-linear least-squares fitter.
 * @constrain: Function to check model parameters constraints, it can be %NULL
 *             for no constraints.
 *
 * Sets the constraint function of a fitter.
 **/
void
gwy_fitter_set_constraint(GwyFitter *fitter,
                          GwyFitterConstrainFunc constrain)
{
    g_return_if_fail(GWY_IS_FITTER(fitter));
    fitter->priv->constrain = constrain;
}

/**
 * gwy_fitter_fit:
 * @fitter: A non-linear least-squares fitter.
 * @user_data: Data passed to functions defined in gwy_fitter_set_funcs().
 *
 * Performs a non-linear least-squares fit with a fitter.
 *
 * Returns: %TRUE if the fit terminated normally.  This means after reaching a
 *          limit (maximum of Marquardt parameter, number of iterations,
 *          minimum change between iterations, ...).  If it terminated due
 *          failure to calculate something, %FALSE is returned.
 **/
gboolean
gwy_fitter_fit(GwyFitter *fitter,
               gpointer user_data)
{
    g_return_val_if_fail(GWY_IS_FITTER(fitter), FALSE);
    Fitter *priv = fitter->priv;
    priv->status = GWY_FITTER_STATUS_NONE;
    g_return_val_if_fail(priv->nparam, FALSE);
    g_return_val_if_fail(priv->valid >= VALID_PARAMS, FALSE);
    g_return_val_if_fail(priv->eval_gradient || priv->eval_gradient_matrix,
                         FALSE);
    if (priv->eval_gradient_matrix) {
        GwyMatrix *matrix = priv->matrix_hessian;
        g_return_val_if_fail(matrix, FALSE);
        g_return_val_if_fail(gwy_matrix_n_rows(matrix) == priv->nparam, FALSE);
        g_return_val_if_fail(gwy_matrix_n_cols(matrix) == priv->nparam, FALSE);
    }
    g_return_val_if_fail(priv->eval_residuum, FALSE);
    priv->fitting = TRUE;
    gboolean ok = fitter_minimize(fitter->priv, user_data);
    priv->fitting = FALSE;
    return ok;
}

/**
 * gwy_fitter_residuum:
 * @fitter: A non-linear least-squares fitter.
 *
 * Obtains the sum of squares of a fitter.
 *
 * This functions returns the value corresponding to the best fit.  Use
 * gwy_fitter_eval_residuum() to explicitly calculate the sum of squares of
 * differences.
 *
 * Returns: The sum of squares of differences.  Negative return value means
 *          the sum of squares is not available.
 **/
gdouble
gwy_fitter_residuum(GwyFitter *fitter)
{
    g_return_val_if_fail(GWY_IS_FITTER(fitter), -1.0);
    Fitter *priv = fitter->priv;
    return (priv->valid >= VALID_FUNCTION) ? priv->f_best : -1.0;
}

/**
 * gwy_fitter_eval_residuum:
 * @fitter: A non-linear least-squares fitter.
 * @user_data: Data passed to functions defined in gwy_fitter_set_funcs().
 *
 * Calculates the sum of squares of a fitter.
 *
 * This functions unconditionally recalculates the sum of squares for the
 * current set of parameters.  Use gwy_fitter_residuum() to obtain the
 * value corresponding to the best fit.
 *
 * Returns: The sum of squares of differences.  Negative return value means
 *          it is not possible to calculate it, e.g. due to unset or
 *          out-of-bound parameters.
 **/
gdouble
gwy_fitter_eval_residuum(GwyFitter *fitter,
                         gpointer user_data)
{
    g_return_val_if_fail(GWY_IS_FITTER(fitter), -1.0);
    Fitter *priv = fitter->priv;
    g_return_val_if_fail(priv->eval_residuum, -1.0);
    if (priv->valid < VALID_PARAMS)
        return -1.0;
    gwy_assign(priv->param, priv->param_best, priv->nparam);
    return eval_residuum_with_check(fitter->priv, user_data) ? priv->f : -1.0;
}

/**
 * gwy_fitter_inverse_hessian:
 * @fitter: A non-linear least-squares fitter.
 * @ihessian: Array to store the inverse Hessian to.  The elements are stored
 *            as described in gwy_lower_triangular_matrix_index().
 *
 * Obtains the inverse Hessian of a fitter.
 *
 * The inverse Hessian can be used to calculate the covariance matrix and
 * parameter errors.  The inversion can fail due to numerical errors.  Of
 * course, if the fitting failed it is not possible to calculate the inverse
 * Hessian either.
 *
 * Returns: %TRUE if the inverse Hessian was filled, %FALSE if it was not.
 **/
gboolean
gwy_fitter_inverse_hessian(GwyFitter *fitter,
                           gdouble *ihessian)
{
    g_return_val_if_fail(GWY_IS_FITTER(fitter), FALSE);
    Fitter *priv = fitter->priv;
    if (priv->valid < VALID_HESSIAN)
        return FALSE;
    if (priv->valid < VALID_INV_HESSIAN)
        fitter_invert_hessian(fitter->priv);
    if (priv->valid >= VALID_INV_HESSIAN) {
        if (ihessian)
            gwy_assign(ihessian, priv->inv_hessian, MATRIX_LEN(priv->nparam));
        return TRUE;
    }
    return FALSE;
}


/**
 * SECTION: fitter
 * @title: GwyFitter
 * @short_description: Non-linear least-squares fitting
 *
 * #GwyFitter is a relatively abstract implementation of Marquardt-Levenberg
 * non-linear least-squares fitting algorithm.  It only operates with concepts
 * such as the sum of squares (often called residuum for short below), its
 * gradient and Hessian.
 *
 * This means it does not know how your data look like of what is the fitted
 * function.  Features such as weighting and fixed or linked parameters have
 * to be implemented by the residuum, gradient and Hessian evaluators.
 * #GwyFitTask can supply them for many common cases although if you need
 * something special you can use the #GwyFitter's model-agnostic interface.
 *
 * #GwyFitter has a batch interface (as opposed to iterative), i.e. all the
 * fitting is done in one function call to gwy_fitter_fit().  The fitting
 * can terminate on a number of criterions, tunable using #GwyFitter
 * properties.
 *
 * The sign convention for gradients and generally differences between
 * theoretical and fitted data is theoretical value minus experimental data
 * point.
 **/

/**
 * GwyFitterStatus:
 * @GWY_FITTER_STATUS_NONE: No termination reason.  This occurs if the
 *                          fitter has not been used for fitting at all or
 *                          since its setup has changed.
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
 *                                 parameter changes due to numerical errors.
 * @GWY_FITTER_STATUS_MATRIX_FAILURE: An abstract matrix operation, such as
 *                                    diagonal extraction or setting, failed.
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
 * @user_data: Data passed to gwy_fitter_fit().
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
 * @user_data: Data passed to gwy_fitter_fit().
 *
 * The type of function calculating the gradient and Hessian for #GwyFitter.
 *
 * This is a low-level function type; the function must do all weighting and
 * summing of the contributions of individual data points.
 *
 * The returned Hessian can be degenerated with certain rows and columns,
 * corresponding to fixed parameters, filled with.  The corresponding elements
 * of @gradient must be zero too then.
 *
 * Returns: %TRUE if the evaluation succeeded, %FALSE on failure, e.g. due to
 *          a parameter domain error.  If %FALSE is returned @gradient and
 *          @hessian elements may be left unset or set to bogus values.
 **/

/**
 * GwyFitterMatrixGradientFunc:
 * @param: Values of parameters.  Note the parameters are not modifiable, if
 *         the function needs a modifiable array of parameters it must make a
 *         copy of @param.
 * @gradient: Array of the same length as @param to store the derivatives of
 *            the residuum by individual parameters to.
 * @hessian: Abstract matrix to be updated to the the Hessian.
 * @user_data: Data passed to gwy_fitter_fit().
 *
 * The type of function calculating the gradient and Hessian for the abstract
 * matrix interface of #GwyFitter.
 *
 * See #GwyFitterGradientFunc for some discussion.
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
 * @user_data: Data passed to gwy_fitter_fit().
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
 *
 * Class of non-linear least-squares fitters.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
