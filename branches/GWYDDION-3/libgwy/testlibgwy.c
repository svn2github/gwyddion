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

#undef G_DISABLE_ASSERT
#define GWY_MATH_POLLUTE_NAMESPACE 1
#include <gwyconfig.h>
#include <string.h>
#include <stdlib.h>
#include "libgwy/libgwy.h"

#ifdef HAVE_VALGRIND
#include <valgrind/valgrind.h>
#else
#define RUNNING_ON_VALGRIND 0
#endif

/***************************************************************************
 *
 * Version
 *
 ***************************************************************************/

static void
test_version(void)
{
    /* These must hold. */
    g_assert_cmpuint(GWY_VERSION_MAJOR, ==, gwy_version_major());
    g_assert_cmpuint(GWY_VERSION_MINOR, ==, gwy_version_minor());
    g_assert_cmpstr(GWY_VERSION_STRING, ==, gwy_version_string());
}

/***************************************************************************
 *
 * String functions
 *
 ***************************************************************************/

/* FIXME: On GNU systems we test the libc memmem() here.  On the other hand,
 * this is the implementation we are going to use, so let's test it. */
static void
test_memmem(void)
{
    static const gchar haystack[] = {
        1, 2, 0, 1, 2, 0, 0, 2, 2, 1, 2, 2,
    };
    static const gchar *needle = haystack;
    static const gchar eye[3] = { 1, 2, 2 };
    static const gchar pin[3] = { 2, 0, 2 };

    /* Successes */
    g_assert(gwy_memmem(NULL, 0, NULL, 0) == NULL);
    g_assert(gwy_memmem(haystack, 0, NULL, 0) == haystack);
    for (gsize n = 0; n <= sizeof(haystack); n++)
        g_assert(gwy_memmem(haystack, sizeof(haystack), needle, n) == haystack);

    g_assert(gwy_memmem(haystack, sizeof(haystack), needle + 4, 2)
             == haystack + 1);
    g_assert(gwy_memmem(haystack, sizeof(haystack), needle + 4, 3)
             == haystack + 4);
    g_assert(gwy_memmem(haystack, sizeof(haystack), eye, sizeof(eye))
             == haystack + sizeof(haystack) - sizeof(eye));

    /* Failures */
    g_assert(gwy_memmem(NULL, 0, needle, 1) == NULL);
    g_assert(gwy_memmem(haystack, sizeof(haystack)-1, needle, sizeof(haystack))
             == NULL);
    g_assert(gwy_memmem(haystack, sizeof(haystack), pin, sizeof(pin)) == NULL);
    g_assert(gwy_memmem(haystack, sizeof(haystack)-1, eye, sizeof(eye))
             == NULL);
}

G_GNUC_NULL_TERMINATED
static void
test_next_line_check(const gchar *text, ...)
{
    gchar *buf = g_strdup(text);
    gchar *p = buf;
    va_list ap;
    const gchar *expected = NULL;

    va_start(ap, text);
    for (gchar *line = gwy_str_next_line(&p);
         line;
         line = gwy_str_next_line(&p)) {
        expected = va_arg(ap, const gchar*);
        g_assert(expected != NULL);
        g_assert_cmpstr(line, ==, expected);
    }
    expected = va_arg(ap, const gchar*);
    g_assert(expected == NULL);
    va_end(ap);
    g_free(buf);
}

static void
test_next_line(void)
{
    test_next_line_check(NULL, NULL);
    test_next_line_check("", NULL);
    test_next_line_check("\r", "", NULL);
    test_next_line_check("\n", "", NULL);
    test_next_line_check("\r\n", "", NULL);
    test_next_line_check("\r\r", "", "", NULL);
    test_next_line_check("\n\n", "", "", NULL);
    test_next_line_check("\r\n\r\n", "", "", NULL);
    test_next_line_check("\n\r\n", "", "", NULL);
    test_next_line_check("\r\r\n", "", "", NULL);
    test_next_line_check("\r\n\r", "", "", NULL);
    test_next_line_check("\n\r\r", "", "", "", NULL);
    test_next_line_check("X", "X", NULL);
    test_next_line_check("X\n", "X", NULL);
    test_next_line_check("X\r", "X", NULL);
    test_next_line_check("X\r\n", "X", NULL);
    test_next_line_check("\nX", "", "X", NULL);
    test_next_line_check("\rX", "", "X", NULL);
    test_next_line_check("\r\nX", "", "X", NULL);
    test_next_line_check("X\r\r", "X", "", NULL);
    test_next_line_check("X\n\n", "X", "", NULL);
    test_next_line_check("X\nY\rZ", "X", "Y", "Z", NULL);
}

/***************************************************************************
 *
 * Packing and unpacking
 *
 ***************************************************************************/

static void
test_pack(void)
{
    enum { expected_size = 57 };
    struct {
        gdouble d, dp;
        gint32 i;
        gint16 a1, a2;
        gint64 q;
        guint64 qu;
        gchar chars[5];
    } out, in = {
        /* XXX: The Pascal format allows for some rounding errors at this
         * moment.  Use a test number that is expected to work exactly. */
        G_PI, 1234.5,
        -3,
        -1, 73,
        G_GINT64_CONSTANT(0x12345678),
        G_GUINT64_CONSTANT(0xfedcba98),
        "test",
    };
    gchar format[] = ". x d r i 2h 13x q Q 5S";
    const gchar *byte_orders = "<>";
    guchar *buf;
    gsize packed_size, consumed;

    buf = g_new(guchar, expected_size);

    while (*byte_orders) {
        format[0] = *byte_orders;
        g_assert(gwy_pack_size(format, NULL) == expected_size);
        packed_size = gwy_pack(format, buf, expected_size, NULL,
                               &in.d, &in.dp,
                               &in.i,
                               &in.a1, &in.a2,
                               &in.q,
                               &in.qu,
                               in.chars);
        g_assert(packed_size == expected_size);
        consumed = gwy_unpack(format, buf, expected_size, NULL,
                              &out.d, &out.dp,
                              &out.i,
                              &out.a1, &out.a2,
                              &out.q,
                              &out.qu,
                              out.chars);
        g_assert(consumed == expected_size);
        g_assert(out.d == in.d);
        g_assert(out.dp == in.dp);
        g_assert(out.i == in.i);
        g_assert(out.a1 == in.a1);
        g_assert(out.a2 == in.a2);
        g_assert(out.q == in.q);
        g_assert(out.qu == in.qu);
        g_assert(memcmp(out.chars, in.chars, sizeof(in.chars)) == 0);

        byte_orders++;
    }

    g_free(buf);
}

/***************************************************************************
 *
 * Math sorting
 *
 ***************************************************************************/

/* Return %TRUE if @array is ordered. */
static gboolean
test_sort_is_strictly_ordered(const gdouble *array, gsize n)
{
    gsize i;

    for (i = 1; i < n; i++, array++) {
        if (array[0] >= array[1])
            return FALSE;
    }
    return TRUE;
}

/* Return %TRUE if @array is ordered and its items correspond to @orig_array
 * items with permutations given by @index_array. */
static gboolean
test_sort_is_ordered_with_index(const gdouble *array, const guint *index_array,
                                const gdouble *orig_array, gsize n)
{
    gsize i;

    for (i = 0; i < n; i++) {
        if (index_array[i] >= n)
            return FALSE;
        if (array[i] != orig_array[index_array[i]])
            return FALSE;
    }
    return TRUE;
}

static void
test_math_sort(void)
{
    gsize n, i, nmin = 0, nmax = 65536;
    gdouble *array, *orig_array;
    guint *index_array;

    if (g_test_quick())
        nmax = 8192;

    array = g_new(gdouble, nmax);
    orig_array = g_new(gdouble, nmax);
    index_array = g_new(guint, nmax);
    for (n = nmin; n < nmax; n = 7*n/6 + 1) {
        for (i = 0; i < n; i++)
            orig_array[i] = sin(n/G_SQRT2 + 1.618*i);

        memcpy(array, orig_array, n*sizeof(gdouble));
        gwy_math_sort(array, NULL, n);
        g_assert(test_sort_is_strictly_ordered(array, n));

        memcpy(array, orig_array, n*sizeof(gdouble));
        for (i = 0; i < n; i++)
            index_array[i] = i;
        gwy_math_sort(array, index_array, n);
        g_assert(test_sort_is_strictly_ordered(array, n));
        g_assert(test_sort_is_ordered_with_index(array, index_array,
                                                 orig_array, n));
    }
    g_free(index_array);
    g_free(orig_array);
    g_free(array);
}

/***************************************************************************
 *
 * Cholesky
 *
 ***************************************************************************/

#define CHOLESKY_MATRIX_LEN(n) (((n) + 1)*(n)/2)
#define SLi gwy_lower_triangular_matrix_index

/* Square a triangular matrix. */
static void
test_cholesky_matsquare(gdouble *a, const gdouble *d, guint n)
{
    for (guint i = 0; i < n; i++) {
        for (guint j = 0; j <= i; j++) {
            SLi(a, i, j) = SLi(d, i, 0) * SLi(d, j, 0);
            for (guint k = 1; k <= MIN(i, j); k++)
                SLi(a, i, j) += SLi(d, i, k) * SLi(d, j, k);
        }
    }
}

/* Multiply two symmetrical matrices (NOT triangular). */
static void
test_cholesky_matmul(gdouble *a, const gdouble *d1, const gdouble *d2, guint n)
{
    for (guint i = 0; i < n; i++) {
        for (guint j = 0; j <= i; j++) {
            SLi(a, i, j) = 0.0;
            for (guint k = 0; k < n; k++) {
                guint ik = MAX(i, k);
                guint ki = MIN(i, k);
                guint jk = MAX(j, k);
                guint kj = MIN(j, k);
                SLi(a, i, j) += SLi(d1, ik, ki) * SLi(d2, jk, kj);
            }
        }
    }
}

/* Multiply a vector with a symmetrical matrix (NOT triangular) from left. */
static void
test_cholesky_matvec(gdouble *a, const gdouble *m, const gdouble *v, guint n)
{
    for (guint i = 0; i < n; i++) {
        a[i] = 0.0;
        for (guint j = 0; j < n; j++) {
            guint ij = MAX(i, j);
            guint ji = MIN(i, j);
            a[i] += SLi(m, ij, ji) * v[j];
        }
    }
}

/* Generate the decomposition.  As long as it has positive numbers on the
 * diagonal, the matrix is positive-definite.   Though maybe not numerically.
 */
static void
test_cholesky_make_matrix(gdouble *d,
                          guint n,
                          GRand *rng)
{
    for (guint j = 0; j < n; j++) {
        SLi(d, j, j) = exp(g_rand_double_range(rng, -5.0, 5.0));
        for (guint k = 0; k < j; k++) {
            SLi(d, j, k) = (g_rand_double_range(rng, -1.0, 1.0)
                            + g_rand_double_range(rng, -1.0, 1.0)
                            + g_rand_double_range(rng, -1.0, 1.0))/5.0;
        }
    }
}

static void
test_cholesky_make_vector(gdouble *d,
                          guint n,
                          GRand *rng)
{
    for (guint j = 0; j < n; j++)
        d[j] = g_rand_double_range(rng, -1.0, 1.0)
               * exp(g_rand_double_range(rng, -5.0, 5.0));
}

/* Note the precision checks are very tolerant as the matrices we generate
 * are not always well-conditioned. */
static void
test_math_cholesky(void)
{
    guint nmax = 8, niter = 50;
    GRand *rng = g_rand_new();

    /* Make it realy reproducible. */
    g_rand_set_seed(rng, 42);

    for (guint n = 1; n < nmax; n++) {
        guint matlen = CHOLESKY_MATRIX_LEN(n);
        /* Use descriptive names for less cryptic g_assert() messages. */
        gdouble *matrix = g_new(gdouble, matlen);
        gdouble *decomp = g_new(gdouble, matlen);
        gdouble *inverted = g_new(gdouble, matlen);
        gdouble *unity = g_new(gdouble, matlen);
        gdouble *vector = g_new(gdouble, n);
        gdouble *solution = g_new(gdouble, n);

        for (guint iter = 0; iter < niter; iter++) {
            test_cholesky_make_matrix(decomp, n, rng);
            test_cholesky_make_vector(vector, n, rng);
            gdouble eps;

            /* Decomposition */
            test_cholesky_matsquare(matrix, decomp, n);
            test_cholesky_matvec(solution, matrix, vector, n);
            g_assert(gwy_cholesky_decompose(matrix, n));
            for (guint j = 0; j < matlen; j++) {
                eps = exp10(j - 15.0);
                g_assert_cmpfloat(fabs(matrix[j] - decomp[j]),
                                  <=,
                                  eps * (fabs(matrix[j]) + fabs(decomp[j])));
            }

            /* Solution */
            eps = exp10(n - 11.0);
            gwy_cholesky_solve(matrix, solution, n);
            for (guint j = 0; j < n; j++) {
                g_assert_cmpfloat(fabs(solution[j] - vector[j]),
                                  <=,
                                  eps * (fabs(solution[j]) + fabs(vector[j])));
            }

            /* Inversion */
            test_cholesky_matsquare(matrix, decomp, n);
            memcpy(inverted, matrix, matlen*sizeof(gdouble));
            g_assert(gwy_cholesky_invert(inverted, n));
            /* Multiplication with inverted must give unity */
            test_cholesky_matmul(unity, matrix, inverted, n);
            for (guint j = 0; j < n; j++) {
                eps = exp10(n - 10.0);
                for (guint k = 0; k < j; k++) {
                    g_assert_cmpfloat(fabs(SLi(unity, j, k)), <=, eps);
                }
                eps = exp10(n - 11.0);
                g_assert_cmpfloat(fabs(SLi(unity, j, j) - 1.0), <=, eps);
            }
            /* Double inversion must give the original */
            eps = exp10(n - 12.0);
            g_assert(gwy_cholesky_invert(inverted, n));
            for (guint j = 0; j < matlen; j++) {
                g_assert_cmpfloat(fabs(matrix[j] - inverted[j]),
                                  <=,
                                  eps * (fabs(matrix[j]) + fabs(inverted[j])));
            }
        }

        g_free(vector);
        g_free(solution);
        g_free(matrix);
        g_free(decomp);
        g_free(inverted);
        g_free(unity);
    }

    g_rand_free(rng);
}

/***************************************************************************
 *
 * Linear algebra
 *
 ***************************************************************************/

/* Multiply a vector with a matrix from left. */
static void
test_linalg_matvec(gdouble *a, const gdouble *m, const gdouble *v, guint n)
{
    for (guint i = 0; i < n; i++) {
        a[i] = 0.0;
        for (guint j = 0; j < n; j++)
            a[i] += m[i*n + j] * v[j];
    }
}

/* Multiply two square matrices. */
static void
test_linalg_matmul(gdouble *a, const gdouble *d1, const gdouble *d2, guint n)
{
    for (guint i = 0; i < n; i++) {
        for (guint j = 0; j < n; j++) {
            a[i*n + j] = 0.0;
            for (guint k = 0; k < n; k++)
                a[i*n + j] += d1[i*n + k] * d2[k*n + j];
        }
    }
}

/* Note the precision checks are very tolerant as the matrices we generate
 * are not always well-conditioned. */
static void
test_math_linalg(void)
{
    guint nmax = 8, niter = 50;
    GRand *rng = g_rand_new();

    /* Make it realy reproducible. */
    g_rand_set_seed(rng, 42);

    for (guint n = 1; n < nmax; n++) {
        /* Use descriptive names for less cryptic g_assert() messages. */
        gdouble *matrix = g_new(gdouble, n*n);
        gdouble *unity = g_new(gdouble, n*n);
        gdouble *decomp = g_new(gdouble, n*n);
        gdouble *vector = g_new(gdouble, n);
        gdouble *rhs = g_new(gdouble, n);
        gdouble *solution = g_new(gdouble, n);

        for (guint iter = 0; iter < niter; iter++) {
            test_cholesky_make_vector(matrix, n*n, rng);
            test_cholesky_make_vector(vector, n, rng);
            gdouble eps;

            /* Solution */
            test_linalg_matvec(rhs, matrix, vector, n);
            memcpy(decomp, matrix, n*n*sizeof(gdouble));
            g_assert(gwy_linalg_solve(decomp, rhs, solution, n));
            eps = exp10(n - 13.0);
            for (guint j = 0; j < n; j++) {
                g_assert_cmpfloat(fabs(solution[j] - vector[j]),
                                  <=,
                                  eps * (fabs(solution[j]) + fabs(vector[j])));
            }
            /* Inversion */
            memcpy(unity, matrix, n*n*sizeof(gdouble));
            g_assert(gwy_linalg_invert(unity, decomp, n));
            /* Multiplication with inverted must give unity */
            test_linalg_matmul(unity, matrix, decomp, n);
            for (guint j = 0; j < n; j++) {
                g_assert_cmpfloat(fabs(unity[j*n + j] - 1.0), <=, eps);
                for (guint i = 0; i < n; i++) {
                    if (i == j)
                        continue;
                    g_assert_cmpfloat(fabs(unity[j*n + i]), <=, eps);
                }
            }
            /* Double inversion must give the original */
            g_assert(gwy_linalg_invert(decomp, unity, n));
            for (guint j = 0; j < n; j++) {
                for (guint i = 0; i < n; i++) {
                    g_assert_cmpfloat(fabs(matrix[j] - unity[j]),
                                      <=,
                                      eps * (fabs(matrix[j]) + fabs(unity[j])));
                }
            }
        }

        g_free(vector);
        g_free(rhs);
        g_free(solution);
        g_free(unity);
        g_free(matrix);
        g_free(decomp);
    }

    g_rand_free(rng);
}

/***************************************************************************
 *
 * Interpolation
 *
 ***************************************************************************/

static void
test_interpolation_constant(GwyInterpolationType interpolation,
                            gdouble value)
{
    guint support_len = gwy_interpolation_get_support_size(interpolation);
    g_assert_cmpint(support_len, >, 0);

    gdouble data[support_len];
    for (guint i = 0; i < support_len; i++)
        data[i] = value;

    if (!gwy_interpolation_has_interpolating_basis(interpolation))
        gwy_interpolation_resolve_coeffs_1d(support_len, data, interpolation);

    for (gdouble x = 0.0; x < 1.0; x += 0.0618) {
        gdouble ivalue = gwy_interpolate_1d(x, data, interpolation);
        g_assert_cmpfloat(fabs(ivalue - value),
                          <=,
                          1e-15 * (fabs(ivalue) + fabs(value)));
    }
}

static void
test_interpolation_linear(GwyInterpolationType interpolation,
                          gdouble a,
                          gdouble b)
{
    guint support_len = gwy_interpolation_get_support_size(interpolation);
    g_assert_cmpint(support_len, >, 0);
    /* This is just an implementation assertion, not consistency check. */
    g_assert_cmpint(support_len % 2, ==, 0);

    gdouble data[support_len];
    for (guint i = 0; i < support_len; i++) {
        gdouble origin = support_len - support_len/2 - 1.0;
        data[i] = (i - origin)*(b - a) + a;
    }

    for (gdouble x = 0.0; x < 1.0; x += 0.0618) {
        gdouble value = x*b + (1.0 - x)*a;
        gdouble ivalue = gwy_interpolate_1d(x, data, interpolation);
        g_assert_cmpfloat(fabs(ivalue - value),
                          <=,
                          1e-14 * (fabs(ivalue) + fabs(value)));
    }
}

static void
test_interpolation(void)
{
    GwyInterpolationType first_const = GWY_INTERPOLATION_ROUND;
    GwyInterpolationType last = GWY_INTERPOLATION_SCHAUM;

    for (GwyInterpolationType interp = first_const; interp <= last; interp++) {
        test_interpolation_constant(interp, 1.0);
        test_interpolation_constant(interp, 0.0);
        test_interpolation_constant(interp, -1.0);
    }

    /* FIXME: The interpolations with non-interpolating basis reproduce
     * the linear function only on infinite interval.  How to check them? */
    GwyInterpolationType reproducing_linear[] = {
        GWY_INTERPOLATION_LINEAR,
        GWY_INTERPOLATION_KEYS,
        GWY_INTERPOLATION_SCHAUM,
    };
    for (guint i = 0; i < G_N_ELEMENTS(reproducing_linear); i++) {
        test_interpolation_linear(reproducing_linear[i], 1.0, 2.0);
        test_interpolation_linear(reproducing_linear[i], 1.0, -1.0);
    }
}

/***************************************************************************
 *
 * Error lists
 *
 ***************************************************************************/

static void
test_error_list(void)
{
    GwyErrorList *errlist = NULL;
    GError *err;

    /* A simple error. */
    gwy_error_list_add(&errlist, GWY_PACK_ERROR, GWY_PACK_ERROR_FORMAT,
                       "Just testing...");
    g_assert_cmpuint(g_slist_length(errlist), ==, 1);

    /* Several errors. */
    gwy_error_list_add(&errlist, GWY_PACK_ERROR, GWY_PACK_ERROR_FORMAT,
                       "Just testing %d...", 2);
    g_assert_cmpuint(g_slist_length(errlist), ==, 2);
    /* The latter must be on top. */
    err = errlist->data;
    g_assert_cmpuint(err->domain, ==, GWY_PACK_ERROR);
    g_assert_cmpuint(err->code, ==, GWY_PACK_ERROR_FORMAT);
    g_assert_cmpstr(err->message, ==, "Just testing 2...");

    /* Destruction. */
    gwy_error_list_clear(&errlist);
    g_assert_cmpuint(g_slist_length(errlist), ==, 0);

    /* Propagation */
    gwy_error_list_propagate(&errlist, NULL);
    g_assert_cmpuint(g_slist_length(errlist), ==, 0);
    err = g_error_new(GWY_PACK_ERROR, GWY_PACK_ERROR_FORMAT,
                      "Just testing 3...");
    gwy_error_list_propagate(&errlist, err);
    g_assert_cmpuint(g_slist_length(errlist), ==, 1);
    gwy_error_list_clear(&errlist);

    /* Ignoring errors. */
    gwy_error_list_add(NULL, GWY_PACK_ERROR, GWY_PACK_ERROR_FORMAT,
                       "Ignored error");
    err = g_error_new(GWY_PACK_ERROR, GWY_PACK_ERROR_FORMAT,
                      "Ignored error");
    gwy_error_list_propagate(NULL, err);
    gwy_error_list_clear(NULL);
}

/***************************************************************************
 *
 * Serialization and deserialization
 *
 ***************************************************************************/

#define GWY_TYPE_SER_TEST \
    (gwy_ser_test_get_type())
#define GWY_SER_TEST(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_SER_TEST, GwySerTest))
#define GWY_IS_SER_TEST(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_SER_TEST))
#define GWY_SER_TEST_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_SER_TEST, GwySerTestClass))

#define gwy_ser_test_duplicate(ser_test) \
        (GWY_SER_TEST(gwy_serializable_duplicate(GWY_SERIALIZABLE(ser_test))))

GType gwy_ser_test_get_type(void) G_GNUC_CONST;

typedef struct _GwySerTest      GwySerTest;
typedef struct _GwySerTestClass GwySerTestClass;

struct _GwySerTestClass {
    GObjectClass g_object_class;
};

struct _GwySerTest {
    GObject g_object;

    guint len;
    gboolean flag;
    gdouble *data;
    gchar *s;
    guchar raw[4];
    gdouble dbl;
    gint16 i16;
    gint32 i32;
    gint64 i64;
    gchar **strlist;
    GwySerTest *child;

    /* Testing stuff. */
    gint done_called;
};

static gsize
gwy_ser_test_n_items(GwySerializable *serializable)
{
    GwySerTest *sertest = GWY_SER_TEST(serializable);

    return 10 + (sertest->child
                 ? gwy_serializable_n_items(GWY_SERIALIZABLE(sertest->child))
                 : 0);
}

// The remaining members get zero-initialized which saves us from doing it.
static const GwySerializableItem default_items[] = {
    /*0*/ { .name = "flag",  .ctype = GWY_SERIALIZABLE_BOOLEAN,      },
    /*1*/ { .name = "data",  .ctype = GWY_SERIALIZABLE_DOUBLE_ARRAY, },
    /*2*/ { .name = "s",     .ctype = GWY_SERIALIZABLE_STRING,       },
    /*3*/ { .name = "raw",   .ctype = GWY_SERIALIZABLE_INT8_ARRAY,   },
    /*4*/ { .name = "child", .ctype = GWY_SERIALIZABLE_OBJECT,       },
    /*5*/ { .name = "dbl",   .ctype = GWY_SERIALIZABLE_DOUBLE,       },
    /*6*/ { .name = "i16",   .ctype = GWY_SERIALIZABLE_INT16,        },
    /*7*/ { .name = "i32",   .ctype = GWY_SERIALIZABLE_INT32,        },
    /*8*/ { .name = "i64",   .ctype = GWY_SERIALIZABLE_INT64,        },
    /*9*/ { .name = "ss",    .ctype = GWY_SERIALIZABLE_STRING_ARRAY, },
};

#define add_item(id) \
    g_return_val_if_fail(items->len - items->n, 0); \
    items->items[items->n++] = it[id]; \
    n_items++

static gsize
gwy_ser_test_itemize(GwySerializable *serializable,
                     GwySerializableItems *items)
{
    GwySerTest *sertest = GWY_SER_TEST(serializable);
    GwySerializableItem it[G_N_ELEMENTS(default_items)];
    gsize n_items = 0;

    memcpy(it, default_items, sizeof(default_items));

    if (sertest->flag) {
        it[0].value.v_boolean = sertest->flag;
        add_item(0);
    }

    if (sertest->data) {
        it[1].value.v_double_array = sertest->data;
        it[1].array_size = sertest->len;
        add_item(1);
    }

    if (sertest->s) {
        it[2].value.v_string = sertest->s;
        add_item(2);
    }

    it[3].value.v_uint8_array = sertest->raw;
    it[3].array_size = 4;
    add_item(3);

    if (sertest->child) {
        it[4].value.v_object = G_OBJECT(sertest->child);
        add_item(4);
        gwy_serializable_itemize(GWY_SERIALIZABLE(sertest->child), items);
    }

    if (sertest->dbl != G_LN2) {
        it[5].value.v_double = sertest->dbl;
        add_item(5);
    }

    if (sertest->i16) {
        it[6].value.v_int16 = sertest->i16;
        add_item(6);
    }

    if (sertest->i32) {
        it[7].value.v_int32 = sertest->i32;
        add_item(7);
    }

    if (sertest->i64) {
        it[8].value.v_int64 = sertest->i64;
        add_item(8);
    }

    if (sertest->strlist) {
        it[9].value.v_string_array = sertest->strlist;
        it[9].array_size = g_strv_length(sertest->strlist);
        add_item(9);
    }

    sertest->done_called--;

    return n_items;
}

static gboolean
gwy_ser_test_construct(GwySerializable *serializable,
                       GwySerializableItems *items,
                       GwyErrorList **error_list)
{
    GwySerializableItem it[G_N_ELEMENTS(default_items)];
    gpointer child;

    memcpy(it, default_items, sizeof(default_items));
    gwy_deserialize_filter_items(it, G_N_ELEMENTS(it), items, "GwySerTest",
                                 error_list);

    if (it[3].array_size != 4) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_INVALID,
                           "Item ‘raw’ has %" G_GSIZE_FORMAT " bytes "
                           "instead of 4.",
                           it[3].array_size);
        goto fail;
    }
    if ((child = it[4].value.v_object) && !GWY_IS_SER_TEST(child)) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_INVALID,
                           "Item ‘child’ is %s instead of GwySerTest.",
                           G_OBJECT_TYPE_NAME(child));
        goto fail;
    }

    GwySerTest *sertest = GWY_SER_TEST(serializable);

    sertest->flag = it[0].value.v_boolean;
    sertest->len  = it[1].array_size;
    sertest->data = it[1].value.v_double_array;
    sertest->s    = it[2].value.v_string;
    sertest->dbl  = it[5].value.v_double;
    sertest->i16  = it[6].value.v_int16;
    sertest->i32  = it[7].value.v_int32;
    sertest->i64  = it[8].value.v_int64;
    memcpy(sertest->raw, it[3].value.v_uint8_array, 4);
    g_free(it[3].value.v_uint8_array);
    sertest->child = child;
    if (it[9].value.v_string_array) {
        sertest->strlist = g_new0(gchar*, it[9].array_size + 1);
        memcpy(sertest->strlist, it[9].value.v_string_array,
               it[9].array_size*sizeof(gchar*));
        g_free(it[9].value.v_string_array);
        it[9].value.v_string_array = NULL;
        it[9].array_size = 0;
    }

    return TRUE;

fail:
    GWY_FREE(it[1].value.v_double_array);
    GWY_FREE(it[2].value.v_string);
    GWY_FREE(it[3].value.v_uint8_array);
    GWY_OBJECT_UNREF(it[4].value.v_object);
    if (it[9].value.v_string_array) {
        for (gsize i = 0; i < it[9].array_size; i++)
            g_free(it[9].value.v_string_array[i]);
        g_free(it[9].value.v_string_array);
        it[9].value.v_string_array = NULL;
        it[9].array_size = 0;
    }

    return FALSE;
}

static void
gwy_ser_test_done(GwySerializable *serializable)
{
    GwySerTest *sertest = GWY_SER_TEST(serializable);

    sertest->done_called++;
}

GwySerTest*
gwy_ser_test_new_filled(gboolean flag,
                        const gdouble *data,
                        guint ndata,
                        const gchar *str,
                        guint32 raw)
{
    GwySerTest *sertest = g_object_newv(GWY_TYPE_SER_TEST, 0, NULL);

    sertest->flag = flag;
    sertest->len = ndata;
    if (sertest->len)
        sertest->data = g_memdup(data, ndata*sizeof(gdouble));
    sertest->s = g_strdup(str);
    memcpy(sertest->raw, &raw, 4);

    return sertest;
}

static GObject*
gwy_ser_test_duplicate_(GwySerializable *serializable)
{
    GwySerTest *sertest = GWY_SER_TEST(serializable);
    GwySerTest *copy = gwy_ser_test_new_filled(sertest->flag,
                                               sertest->data, sertest->len,
                                               sertest->s, 0);
    copy->dbl = sertest->dbl;
    copy->i16 = sertest->i16;
    copy->i32 = sertest->i32;
    copy->i64 = sertest->i64;
    memcpy(copy->raw, sertest->raw, 4);
    copy->strlist = g_strdupv(sertest->strlist);
    copy->child = gwy_ser_test_duplicate(sertest->child);

    return G_OBJECT(copy);
}

static void
gwy_ser_test_serializable_init(GwySerializableInterface *iface)
{
    iface->n_items   = gwy_ser_test_n_items;
    iface->itemize   = gwy_ser_test_itemize;
    iface->done      = gwy_ser_test_done;
    iface->construct = gwy_ser_test_construct;
    iface->duplicate = gwy_ser_test_duplicate_;
    /*
    iface->assign = gwy_ser_test_assign_;
    */
}

G_DEFINE_TYPE_EXTENDED
    (GwySerTest, gwy_ser_test, G_TYPE_OBJECT, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_ser_test_serializable_init))

static void
gwy_ser_test_dispose(GObject *object)
{
    GwySerTest *sertest = GWY_SER_TEST(object);

    GWY_OBJECT_UNREF(sertest->child);
    G_OBJECT_CLASS(gwy_ser_test_parent_class)->dispose(object);
}

static void
gwy_ser_test_finalize(GObject *object)
{
    GwySerTest *sertest = GWY_SER_TEST(object);

    GWY_FREE(sertest->data);
    GWY_FREE(sertest->s);
    G_OBJECT_CLASS(gwy_ser_test_parent_class)->finalize(object);
    g_strfreev(sertest->strlist);
}

static void
gwy_ser_test_class_init(GwySerTestClass *klass)
{
    GObjectClass *g_object_class = G_OBJECT_CLASS(klass);

    g_object_class->dispose = gwy_ser_test_dispose;
    g_object_class->finalize = gwy_ser_test_finalize;
}

static void
gwy_ser_test_init(GwySerTest *sertest)
{
    sertest->dbl = G_LN2;
}

static const guchar ser_test_simple[] = {
    0x47, 0x77, 0x79, 0x53, 0x65, 0x72, 0x54, 0x65, 0x73, 0x74, 0x00, 0x11,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x72, 0x61, 0x77, 0x00, 0x43,
    0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* Serialize a simple GwySerTest object and check if it matches byte-for-byte
 * with the stored representation above. */
static void
test_serialize_simple(void)
{
    GwySerTest *sertest;
    GOutputStream *stream;
    GMemoryOutputStream *memstream;
    GError *error = NULL;
    gboolean ok;
    guint len;

    sertest = g_object_newv(GWY_TYPE_SER_TEST, 0, NULL);
    stream = g_memory_output_stream_new(malloc(200), 200, NULL, &free);
    memstream = G_MEMORY_OUTPUT_STREAM(stream);
    ok = gwy_serialize_gio(GWY_SERIALIZABLE(sertest), stream, &error);
    g_assert(ok);
    g_assert_cmpint(sertest->done_called, ==, 0);
    len = g_memory_output_stream_get_data_size(memstream);
    g_assert_cmpuint(len, ==, sizeof(ser_test_simple));
    g_assert_cmpuint(memcmp(g_memory_output_stream_get_data(memstream),
                            ser_test_simple, sizeof(ser_test_simple)), ==, 0);
    g_object_unref(stream);
    g_object_unref(sertest);
    g_clear_error(&error);
}

/* Deserialize a simple GwySerTest object and check if the restored data match
 * what we expect (mostly defaults). */
static void
test_deserialize_simple(void)
{
    GwySerTest *sertest;
    GwyErrorList *error_list = NULL;
    gsize bytes_consumed;

    g_type_class_ref(GWY_TYPE_SER_TEST);
    sertest = (GwySerTest*)gwy_deserialize_memory(ser_test_simple,
                                                  sizeof(ser_test_simple),
                                                  &bytes_consumed, &error_list);
    g_assert(sertest);
    g_assert(GWY_IS_SER_TEST(sertest));
    g_assert_cmpuint(bytes_consumed, ==, sizeof(ser_test_simple));
    g_assert_cmpuint(g_slist_length(error_list), ==, 0);
    g_assert_cmpuint(sertest->flag, ==, FALSE);
    g_assert_cmpuint(sertest->len, ==, 0);
    g_assert(sertest->data == NULL);
    g_assert(sertest->s == NULL);

    GWY_OBJECT_UNREF(sertest);
}

static const guchar ser_test_data[] = {
    0x47, 0x77, 0x79, 0x53, 0x65, 0x72, 0x54, 0x65, 0x73, 0x74, 0x00, 0xad,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x66, 0x6c, 0x61, 0x67, 0x00,
    0x62, 0x01, 0x64, 0x61, 0x74, 0x61, 0x00, 0x44, 0x04, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x3f,
    0x18, 0x2d, 0x44, 0x54, 0xfb, 0x21, 0x09, 0x40, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xf0, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
    0x73, 0x00, 0x73, 0x54, 0x65, 0x73, 0x74, 0x20, 0x54, 0x65, 0x73, 0x74,
    0x00, 0x72, 0x61, 0x77, 0x00, 0x43, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x78, 0x56, 0x34, 0x12, 0x64, 0x62, 0x6c, 0x00, 0x64, 0x5e,
    0x5a, 0x75, 0x04, 0x23, 0xcf, 0xe2, 0x3f, 0x69, 0x31, 0x36, 0x00, 0x68,
    0x34, 0x12, 0x69, 0x33, 0x32, 0x00, 0x69, 0xef, 0xbe, 0xad, 0xde, 0x69,
    0x36, 0x34, 0x00, 0x71, 0x80, 0x70, 0x60, 0x50, 0x40, 0x30, 0x20, 0x10,
    0x73, 0x73, 0x00, 0x53, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x46, 0x69, 0x72, 0x73, 0x74, 0x20, 0x74, 0x68, 0x69, 0x6e, 0x67, 0x73,
    0x20, 0x66, 0x69, 0x72, 0x73, 0x74, 0x00, 0x57, 0x61, 0x69, 0x74, 0x20,
    0x61, 0x20, 0x73, 0x65, 0x63, 0x6f, 0x6e, 0x64, 0x2e, 0x2e, 0x2e, 0x00
};

/* Serialize a GwySerTest object with some non-trivial data and check if it
 * matches byte-for-byte with the stored representation above. */
static void
test_serialize_data(void)
{
    static const gdouble data[] = { 1.0, G_PI, HUGE_VAL, -0.0 };

    GwySerTest *sertest;
    GOutputStream *stream;
    GMemoryOutputStream *memstream;
    GError *error = NULL;
    gboolean ok;
    guint len;

    sertest = gwy_ser_test_new_filled(TRUE, data, G_N_ELEMENTS(data),
                                      "Test Test", 0x12345678);
    sertest->i16 = 0x1234;
    sertest->i32 = (gint32)0xdeadbeef;
    sertest->i64 = G_GINT64_CONSTANT(0x1020304050607080);
    sertest->dbl = sin(G_PI/5);
    sertest->strlist = g_new0(gchar*, 3);
    sertest->strlist[0] = g_strdup("First things first");
    sertest->strlist[1] = g_strdup("Wait a second...");
    stream = g_memory_output_stream_new(malloc(200), 200, NULL, &free);
    memstream = G_MEMORY_OUTPUT_STREAM(stream);
    ok = gwy_serialize_gio(GWY_SERIALIZABLE(sertest), stream, &error);
    g_assert(ok);
    g_assert_cmpint(sertest->done_called, ==, 0);
    len = g_memory_output_stream_get_data_size(memstream);
    g_assert_cmpuint(len, ==, sizeof(ser_test_data));
    g_assert_cmpint(memcmp(g_memory_output_stream_get_data(memstream),
                           ser_test_data, sizeof(ser_test_data)), ==, 0);
    g_object_unref(stream);
    g_object_unref(sertest);
    g_clear_error(&error);
}

/* Deserialize a GwySerTest object with some non-trivial data and check if the
 * restored data match what we expect. */
static void
test_deserialize_data(void)
{
    static const gdouble data[] = { 1.0, G_PI, HUGE_VAL, -0.0 };

    GwySerTest *sertest;
    GwyErrorList *error_list = NULL;
    gsize bytes_consumed;

    g_type_class_ref(GWY_TYPE_SER_TEST);
    sertest = (GwySerTest*)gwy_deserialize_memory(ser_test_data,
                                                  sizeof(ser_test_data),
                                                  &bytes_consumed, &error_list);
    g_assert(sertest);
    g_assert(GWY_IS_SER_TEST(sertest));
    g_assert_cmpuint(bytes_consumed, ==, sizeof(ser_test_data));
    g_assert_cmpuint(g_slist_length(error_list), ==, 0);
    g_assert_cmpint(sertest->flag, ==, TRUE);
    g_assert_cmpuint(sertest->len, ==, G_N_ELEMENTS(data));
    g_assert_cmpint(memcmp(sertest->data, data, sizeof(data)), ==, 0);
    g_assert_cmpstr(sertest->s, ==, "Test Test");
    g_assert_cmpfloat(sertest->dbl, ==, sin(G_PI/5));
    g_assert_cmpint(sertest->i16, ==, 0x1234);
    g_assert_cmpint(sertest->i32, ==, (gint32)0xdeadbeef);
    g_assert_cmpint(sertest->i64, ==, G_GINT64_CONSTANT(0x1020304050607080));
    g_assert_cmpuint(g_strv_length(sertest->strlist), ==, 2);
    g_assert_cmpstr(sertest->strlist[0], ==, "First things first");
    g_assert_cmpstr(sertest->strlist[1], ==, "Wait a second...");

    GWY_OBJECT_UNREF(sertest);
}

static const guchar ser_test_nested[] = {
    0x47, 0x77, 0x79, 0x53, 0x65, 0x72, 0x54, 0x65, 0x73, 0x74, 0x00, 0x67,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x72, 0x61, 0x77, 0x00, 0x43,
    0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x63, 0x68, 0x69, 0x6c, 0x64, 0x00, 0x6f, 0x47, 0x77, 0x79, 0x53, 0x65,
    0x72, 0x54, 0x65, 0x73, 0x74, 0x00, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x72, 0x61, 0x77, 0x00, 0x43, 0x04, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x63, 0x68, 0x69, 0x6c, 0x64,
    0x00, 0x6f, 0x47, 0x77, 0x79, 0x53, 0x65, 0x72, 0x54, 0x65, 0x73, 0x74,
    0x00, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x72, 0x61, 0x77,
    0x00, 0x43, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00
};

/* Serialize a GwySerTest object with contained GwySerTest objects (2 levels
 * deep) and check if it matches byte-for-byte with the stored representation
 * above. */
static void
test_serialize_nested(void)
{
    GwySerTest *sertest, *child, *grandchild;
    GOutputStream *stream;
    GMemoryOutputStream *memstream;
    GError *error = NULL;
    gboolean ok;
    guint len;

    /* Nested objects */
    sertest = g_object_newv(GWY_TYPE_SER_TEST, 0, NULL);
    child = sertest->child = g_object_newv(GWY_TYPE_SER_TEST, 0, NULL);
    grandchild = child->child = g_object_newv(GWY_TYPE_SER_TEST, 0, NULL);
    stream = g_memory_output_stream_new(malloc(200), 200, NULL, &free);
    memstream = G_MEMORY_OUTPUT_STREAM(stream);
    ok = gwy_serialize_gio(GWY_SERIALIZABLE(sertest), stream, &error);
    g_assert(ok);
    g_assert_cmpint(sertest->done_called, ==, 0);
    g_assert_cmpint(child->done_called, ==, 0);
    g_assert_cmpint(grandchild->done_called, ==, 0);
    len = g_memory_output_stream_get_data_size(memstream);
    g_assert_cmpuint(len, ==, sizeof(ser_test_nested));
    g_assert_cmpint(memcmp(g_memory_output_stream_get_data(memstream),
                           ser_test_nested, sizeof(ser_test_nested)), ==, 0);
    g_object_unref(stream);
    g_object_unref(sertest);
    g_clear_error(&error);
}

/* Deserialize a GwySerTest object with contained GwySerTest objects (2 levels
 * deep) and check if the restored object tree looks as expected. */
static void
test_deserialize_nested(void)
{
    GwySerTest *sertest, *child;
    GwyErrorList *error_list = NULL;
    gsize bytes_consumed;

    g_type_class_ref(GWY_TYPE_SER_TEST);
    sertest = (GwySerTest*)gwy_deserialize_memory(ser_test_nested,
                                                  sizeof(ser_test_nested),
                                                  &bytes_consumed, &error_list);
    g_assert(sertest);
    g_assert(GWY_IS_SER_TEST(sertest));
    g_assert_cmpuint(bytes_consumed, ==, sizeof(ser_test_nested));
    g_assert_cmpuint(g_slist_length(error_list), ==, 0);
    g_assert_cmpint(sertest->flag, ==, FALSE);
    g_assert_cmpuint(sertest->len, ==, 0);
    g_assert(sertest->data == NULL);
    g_assert(sertest->s == NULL);

    child = sertest->child;
    g_assert(child);
    g_assert(GWY_IS_SER_TEST(child));
    g_assert_cmpuint(bytes_consumed, ==, sizeof(ser_test_nested));
    g_assert_cmpuint(g_slist_length(error_list), ==, 0);
    g_assert_cmpint(child->flag, ==, FALSE);
    g_assert_cmpuint(child->len, ==, 0);
    g_assert(child->data == NULL);
    g_assert(child->s == NULL);

    child = child->child;
    g_assert(child);
    g_assert(GWY_IS_SER_TEST(child));
    g_assert_cmpuint(bytes_consumed, ==, sizeof(ser_test_nested));
    g_assert_cmpuint(g_slist_length(error_list), ==, 0);
    g_assert_cmpint(child->flag, ==, FALSE);
    g_assert_cmpuint(child->len, ==, 0);
    g_assert(child->data == NULL);
    g_assert(child->s == NULL);
    g_assert(child->child == NULL);

    GWY_OBJECT_UNREF(sertest);
}

/* Serialization to a one-byte too short buffer, check failure. */
static void
test_serialize_error(void)
{
    static const gdouble data[] = { 1.0, G_PI, HUGE_VAL, -0.0 };

    GwySerTest *sertest;
    GOutputStream *stream;
    GMemoryOutputStream *memstream;
    GError *error = NULL;
    gboolean ok;

    /* Too small buffer */
    sertest = gwy_ser_test_new_filled(TRUE, data, G_N_ELEMENTS(data),
                                      "Test Test", 0x12345678);
    for (gsize i = 1; i < 102; i++) {
        stream = g_memory_output_stream_new(malloc(i), i, NULL, &free);
        memstream = G_MEMORY_OUTPUT_STREAM(stream);
        ok = gwy_serialize_gio(GWY_SERIALIZABLE(sertest), stream, &error);
        g_assert(!ok);
        g_assert(error);
        g_assert_cmpint(sertest->done_called, ==, 0);
        g_object_unref(stream);
        g_clear_error(&error);
    }
    g_object_unref(sertest);
}

/* Randomly perturb serialized object representations above and try to
 * deserialize the result. */
static void
test_deserialize_garbage(void)
{
    typedef struct { const guchar *buffer; gsize size; } OrigBuffer;
    static const OrigBuffer origs[] = {
        { ser_test_simple, sizeof(ser_test_simple), },
        { ser_test_data,   sizeof(ser_test_data),   },
        { ser_test_nested, sizeof(ser_test_nested), },
    };
    GRand *rng = g_rand_new();

    /* Make it realy reproducible. */
    g_rand_set_seed(rng, 42);

    gsize niter = g_test_slow() ? 100000 : 10000;

    g_type_class_ref(GWY_TYPE_SER_TEST);
    for (gsize i = 0; i < niter; i++) {
        GObject *object = NULL;
        GwyErrorList *error_list = NULL;
        gsize bytes_consumed;
        guint n;

        /* Choose a serialized representation and perturb it. */
        n = g_rand_int_range(rng, 0, G_N_ELEMENTS(origs));
        GArray *buffer = g_array_sized_new(FALSE, FALSE, 1, origs[n].size + 20);
        g_array_append_vals(buffer, origs[n].buffer, origs[n].size);

        n = g_rand_int_range(rng, 1, 12);
        for (guint j = 0; j < n; j++) {
            guint pos, pos2;
            guint8 b, b2;

            switch (g_rand_int_range(rng, 0, 3)) {
                case 0:
                pos = g_rand_int_range(rng, 0, buffer->len);
                g_array_remove_index(buffer, pos);
                break;

                case 1:
                pos = g_rand_int_range(rng, 0, buffer->len+1);
                b = g_rand_int_range(rng, 0, 0x100);
                g_array_insert_val(buffer, pos, b);
                break;

                case 2:
                pos = g_rand_int_range(rng, 0, buffer->len);
                pos2 = g_rand_int_range(rng, 0, buffer->len);
                b = g_array_index(buffer, guint8, pos);
                b2 = g_array_index(buffer, guint8, pos2);
                g_array_index(buffer, guint8, pos) = b2;
                g_array_index(buffer, guint8, pos2) = b;
                break;
            }
        }

        object = gwy_deserialize_memory((const guchar*)buffer->data,
                                        buffer->len,
                                        &bytes_consumed, &error_list);

        /* No checks.  The goal is not to crash... */
        g_array_free(buffer, TRUE);
        GWY_OBJECT_UNREF(object);
        gwy_error_list_clear(&error_list);
    }

    g_rand_free(rng);
}

/***************************************************************************
 *
 * Serialization and deserialization of boxed types
 *
 ***************************************************************************/

#define GWY_TYPE_SER_BOX_TEST \
    (gwy_ser_box_test_get_type())
#define GWY_SER_BOX_TEST(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_SER_BOX_TEST, GwySerBoxTest))
#define GWY_IS_SER_BOX_TEST(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_SER_BOX_TEST))
#define GWY_SER_BOX_TEST_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_SER_BOX_TEST, GwySerBoxTestClass))

#define gwy_ser_box_test_duplicate(ser_test) \
        (GWY_SER_BOX_TEST(gwy_serializable_duplicate(GWY_SERIALIZABLE(ser_test))))

GType gwy_ser_box_test_get_type(void) G_GNUC_CONST;

typedef struct _GwySerBoxTest      GwySerBoxTest;
typedef struct _GwySerBoxTestClass GwySerBoxTestClass;

struct _GwySerBoxTestClass {
    GObjectClass g_object_class;
};

struct _GwySerBoxTest {
    GObject g_object;
    GwyRGBA color;
};

static gsize
gwy_ser_box_test_n_items(G_GNUC_UNUSED GwySerializable *serializable)
{
    return 1 + gwy_serializable_boxed_n_items(GWY_TYPE_RGBA);
}

// The remaining members get zero-initialized which saves us from doing it.
static const GwySerializableItem default_items_box[] = {
    /*0*/ { .name = "color", .ctype = GWY_SERIALIZABLE_BOXED, },
};

#define add_item(id) \
    g_return_val_if_fail(items->len - items->n, 0); \
    items->items[items->n++] = it[id]; \
    n_items++

static gsize
gwy_ser_box_test_itemize(GwySerializable *serializable,
                         GwySerializableItems *items)
{
    GwySerBoxTest *sertest = GWY_SER_BOX_TEST(serializable);
    g_return_val_if_fail(items->len - items->n
                         >= G_N_ELEMENTS(default_items_box), 0);

    GwySerializableItem it = default_items_box[0];
    it.value.v_boxed = &sertest->color;
    items->items[items->n++] = it;

    gwy_serializable_boxed_itemize(GWY_TYPE_RGBA, &sertest->color, items);

    return G_N_ELEMENTS(default_items_box);
}

static gboolean
gwy_ser_box_test_construct(GwySerializable *serializable,
                           GwySerializableItems *items,
                           GwyErrorList **error_list)
{
    GwySerializableItem it[G_N_ELEMENTS(default_items_box)];

    memcpy(it, default_items_box, sizeof(default_items_box));
    gwy_deserialize_filter_items(it, G_N_ELEMENTS(it), items, "GwySerBoxTest",
                                 error_list);

    GwySerBoxTest *sertest = GWY_SER_BOX_TEST(serializable);

    if (it[0].value.v_boxed) {
        g_assert(it[0].array_size == GWY_TYPE_RGBA);
        GwyRGBA *rgba = it[0].value.v_boxed;
        sertest->color = *rgba;
        gwy_rgba_free(rgba);
        it[0].value.v_boxed = NULL;
    }

    return TRUE;
}

static GObject*
gwy_ser_box_test_duplicate_(GwySerializable *serializable)
{
    GwySerBoxTest *sertest = GWY_SER_BOX_TEST(serializable);
    GwySerBoxTest *copy = g_object_newv(GWY_TYPE_SER_BOX_TEST, 0, NULL);
    copy->color = sertest->color;
    return G_OBJECT(copy);
}

static void
gwy_ser_box_test_serializable_init(GwySerializableInterface *iface)
{
    iface->n_items   = gwy_ser_box_test_n_items;
    iface->itemize   = gwy_ser_box_test_itemize;
    iface->construct = gwy_ser_box_test_construct;
    iface->duplicate = gwy_ser_box_test_duplicate_;
    /*
    iface->assign = gwy_ser_box_test_assign_;
    */
}

G_DEFINE_TYPE_EXTENDED
    (GwySerBoxTest, gwy_ser_box_test, G_TYPE_OBJECT, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_ser_box_test_serializable_init))

static void
gwy_ser_box_test_class_init(G_GNUC_UNUSED GwySerBoxTestClass *klass)
{
}

static void
gwy_ser_box_test_init(G_GNUC_UNUSED GwySerBoxTest *sertest)
{
}

static const guchar ser_test_box[] = {
    0x47, 0x77, 0x79, 0x53, 0x65, 0x72, 0x42, 0x6f, 0x78, 0x54, 0x65, 0x73,
    0x74, 0x00, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x63, 0x6f,
    0x6c, 0x6f, 0x72, 0x00, 0x78, 0x47, 0x77, 0x79, 0x52, 0x47, 0x42, 0x41,
    0x00, 0x2c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x72, 0x00, 0x64,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x3f, 0x67, 0x00, 0x64, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x62, 0x00, 0x64, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x61, 0x00, 0x64, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0xe0, 0x3f
};

static void
test_serialize_boxed(void)
{
    GwySerBoxTest *sertest;
    GOutputStream *stream;
    GMemoryOutputStream *memstream;
    GError *error = NULL;
    gboolean ok;
    guint len;

    sertest = g_object_newv(GWY_TYPE_SER_BOX_TEST, 0, NULL);
    sertest->color.r = 1.0;
    sertest->color.a = 0.5;
    stream = g_memory_output_stream_new(malloc(200), 200, NULL, &free);
    memstream = G_MEMORY_OUTPUT_STREAM(stream);
    ok = gwy_serialize_gio(GWY_SERIALIZABLE(sertest), stream, &error);
    g_assert(ok);
    len = g_memory_output_stream_get_data_size(memstream);
    g_assert_cmpuint(len, ==, sizeof(ser_test_box));
    g_assert_cmpuint(memcmp(g_memory_output_stream_get_data(memstream),
                            ser_test_box, sizeof(ser_test_box)), ==, 0);
    g_object_unref(stream);
    g_object_unref(sertest);
    g_clear_error(&error);
}

static void
test_deserialize_boxed(void)
{
    GwySerBoxTest *sertest;
    GwyErrorList *error_list = NULL;
    GError *err;
    gsize bytes_consumed;
    guchar *bad_ser_test_box, *t;

    /* Good */
    g_type_class_ref(GWY_TYPE_SER_BOX_TEST);
    sertest = (GwySerBoxTest*)gwy_deserialize_memory(ser_test_box,
                                                     sizeof(ser_test_box),
                                                     &bytes_consumed,
                                                     &error_list);
    g_assert(sertest);
    g_assert(GWY_IS_SER_BOX_TEST(sertest));
    g_assert_cmpuint(bytes_consumed, ==, sizeof(ser_test_box));
    g_assert_cmpuint(g_slist_length(error_list), ==, 0);
    g_assert_cmpfloat(sertest->color.r, ==, 1.0);
    g_assert_cmpfloat(sertest->color.g, ==, 0.0);
    g_assert_cmpfloat(sertest->color.b, ==, 0.0);
    g_assert_cmpfloat(sertest->color.a, ==, 0.5);
    GWY_OBJECT_UNREF(sertest);

    /* Soft error (deserialization has to dispose the boxed type itself) */
    bad_ser_test_box = g_memdup(ser_test_box, sizeof(ser_test_box));
    t = gwy_memmem(bad_ser_test_box, sizeof(ser_test_box),
                   "color", strlen("color"));
    g_assert(t);
    g_assert(strlen("color") == strlen("skunk"));
    memcpy(t, "skunk", strlen("skunk"));

    sertest = (GwySerBoxTest*)gwy_deserialize_memory(bad_ser_test_box,
                                                     sizeof(ser_test_box),
                                                     &bytes_consumed,
                                                     &error_list);
    g_assert_cmpuint(g_slist_length(error_list), ==, 1);
    err = error_list->data;
    g_assert_cmpuint(err->domain, ==, GWY_DESERIALIZE_ERROR);
    g_assert_cmpuint(err->code, ==, GWY_DESERIALIZE_ERROR_ITEM);
    g_assert(sertest);
    g_assert(GWY_IS_SER_BOX_TEST(sertest));
    g_assert_cmpuint(bytes_consumed, ==, sizeof(ser_test_box));
    GWY_OBJECT_UNREF(sertest);
    gwy_error_list_clear(&error_list);
    g_free(bad_ser_test_box);

    /* Hard error */
    bad_ser_test_box = g_memdup(ser_test_box, sizeof(ser_test_box));
    t = gwy_memmem(bad_ser_test_box, sizeof(ser_test_box),
                   "GwyRGBA", strlen("GwyRGBA"));
    g_assert(t);
    g_assert(strlen("GwyRGBA") == strlen("BREAKME"));
    memcpy(t, "BREAKME", strlen("BREAKME"));

    sertest = (GwySerBoxTest*)gwy_deserialize_memory(bad_ser_test_box,
                                                     sizeof(ser_test_box),
                                                     &bytes_consumed,
                                                     &error_list);
    g_assert_cmpuint(g_slist_length(error_list), ==, 1);
    err = error_list->data;
    g_assert_cmpuint(err->domain, ==, GWY_DESERIALIZE_ERROR);
    g_assert_cmpuint(err->code, ==, GWY_DESERIALIZE_ERROR_OBJECT);
    g_assert(!sertest);
    g_assert_cmpuint(bytes_consumed, ==, sizeof(ser_test_box));
    gwy_error_list_clear(&error_list);
    g_free(bad_ser_test_box);
}

/***************************************************************************
 *
 * Units
 *
 ***************************************************************************/

static void
test_unit_parse(void)
{
    gint pw1, pw2, pw3, pw4, pw5, pw6, pw7, pw8, pw9;
    gint pw11, pw12, pw13, pw14, pw15, pw16, pw17, pw18;

    /* Simple notations */
    GwyUnit *u1 = gwy_unit_new_from_string("m", &pw1);
    GwyUnit *u2 = gwy_unit_new_from_string("km", &pw2);
    GwyUnit *u3 = gwy_unit_new_from_string("Å", &pw3);

    g_assert(gwy_unit_equal(u1, u2));
    g_assert(gwy_unit_equal(u2, u3));
    g_assert(gwy_unit_equal(u3, u1));
    g_assert_cmpint(pw1, ==, 0);
    g_assert_cmpint(pw2, ==, 3);
    g_assert_cmpint(pw3, ==, -10);

    g_object_unref(u1);
    g_object_unref(u2);
    g_object_unref(u3);

    /* Powers and comparision */
    GwyUnit *u4 = gwy_unit_new_from_string("um s^-1", &pw4);
    GwyUnit *u5 = gwy_unit_new_from_string("mm/ps", &pw5);
    GwyUnit *u6 = gwy_unit_new_from_string("μs<sup>-1</sup> cm", &pw6);

    g_assert(gwy_unit_equal(u4, u5));
    g_assert(gwy_unit_equal(u5, u6));
    g_assert(gwy_unit_equal(u6, u4));
    g_assert_cmpint(pw4, ==, -6);
    g_assert_cmpint(pw5, ==, 9);
    g_assert_cmpint(pw6, ==, 4);

    g_object_unref(u4);
    g_object_unref(u5);
    g_object_unref(u6);

    /* Cancellation */
    GwyUnit *u7 = gwy_unit_new_from_string(NULL, &pw7);
    GwyUnit *u8 = gwy_unit_new_from_string("10%", &pw8);
    GwyUnit *u9 = gwy_unit_new_from_string("m^3 cm^-2/km", &pw9);

    g_assert(gwy_unit_equal(u7, u8));
    g_assert(gwy_unit_equal(u8, u9));
    g_assert(gwy_unit_equal(u9, u7));
    g_assert_cmpint(pw7, ==, 0);
    g_assert_cmpint(pw8, ==, -1);
    g_assert_cmpint(pw9, ==, 1);

    g_object_unref(u7);
    g_object_unref(u8);
    g_object_unref(u9);

    /* Silly notations: micron */
    GwyUnit *u11 = gwy_unit_new_from_string("μs", &pw11);
    GwyUnit *u12 = gwy_unit_new_from_string("µs", &pw12);
    GwyUnit *u13 = gwy_unit_new_from_string("us", &pw13);
    GwyUnit *u14 = gwy_unit_new_from_string("~s", &pw14);
    GwyUnit *u15 = gwy_unit_new_from_string("\265s", &pw15);

    g_assert(gwy_unit_equal(u11, u12));
    g_assert(gwy_unit_equal(u12, u13));
    g_assert(gwy_unit_equal(u13, u14));
    g_assert(gwy_unit_equal(u14, u15));
    g_assert(gwy_unit_equal(u15, u11));
    g_assert_cmpint(pw11, ==, -6);
    g_assert_cmpint(pw12, ==, -6);
    g_assert_cmpint(pw13, ==, -6);
    g_assert_cmpint(pw14, ==, -6);
    g_assert_cmpint(pw15, ==, -6);

    g_object_unref(u11);
    g_object_unref(u12);
    g_object_unref(u13);
    g_object_unref(u14);
    g_object_unref(u15);

    /* Silly notations: squares */
    GwyUnit *u16 = gwy_unit_new_from_string("m²", &pw16);
    GwyUnit *u17 = gwy_unit_new_from_string("m m", &pw17);
    GwyUnit *u18 = gwy_unit_new_from_string("m\262", &pw18);

    g_assert(gwy_unit_equal(u16, u17));
    g_assert(gwy_unit_equal(u17, u18));
    g_assert(gwy_unit_equal(u18, u16));
    g_assert_cmpint(pw16, ==, 0);
    g_assert_cmpint(pw17, ==, 0);
    g_assert_cmpint(pw18, ==, 0);

    g_object_unref(u16);
    g_object_unref(u17);
    g_object_unref(u18);
}

static void
test_unit_arithmetic(void)
{
    GwyUnit *u1, *u2, *u3, *u4, *u5, *u6, *u7, *u8, *u9, *u0;

    u1 = gwy_unit_new_from_string("kg m s^-2", NULL);
    u2 = gwy_unit_new_from_string("s/kg", NULL);
    u3 = gwy_unit_new_from_string("m/s", NULL);

    u4 = gwy_unit_multiply(NULL, u1, u2);
    g_assert(gwy_unit_equal(u3, u4));

    u5 = gwy_unit_power(NULL, u1, -1);
    u6 = gwy_unit_power_multiply(NULL, u5, 2, u2, -2);
    u7 = gwy_unit_power(NULL, u3, -2);
    g_assert(gwy_unit_equal(u6, u7));

    u8 = gwy_unit_nth_root(NULL, u6, 2);
    gwy_unit_power(u8, u8, -1);
    g_assert(gwy_unit_equal(u8, u3));

    gwy_unit_divide(u8, u8, u3);
    u0 = gwy_unit_new();
    g_assert(gwy_unit_equal(u8, u0));

    u9 = gwy_unit_power(NULL, u3, 4);
    gwy_unit_power_multiply(u9, u9, 1, u1, -3);
    gwy_unit_power_multiply(u9, u2, 3, u9, -1);
    gwy_unit_multiply(u9, u9, u3);
    g_assert(gwy_unit_equal(u9, u0));

    g_object_unref(u1);
    g_object_unref(u2);
    g_object_unref(u3);
    g_object_unref(u4);
    g_object_unref(u5);
    g_object_unref(u6);
    g_object_unref(u7);
    g_object_unref(u8);
    g_object_unref(u9);
    g_object_unref(u0);
}

static void
test_unit_serialize_one(GwyUnit *unit1)
{
    GwyUnit *unit2;
    GOutputStream *stream;
    GMemoryOutputStream *memstream;
    GError *error = NULL;
    GwyErrorList *error_list = NULL;
    gsize len;
    gboolean ok;

    stream = g_memory_output_stream_new(malloc(100), 100, NULL, &free);
    memstream = G_MEMORY_OUTPUT_STREAM(stream);
    ok = gwy_serialize_gio(GWY_SERIALIZABLE(unit1), stream, &error);
    g_assert(ok);
    g_assert(error == NULL);
    len = g_memory_output_stream_get_data_size(memstream);
    unit2 = (GwyUnit*)(gwy_deserialize_memory
                       (g_memory_output_stream_get_data(memstream),
                        g_memory_output_stream_get_data_size(memstream),
                        &len, &error_list));
    g_assert_cmpuint(len, ==, g_memory_output_stream_get_data_size(memstream));
    g_assert_cmpuint(g_slist_length(error_list), ==, 0);
    g_assert(GWY_IS_UNIT(unit2));
    g_assert(gwy_unit_equal(unit2, unit1));

    g_object_unref(stream);
    g_object_unref(unit2);
}

static void
test_unit_serialize(void)
{
    GwyUnit *unit;

    /* Trivial unit */
    unit = gwy_unit_new();
    test_unit_serialize_one(unit);
    g_object_unref(unit);

    /* Less trivial unit */
    unit = gwy_unit_new_from_string("kg m s^-2/nA", NULL);
    test_unit_serialize_one(unit);
    g_object_unref(unit);
}

/***************************************************************************
 *
 * Value formatting
 *
 ***************************************************************************/

static void
test_value_format_simple(void)
{
    GwyUnit *unit = gwy_unit_new_from_string("m", NULL);
    GwyValueFormat *format;

    /* Base cases */
    format = gwy_unit_format_with_resolution(unit, GWY_VALUE_FORMAT_PLAIN,
                                             1e-6, 1e-9, NULL);
    g_assert_cmpstr(gwy_value_format_print(format, 1.23456e-7), ==, "0.123 µm");
    g_assert_cmpstr(gwy_value_format_print_number(format, 1.23456e-7), ==, "0.123");

    gwy_unit_format_with_resolution(unit, GWY_VALUE_FORMAT_PLAIN,
                                    1e-7, 1e-10, format);
    g_assert_cmpstr(gwy_value_format_print(format, 1.23456e-7), ==, "123.5 nm");
    g_assert_cmpstr(gwy_value_format_print_number(format, 1.23456e-7), ==, "123.5");

    gwy_unit_format_with_resolution(unit, GWY_VALUE_FORMAT_PLAIN,
                                    1e-7, 1e-9, format);
    g_assert_cmpstr(gwy_value_format_print(format, 1.23456e-7), ==, "123 nm");

    /* Near-base cases, ensure values differing by step are distinguishable */
    gwy_unit_format_with_resolution(unit, GWY_VALUE_FORMAT_PLAIN,
                                    1e-7, 1.01e-10, format);
    g_assert_cmpstr(gwy_value_format_print(format, 1.23456e-7), ==, "123.5 nm");

    gwy_unit_format_with_resolution(unit, GWY_VALUE_FORMAT_PLAIN,
                                    1e-7, 0.99e-10, format);
    g_assert_cmpstr(gwy_value_format_print(format, 1.23456e-7), ==, "123.46 nm");

    /* Fancy formatting with base not a power of 10 */
    g_object_set(format,
                 "style", GWY_VALUE_FORMAT_PLAIN,
                 "base", G_PI/180.0,
                 "precision", 1,
                 "glue", " ",
                 "units", "deg",
                 NULL);
    g_assert_cmpstr(gwy_value_format_print(format, G_PI/6.0), ==, "30.0 deg");

    g_object_unref(format);
    g_object_unref(unit);
}

/***************************************************************************
 *
 * Expr
 *
 ***************************************************************************/

static void
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

static void
test_expr_vector(void)
{
    GwyExpr *expr = gwy_expr_new();
    GError *error = NULL;
    gboolean ok;

    ok = gwy_expr_define_constant(expr, "π", G_PI, &error);
    g_assert(ok);
    g_assert(!error);
    g_clear_error(&error);

    ok = gwy_expr_compile(expr, "sqrt(a^2+π*β/c₂)-sqrt(a^2+π*c₂/β)", &error);
    g_assert(ok);
    g_assert(!error);
    g_clear_error(&error);

    gsize nvars = gwy_expr_get_variables(expr, NULL);
    g_assert_cmpuint(nvars, ==, 4);

    gsize n = 10000;
    gdouble **input = g_new0(gdouble*, nvars);
    for (gsize i = 1; i < nvars; i++) {
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

    for (gsize i = 1; i < nvars; i++)
        g_free(input[i]);
    g_free(input);
    g_free(result);

    g_object_unref(expr);
}

static void
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

/***************************************************************************
 *
 * Container
 *
 ***************************************************************************/

static void
container_item_changed(G_GNUC_UNUSED GwyContainer *container,
                       gpointer arg1,
                       const gchar **item_key)
{
    *item_key = g_quark_to_string(GPOINTER_TO_UINT(arg1));
}

static void
container_item_changed_count(G_GNUC_UNUSED GwyContainer *container,
                             G_GNUC_UNUSED gpointer arg1,
                             guint *called)
{
    (*called)++;
}

static void
test_container_data(void)
{
    GwyContainer *container = gwy_container_new();
    const gchar *item_key;
    guint int_changed = 0;
    GQuark quark;
    gboolean ok;
    guint n;

    g_assert_cmpuint(gwy_container_n_items(container), ==, 0);
    g_signal_connect(container, "item-changed",
                     G_CALLBACK(container_item_changed), &item_key);
    g_signal_connect(container, "item-changed::/pfx/int",
                     G_CALLBACK(container_item_changed_count), &int_changed);

    item_key = "";
    gwy_container_set_int32_n(container, "/pfx/int", 42);
    quark = g_quark_try_string("/pfx/int");
    g_assert(quark);

    g_assert(gwy_container_contains(container, quark));
    g_assert(gwy_container_get_int32(container, quark) == 42);
    g_assert_cmpstr(item_key, ==, "/pfx/int");
    g_assert_cmpuint(int_changed, ==, 1);

    item_key = "";
    gwy_container_set_int32(container, quark, -3);
    g_assert(gwy_container_contains_n(container, "/pfx/int"));
    g_assert_cmpint(gwy_container_get_int32_n(container, "/pfx/int"), ==, -3);
    g_assert_cmpstr(item_key, ==, "/pfx/int");
    g_assert_cmpuint(int_changed, ==, 2);

    item_key = "";
    gwy_container_set_char_n(container, "/pfx/char", '@');
    g_assert(gwy_container_contains_n(container, "/pfx/char"));
    g_assert_cmpint(gwy_container_get_char_n(container, "/pfx/char"), ==, '@');
    g_assert_cmpstr(item_key, ==, "/pfx/char");

    item_key = "";
    gwy_container_set_int64_n(container, "/pfx/int64",
                              G_GUINT64_CONSTANT(0xdeadbeefdeadbeef));
    g_assert(gwy_container_contains_n(container, "/pfx/int64"));
    g_assert((guint64)gwy_container_get_int64_n(container, "/pfx/int64")
             == G_GUINT64_CONSTANT(0xdeadbeefdeadbeef));
    g_assert_cmpstr(item_key, ==, "/pfx/int64");

    item_key = "";
    gwy_container_set_double_n(container, "/pfx/double", G_LN2);
    g_assert(gwy_container_contains_n(container, "/pfx/double"));
    g_assert_cmpfloat(gwy_container_get_double_n(container, "/pfx/double"),
                      ==, G_LN2);
    g_assert_cmpstr(item_key, ==, "/pfx/double");

    item_key = "";
    gwy_container_take_string_n(container, "/pfx/string",
                                g_strdup("Test Test"));
    g_assert(gwy_container_contains_n(container, "/pfx/string"));
    g_assert_cmpstr(gwy_container_get_string_n(container, "/pfx/string"),
                    ==, "Test Test");
    g_assert_cmpstr(item_key, ==, "/pfx/string");

    /* No value change */
    item_key = "";
    gwy_container_set_string_n(container, "/pfx/string", "Test Test");
    g_assert(gwy_container_contains_n(container, "/pfx/string"));
    g_assert_cmpstr(gwy_container_get_string_n(container, "/pfx/string"),
                    ==, "Test Test");
    g_assert_cmpstr(item_key, ==, "");

    item_key = "";
    gwy_container_set_string_n(container, "/pfx/string", "Test Test Test");
    g_assert(gwy_container_contains_n(container, "/pfx/string"));
    g_assert_cmpstr(gwy_container_get_string_n(container, "/pfx/string"),
                    ==, "Test Test Test");
    g_assert_cmpstr(item_key, ==, "/pfx/string");

    g_assert_cmpuint(gwy_container_n_items(container), ==, 5);

    gwy_container_transfer(container, container, "/pfx", "/elsewhere",
                           TRUE, TRUE);
    g_assert_cmpuint(gwy_container_n_items(container), ==, 10);

    ok = gwy_container_remove_n(container, "/pfx/string/ble");
    g_assert(!ok);

    n = gwy_container_remove_prefix(container, "/pfx/string/ble");
    g_assert_cmpuint(n, ==, 0);

    item_key = "";
    ok = gwy_container_remove_n(container, "/pfx/string");
    g_assert(ok);
    g_assert_cmpuint(gwy_container_n_items(container), ==, 9);
    g_assert_cmpstr(item_key, ==, "/pfx/string");

    item_key = "";
    n = gwy_container_remove_prefix(container, "/pfx/int");
    g_assert_cmpuint(n, ==, 1);
    g_assert_cmpuint(gwy_container_n_items(container), ==, 8);
    g_assert_cmpstr(item_key, ==, "/pfx/int");

    item_key = "";
    ok = gwy_container_rename(container,
                             g_quark_try_string("/pfx/int64"),
                             g_quark_try_string("/pfx/int"),
                             TRUE);
    g_assert(ok);
    g_assert_cmpuint(int_changed, ==, 4);
    g_assert_cmpstr(item_key, ==, "/pfx/int");

    n = gwy_container_remove_prefix(container, "/pfx");
    g_assert_cmpuint(n, ==, 3);
    g_assert_cmpuint(gwy_container_n_items(container), ==, 5);
    g_assert_cmpuint(int_changed, ==, 5);

    gwy_container_remove_prefix(container, NULL);
    g_assert_cmpuint(gwy_container_n_items(container), ==, 0);

    g_object_unref(container);
}

static void
test_container_refcount(void)
{
    GwyContainer *container = gwy_container_new();

    GwySerTest *st1 = g_object_newv(GWY_TYPE_SER_TEST, 0, NULL);
    g_assert_cmpuint(G_OBJECT(st1)->ref_count, ==, 1);
    gwy_container_set_object_n(container, "/pfx/object", st1);
    g_assert_cmpuint(G_OBJECT(st1)->ref_count, ==, 2);

    GwySerTest *st2 = g_object_newv(GWY_TYPE_SER_TEST, 0, NULL);
    g_assert_cmpuint(G_OBJECT(st2)->ref_count, ==, 1);
    gwy_container_set_object_n(container, "/pfx/object", st2);
    g_assert_cmpuint(G_OBJECT(st2)->ref_count, ==, 2);
    g_assert_cmpuint(G_OBJECT(st1)->ref_count, ==, 1);
    g_object_unref(st1);

    GwySerTest *st3 = g_object_newv(GWY_TYPE_SER_TEST, 0, NULL);
    g_assert_cmpuint(G_OBJECT(st3)->ref_count, ==, 1);
    g_object_ref(st3);
    gwy_container_take_object_n(container, "/pfx/taken", st3);
    g_assert_cmpuint(G_OBJECT(st3)->ref_count, ==, 2);

    g_object_unref(container);
    g_assert_cmpuint(G_OBJECT(st2)->ref_count, ==, 1);
    g_object_unref(st2);
    g_assert_cmpuint(G_OBJECT(st3)->ref_count, ==, 1);
    g_object_unref(st3);

    container = gwy_container_new();
    st1 = g_object_newv(GWY_TYPE_SER_TEST, 0, NULL);
    gwy_container_set_object_n(container, "/pfx/object", st1);
    gwy_container_transfer(container, container, "/pfx", "/elsewhere",
                           FALSE, TRUE);
    g_assert_cmpuint(G_OBJECT(st1)->ref_count, ==, 3);
    st2 = gwy_container_get_object_n(container, "/elsewhere/object");
    g_assert(st2 == st1);

    gwy_container_transfer(container, container, "/pfx", "/faraway",
                           TRUE, TRUE);
    st2 = gwy_container_get_object_n(container, "/faraway/object");
    g_assert(GWY_IS_SER_TEST(st2));
    g_assert(st2 != st1);
    g_assert_cmpuint(G_OBJECT(st1)->ref_count, ==, 3);
    g_assert_cmpuint(G_OBJECT(st2)->ref_count, ==, 1);
    g_object_ref(st2);

    g_object_unref(container);
    g_assert_cmpuint(G_OBJECT(st1)->ref_count, ==, 1);
    g_assert_cmpuint(G_OBJECT(st2)->ref_count, ==, 1);
    g_object_unref(st1);
    g_object_unref(st2);
}

static void
test_container_serialize(void)
{
    GwyContainer *container = gwy_container_new();
    gwy_container_set_char_n(container, "char", '\xfe');
    gwy_container_set_boolean_n(container, "bool", TRUE);
    gwy_container_set_int32_n(container, "int32", -123456);
    gwy_container_set_int64_n(container, "int64",
                              G_GINT64_CONSTANT(-1234567890));
    gwy_container_set_string_n(container, "string", "Mud");
    gwy_container_set_double_n(container, "double", G_E);
    GwyUnit *unit = gwy_unit_new_from_string("uPa", NULL);
    gwy_container_take_object_n(container, "unit", unit);

    GOutputStream *stream;
    GMemoryOutputStream *memstream;
    stream = g_memory_output_stream_new(malloc(200), 200, NULL, &free);
    memstream = G_MEMORY_OUTPUT_STREAM(stream);
    GError *error = NULL;
    gboolean ok = gwy_serialize_gio(GWY_SERIALIZABLE(container), stream,
                                    &error);
    g_assert(ok);
    g_assert(!error);
    g_clear_error(&error);

    gsize len = g_memory_output_stream_get_data_size(memstream);
    const guchar *buffer = g_memory_output_stream_get_data(memstream);
    gsize bytes_consumed = 0;
    GwyErrorList *error_list = NULL;
    GwyContainer *copy = GWY_CONTAINER(gwy_deserialize_memory(buffer, len,
                                                              &bytes_consumed,
                                                              &error_list));
    g_assert(GWY_IS_CONTAINER(copy));
    g_assert(!error_list);
    g_assert_cmpuint(bytes_consumed, ==, len);
    g_object_unref(stream);
    gwy_error_list_clear(&error_list);

    GwyUnit *unitcopy = NULL;
    g_assert(gwy_container_gis_object_n(container, "unit", &unitcopy));
    g_assert(GWY_IS_UNIT(unitcopy));
    g_assert(gwy_unit_equal(unitcopy, unit));

    guint change_count = 0;
    g_signal_connect(container, "item-changed",
                     G_CALLBACK(container_item_changed_count), &change_count);
    gwy_container_transfer(copy, container, "", "", FALSE, TRUE);
    /* Unit must change, but others should be detected as same-value. */
    g_assert_cmpuint(change_count, ==, 1);
    /* Not even units can change here. */
    gwy_container_transfer(copy, container, "", "", FALSE, TRUE);
    /* Unit must change, but others should be detected as same-value. */
    gwy_container_transfer(copy, container, "", "", TRUE, TRUE);

    g_object_unref(copy);
    g_object_unref(container);
}

static void
test_container_text(void)
{
    GwyContainer *container = gwy_container_new();
    gwy_container_set_char_n(container, "char", '\xfe');
    gwy_container_set_boolean_n(container, "bool", TRUE);
    gwy_container_set_int32_n(container, "int32", -123456);
    gwy_container_set_int64_n(container, "int64",
                              G_GINT64_CONSTANT(-1234567890));
    gwy_container_set_string_n(container, "string", "Mud");
    gwy_container_set_double_n(container, "double", G_E);

    gchar **lines = gwy_container_dump_to_text(container);
    g_assert(lines);
    gchar *text = g_strjoinv("\n", lines);
    g_strfreev(lines);
    GwyContainer *copy = gwy_container_new_from_text(text);
    g_free(text);
    g_assert(GWY_IS_CONTAINER(copy));
    g_assert_cmpuint(gwy_container_n_items(container), ==, 6);

    guint change_count = 0;
    g_signal_connect(container, "item-changed",
                     G_CALLBACK(container_item_changed_count), &change_count);
    gwy_container_transfer(copy, container, "", "", FALSE, TRUE);
    /* Atomic types must be detected as same-value. */
    g_assert_cmpuint(change_count, ==, 0);

    g_object_unref(copy);
    g_object_unref(container);
}

static void
test_container_boxed(void)
{
    GwyContainer *container = gwy_container_new();
    GwyRGBA color = { 0.9, 0.6, 0.3, 1.0 };
    gwy_container_set_boxed_n(container, "color", GWY_TYPE_RGBA, &color);
    GwyRGBA color2 = { 0.0, 0.5, 0.5, 1.0 };
    gwy_container_set_boxed_n(container, "color", GWY_TYPE_RGBA, &color2);
    const GwyRGBA *color3 = gwy_container_get_boxed_n(container, "color",
                                                      GWY_TYPE_RGBA);
    g_assert(color3);
    g_assert_cmpint(memcmp(&color2, color3, sizeof(GwyRGBA)), ==, 0);
    GwyRGBA color4;
    g_assert(gwy_container_gis_boxed_n(container, "color", GWY_TYPE_RGBA,
                                       &color4));
    g_assert_cmpint(memcmp(&color2, &color4, sizeof(GwyRGBA)), ==, 0);
    g_assert(gwy_container_gis_boxed_n(container, "color", G_TYPE_BOXED,
                                       &color4));
    g_assert_cmpint(memcmp(&color2, &color4, sizeof(GwyRGBA)), ==, 0);
    g_object_unref(container);
}

/***************************************************************************
 *
 * Array
 *
 ***************************************************************************/

static void
record_item_change(G_GNUC_UNUSED GObject *object,
                   guint pos,
                   guint64 *counter)
{
    *counter <<= 4;
    *counter |= pos + 1;
}

static void
test_array_data(void)
{
    GwyRGBA init_colors[] = { { 1.0, 1.0, 1.0, 1.0 }, { 0.0, 0.0, 0.0, 0.0 } };
    GwyArray *array = gwy_array_new_with_data(sizeof(GwyRGBA), NULL,
                                              init_colors,
                                              G_N_ELEMENTS(init_colors));
    g_assert(GWY_IS_ARRAY(array));
    g_assert_cmpuint(gwy_array_n_items(array), ==, 2);
    guint64 insert_log = 0, update_log = 0, delete_log = 0;
    g_signal_connect(array, "item-inserted",
                     G_CALLBACK(record_item_change), &insert_log);
    g_signal_connect(array, "item-updated",
                     G_CALLBACK(record_item_change), &update_log);
    g_signal_connect(array, "item-deleted",
                     G_CALLBACK(record_item_change), &delete_log);

    // Single-item operations.
    GwyRGBA color1 = { 0.1, 0.1, 0.1, 0.1 };
    GwyRGBA color2 = { 0.2, 0.2, 0.2, 0.2 };
    GwyRGBA color3 = { 0.3, 0.3, 0.3, 0.3 };
    gwy_array_insert1(array, 1, &color1);
    gwy_array_insert1(array, 3, &color2);
    gwy_array_insert1(array, 0, &color3);
    g_assert_cmpuint(gwy_array_n_items(array), ==, 5);
    g_assert_cmphex(insert_log, ==, 0x241);
    g_assert_cmphex(update_log, ==, 0);
    g_assert_cmphex(delete_log, ==, 0);
    insert_log = 0;

    GwyRGBA *color;
    color = gwy_array_get(array, 0);
    g_assert(color);
    g_assert_cmpint(memcmp(color, &color3, sizeof(GwyRGBA)), ==, 0);

    color = gwy_array_get(array, 1);
    g_assert(color);
    g_assert_cmpint(memcmp(color, init_colors + 0, sizeof(GwyRGBA)), ==, 0);

    color = gwy_array_get(array, 2);
    g_assert(color);
    g_assert_cmpint(memcmp(color, &color1, sizeof(GwyRGBA)), ==, 0);

    color = gwy_array_get(array, 3);
    g_assert(color);
    g_assert_cmpint(memcmp(color, init_colors + 1, sizeof(GwyRGBA)), ==, 0);

    color = gwy_array_get(array, 4);
    g_assert(color);
    g_assert_cmpint(memcmp(color, &color2, sizeof(GwyRGBA)), ==, 0);

    gwy_array_delete1(array, 2);
    gwy_array_delete1(array, 1);
    gwy_array_delete1(array, 1);
    g_assert_cmpuint(gwy_array_n_items(array), ==, 2);
    g_assert_cmphex(insert_log, ==, 0);
    g_assert_cmphex(update_log, ==, 0);
    g_assert_cmphex(delete_log, ==, 0x322);
    delete_log = 0;

    color = gwy_array_get(array, 0);
    g_assert(color);
    g_assert_cmpint(memcmp(color, &color3, sizeof(GwyRGBA)), ==, 0);

    color = gwy_array_get(array, 1);
    g_assert(color);
    g_assert_cmpint(memcmp(color, &color2, sizeof(GwyRGBA)), ==, 0);

    gwy_array_append1(array, &color1);
    gwy_array_replace1(array, 0, &color1);
    g_assert_cmpuint(gwy_array_n_items(array), ==, 3);
    g_assert_cmphex(insert_log, ==, 0x3);
    g_assert_cmphex(update_log, ==, 0x1);
    g_assert_cmphex(delete_log, ==, 0);
    insert_log = update_log = 0;

    color = gwy_array_get(array, 0);
    g_assert(color);
    g_assert_cmpint(memcmp(color, &color1, sizeof(GwyRGBA)), ==, 0);

    // Multi-item operations.
    GwyRGBA colors[] = { color1, color2, color3 };
    gwy_array_insert(array, 1, init_colors, 2);
    g_assert_cmpuint(gwy_array_n_items(array), ==, 5);
    g_assert_cmphex(insert_log, ==, 0x23);
    g_assert_cmphex(update_log, ==, 0);
    g_assert_cmphex(delete_log, ==, 0);
    insert_log = 0;

    color = gwy_array_get(array, 1);
    g_assert(color);
    g_assert_cmpint(memcmp(color, init_colors + 0, sizeof(GwyRGBA)), ==, 0);

    color = gwy_array_get(array, 2);
    g_assert(color);
    g_assert_cmpint(memcmp(color, init_colors + 1, sizeof(GwyRGBA)), ==, 0);

    gwy_array_replace(array, 2, colors, 3);
    g_assert_cmpuint(gwy_array_n_items(array), ==, 5);
    g_assert_cmphex(insert_log, ==, 0);
    g_assert_cmphex(update_log, ==, 0x345);
    g_assert_cmphex(delete_log, ==, 0);
    update_log = 0;

    color = gwy_array_get(array, 2);
    g_assert(color);
    g_assert_cmpint(memcmp(color, &color1, sizeof(GwyRGBA)), ==, 0);

    color = gwy_array_get(array, 3);
    g_assert(color);
    g_assert_cmpint(memcmp(color, &color2, sizeof(GwyRGBA)), ==, 0);

    color = gwy_array_get(array, 4);
    g_assert(color);
    g_assert_cmpint(memcmp(color, &color3, sizeof(GwyRGBA)), ==, 0);

    gwy_array_delete(array, 1, 3);
    g_assert_cmpuint(gwy_array_n_items(array), ==, 2);
    g_assert_cmphex(insert_log, ==, 0);
    g_assert_cmphex(update_log, ==, 0);
    g_assert_cmphex(delete_log, ==, 0x432);
    delete_log = 0;

    color = gwy_array_get(array, 1);
    g_assert(color);
    g_assert_cmpint(memcmp(color, &color3, sizeof(GwyRGBA)), ==, 0);

    gwy_array_set_data(array, colors, 3);
    g_assert_cmphex(insert_log, ==, 0x3);
    g_assert_cmphex(update_log, ==, 0x12);
    g_assert_cmphex(delete_log, ==, 0);
    insert_log = update_log = 0;

    color = gwy_array_get_data(array);
    g_assert(color);
    g_assert_cmpint(memcmp(color, colors, 3*sizeof(GwyRGBA)), ==, 0);

    gwy_array_set_data(array, init_colors, 2);
    g_assert_cmphex(insert_log, ==, 0);
    g_assert_cmphex(update_log, ==, 0x12);
    g_assert_cmphex(delete_log, ==, 0x3);
    update_log = delete_log = 0;

    color = gwy_array_get_data(array);
    g_assert(color);
    g_assert_cmpint(memcmp(color, init_colors, 2*sizeof(GwyRGBA)), ==, 0);

    g_object_unref(array);
}

/***************************************************************************
 *
 * Inventory
 *
 ***************************************************************************/

typedef struct {
    gchar *name;
    gint value;
} GwyItemTest;

static int item_destroy_count;

static GwyItemTest*
item_new(const gchar *name, gint value)
{
    GwyItemTest *itemtest = g_slice_new(GwyItemTest);
    itemtest->name = g_strdup(name);
    itemtest->value = value;
    return itemtest;
}

static const gchar*
item_get_name(gconstpointer item)
{
    const GwyItemTest *itemtest = (const GwyItemTest*)item;
    return itemtest->name;
}

static gboolean
item_is_modifiable(gconstpointer item)
{
    const GwyItemTest *itemtest = (const GwyItemTest*)item;
    return itemtest->value >= 0;
}

static gint
item_compare(gconstpointer a,
             gconstpointer b)
{
    const GwyItemTest *itemtesta = (const GwyItemTest*)a;
    const GwyItemTest *itemtestb = (const GwyItemTest*)b;
    return strcmp(itemtesta->name, itemtestb->name);
}

static void
item_rename(gpointer item,
            const gchar *newname)
{
    GwyItemTest *itemtest = (GwyItemTest*)item;
    g_free(itemtest->name);
    itemtest->name = g_strdup(newname);
}

static void
item_destroy(gpointer item)
{
    GwyItemTest *itemtest = (GwyItemTest*)item;
    g_free(itemtest->name);
    g_slice_free(GwyItemTest, item);
    item_destroy_count++;
}

static gpointer
item_copy(gconstpointer item)
{
    const GwyItemTest *itemtest = (const GwyItemTest*)item;
    GwyItemTest *copy = g_slice_dup(GwyItemTest, item);
    copy->name = g_strdup(itemtest->name);
    return copy;
}

static void
test_inventory_data(void)
{
    GwyInventoryItemType item_type = {
        0,
        NULL,
        item_get_name,
        item_is_modifiable,
        item_compare,
        item_rename,
        item_destroy,
        item_copy,
        NULL,
        NULL,
        NULL,
    };

    GwyInventory *inventory = gwy_inventory_new();
    g_assert(GWY_IS_INVENTORY(inventory));
    gwy_inventory_set_item_type(inventory, &item_type);
    item_destroy_count = 0;

    guint64 insert_log = 0, update_log = 0, delete_log = 0;
    g_signal_connect(inventory, "item-inserted",
                     G_CALLBACK(record_item_change), &insert_log);
    g_signal_connect(inventory, "item-updated",
                     G_CALLBACK(record_item_change), &update_log);
    g_signal_connect(inventory, "item-deleted",
                     G_CALLBACK(record_item_change), &delete_log);

    gwy_inventory_insert(inventory, item_new("Fixme", -1));
    gwy_inventory_insert(inventory, item_new("Second", 2));
    gwy_inventory_insert(inventory, item_new("First", 1));
    g_assert_cmpuint(gwy_inventory_n_items(inventory), ==, 3);
    g_assert_cmphex(insert_log, ==, 0x121);
    g_assert_cmphex(update_log, ==, 0);
    g_assert_cmphex(delete_log, ==, 0);
    insert_log = 0;

    GwyItemTest *itemtest;
    g_assert((itemtest = gwy_inventory_get(inventory, "Fixme")));
    g_assert_cmpint(itemtest->value, ==, -1);
    g_assert((itemtest = gwy_inventory_get(inventory, "Second")));
    g_assert_cmpint(itemtest->value, ==, 2);
    g_assert((itemtest = gwy_inventory_get(inventory, "First")));
    g_assert_cmpint(itemtest->value, ==, 1);

    g_assert((itemtest = gwy_inventory_get_nth(inventory, 0)));
    g_assert_cmpstr(itemtest->name, ==, "First");
    g_assert((itemtest = gwy_inventory_get_nth(inventory, 1)));
    g_assert_cmpstr(itemtest->name, ==, "Fixme");
    g_assert((itemtest = gwy_inventory_get_nth(inventory, 2)));
    g_assert_cmpstr(itemtest->name, ==, "Second");

    gwy_inventory_forget_order(inventory);
    g_assert_cmphex(insert_log, ==, 0);
    g_assert_cmphex(update_log, ==, 0);
    g_assert_cmphex(delete_log, ==, 0);

    gwy_inventory_insert(inventory, item_new("Abel", 0));
    gwy_inventory_insert(inventory, item_new("Kain", 1));
    g_assert_cmphex(insert_log, ==, 0x45);
    g_assert_cmphex(update_log, ==, 0);
    g_assert_cmphex(delete_log, ==, 0);
    insert_log = 0;
    gwy_inventory_restore_order(inventory);
    g_assert_cmpuint(gwy_inventory_n_items(inventory), ==, 5);
    g_assert_cmphex(insert_log, ==, 0);
    g_assert_cmphex(update_log, ==, 0);
    g_assert_cmphex(delete_log, ==, 0);

    g_assert((itemtest = gwy_inventory_get_nth(inventory, 0)));
    g_assert_cmpstr(itemtest->name, ==, "Abel");
    g_assert((itemtest = gwy_inventory_get_nth(inventory, 3)));
    g_assert_cmpstr(itemtest->name, ==, "Kain");

    g_assert((itemtest = gwy_inventory_get(inventory, "Fixme")));
    itemtest->value = 3;
    gwy_inventory_rename(inventory, "Fixme", "Third");
    g_assert_cmphex(insert_log, ==, 0x5);
    g_assert_cmphex(update_log, ==, 0x5);
    g_assert_cmphex(delete_log, ==, 0x3);
    insert_log = update_log = delete_log = 0;

    g_assert((itemtest = gwy_inventory_get_nth(inventory, 4)));
    g_assert_cmpstr(itemtest->name, ==, "Third");

    gwy_inventory_delete_nth(inventory, 0);
    gwy_inventory_delete(inventory, "Kain");
    g_assert_cmpuint(gwy_inventory_n_items(inventory), ==, 3);
    g_assert_cmphex(insert_log, ==, 0);
    g_assert_cmphex(update_log, ==, 0);
    g_assert_cmphex(delete_log, ==, 0x12);
    delete_log = 0;

    g_object_unref(inventory);
    g_assert_cmpuint(item_destroy_count, ==, 5);
}

/***************************************************************************
 *
 * Fit Task
 *
 ***************************************************************************/

static gboolean
test_fit_task_gaussian_point(gdouble x,
                             gdouble *retval,
                             gdouble xoff,
                             gdouble yoff,
                             gdouble b,
                             gdouble a)
{
    x = (x - xoff)/b;
    *retval = yoff + a*exp(-x*x);
    return b != 0.0;
}

static gboolean
test_fit_task_gaussian_vector(guint i,
                              gpointer user_data,
                              gdouble *retval,
                              gdouble xoff,
                              gdouble yoff,
                              gdouble b,
                              gdouble a)
{
    GwyPointXY *pts = (GwyPointXY*)user_data;
    gdouble x = (pts[i].x - xoff)/b;
    *retval = yoff + a*exp(-x*x) - pts[i].y;
    return b != 0.0;
}

static gboolean
test_fit_task_gaussian_vfunc(guint i,
                             gpointer user_data,
                             gdouble *retval,
                             const gdouble *params)
{
    GwyPointXY *pts = (GwyPointXY*)user_data;
    gdouble x = (pts[i].x - params[0])/params[2];
    *retval = params[1] + params[3]*exp(-x*x) - pts[i].y;
    return params[2] != 0.0;
}

static GwyPointXY*
test_fitter_make_gaussian_data(gdouble xoff,
                               gdouble yoff,
                               gdouble b,
                               gdouble a,
                               guint ndata,
                               guint seed)
{
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, seed);
    GwyPointXY *data = g_new(GwyPointXY, ndata);
    gdouble xmin = xoff - b*(2 + 3*g_rand_double(rng));
    gdouble xmax = xoff + b*(2 + 3*g_rand_double(rng));
    gdouble noise = 0.15*a;
    for (guint i = 0; i < ndata; i++) {
        data[i].x = g_rand_double_range(rng, xmin, xmax);
        test_fit_task_gaussian_point(data[i].x, &data[i].y,
                                     xoff, yoff, b, a);
        data[i].y += g_rand_double_range(rng, -noise, noise);
    }
    g_rand_free(rng);
    return data;
}

static void
test_fit_task_check_fit(GwyFitTask *fittask,
                        const gdouble *param_good)
{
    GwyFitter *fitter = gwy_fit_task_get_fitter(fittask);
    guint nparam = gwy_fitter_get_n_params(fitter);
    gdouble res_init = gwy_fit_task_eval_residuum(fittask);
    g_assert_cmpfloat(res_init, >, 0.0);
    g_assert(gwy_fit_task_fit(fittask));
    gdouble res = gwy_fitter_get_residuum(fitter);
    g_assert_cmpfloat(res, >, 0.0);
    g_assert_cmpfloat(res, <, 0.01*res_init);
    gdouble param_final[nparam];
    g_assert(gwy_fitter_get_params(fitter, param_final));
    /* Conservative result check */
    gdouble eps = 0.2;
    g_assert_cmpfloat(fabs(param_final[0] - param_good[0]),
                      <=,
                      eps*fabs(param_good[0]));
    g_assert_cmpfloat(fabs(param_final[1] - param_good[1]),
                      <=,
                      eps*fabs(param_good[1]));
    g_assert_cmpfloat(fabs(param_final[2] - param_good[2]),
                      <=,
                      eps*fabs(param_good[2]));
    g_assert_cmpfloat(fabs(param_final[3] - param_good[3]),
                      <=,
                      eps*fabs(param_good[3]));
    /* Error estimate check */
    gdouble error[nparam];
    eps = 0.3;
    g_assert(gwy_fit_task_get_param_errors(fittask, TRUE, error));
    g_assert_cmpfloat(fabs((param_final[0] - param_good[0])/error[0]),
                      <=,
                      1.0 + eps);
    g_assert_cmpfloat(fabs((param_final[1] - param_good[1])/error[1]),
                      <=,
                      1.0 + eps);
    g_assert_cmpfloat(fabs((param_final[2] - param_good[2])/error[2]),
                      <=,
                      1.0 + eps);
    g_assert_cmpfloat(fabs((param_final[3] - param_good[3])/error[3]),
                      <=,
                      1.0 + eps);
}

static void
test_fit_task_point(void)
{
    enum { nparam = 4, ndata = 100 };
    const gdouble param[nparam] = { 1e-5, 1e6, 1e-4, 2e5 };
    const gdouble param_init[nparam] = { 4e-5, -1e6, 2e-4, 4e5 };
    GwyFitTask *fittask = gwy_fit_task_new();
    GwyFitter *fitter = gwy_fit_task_get_fitter(fittask);
    GwyPointXY *data = test_fitter_make_gaussian_data(param[0], param[1],
                                                      param[2], param[3],
                                                      ndata, 42);
    gwy_fit_task_set_point_function
        (fittask, nparam, (GwyFitTaskPointFunc)test_fit_task_gaussian_point);
    gwy_fit_task_set_point_data(fittask, data, ndata);
    gwy_fitter_set_params(fitter, param_init);
    test_fit_task_check_fit(fittask, param);
    g_free(data);
    g_object_unref(fittask);
}

static void
test_fit_task_fixed(void)
{
    enum { nparam = 4, ndata = 100 };
    const gdouble param[nparam] = { 1e-5, 1e6, 1e-4, 2e5 };
    const gdouble param_init[nparam] = { 4e-5, -1e6, 2e-4, 4e5 };
    GwyFitTask *fittask = gwy_fit_task_new();
    GwyFitter *fitter = gwy_fit_task_get_fitter(fittask);
    GwyPointXY *data = test_fitter_make_gaussian_data(param[0], param[1],
                                                      param[2], param[3],
                                                      ndata, 42);
    gwy_fit_task_set_point_function
        (fittask, nparam, (GwyFitTaskPointFunc)test_fit_task_gaussian_point);
    gwy_fit_task_set_point_data(fittask, data, ndata);
    for (guint i = 0; i < nparam; i++) {
        gwy_fitter_set_params(fitter, param_init);
        gwy_fit_task_set_fixed_param(fittask, i, TRUE);
        gdouble res_init = gwy_fit_task_eval_residuum(fittask);
        g_assert_cmpfloat(res_init, >, 0.0);
        g_assert(gwy_fit_task_fit(fittask));
        gdouble res = gwy_fitter_get_residuum(fitter);
        g_assert_cmpfloat(res, >, 0.0);
        g_assert_cmpfloat(res, <, 0.1*res_init);
        /* Fixed params are not touched. */
        gdouble param_final[nparam];
        g_assert(gwy_fitter_get_params(fitter, param_final));
        g_assert_cmpfloat(param_final[i], ==, param_init[i]);
        gdouble error[nparam];
        g_assert(gwy_fit_task_get_param_errors(fittask, TRUE, error));
        g_assert_cmpfloat(error[i], ==, 0);
        gwy_fit_task_set_fixed_param(fittask, i, FALSE);
    }
    g_free(data);
    g_object_unref(fittask);
}

static void
test_fit_task_vector(void)
{
    enum { nparam = 4, ndata = 100 };
    const gdouble param[nparam] = { 1e-5, 1e6, 1e-4, 2e5 };
    const gdouble param_init[nparam] = { 4e-5, -1e6, 2e-4, 4e5 };
    GwyFitTask *fittask = gwy_fit_task_new();
    GwyFitter *fitter = gwy_fit_task_get_fitter(fittask);
    GwyPointXY *data = test_fitter_make_gaussian_data(param[0], param[1],
                                                      param[2], param[3],
                                                      ndata, 42);
    gwy_fit_task_set_vector_function
        (fittask, nparam, (GwyFitTaskVectorFunc)test_fit_task_gaussian_vector);
    gwy_fit_task_set_vector_data(fittask, data, ndata);
    gwy_fitter_set_params(fitter, param_init);
    test_fit_task_check_fit(fittask, param);
    g_free(data);
    g_object_unref(fittask);
}

static void
test_fit_task_vfunc(void)
{
    enum { nparam = 4, ndata = 100 };
    const gdouble param[nparam] = { 1e-5, 1e6, 1e-4, 2e5 };
    const gdouble param_init[nparam] = { 4e-5, -1e6, 2e-4, 4e5 };
    GwyFitTask *fittask = gwy_fit_task_new();
    GwyFitter *fitter = gwy_fit_task_get_fitter(fittask);
    GwyPointXY *data = test_fitter_make_gaussian_data(param[0], param[1],
                                                      param[2], param[3],
                                                      ndata, 42);
    gwy_fit_task_set_vector_vfunction
        (fittask, nparam,
         (GwyFitTaskVectorVFunc)test_fit_task_gaussian_vfunc, NULL);
    gwy_fit_task_set_vector_data(fittask, data, ndata);
    gwy_fitter_set_params(fitter, param_init);
    test_fit_task_check_fit(fittask, param);
    g_free(data);
    g_object_unref(fittask);
}

/***************************************************************************
 *
 * Main
 *
 ***************************************************************************/

int
main(int argc, char *argv[])
{
    if (RUNNING_ON_VALGRIND)
        setenv("G_SLICE", "always-malloc", TRUE);

    g_test_init(&argc, &argv, NULL);
    g_type_init();

    g_test_add_func("/testlibgwy/version", test_version);
    g_test_add_func("/testlibgwy/error-list", test_error_list);
    g_test_add_func("/testlibgwy/memmem", test_memmem);
    g_test_add_func("/testlibgwy/next-line", test_next_line);
    g_test_add_func("/testlibgwy/pack", test_pack);
    g_test_add_func("/testlibgwy/math/sort", test_math_sort);
    g_test_add_func("/testlibgwy/math/cholesky", test_math_cholesky);
    g_test_add_func("/testlibgwy/math/linalg", test_math_linalg);
    g_test_add_func("/testlibgwy/interpolation", test_interpolation);
    g_test_add_func("/testlibgwy/expr/evaluate", test_expr_evaluate);
    g_test_add_func("/testlibgwy/expr/vector", test_expr_vector);
    g_test_add_func("/testlibgwy/expr/garbage", test_expr_garbage);
    g_test_add_func("/testlibgwy/fit-task/point", test_fit_task_point);
    g_test_add_func("/testlibgwy/fit-task/vector", test_fit_task_vector);
    g_test_add_func("/testlibgwy/fit-task/vfunc", test_fit_task_vfunc);
    g_test_add_func("/testlibgwy/fit-task/fixed", test_fit_task_fixed);
    g_test_add_func("/testlibgwy/serialize/simple", test_serialize_simple);
    g_test_add_func("/testlibgwy/serialize/data", test_serialize_data);
    g_test_add_func("/testlibgwy/serialize/nested", test_serialize_nested);
    g_test_add_func("/testlibgwy/serialize/error", test_serialize_error);
    g_test_add_func("/testlibgwy/serialize/boxed", test_serialize_boxed);
    g_test_add_func("/testlibgwy/deserialize/simple", test_deserialize_simple);
    g_test_add_func("/testlibgwy/deserialize/data", test_deserialize_data);
    g_test_add_func("/testlibgwy/deserialize/nested", test_deserialize_nested);
    g_test_add_func("/testlibgwy/deserialize/boxed", test_deserialize_boxed);
    g_test_add_func("/testlibgwy/deserialize/garbage", test_deserialize_garbage);
    g_test_add_func("/testlibgwy/unit/parse", test_unit_parse);
    g_test_add_func("/testlibgwy/unit/arithmetic", test_unit_arithmetic);
    g_test_add_func("/testlibgwy/unit/serialize", test_unit_serialize);
    g_test_add_func("/testlibgwy/value-format/simple", test_value_format_simple);
    g_test_add_func("/testlibgwy/container/data", test_container_data);
    g_test_add_func("/testlibgwy/container/refcount", test_container_refcount);
    g_test_add_func("/testlibgwy/container/serialize", test_container_serialize);
    g_test_add_func("/testlibgwy/container/text", test_container_text);
    g_test_add_func("/testlibgwy/container/boxed", test_container_boxed);
    g_test_add_func("/testlibgwy/array/data", test_array_data);
    g_test_add_func("/testlibgwy/inventory/data", test_inventory_data);

    return g_test_run();
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
