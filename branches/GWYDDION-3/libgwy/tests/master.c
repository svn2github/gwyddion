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
#include <stdlib.h>

#ifdef HAVE_VALGRIND
#include <valgrind/valgrind.h>
#else
#define RUNNING_ON_VALGRIND 0
#endif

/***************************************************************************
 *
 * Master
 *
 ***************************************************************************/

typedef struct {
    guint64 from;
    guint64 to;
    guint64 result;
} SumNumbersTask;

typedef struct {
    guint64 size;
    guint64 chunk_size;
    guint64 current;
    guint64 total_sum;
} SumNumbersState;

typedef struct {
    guint *data;
    GwyLinePart *segment;
} IncrIntervalsTask;

typedef struct {
    GArray *segments;
    guint *data;
    GList *blocked;
    guint current;
    guint noverlaps;
} IncrIntervalsState;

static gpointer
sum_numbers_worker(gpointer taskp,
                   G_GNUC_UNUSED gpointer data)
{
    SumNumbersTask *task = (SumNumbersTask*)taskp;
    guint64 s = 0, from = task->from, to = task->to;

    g_assert_cmpuint(to, >, from);
    //g_printerr("%" G_GUINT64_FORMAT " %" G_GUINT64_FORMAT "\n", from, to);
    for (guint64 i = from; i < to; i++)
        s += i;

    task->result = s;
    return taskp;
}

static gpointer
sum_numbers_task(gpointer user_data)
{
    SumNumbersState *state = (SumNumbersState*)user_data;
    if (state->current == state->size)
        return NULL;
    g_assert_cmpuint(state->current, <, state->size);

    SumNumbersTask *task = g_slice_new(SumNumbersTask);
    task->from = state->current;
    task->to = state->current + MIN(state->chunk_size,
                                    state->size - state->current);
    state->current = task->to;
    return task;
}

static void
sum_numbers_result(gpointer result,
                   gpointer user_data)
{
    SumNumbersTask *task = (SumNumbersTask*)result;
    SumNumbersState *state = (SumNumbersState*)user_data;

    state->total_sum += task->result;
    g_slice_free(SumNumbersTask, task);
}

static void
master_sum_numbers_one(guint nproc)
{
    GError *error = NULL;
    enum { niter = 200 };
    GRand *rng = g_rand_new_with_seed(42);

    //g_test_timer_start();
    for (guint iter = 0; iter < niter; iter++) {
        GwyMaster *master = gwy_master_new();
        guint64 size = g_rand_int_range(rng, 1000, 1000000);
        guint64 chunk_size = g_rand_int_range(rng, 1, size+1);

        gboolean ok = gwy_master_create_workers(master, nproc, &error);
        g_assert_no_error(error);
        g_assert(ok);

        SumNumbersState state = { size, chunk_size, 0, 0 };
        ok = gwy_master_manage_tasks(master, 0, &sum_numbers_worker,
                                     &sum_numbers_task, &sum_numbers_result,
                                     &state,
                                     NULL);
        g_assert(ok);

        g_object_unref(master);

        guint64 expected = gwy_power_sum(size-1, 1);
        g_assert_cmpfloat(fabs(state.total_sum - expected),
                          <=,
                          1e-14*expected);
    }
    //g_printerr("%g\n", g_test_timer_elapsed());

    g_rand_free(rng);
}

void
test_master_sum_1(void)
{
    master_sum_numbers_one(1);
}

void
test_master_sum_2(void)
{
    master_sum_numbers_one(2);
}

void
test_master_sum_3(void)
{
    master_sum_numbers_one(3);
}

void
test_master_sum_4(void)
{
    master_sum_numbers_one(4);
}

void
test_master_sum_8(void)
{
    master_sum_numbers_one(8);
}

void
test_master_sum_16(void)
{
    master_sum_numbers_one(16);
}

void
test_master_sum_auto(void)
{
    master_sum_numbers_one(0);
}

enum { TASK_TIME = 200 };

static gpointer
cancel_worker(gpointer task,
              G_GNUC_UNUSED gpointer data)
{
    g_assert_cmpuint(GPOINTER_TO_UINT(task), ==, 0xdeadbeefU);
    g_usleep(TASK_TIME);
    return GUINT_TO_POINTER(0xbaddecafU);
}

static gpointer
cancel_task(G_GNUC_UNUSED gpointer user_data)
{
    return GUINT_TO_POINTER(0xdeadbeefU);
}

static void
cancel_result(gpointer result,
              gpointer user_data)
{
    g_assert_cmpuint(GPOINTER_TO_UINT(result), ==, 0xbaddecafU);
    guint *count = (guint*)user_data;
    (*count)++;
}

static gpointer
cancel_cancel(gpointer user_data)
{
    GCancellable *cancellable = (GCancellable*)user_data;
    g_assert(G_IS_CANCELLABLE(cancellable));
    guint delay = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(cancellable),
                                                     "delay"));
    g_usleep(delay);
    g_cancellable_cancel(cancellable);
    return NULL;
}

static void
master_cancel_one(guint nproc)
{
    guint64 timeout;
    if (RUNNING_ON_VALGRIND)
        timeout = G_GUINT64_CONSTANT(1000000000);
    else
        timeout = G_GUINT64_CONSTANT(10000000);

    if (g_test_trap_fork(timeout, 0)) {
        GError *error = NULL;
        enum { niter = 200 };
        GRand *rng = g_rand_new_with_seed(42);

        for (guint iter = 0; iter < niter; iter++) {
            GwyMaster *master = gwy_master_new();
            guint delay = g_rand_int_range(rng, 0, 10000);

            gboolean ok = gwy_master_create_workers(master, nproc, &error);
            g_assert_no_error(error);
            g_assert(ok);

            GCancellable *cancellable = g_cancellable_new();
            g_object_set_data(G_OBJECT(cancellable),
                              "delay", GUINT_TO_POINTER(delay));
            GThread *cthread = g_thread_new("canceller", &cancel_cancel,
                                            cancellable);
            g_assert(cthread);

            guint count = 0;
            ok = gwy_master_manage_tasks(master, 0, &cancel_worker,
                                         &cancel_task, &cancel_result,
                                         &count, cancellable);
            g_assert(!ok);

            // Wait until the cancellation is performed.  Fixes master being
            // destroyed and test terminated before cancel_cancel() gets to
            // finish.
            g_thread_join(cthread);

            g_object_unref(master);
            g_object_unref(cancellable);

            // The actual number of calls can differ wildly depending on the
            // scheduling.  How to check something meaningful?
            //gdouble expected_count = (gdouble)delay/TASK_TIME;
        }
        g_rand_free(rng);
        exit(0);
    }
    else {
         g_test_trap_assert_passed();
    }
}

// XXX: The cancellation tests occasionally (rarely) fail.  This is because
// g_test_trap_fork() interacts badly with threads and should be avoided, see
// https://bugzilla.gnome.org/show_bug.cgi?id=679683
// It must rewritten once something better is available.
void
test_master_cancel_1(void)
{
    master_cancel_one(1);
}

void
test_master_cancel_2(void)
{
    master_cancel_one(2);
}

void
test_master_cancel_3(void)
{
    master_cancel_one(3);
}

void
test_master_cancel_4(void)
{
    master_cancel_one(4);
}

void
test_master_cancel_8(void)
{
    master_cancel_one(8);
}

void
test_master_cancel_16(void)
{
    master_cancel_one(16);
}

void
test_master_cancel_auto(void)
{
    master_cancel_one(0);
}

typedef struct {
    gint create_count;
    gint destroy_count;
} DataCount;

static gpointer
data_create(gpointer user_data)
{
    DataCount *counts = (DataCount*)user_data;
    g_atomic_int_inc(&counts->create_count);
    return counts;
}

static void
data_destroy(gpointer data)
{
    DataCount *counts = (DataCount*)data;
    g_atomic_int_inc(&counts->destroy_count);
}

void
test_master_data(void)
{
    for (gint nproc = 1; nproc < 20; nproc++) {
        GError *error = NULL;
        GwyMaster *master = gwy_master_new();
        gboolean ok = gwy_master_create_workers(master, nproc, &error);
        g_assert_no_error(error);
        g_assert(ok);

        DataCount counts = { 0, 0 };

        gwy_master_create_data(master, &data_create, &counts);
        gwy_master_destroy_data(master, &data_destroy);

        g_assert_cmpint(counts.create_count, ==, nproc);
        g_assert_cmpint(counts.destroy_count, ==, nproc);

        g_object_unref(master);
    }
}

static gpointer
incr_intervals_worker(gpointer taskp,
                      G_GNUC_UNUSED gpointer data)
{
    IncrIntervalsTask *task = (IncrIntervalsTask*)taskp;
    guint pos = task->segment->pos, len = task->segment->len;

    // Make failure much more probable if blocking does not work by creating
    // a time window between read and write.
    guint *d = g_memdup(task->data + pos, len*sizeof(gdouble));
    for (guint i = 0; i < len; i++)
        d[i]++;
    gwy_assign(task->data + pos, d, len);
    g_free(d);

    return taskp;
}

static gpointer
incr_intervals_task(gpointer user_data)
{
    IncrIntervalsState *state = (IncrIntervalsState*)user_data;
    GArray *segments = state->segments;
    if (state->current == segments->len)
        return NULL;
    g_assert_cmpuint(state->current, <, segments->len);

    GwyLinePart *segment = &g_array_index(state->segments,
                                          GwyLinePart, state->current);
    for (GList *l = state->blocked; l; l = g_list_next(l)) {
        GwyLinePart *s = (GwyLinePart*)l->data;
        if (gwy_overlapping(segment->pos, segment->len, s->pos, s->len)) {
            state->noverlaps++;
            return GWY_MASTER_TRY_AGAIN;
        }
    }

    state->blocked = g_list_append(state->blocked, segment);
    IncrIntervalsTask *task = g_slice_new(IncrIntervalsTask);
    task->segment = segment;
    task->data = state->data;
    state->current++;
    return task;
}

static void
incr_intervals_result(gpointer result,
                      gpointer user_data)
{
    IncrIntervalsTask *task = (IncrIntervalsTask*)result;
    IncrIntervalsState *state = (IncrIntervalsState*)user_data;

    g_assert(g_list_find(state->blocked, task->segment));
    state->blocked = g_list_remove(state->blocked, task->segment);
    g_slice_free(IncrIntervalsTask, task);
}

static void
master_try_again_one(guint nproc)
{
    GError *error = NULL;
    enum { niter = 200, nsum = 20 };
    GRand *rng = g_rand_new_with_seed(42);

    //g_test_timer_start();
    for (guint iter = 0; iter < niter; iter++) {
        GwyMaster *master = gwy_master_new();
        guint64 size = g_rand_int_range(rng, 100, 1000);
        guint *data = g_new0(guint, size);
        GArray *segments = g_array_new(FALSE, FALSE, sizeof(GwyLinePart));

        // Create random segments of @data ensuring that each item is contained
        // in exactly @nsum segments.
        for (guint s = 0; s < nsum; s++) {
            guint start = 0;
            while (start < size) {
                guint step = g_rand_int_range(rng, 1, size/10);
                if (step + start > size)
                    step = size - start;
                GwyLinePart segment = { start, step };
                g_array_append_val(segments, segment);
                start += step;
            }
        }
        for (guint i = 0; i < size; i++) {
            guint j1 = g_rand_int_range(rng, 0, segments->len);
            guint j2 = g_rand_int_range(rng, 0, segments->len);
            GWY_SWAP(GwyLinePart,
                     g_array_index(segments, GwyLinePart, j1),
                     g_array_index(segments, GwyLinePart, j2));
        }

        gboolean ok = gwy_master_create_workers(master, nproc, &error);
        g_assert_no_error(error);
        g_assert(ok);

        IncrIntervalsState state = { segments, data, NULL, 0, 0 };
        ok = gwy_master_manage_tasks(master, 0, &incr_intervals_worker,
                                     &incr_intervals_task,
                                     &incr_intervals_result,
                                     &state,
                                     NULL);
        g_assert(ok);

        for (guint i = 0; i < size; i++) {
            g_assert_cmpuint(data[i], ==, nsum);
        }

        if (nproc == 1) {
            g_assert_cmpuint(state.noverlaps, ==, 0);
        }

        g_object_unref(master);
        g_free(data);
        g_array_free(segments, TRUE);
    }
    //g_printerr("%g\n", g_test_timer_elapsed());

    g_rand_free(rng);
}

void
test_master_try_again_1(void)
{
    master_try_again_one(1);
}

void
test_master_try_again_2(void)
{
    master_try_again_one(2);
}

void
test_master_try_again_3(void)
{
    master_try_again_one(3);
}

void
test_master_try_again_4(void)
{
    master_try_again_one(4);
}

void
test_master_try_again_8(void)
{
    master_try_again_one(8);
}

void
test_master_try_again_16(void)
{
    master_try_again_one(16);
}

void
test_master_try_again_auto(void)
{
    master_try_again_one(0);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
