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

#include "testlibgwy.h"

/***************************************************************************
 *
 * Int Set
 *
 ***************************************************************************/

static gint
random_integer(GRand *rng)
{
    gdouble x = -5.0*log(g_rand_double(rng));
    if (g_rand_boolean(rng))
        x = -x;

    return (gint)gwy_round(x);
}

static void
int_set_randomize(GwyIntSet *intset,
                  GRand *rng)
{
    guint size = g_rand_int_range(rng, 0, 20) + g_rand_int_range(rng, 0, 10);

    for (guint i = 0; i < size; i++)
        gwy_int_set_add(intset, random_integer(rng));
}

void
int_set_randomize_range(GwyIntSet *intset,
                        GRand *rng,
                        gint min,
                        gint max)
{
    guint n = 2*(max - min);

    while (n--) {
        gint value = g_rand_int_range(rng, min, max+1);
        gwy_int_set_add(intset, value);
    }
}

void
int_set_assert_equal(const GwyIntSet *result,
                     const GwyIntSet *reference)
{
    g_assert(GWY_IS_INT_SET(result));
    g_assert(GWY_IS_INT_SET(reference));
    g_assert_cmpuint(gwy_int_set_size(result), ==, gwy_int_set_size(reference));
    compare_properties(G_OBJECT(result), G_OBJECT(reference));

    guint nres, nref;
    gint *resvalues = gwy_int_set_values(result, &nres);
    gint *refvalues = gwy_int_set_values(reference, &nref);
    g_assert_cmpuint(nres, ==, nref);

    for (guint i = 0; i < nref; i++) {
        g_assert_cmpint(resvalues[i], ==, refvalues[i]);
    }
    g_free(refvalues);
    g_free(resvalues);
}

static void
int_set_assert_equal_object(GObject *object, GObject *reference)
{
    int_set_assert_equal(GWY_INT_SET(object), GWY_INT_SET(reference));
}

static void
int_set_assert_order(GwyIntSet *intset)
{
    guint n;
    gint *values = gwy_int_set_values(intset, &n);

    for (guint i = 1; i < n; i++) {
        g_assert_cmpint(values[i], >, values[i-1]);
    }

    g_free(values);
}

void
test_int_set_serialize(void)
{
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = g_test_slow() ? 200 : 100;

    for (guint iter = 0; iter < niter; iter++) {
        GwyIntSet *original = gwy_int_set_new();
        int_set_randomize(original, rng);
        GwyIntSet *copy;

        serializable_duplicate(GWY_SERIALIZABLE(original),
                               int_set_assert_equal_object);
        serializable_assign(GWY_SERIALIZABLE(original),
                            int_set_assert_equal_object);
        copy = GWY_INT_SET(serialize_and_back(G_OBJECT(original),
                                              int_set_assert_equal_object));
        int_set_assert_order(copy);
        g_object_unref(copy);
        g_object_unref(original);
    }
    g_rand_free(rng);
}

void
test_int_set_serialize_failure_odd(void)
{
    GOutputStream *stream = g_memory_output_stream_new(NULL, 0,
                                                       g_realloc, g_free);
    GDataOutputStream *datastream = g_data_output_stream_new(stream);
    g_data_output_stream_set_byte_order(datastream,
                                        G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN);

    data_stream_put_string0(datastream, "GwyIntSet", NULL, NULL);
    g_data_output_stream_put_uint64(datastream, 0, NULL, NULL);
    guint len = 5;
    data_stream_put_string0(datastream, "ranges", NULL, NULL);
    g_data_output_stream_put_byte(datastream, GWY_SERIALIZABLE_INT32_ARRAY,
                                  NULL, NULL);
    g_data_output_stream_put_uint64(datastream, len, NULL, NULL);
    for (guint i = 0; i < len; i++)
        g_data_output_stream_put_uint32(datastream, i, NULL, NULL);

    GwyErrorList *error_list = NULL;
    gwy_error_list_add(&error_list,
                       GWY_DESERIALIZE_ERROR, GWY_DESERIALIZE_ERROR_INVALID,
                       "Data length of ‘GwyIntSet’ is %lu which is not "
                       "a multiple of 2.",
                       (gulong)len);

    deserialize_assert_failure(G_MEMORY_OUTPUT_STREAM(stream), error_list);
    gwy_error_list_clear(&error_list);
    g_object_unref(datastream);
    g_object_unref(stream);
}

void
test_int_set_serialize_failure_noncanon(void)
{
    GOutputStream *stream = g_memory_output_stream_new(NULL, 0,
                                                       g_realloc, g_free);
    GDataOutputStream *datastream = g_data_output_stream_new(stream);
    g_data_output_stream_set_byte_order(datastream,
                                        G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN);

    data_stream_put_string0(datastream, "GwyIntSet", NULL, NULL);
    g_data_output_stream_put_uint64(datastream, 0, NULL, NULL);
    guint len = 8;
    data_stream_put_string0(datastream, "ranges", NULL, NULL);
    g_data_output_stream_put_byte(datastream, GWY_SERIALIZABLE_INT32_ARRAY,
                                  NULL, NULL);
    g_data_output_stream_put_uint64(datastream, len, NULL, NULL);
    for (guint i = 0; i < len; i++)
        g_data_output_stream_put_uint32(datastream, i/3, NULL, NULL);

    GwyErrorList *error_list = NULL;
    gwy_error_list_add(&error_list,
                       GWY_DESERIALIZE_ERROR, GWY_DESERIALIZE_ERROR_INVALID,
                       "GwyIntSet ranges are not in canonical form.");

    deserialize_assert_failure(G_MEMORY_OUTPUT_STREAM(stream), error_list);
    gwy_error_list_clear(&error_list);
    g_object_unref(datastream);
    g_object_unref(stream);
}

void
test_int_set_assign(void)
{
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = 20;

    for (guint iter = 0; iter < niter; iter++) {
        GwyIntSet *original = gwy_int_set_new(), *copy = gwy_int_set_new();
        int_set_randomize(original, rng);

        guint counter = 0, xcounter = 0;
        g_signal_connect_swapped(copy, "assigned",
                                 G_CALLBACK(record_signal), &counter);
        g_signal_connect_swapped(copy, "added",
                                 G_CALLBACK(record_signal), &xcounter);
        g_signal_connect_swapped(copy, "removed",
                                 G_CALLBACK(record_signal), &xcounter);
        g_signal_connect_swapped(original, "added",
                                 G_CALLBACK(record_signal), &xcounter);
        g_signal_connect_swapped(original, "removed",
                                 G_CALLBACK(record_signal), &xcounter);
        g_signal_connect_swapped(original, "assigned",
                                 G_CALLBACK(record_signal), &xcounter);
        gwy_int_set_assign(copy, original);
        int_set_assert_order(copy);
        int_set_assert_equal(copy, original);
        g_assert_cmpuint(counter, ==, 1);
        g_assert_cmpuint(xcounter, ==, 0);

        g_object_unref(copy);
        g_object_unref(original);
    }
    g_rand_free(rng);
}

static void
check_add(GwyIntSet *intset,
          GHashTable *reference,
          gint value)
{
    guint acounter = 0, rcounter = 0, n = gwy_int_set_size(intset);
    gboolean present = !!g_hash_table_lookup(reference, GINT_TO_POINTER(value));

    gulong aid = g_signal_connect_swapped(intset, "added",
                                          G_CALLBACK(record_signal), &acounter);
    gulong rid = g_signal_connect_swapped(intset, "removed",
                                          G_CALLBACK(record_signal), &rcounter);
    gboolean changed = gwy_int_set_add(intset, value);
    g_signal_handler_disconnect(intset, rid);
    g_signal_handler_disconnect(intset, aid);

    if (present) {
        g_assert(!changed);
        g_assert_cmpuint(gwy_int_set_size(intset), ==, n);
        g_assert_cmpuint(acounter, ==, 0);
        g_assert_cmpuint(rcounter, ==, 0);
    }
    else {
        g_assert(changed);
        g_assert_cmpuint(gwy_int_set_size(intset), ==, n+1);
        g_assert_cmpuint(acounter, ==, 1);
        g_assert_cmpuint(rcounter, ==, 0);
        g_hash_table_insert(reference,
                            GINT_TO_POINTER(value), GUINT_TO_POINTER(TRUE));
    }
    g_assert(gwy_int_set_contains(intset, value));
    g_assert(g_hash_table_lookup(reference, GINT_TO_POINTER(value)));
}

static void
check_remove(GwyIntSet *intset,
             GHashTable *reference,
             gint value)
{
    guint acounter = 0, rcounter = 0, n = gwy_int_set_size(intset);
    gboolean present = !!g_hash_table_lookup(reference, GINT_TO_POINTER(value));

    gulong aid = g_signal_connect_swapped(intset, "added",
                                          G_CALLBACK(record_signal), &acounter);
    gulong rid = g_signal_connect_swapped(intset, "removed",
                                          G_CALLBACK(record_signal), &rcounter);
    gboolean changed = gwy_int_set_remove(intset, value);
    g_signal_handler_disconnect(intset, rid);
    g_signal_handler_disconnect(intset, aid);

    if (present) {
        g_assert(changed);
        g_assert_cmpuint(gwy_int_set_size(intset), ==, n-1);
        g_assert_cmpuint(acounter, ==, 0);
        g_assert_cmpuint(rcounter, ==, 1);
        g_hash_table_remove(reference, GINT_TO_POINTER(value));
    }
    else {
        g_assert(!changed);
        g_assert_cmpuint(gwy_int_set_size(intset), ==, n);
        g_assert_cmpuint(acounter, ==, 0);
        g_assert_cmpuint(rcounter, ==, 0);
    }
    g_assert(!gwy_int_set_contains(intset, value));
    g_assert(!g_hash_table_lookup(reference, GINT_TO_POINTER(value)));
}

static void
check_toggle(GwyIntSet *intset,
             GHashTable *reference,
             gint value)
{
    guint acounter = 0, rcounter = 0, n = gwy_int_set_size(intset);
    gboolean present = !!g_hash_table_lookup(reference, GINT_TO_POINTER(value));

    gulong aid = g_signal_connect_swapped(intset, "added",
                                          G_CALLBACK(record_signal), &acounter);
    gulong rid = g_signal_connect_swapped(intset, "removed",
                                          G_CALLBACK(record_signal), &rcounter);
    gboolean present_afterwards = gwy_int_set_toggle(intset, value);
    g_signal_handler_disconnect(intset, rid);
    g_signal_handler_disconnect(intset, aid);

    g_assert_cmpint(present_afterwards, !=, present);
    if (present) {
        g_assert_cmpuint(gwy_int_set_size(intset), ==, n-1);
        g_assert_cmpuint(acounter, ==, 0);
        g_assert_cmpuint(rcounter, ==, 1);
        g_hash_table_remove(reference, GINT_TO_POINTER(value));
    }
    else {
        g_assert_cmpuint(gwy_int_set_size(intset), ==, n+1);
        g_assert_cmpuint(acounter, ==, 1);
        g_assert_cmpuint(rcounter, ==, 0);
        g_hash_table_insert(reference,
                            GINT_TO_POINTER(value), GUINT_TO_POINTER(TRUE));
    }
    g_assert_cmpint(gwy_int_set_contains(intset, value),
                    ==, present_afterwards);
    g_assert_cmpint(!!g_hash_table_lookup(reference, GINT_TO_POINTER(value)),
                    ==, present_afterwards);
}

void
test_int_set_add_remove(void)
{
    enum { niter = 50, max_size = 1000 };
    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        GwyIntSet *intset = gwy_int_set_new();
        GHashTable *reference = g_hash_table_new(g_direct_hash, g_direct_equal);

        for (guint k = 0; k < max_size; k++) {
            gint v = g_rand_int_range(rng, 0, 3);
            if (v == 0)
                check_add(intset, reference, random_integer(rng));
            else if (v == 1)
                check_remove(intset, reference, random_integer(rng));
            else
                check_toggle(intset, reference, random_integer(rng));

            g_assert(!!gwy_int_set_size(intset)
                     == gwy_int_set_is_nonempty(intset));
        }
        int_set_assert_order(intset);

        g_hash_table_destroy(reference);
        g_object_unref(intset);
    }
    g_rand_free(rng);
}

static gboolean
is_present(const gint *values, guint n, gint value)
{
    while (n--) {
        if (*values == value)
            return TRUE;
        values++;
    }
    return FALSE;
}

void
test_int_set_values(void)
{
    enum { niter = 200, max_size = 50 };
    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        guint size = g_rand_int_range(rng, 0, max_size);
        gint *values = g_new(gint, MAX(size, 1));
        for (guint i = 0; i < size; i++)
            values[i] = random_integer(rng);

        GwyIntSet *intset = gwy_int_set_new_with_values(values, size);

        guint len;
        gint *isvalues = gwy_int_set_values(intset, &len);

        for (guint i = 0; i < size; i++) {
            g_assert(is_present(isvalues, len, values[i]));
        }
        for (guint i = 0; i < len; i++) {
            g_assert(is_present(values, size, isvalues[i]));
        }

        g_free(isvalues);
        g_free(values);

        int_set_assert_order(intset);
        g_object_unref(intset);
    }
    g_rand_free(rng);
}

void
test_int_set_ranges(void)
{
    enum { niter = 200, max_size = 50 };
    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        guint size = g_rand_int_range(rng, 0, max_size);
        gint *values = g_new(gint, MAX(size, 1));
        for (guint i = 0; i < size; i++)
            values[i] = random_integer(rng);

        GwyIntSet *intset = gwy_int_set_new_with_values(values, size);
        g_free(values);

        guint len;
        const GwyIntRange *ranges = gwy_int_set_ranges(intset, &len);

        GwyIntSet *copy = gwy_int_set_new();
        for (guint i = 0; i < len; i++) {
            g_assert_cmpint(ranges[i].from, <=, ranges[i].to);
            for (gint j = ranges[i].from; j <= ranges[i].to; j++)
                gwy_int_set_add(copy, j);
        }

        int_set_assert_equal(copy, intset);

        g_object_unref(copy);
        g_object_unref(intset);
    }
    g_rand_free(rng);
}

void
test_int_set_update(void)
{
    enum { niter = 500, max_size = 50 };
    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        guint size1 = g_rand_int_range(rng, 0, max_size);
        gint *values1 = g_new(gint, MAX(size1, 1));
        for (guint i = 0; i < size1; i++)
            values1[i] = random_integer(rng);

        guint size2 = g_rand_int_range(rng, 0, max_size);
        gint *values2 = g_new(gint, MAX(size2, 1));
        for (guint i = 0; i < size2; i++)
            values2[i] = random_integer(rng);

        GwyIntSet *intset1 = gwy_int_set_new_with_values(values1, size1);
        GwyIntSet *intset2 = gwy_int_set_new_with_values(values2, size2);
        gwy_int_set_update(intset1, values2, size2);
        int_set_assert_equal(intset1, intset2);
        int_set_assert_order(intset1);

        g_free(values2);
        g_free(values1);
        g_object_unref(intset2);
        g_object_unref(intset1);
    }
    g_rand_free(rng);
}

void
test_int_set_fill(void)
{
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = 20;

    for (guint iter = 0; iter < niter; iter++) {
        GwyIntSet *original = gwy_int_set_new(),
                  *copy = gwy_int_set_new();
        int_set_randomize(original, rng);

        guint counter = 0, xcounter = 0;
        g_signal_connect_swapped(copy, "assigned",
                                 G_CALLBACK(record_signal), &counter);
        g_signal_connect_swapped(copy, "added",
                                 G_CALLBACK(record_signal), &xcounter);
        g_signal_connect_swapped(copy, "removed",
                                 G_CALLBACK(record_signal), &xcounter);
        g_signal_connect_swapped(original, "added",
                                 G_CALLBACK(record_signal), &xcounter);
        g_signal_connect_swapped(original, "removed",
                                 G_CALLBACK(record_signal), &xcounter);
        g_signal_connect_swapped(original, "assigned",
                                 G_CALLBACK(record_signal), &xcounter);

        guint len;
        gint *isvalues = gwy_int_set_values(original, &len);
        gwy_int_set_fill(copy, isvalues, len);
        g_free(isvalues);

        int_set_assert_order(copy);
        int_set_assert_equal(copy, original);
        g_assert_cmpuint(counter, ==, 1);
        g_assert_cmpuint(xcounter, ==, 0);

        g_object_unref(copy);
        g_object_unref(original);
    }
    g_rand_free(rng);
}

typedef struct {
    GwyIntSet *reference;
    gint last;
} ForeachTestData;

static void
foreach_func(gint value,
             gpointer user_data)
{
    ForeachTestData *data = (ForeachTestData*)user_data;

    g_assert_cmpint(value, >, data->last);
    g_assert(!gwy_int_set_contains(data->reference, value));
    gwy_int_set_add(data->reference, value);
    data->last = value;
}

void
test_int_set_foreach(void)
{
    enum { niter = 500 };
    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        GwyIntSet *intset = gwy_int_set_new();
        int_set_randomize(intset, rng);

        ForeachTestData data = { gwy_int_set_new(), G_MININT };
        gwy_int_set_foreach(intset, foreach_func, &data);
        int_set_assert_equal(intset, data.reference);

        g_object_unref(data.reference);
        g_object_unref(intset);
    }
    g_rand_free(rng);
}

void
test_int_set_iter(void)
{
    enum { niter = 500 };
    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        GwyIntSet *intset = gwy_int_set_new();
        int_set_randomize(intset, rng);

        GwyIntSet *reference = gwy_int_set_new();
        GwyIntSetIter iiter;
        gint prev = G_MININT;

        if (gwy_int_set_first(intset, &iiter)) {
            do {
                g_assert(gwy_int_set_contains(intset, iiter.value));
                g_assert(!gwy_int_set_contains(reference, iiter.value));
                g_assert_cmpint(iiter.value, >, prev);
                gwy_int_set_add(reference, iiter.value);
            } while (gwy_int_set_next(intset, &iiter));
            g_assert(!gwy_int_set_next(intset, &iiter));
        }
        int_set_assert_equal(intset, reference);

        g_object_unref(reference);
        g_object_unref(intset);
    }
    g_rand_free(rng);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
