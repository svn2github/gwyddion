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
sum_numbers_task(G_GNUC_UNUSED GwyMaster *master,
                 gpointer user_data)
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
sum_numbers_result(G_GNUC_UNUSED GwyMaster *master,
                   gpointer result,
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
    enum { niter = 1000 };
    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        GwyMaster *master = gwy_master_new();
        guint64 size = g_rand_int_range(rng, 1000, 1000000);
        guint64 chunk_size = g_rand_int_range(rng, 1, size+1);

        gwy_master_set_worker_func(master, &sum_numbers_worker);
        gboolean ok = gwy_master_create_workers(master, nproc, &error);
        g_assert_no_error(error);
        g_assert(ok);

        SumNumbersState state = { size, chunk_size, 0, 0 };
        gwy_master_set_task_func(master, &sum_numbers_task, &state);
        gwy_master_set_result_func(master, &sum_numbers_result, &state);
        gwy_master_manage_tasks(master);

        g_object_unref(master);

        guint64 expected = gwy_power_sum(size-1, 1);
        g_assert_cmpfloat(fabs(state.total_sum - expected),
                          <=,
                          1e-14*expected);
    }

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

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
