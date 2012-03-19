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

#include <stdlib.h>
#include "testlibgwy.h"

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

void
master_cancel_one(guint nproc)
{
    if (g_test_trap_fork(10000000UL, 0)) {
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
            GThread *cthread = g_thread_create(&cancel_cancel,
                                               cancellable, FALSE, &error);
            g_assert_no_error(error);
            g_assert(cthread);

            guint count = 0;
            ok = gwy_master_manage_tasks(master, 0, &cancel_worker,
                                         &cancel_task, &cancel_result,
                                         &count, cancellable);

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

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
