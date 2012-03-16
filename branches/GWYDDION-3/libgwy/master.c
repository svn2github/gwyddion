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
#include "libgwy/main.h"
#include "libgwy/master.h"

typedef enum {
    MSG_TASK_PUBLISHED = 1,
    MSG_TASK_TAKEN     = 2,
    MSG_TASK_DONE      = 3,
    MSG_RETIRE         = 4,
    MSG_RETIRED        = 5,
} MessageType;

typedef struct {
    MessageType type;
    gulong id;
    gpointer data;
} Message;

typedef struct {
    GArray *messages;
    GMutex *lock;
    GCond *cond;
    const gchar *name;
} MessageQueue;

struct _GwyMasterPrivate {
    MessageQueue master_to_workers;
    MessageQueue workers_to_master;

    guint nworkers;
    GThread **threads;

    GwyMasterWorkerFunc worker;

    GwyMasterCreateDataFunc create_data;
    gpointer create_data_user_data;

    GwyMasterDestroyDataFunc destroy_data;

    GwyMasterTaskFunc provide_task;
    gpointer provide_task_user_data;

    GwyMasterResultFunc consume_result;
    gpointer consume_result_user_data;
};

typedef struct _GwyMasterPrivate Master;

static void     gwy_master_finalize   (GObject *object);
static void     init_message_queue    (MessageQueue *queue,
                                       const gchar *name);
static void     finalize_message_queue(MessageQueue *queue);
static void     retire_workers        (GwyMaster *master);
static void     publish_message       (MessageQueue *queue,
                                       const Message *message);
static Message  receive_message       (MessageQueue *queue);
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
    init_message_queue(&master->priv->master_to_workers, "Master to workers");
    init_message_queue(&master->priv->workers_to_master, "Workers to master");
}

static void
gwy_master_finalize(GObject *object)
{
    GwyMaster *master = GWY_MASTER(object);
    Master *priv = master->priv;

    retire_workers(master);
    finalize_message_queue(&priv->master_to_workers);
    finalize_message_queue(&priv->workers_to_master);

    G_OBJECT_CLASS(gwy_master_parent_class)->finalize(object);
}

static void
init_message_queue(MessageQueue *queue,
                   const gchar *name)
{
    queue->name = name;
    queue->messages = g_array_sized_new(FALSE, FALSE, sizeof(Message), 16);
    queue->lock = g_mutex_new();
    queue->cond = g_cond_new();
}

static void
finalize_message_queue(MessageQueue *queue)
{
    // XXX: Check for locked mutex/waited on queue and complain?
    g_cond_free(queue->cond);
    g_mutex_free(queue->lock);
}

static void
retire_workers(GwyMaster *master)
{
    Master *priv = master->priv;
    MessageQueue *master_to_workers = &priv->master_to_workers;
    MessageQueue *workers_to_master = &priv->workers_to_master;

    for (unsigned int i = 0; i < priv->nworkers; i++) {
        Message message = { MSG_RETIRE, i, NULL };
        publish_message(master_to_workers, &message);
    }

    while (priv->nworkers) {
        Message message = receive_message(workers_to_master);
        if (message.type == MSG_RETIRED) {
            priv->nworkers--;
        }
        else {
            g_printerr("Bogus message to master %u\n", message.type);
        }
    }
}

#ifdef DEBUG
static inline gulong
thread_id(void)
{
    return (gulong)GPOINTER_TO_SIZE(g_thread_self());
}

static inline const gchar*
message_name(guint id)
{
    static const gchar *message_names[] = {
        NULL,
        "TASK PUBLISHED",
        "TASK TAKEN",
        "TASK DONE",
        "RETIRE",
        "RETIRED",
    };

    if (id && id < G_N_ELEMENTS(message_names))
        return message_names[id];
    return "???";
}
#endif

static void
publish_message(MessageQueue *queue,
                const Message *message)
{
#ifdef DEBUG
    g_printerr("Thread %lx sends %d %s using queue %s (id %lu)\n",
               thread_id(),
               message->type, message_name(message->type),
               queue->name, message->id);
#endif
    g_mutex_lock(queue->lock);
#ifdef DEBUG
    g_printerr("Thread %lx locked queue %s\n",
               thread_id(), queue->name);
#endif
    g_array_append_vals(queue->messages, message, 1);
#ifdef DEBUG
    g_printerr("Thread %lx signals message in queue %s\n",
               thread_id(), queue->name);
#endif
    g_cond_signal(queue->cond);
    g_mutex_unlock(queue->lock);
#ifdef DEBUG
    g_printerr("  Thread %lx unlocked queue %s\n",
            thread_id(), queue->name);
#endif
}

static Message
receive_message(MessageQueue *queue)
{
#ifdef DEBUG
    g_printerr("Thread %lx checks for message in queue %s\n",
               thread_id(), queue->name);
#endif
    GArray *messages = queue->messages;

restart:
    g_mutex_lock(queue->lock);
#ifdef DEBUG
    g_printerr("Thread %lx locked queue %s\n",
               thread_id(), queue->name);
#endif
    if (!messages->len) {
#ifdef DEBUG
        g_printerr("Thread %lx found queue %s empty, waiting on condition\n",
                   thread_id(), queue->name);
#endif
        g_cond_wait(queue->cond, queue->lock);
#ifdef DEBUG
        g_printerr("Thread %lx waiting on queue %s was waken on condition\n",
                   thread_id(), queue->name);
        g_printerr("Thread %lx finds queue %s to have %u items after wakeup\n",
                   thread_id(), queue->name, messages->len);
#endif
        if (!messages->len) {
            g_mutex_unlock(queue->lock);
            goto restart;
        }
    }
    Message message = g_array_index(messages, Message, 0);
    g_array_remove_index(messages, 0);
#ifdef DEBUG
    g_printerr("Thread %lx picked up message %d %s from queue %s (id %lu)\n",
               thread_id(),
               message.type, message_name(message.type),
               queue->name, message.id);
#endif
    g_mutex_unlock(queue->lock);
#ifdef DEBUG
    g_printerr("Thread %lx unlocked queue %s\n",
               thread_id(), queue->name);
#endif
    return message;
}

static gpointer
worker_thread_main(gpointer thread_data)
{
    GwyMaster *master = GWY_MASTER(thread_data);
    Master *priv = master->priv;

    GwyMasterWorkerFunc worker = priv->worker;
    MessageQueue *master_to_workers = &priv->master_to_workers;
    MessageQueue *workers_to_master = &priv->workers_to_master;

    void *data = NULL;
    gulong id;

    if (priv->create_data)
        data = priv->create_data(priv->create_data_user_data);

    while (TRUE) {
        // Run calculation.
        Message message = receive_message(master_to_workers);
        gpointer task = message.data;

        id = message.id;
        if (message.type == MSG_RETIRE)
            break;

        if (message.type != MSG_TASK_PUBLISHED) {
            g_warning("Bogus message to worker %u\n", message.type);
            continue;
        }

        message = (Message){ MSG_TASK_TAKEN, id, NULL };
        publish_message(workers_to_master, &message);

        gpointer result = worker(task, data);

        message = (Message){ MSG_TASK_DONE, id, result };
        publish_message(workers_to_master, &message);
    }

    if (priv->destroy_data)
        priv->destroy_data(data);

    Message message = { MSG_RETIRED, id, NULL };
    publish_message(workers_to_master, &message);

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
 * @error: Return location for the error, or %NULL.
 *
 * Creates worker threads for a parallel task manager.
 *
 * This method can be only called after the worker functions were set up with
 * gwy_master_set_worker_func() and possibly gwy_master_set_create_data_func()
 * and gwy_master_set_destroy_data_func().
 *
 * The threads are not destroyed until @master is finalised.
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

    if (nworkers < 1)
        nworkers = gwy_n_cpus();

    Master *priv = master->priv;
    g_return_val_if_fail(priv->worker, FALSE);

    if (priv->threads) {
        g_warning("Master already has workers.");
        return TRUE;
    }

    priv->threads = g_new0(GThread*, nworkers);
    for (guint i = 0; i < nworkers; i++) {
        if (!(priv->threads[i] = g_thread_create(&worker_thread_main,
                                                 master, FALSE, error))) {
            retire_workers(master);
            return FALSE;
        }
        priv->nworkers++;
    }

    return TRUE;
}

/**
 * gwy_master_manage_tasks:
 * @master: Parallel task manager.
 *
 * Runs a chunked parallel calculation.
 *
 * This method returns when all the work is done.
 **/
void
gwy_master_manage_tasks(GwyMaster *master)
{
    g_return_if_fail(GWY_IS_MASTER(master));

    Master *priv = master->priv;
    g_return_if_fail(priv->provide_task);
    g_return_if_fail(priv->nworkers);

    MessageQueue *master_to_workers = &priv->master_to_workers;
    MessageQueue *workers_to_master = &priv->workers_to_master;
    GwyMasterTaskFunc provide_task = priv->provide_task;
    GwyMasterResultFunc consume_result = priv->consume_result;
    gpointer task_data = priv->provide_task_user_data;
    gpointer result_data = priv->consume_result_user_data;
    guint active_tasks = 0, unfinished_tasks = 0, nworkers = priv->nworkers;
    gulong id = 0;
    gboolean exhausted = FALSE;

    while (!exhausted || unfinished_tasks) {
        // Obtain a new task if we can.
        if (active_tasks < nworkers && !exhausted) {
            gpointer task_to_do = provide_task(master, task_data);
            if (task_to_do) {
                Message message = { MSG_TASK_PUBLISHED, id++, task_to_do };
                publish_message(master_to_workers, &message);
                unfinished_tasks++;
            }
            else
                exhausted = TRUE;
        }

        // Receive a message from workers.
        if (unfinished_tasks) {
            Message message = receive_message(workers_to_master);
            if (message.type == MSG_TASK_DONE) {
                if (consume_result)
                    consume_result(master, message.data, result_data);
                unfinished_tasks--;
                active_tasks--;
            }
            else if (message.type == MSG_TASK_TAKEN) {
                active_tasks++;
            }
            else {
                g_warning("Bogus message to master %u\n", message.type);
            }
        }
    }
}

/**
 * gwy_master_set_worker_func:
 * @master: Parallel task manager.
 * @worker: Worker function.
 *
 * Sets the worker function for a parallel task manager.
 *
 * The worker function is executed, possibly repeatedly, in the worker threads
 * with tasks provided by the task function.
 **/
void
gwy_master_set_worker_func(GwyMaster *master,
                           GwyMasterWorkerFunc worker)
{
    g_return_if_fail(GWY_IS_MASTER(master));
    Master *priv = master->priv;
    priv->worker = worker;
}

/**
 * gwy_master_set_create_data_func:
 * @master: Parallel task manager.
 * @createdata: Function to create worker data.
 * @user_data: User data passed to function @createdata.
 *
 * Sets the worker data creation function for a parallel task manager.
 *
 * The worker data creation function is executed in each worker thread exactly
 * once before the tasks start coming.  Its purpose is to create auxiliary
 * data the worker thread will repeatedly reuse.
 *
 * Calling gwy_master_set_create_data_func() after gwy_master_create_workers()
 * has no effect.
 **/
void
gwy_master_set_create_data_func(GwyMaster *master,
                                GwyMasterCreateDataFunc createdata,
                                gpointer user_data)
{
    g_return_if_fail(GWY_IS_MASTER(master));
    Master *priv = master->priv;
    priv->create_data = createdata;
    priv->create_data_user_data = user_data;
}

/**
 * gwy_master_set_destroy_data_func:
 * @master: Parallel task manager.
 * @destroydata: Function to destroy worker data.
 *
 * Sets the worker data destruction function for a parallel task manager.
 *
 * The worker data destruction function is executed in each worker thread
 * exactly once before the thread terminates.  Its purpose is to destruct
 * auxiliary data created by the function set by
 * gwy_master_set_create_data_func().
 *
 * Calling gwy_master_set_destroy_data_func() after gwy_master_create_workers()
 * has no effect.
 **/
void
gwy_master_set_destroy_data_func(GwyMaster *master,
                                 GwyMasterDestroyDataFunc destroydata)
{
    g_return_if_fail(GWY_IS_MASTER(master));
    Master *priv = master->priv;
    priv->destroy_data = destroydata;
}

/**
 * gwy_master_set_task_func:
 * @master: Parallel task manager.
 * @provide_task: Function providing individual tasks.
 * @user_data: User data passed to @provide_task.
 *
 * Sets the function that will provide the individual tasks managed by a
 * parallel task manager.
 *
 * The task provider function will be called in the master thread to provide
 * individual tasks.  Once it returns %NULL it is assumed there is no more work
 * to do in this batch.
 *
 * The task function must be set before calling gwy_master_manage_tasks().
 **/
void
gwy_master_set_task_func(GwyMaster *master,
                         GwyMasterTaskFunc provide_task,
                         gpointer user_data)
{
    g_return_if_fail(GWY_IS_MASTER(master));
    Master *priv = master->priv;
    priv->provide_task = provide_task;
    priv->provide_task_user_data = user_data;
}

/**
 * gwy_master_set_result_func:
 * @master: Parallel task manager.
 * @consume_result: Function consuming task results.
 * @user_data: User data passed to @consume_result.
 *
 * Sets the function that will consume the results of individual tasks managed
 * by a parallel task manager.
 *
 * In most cases, it is avisable that the results are not actually created by
 * workers.  Instead, the task function specified storage for the result and
 * the worker then returns the task again as the result – just with the
 * resulting values filled in.
 **/
void
gwy_master_set_result_func(GwyMaster *master,
                           GwyMasterResultFunc consume_result,
                           gpointer user_data)
{
    g_return_if_fail(GWY_IS_MASTER(master));
    Master *priv = master->priv;
    priv->consume_result = consume_result;
    priv->consume_result_user_data = user_data;
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
 * @task: Task data returned by the task provider set by
 *        gwy_master_set_task_func().
 * @data: Auxiliary data created by the auxiliary data creation function set by
 *        gwy_master_set_create_data_func().
 *
 * Type of function performing one chunk in parallel processing in the
 * individual worker threads.
 *
 * Returns: Result pointer passed to function set up by
 *          gwy_master_set_result_func().  If no such function is set up the
 *          return value is ignored.
 **/

/**
 * GwyMasterCreateDataFunc:
 * @user_data: User data speficied in gwy_master_set_create_data_func().
 *
 * Type of function creating auxiliary data structures for each worker thread
 * in a parallel processing.
 *
 * Returns: Pointer to the auxiliary data, presumably some newly created
 *          buffers but it can be anything the worker thread expects.
 **/

/**
 * GwyMasterDestroyDataFunc:
 * @worker_data: Data created by function set by
 *               gwy_master_set_destroy_data_func().
 *
 * Type of function destroying auxiliary data structures in each worker thread
 * in a parallel task processing.
 **/

/**
 * GwyMasterTaskFunc:
 * @master: Parallel task manager.
 * @user_data: User data speficied in gwy_master_set_task_func().
 *
 * Type of function providing individual tasks in chunked parallel processing.
 *
 * Returns: The new task, passed to the worker function if not %NULL.  If it is
 *          %NULL it means there is no more work to do.
 **/

/**
 * GwyMasterResultFunc:
 * @master: Parallel task manager.
 * @result: Result data of one task obtained from a worker.
 * @user_data: User data speficied in gwy_master_set_result_func().
 *
 * Type of function gathering results of individual tasks.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
