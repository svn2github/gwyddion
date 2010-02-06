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

#include "testlibgwy.h"

/***************************************************************************
 *
 * Expr
 *
 ***************************************************************************/

void
test_expr_evaluate(void)
{
    GwyExpr *expr = gwy_expr_new();
    GError *error = NULL;
    gdouble result;
    gboolean ok;

    ok = gwy_expr_evaluate(expr, "1/5 hypot(3 cos 0, sqrt 16)", &result, &error);
    g_assert(ok);
    g_assert(!error);
    g_assert_cmpfloat(fabs(result - 1.0), <, 1e-15);
    g_clear_error(&error);

    ok = gwy_expr_evaluate(expr, "1+1", &result, &error);
    g_assert(ok);
    g_assert(!error);
    g_assert_cmpfloat(result, ==, 2.0);
    g_clear_error(&error);

    ok = gwy_expr_evaluate(expr, "exp atanh (1/2)^2", &result, &error);
    g_assert(ok);
    g_assert(!error);
    g_assert_cmpfloat(fabs(result - 3.0), <, 1e-15);
    g_clear_error(&error);

    ok = gwy_expr_evaluate(expr, "exp lgamma 5/exp lgamma 4", &result, &error);
    g_assert(ok);
    g_assert(!error);
    g_assert_cmpfloat(fabs(result - 4.0), <, 1e-15);
    g_clear_error(&error);

    ok = gwy_expr_evaluate(expr, "-5(-4--3)(-1+2)", &result, &error);
    g_assert(ok);
    g_assert(!error);
    g_assert_cmpfloat(fabs(result - 5.0), <, 1e-15);
    g_clear_error(&error);

    ok = gwy_expr_evaluate(expr, "3^2^2*-3^-2 - hypot hypot 1,2,2",
                           &result, &error);
    g_assert(ok);
    g_assert(!error);
    g_assert_cmpfloat(fabs(result - 6.0), <, 1e-15);
    g_clear_error(&error);

    g_object_unref(expr);
}

void
test_expr_vector(void)
{
    GwyExpr *expr = gwy_expr_new();
    GError *error = NULL;
    gboolean ok;

    ok = gwy_expr_define_constant(expr, "π", G_PI, &error);
    g_assert(ok);
    g_assert(!error);
    g_clear_error(&error);

    ok = gwy_expr_compile(expr, "1+1", &error);
    g_assert(ok);
    g_assert(!error);
    g_clear_error(&error);

    gsize nvars1 = gwy_expr_get_variables(expr, NULL);
    g_assert_cmpuint(nvars1, ==, 1);

    ok = gwy_expr_compile(expr, "sqrt(a^2+π*β/c₂)-sqrt(a^2+π*c₂/β)", &error);
    g_assert(ok);
    g_assert(!error);
    g_clear_error(&error);

    gsize nvars4 = gwy_expr_get_variables(expr, NULL);
    g_assert_cmpuint(nvars4, ==, 4);

    gsize n = 10000;
    gdouble **input = g_new0(gdouble*, nvars4);
    for (gsize i = 1; i < nvars4; i++) {
        input[i] = g_new(gdouble, n);
        for (gsize j = 0; j < n; j++) {
            input[i][j] = j/(0.1 + i);
        }
    }
    gdouble *result = g_new(gdouble, n);

    const gchar *varnames[] = { "a", "β", "c₂" };
    guint indices[G_N_ELEMENTS(varnames)];
    gsize extravars = gwy_expr_resolve_variables(expr, G_N_ELEMENTS(varnames),
                                                 varnames, indices);
    g_assert_cmpuint(extravars, ==, 0);
    g_assert_cmpuint(indices[0], >=, 1);
    g_assert_cmpuint(indices[0], <=, 3);
    g_assert_cmpuint(indices[1], >=, 1);
    g_assert_cmpuint(indices[1], <=, 3);
    g_assert_cmpuint(indices[2], >=, 1);
    g_assert_cmpuint(indices[2], <=, 3);
    g_assert_cmpuint(indices[0], !=, indices[1]);
    g_assert_cmpuint(indices[1], !=, indices[2]);
    g_assert_cmpuint(indices[2], !=, indices[0]);

    // Execute normally
    gwy_expr_vector_execute(expr, n, (const gdouble**)input, result);
    for (gsize j = 0; j < n; j++) {
        gdouble a = input[indices[0]][j];
        gdouble beta = input[indices[1]][j];
        gdouble c2 = input[indices[2]][j];
        gdouble values[] = { 0, input[1][j], input[2][j], input[3][j] };
        gdouble expected = sqrt(a*a+G_PI*beta/c2) - sqrt(a*a+G_PI*c2/beta);
        gdouble evaluated = gwy_expr_execute(expr, values);
        if (!isnan(expected) && isnan(result[j])) {
            g_assert_cmpfloat(fabs(expected - result[j]), <, 1e-15);
            g_assert_cmpfloat(fabs(expected - evaluated), <, 1e-15);
        }
    }
    // Execute ovewriting one of the input fields
    gwy_expr_vector_execute(expr, n, (const gdouble**)input, input[1]);
    // Compare.
    g_assert_cmpint(memcmp(input[1], result, n*sizeof(gdouble)), ==, 0);

    for (gsize i = 1; i < nvars4; i++)
        g_free(input[i]);
    g_free(input);
    g_free(result);

    g_object_unref(expr);
}

void
test_expr_garbage(void)
{
    static const gchar *tokens[] = {
        "~", "+", "-", "*", "/", "%", "^", "(", "(", "(", ")", ")", ")",  ",",
        "abs", "acos", "acosh", "asin", "asinh", "atan", "atan2", "atanh",
        "cbrt", "ceil", "cos", "cosh", "erf", "erfc", "exp", "exp10", "exp2",
        "floor", "hypot", "lgamma", "ln", "log", "log10", "log2", "max", "min",
        "mod", "pow", "sin", "sinh", "sqrt", "step", "tan", "tanh",
    };
    GwyExpr *expr = gwy_expr_new();

    gsize n = 10000;
    GString *garbage = g_string_new(NULL);
    GRand *rng = g_rand_new();
    guint count = 0;

    g_rand_set_seed(rng, 42);

    /* No checks.  The goal is not to crash... */
    for (gsize i = 0; i < n; i++) {
        GError *error = NULL;
        gdouble result;
        gboolean ok;
        gsize ntoks = g_rand_int_range(rng, 0, 10)
                       + g_rand_int_range(rng, 0, 20);

        g_string_truncate(garbage, 0);
        for (gsize j = 0; j < ntoks; j++) {
            gsize what = g_rand_int_range(rng, 0, G_N_ELEMENTS(tokens) + 5);

            if (g_rand_int_range(rng, 0, 10))
                g_string_append_c(garbage, ' ');

            if (what == G_N_ELEMENTS(tokens))
                g_string_append_c(garbage, g_rand_int_range(rng, 1, 0x100));
            else if (what < G_N_ELEMENTS(tokens))
                g_string_append(garbage, tokens[what]);
            else
                g_string_append_printf(garbage, "%g", -log(g_rand_double(rng)));
        }

        ok = gwy_expr_evaluate(expr, garbage->str, &result, &error);
        g_assert(ok || error);
        g_clear_error(&error);
        if (ok)
            count++;
    }
    g_assert_cmpuint(count, ==, 38);

    g_string_free(garbage, TRUE);
    g_rand_free(rng);
    g_object_unref(expr);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
