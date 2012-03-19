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

// TODO: If create-data and destroy-data are implemented as messages (i.e.
// the former is separated from the thread creation and the latter from RETIRE)
// then we can perform complete teardown/setup cycle without terminating and
// re-creating the threads.  Consequently, one Master could be recycled for
// everything.

#include "config.h"
#include <string.h>
#include <glib.h>
#include "libgwy/macros.h"
#include "libgwy/main.h"
#include "libgwy/master.h"

typedef enum {
    MSG_0,
    MSG_TASK,
    MSG_WORKER,
    MSG_CREATE,
    MSG_DESTROY,
    MSG_RETIRE,
} MessageType;

typedef struct {
    MessageType type;
    guint worker_id;
    gulong task_id;
    gpointer data;
} Message;

typedef struct {
    GwyMasterCreateDataFunc func;
    gpointer user_data;
} CreateDataFuncInfo;

typedef struct {
    GAsyncQueue *from_master;
    GAsyncQueue *to_master;
} WorkerInfo;

typedef struct {
    GThread *thread;
    GAsyncQueue *queue;
    WorkerInfo *info;
    gulong task_id;
} WorkerData;

struct _GwyMasterPrivate {
    GAsyncQueue *queue;

    GSList *idle_workers;
    gulong task_id;
    guint active_tasks;
    gboolean cancelled;
    gboolean exhausted;

    guint nworkers;
    WorkerData *workers;

    GwyMasterWorkerFunc work;
};

typedef struct _GwyMasterPrivate Master;

static void     gwy_master_finalize   (GObject *object);
static void     retire_workers        (GwyMaster *master);
static gpointer worker_thread_main    (gpointer thread_data);

G_DEFINE_TYPE(GwyMaster, gwy_master, G_TYPE_OBJECT);

static void
gwy_master_class_init(GwyMasterClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(Master));

    gobject_class->finalize = gwy_master_finalize;
}

static void
gwy_master_init(GwyMaster *master)
{
    master->priv = G_TYPE_INSTANCE_GET_PRIVATE(master, GWY_TYPE_MASTER, Master);
    Master *priv = master->priv;
    priv->queue = g_async_queue_new();
}

static void
gwy_master_finalize(GObject *object)
{
    GwyMaster *master = GWY_MASTER(object);
    Master *priv = master->priv;

    g_assert(!priv->active_tasks);

    retire_workers(master);
    g_async_queue_unref(priv->queue);

    G_OBJECT_CLASS(gwy_master_parent_class)->finalize(object);
}

static Message*
message_new(Master *priv,
            MessageType type,
            guint worker_id,
            gpointer data)
{
    Message *message = g_slice_new(Message);
    message->type = type;
    message->worker_id = worker_id;
    message->task_id = ++priv->task_id;
    message->data = data;
    return message;
}

static void
notify_all_workers(GwyMaster *master,
                   MessageType type,
                   gpointer data)
{
    Master *priv = master->priv;
    g_return_if_fail(!priv->active_tasks);

    for (guint i = 0; i < priv->nworkers; i++) {
        Message *message = message_new(priv, type, i, data);
        WorkerData *workerdata = priv->workers + i;
        workerdata->task_id = 1;
        g_async_queue_push(workerdata->queue, message);
    }

    guint nworkers = priv->nworkers;
    for (guint i = 0; i < nworkers; i++) {
        Message *message = g_async_queue_pop(priv->queue);
        guint worker_id = message->worker_id;
        g_assert(worker_id < nworkers);
        WorkerData *workerdata = priv->workers + worker_id;
        g_assert(workerdata->task_id == 1);
        g_assert(message->type == type);
        workerdata->task_id = 0;
        g_slice_free(Message, message);
    }
}

static void
retire_workers(GwyMaster *master)
{
    Master *priv = master->priv;
    notify_all_workers(master, MSG_RETIRE, NULL);
    for (guint i = 0; i < priv->nworkers; i++) {
        g_async_queue_unref(priv->workers[i].queue);
        g_slice_free(WorkerInfo, priv->workers[i].info);
    }
    g_slist_free(priv->idle_workers);
    priv->idle_workers = NULL;
    GWY_FREE(priv->workers);
    priv->nworkers = 0;
}

static gpointer
worker_thread_main(gpointer thread_data)
{
    WorkerInfo *winfo = (WorkerInfo*)thread_data;
    GAsyncQueue *from_master = winfo->from_master;
    GAsyncQueue *to_master = winfo->to_master;
    GwyMasterWorkerFunc worker = NULL;
    void *data = NULL;
    MessageType type = MSG_0;

    do {
        Message *message = g_async_queue_pop(from_master);
        type = message->type;

        if (message->type == MSG_TASK) {
            gpointer task = message->data;
            if (G_LIKELY(worker))
                message->data = worker(task, data);
            else
                g_critical("Trying to run caclulation "
                           "with no worker function set.");
        }
        else if (message->type == MSG_WORKER) {
            worker = (GwyMasterWorkerFunc)message->data;
        }
        else if (message->type == MSG_CREATE) {
            CreateDataFuncInfo *cdinfo = (CreateDataFuncInfo*)message->data;
            data = cdinfo->func ? cdinfo->func(cdinfo->user_data) : NULL;
        }
        else if (message->type == MSG_DESTROY) {
            GwyMasterDestroyDataFunc destroy = (GwyMasterDestroyDataFunc)message->data;
            if (destroy)
                destroy(data);
            data = NULL;
        }
        else if (type == MSG_RETIRE) {
            // Nothing special.
        }
        else {
            g_warning("Bogus message to worker %u\n", message->type);
        }

        g_async_queue_push(to_master, message);
    } while (type != MSG_RETIRE);

    return NULL;
}

/**
 * gwy_master_new:
 *
 * Creates a new parallel task manager.
 *
 * Returns: A new parallel task manager.
 **/
GwyMaster*
gwy_master_new(void)
{
    return g_object_newv(GWY_TYPE_MASTER, 0, NULL);
}

/**
 * gwy_master_create_workers:
 * @master: Parallel task manager.
 * @nworkers: Number of worker threads to create.  Passing zero means leaving
 *            the decision on @master which normally results in creating as
 *            many worker threads as there are available processor cores.
 * @error: Return location for the error, or %NULL.  The error canbe from the
 *         #GThreadError domain.
 *
 * Creates worker threads for a parallel task manager.
 *
 * The threads are not destroyed until @master is finalised.
 *
 * Worker threads must be created prior to running functions that execute some
 * code in each worker thread, such as gwy_master_manage_tasks(),
 * gwy_master_create_data() or gwy_master_destroy_data().
 *
 * Returns: %TRUE if all worker threads were successfully created, %FALSE on
 *          failure.
 **/
gboolean
gwy_master_create_workers(GwyMaster *master,
                          guint nworkers,
                          GError **error)
{
    g_return_val_if_fail(GWY_IS_MASTER(master), FALSE);

    Master *priv = master->priv;
    if (priv->workers) {
        g_warning("Master already has workers.");
        return TRUE;
    }

    if (nworkers < 1)
        nworkers = gwy_n_cpus();

    priv->workers = g_new0(WorkerData, nworkers);
    for (guint i = 0; i < nworkers; i++) {
        WorkerData *workerdata = priv->workers + i;
        WorkerInfo *winfo = g_slice_new(WorkerInfo);
        workerdata->info = winfo;
        workerdata->queue = g_async_queue_new();
        winfo->from_master = workerdata->queue;
        winfo->to_master = priv->queue;
        if (!(workerdata->thread = g_thread_create(&worker_thread_main,
                                                   winfo, FALSE, error))) {
            g_slice_free(WorkerInfo, winfo);
            retire_workers(master);
            return FALSE;
        }
        priv->nworkers++;
    }

    for (guint i = 0; i < nworkers; i++)
        priv->idle_workers = g_slist_prepend(priv->idle_workers,
                                             GUINT_TO_POINTER(i));

    return TRUE;
}

/**
 * gwy_master_manage_tasks:
 * @master: Parallel task manager.
 * @nworkers: Maximum number of parallel tasks to run.  The actual number will
 *            never be larger than the number of worker threads created by
 *            gwy_master_create_workers().  Pass zero for no specific limit.
 * @work: Worker function called, usually repeatedly, in worker threads
 *        to perform individual chunks of the work.
 * @provide_task: Function providing individual tasks.
 * @consume_result: (allow-none):
 *                  Function consuming task results.
 * @user_data: (scope call):
 *             User data passed to function @provide_task and @consume_result.
 * @cancellable: (allow-none):
 *               A #GCancellable for the calculation.
 *
 * Runs a chunked parallel calculation.
 *
 * Prior to executing this method it is necessary to create the worker threads
 * (once) using gwy_master_create_workers().
 *
 * This method is <emphasis>synchronous</emphasis> in the sense it returns when
 * all the work is done (or cancelled).  Therefore, you probably want to run it
 * in a separate thread in GUI programs but it is usually all right to just
 * call it in the main thread in the case of batch processing.
 *
 * Staged calculations that require a part of the calculation to be completed
 * before the next one can be started are implemented by running
 * gwy_master_manage_tasks() for each stage separately without creating and
 * destroying the auxiliary data.
 *
 * Functions @task_func and @result_func will be called in the master thread.
 * Therefore, they should be quite lightweight.
 *
 * This method blocks until the operation finishes.
 *
 * Returns: %TRUE if the caculation finished by exhausting tasks; %FALSE if
 *          it was cancelled.
 **/
gboolean
gwy_master_manage_tasks(GwyMaster *master,
                        guint nworkers,
                        GwyMasterWorkerFunc work,
                        GwyMasterTaskFunc provide_task,
                        GwyMasterResultFunc consume_result,
                        gpointer user_data,
                        GCancellable *cancellable)
{
    g_return_val_if_fail(GWY_IS_MASTER(master), FALSE);
    g_return_val_if_fail(provide_task, FALSE);
    g_return_val_if_fail(work, FALSE);

    Master *priv = master->priv;
    g_return_val_if_fail(priv->workers, FALSE);

    GAsyncQueue *queue = priv->queue;
    nworkers = MIN(nworkers ? nworkers : G_MAXUINT, priv->nworkers);

    if (priv->work != work) {
        notify_all_workers(master, MSG_WORKER, work);
        priv->work = work;
    }
    g_assert(g_slist_length(priv->idle_workers) == priv->nworkers);

    priv->exhausted = priv->cancelled = FALSE;
    priv->active_tasks = 0;

    while ((!priv->exhausted && !priv->cancelled) || priv->active_tasks) {
        // Obtain new tasks if we can and send them to idle workers.
        if (!priv->exhausted && !priv->cancelled) {
            while (priv->idle_workers) {
                gpointer task = provide_task(user_data);
                if (!task) {
                    priv->exhausted = TRUE;
                    break;
                }
                guint worker_id = GPOINTER_TO_UINT(priv->idle_workers->data);
                priv->idle_workers = g_slist_delete_link(priv->idle_workers,
                                                         priv->idle_workers);
                priv->active_tasks++;
                Message *message = message_new(priv, MSG_TASK, worker_id, task);
                WorkerData *workerdata = priv->workers + worker_id;
                g_assert(!workerdata->task_id);
                workerdata->task_id = message->task_id;
                //g_printerr("SEND TASK %lu to worker %u.\n", message->task_id, message->worker_id);
                g_async_queue_push(workerdata->queue, message);
            }
        }
        if (!priv->active_tasks)
            break;
        // Receive results from workers.
        //g_printerr("WAIT for a result (%u active tasks).\n", priv->active_tasks);
        Message *message = g_async_queue_pop(queue);
        do {
            guint worker_id = message->worker_id;
            g_assert(worker_id < nworkers);
            WorkerData *workerdata = priv->workers + worker_id;
            g_assert(workerdata->task_id == message->task_id);
            //g_printerr("GOT RESULT %lu from worker %u.\n", message->task_id, message->worker_id);
            if (consume_result)
                consume_result(message->data, user_data);
            g_slice_free(Message, message);

            g_assert(priv->active_tasks > 0);
            priv->active_tasks--;

            workerdata->task_id = 0;
            priv->idle_workers = g_slist_prepend(priv->idle_workers,
                                                 GUINT_TO_POINTER(worker_id));
            //g_printerr("TRY-WAIT for a result (%u active tasks).\n", priv->active_tasks);
        } while (priv->active_tasks
                 && (message = g_async_queue_try_pop(queue)));

        if (cancellable && g_cancellable_is_cancelled(cancellable))
            priv->cancelled = TRUE;
    }

    g_assert(priv->active_tasks == 0);
    g_assert(g_slist_length(priv->idle_workers) == priv->nworkers);

    return !priv->cancelled;
}

/**
 * gwy_master_create_data:
 * @master: Parallel task manager.
 * @create_data: (allow-none):
 *               Function to create worker data.  Passing %NULL is the same as
 *               passing a function that returns %NULL.
 * @user_data: User data passed to function @createdata.
 *
 * Invokes auxiliary worker data creation in each worker thread of a parallel
 * task manager.
 *
 * The threads must be created beforehand using gwy_master_create_workers().
 * Worker data creation must not be invoked while the calculation is running.
 *
 * This method blocks until the operation finishes.
 **/
void
gwy_master_create_data(GwyMaster *master,
                       GwyMasterCreateDataFunc create_data,
                       gpointer user_data)
{
    g_return_if_fail(GWY_IS_MASTER(master));
    Master *priv = master->priv;
    if (G_UNLIKELY(priv->active_tasks)) {
        g_critical("Cannot modify the calculation setup while it is running.");
    }
    else if (G_UNLIKELY(!priv->nworkers)) {
        g_critical("No worker threads are available.");
    }
    else {
        CreateDataFuncInfo info = { create_data, user_data };
        notify_all_workers(master, MSG_CREATE, &info);
    }
}

/**
 * gwy_master_destroy_data:
 * @master: Parallel task manager.
 * @destroy_data: (allow-none):
 *                Function to destroy worker data.  Passing %NULL is possible
 *                and it means no destruction will be performed; this is
 *                meaningful only when unsetting a previously set destruction
 *                function.
 *
 * Invokes auxiliary worker data destruction in each worker thread of a
 * parallel task manager.
 *
 * The threads must be created beforehand using gwy_master_create_workers().
 * Worker data destruction must not be invoked while the calculation is
 * running.
 *
 * This method blocks until the operation finishes.
 **/
void
gwy_master_destroy_data(GwyMaster *master,
                        GwyMasterDestroyDataFunc destroy_data)
{
    g_return_if_fail(GWY_IS_MASTER(master));
    Master *priv = master->priv;
    if (G_UNLIKELY(priv->active_tasks)) {
        g_critical("Cannot modify the calculation setup while it is running.");
    }
    else if (G_UNLIKELY(!priv->nworkers)) {
        g_critical("No worker threads are available.");
    }
    else {
        notify_all_workers(master, MSG_DESTROY, destroy_data);
    }
}

/**
 * SECTION: master
 * @title: GwyMaster
 * @short_description: Parallel task manager.
 *
 * #GwyMaster is a batch parallel processing facility.  Its only goal is to get
 * the job done by distributing it to workers.  Interactive features such as
 * cancellation and progress reporting are not provided.  However, they can be
 * achieved in the user part of the code.  Namely, if the provider of the tasks
 * stops providing them the work ceases whether it is truly finished or
 * cancelled.  Simialrly, the consumer of the results can easily do accounting
 * of the finished tasks.
 *
 * The main difference between #GwyMaster and #GThreadPool is, therefore, that
 * #GThreadPool is intended for running jobs in the background, i.e. it has a
 * non-blocking interface, whereas #GwyMaster is intended for parallelisation
 * of an immediately performed work, i.e. it has a blocking interface.
 * #GwyMastter is not thread-safe in the sense the object could be meaningfully
 * accessed from multiple threads apart from cancelling the work using the
 * #GCancellable passed to gwy_master_manage_tasks().
 *
 * A complete example showing parallelisation of the summation of numbers up to
 * @n (of course, one would use gwy_power_sum() in practice):
 * |[
 * typedef struct {
 *     guint64 from;
 *     guint64 to;
 *     guint64 result;
 * } SumNumbersTask;
 *
 * typedef struct {
 *     guint64 size;
 *     guint64 chunk_size;
 *     guint64 current;
 *     guint64 total_sum;
 * } SumNumbersState;
 *
 * static gpointer
 * sum_numbers_worker(gpointer taskp,
 *                    G_GNUC_UNUSED gpointer data)
 * {
 *     SumNumbersTask *task = (SumNumbersTask*)taskp;
 *     guint64 s = 0, from = task->from, to = task->to;
 *
 *     for (guint64 i = from; i < to; i++)
 *         s += i;
 *
 *     task->result = s;
 *     return taskp;
 * }
 *
 * static gpointer
 * sum_numbers_task(gpointer user_data)
 * {
 *     SumNumbersState *state = (SumNumbersState*)user_data;
 *     if (state->current == state->size)
 *         return NULL;
 *
 *     SumNumbersTask *task = g_slice_new(SumNumbersTask);
 *     task->from = state->current;
 *     task->to = state->current + MIN(state->chunk_size,
 *                                     state->size - state->current);
 *     state->current = task->to;
 *     return task;
 * }
 *
 * static void
 * sum_numbers_result(gpointer result,
 *                    gpointer user_data)
 * {
 *     SumNumbersTask *task = (SumNumbersTask*)result;
 *     SumNumbersState *state = (SumNumbersState*)user_data;
 *
 *     state->total_sum += task->result;
 *     g_slice_free(SumNumbersTask, task);
 * }
 *
 * static void
 * sum_numbers(guint from, guint to, guint chunk_size)
 * {
 *     GwyMaster *master = gwy_master_new();
 *     gwy_master_create_workers(master, 0, NULL);
 *
 *     SumNumbersState state = { to, chunk_size, from, 0 };
 *     gwy_master_manage_tasks(master, 0, &sum_numbers_worker,
 *                             &sum_numbers_task, &sum_numbers_result,
 *                             &state,
 *                             NULL);
 *     g_object_unref(master);
 *
 *     return state.total_sum;
 * }
 * ]|
 **/

/**
 * GwyMaster:
 *
 * Object representing parallel task manager.
 *
 * The #GwyMaster struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyMasterClass:
 *
 * Class of parallel task managers.
 **/

/**
 * GwyMasterWorkerFunc:
 * @task: Task data returned by the task provider specified while calling
 *        gwy_master_manage_tasks().
 * @data: Auxiliary data created by the auxiliary data creation function set by
 *        gwy_master_create_data().
 *
 * Type of function performing one chunk in parallel processing in the
 * individual worker threads.
 *
 * Returns: Result pointer passed to function set specified while calling
 *          gwy_master_manage_tasks().  If no such function is set up the
 *          return value is ignored.
 **/

/**
 * GwyMasterCreateDataFunc:
 * @user_data: User data speficied in gwy_master_create_data().
 *
 * Type of function creating auxiliary data structures for each worker thread
 * in a parallel processing.
 *
 * It is invoked, immediately but in parallel, in each worker thread by
 * gwy_master_create_data().
 *
 * Returns: Pointer to the auxiliary data, presumably some newly created
 *          buffers but it can be anything the worker thread expects.
 **/

/**
 * GwyMasterDestroyDataFunc:
 * @worker_data: Data created by function set by gwy_master_destroy_data().
 *
 * Type of function destroying auxiliary data structures in each worker thread
 * in a parallel task processing.
 *
 * It is invoked, immediately but in parallel, in each worker thread by
 * gwy_master_destroy_data().  It is <emphasis>not</emphasis> invoked when the
 * #GwyMaster is finialised.
 **/

/**
 * GwyMasterTaskFunc:
 * @user_data: User data speficied in gwy_master_manage_tasks().
 *
 * Type of function providing individual tasks in chunked parallel processing.
 *
 * It is not guaranteed that the function will be called until it returns
 * %NULL as the work may be cancelled.  So if a clean-up is necessary it should
 * be done by the caller of gwy_master_manage_tasks().
 *
 * It is guaranteed that once it returns %NULL it will not be called any more.
 *
 * Returns: The new task, passed to the worker function if not %NULL.  If it is
 *          %NULL it means there is no more work to do.
 **/

/**
 * GwyMasterResultFunc:
 * @result: Result data of one task obtained from a worker.
 * @user_data: User data speficied in gwy_master_manage_tasks().
 *
 * Type of function gathering results of individual tasks.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
