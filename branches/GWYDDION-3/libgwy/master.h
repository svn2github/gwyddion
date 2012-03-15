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

#ifndef __LIBGWY_MASTER_H__
#define __LIBGWY_MASTER_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GWY_TYPE_MASTER \
    (gwy_master_get_type())
#define GWY_MASTER(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_MASTER, GwyMaster))
#define GWY_MASTER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_MASTER, GwyMasterClass))
#define GWY_IS_MASTER(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_MASTER))
#define GWY_IS_MASTER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_MASTER))
#define GWY_MASTER_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_MASTER, GwyMasterClass))

typedef struct _GwyMaster      GwyMaster;
typedef struct _GwyMasterClass GwyMasterClass;

typedef gpointer (*GwyMasterWorkerFunc)(gpointer task,
                                         gpointer data);
typedef gpointer (*GwyMasterCreateDataFunc)(gpointer user_data);
typedef void     (*GwyMasterDestroyDataFunc)(gpointer worker_data);
typedef gpointer (*GwyMasterTaskFunc)(GwyMaster *master,
                                       gpointer user_data);
typedef void     (*GwyMasterResultFunc)(GwyMaster *master,
                                        gpointer result,
                                        gpointer user_data);

struct _GwyMaster {
    GObject g_object;
    struct _GwyMasterPrivate *priv;
};

struct _GwyMasterClass {
    /*<private>*/
    GObjectClass g_object_class;
};

GType      gwy_master_get_type             (void)                                 G_GNUC_CONST;
GwyMaster* gwy_master_new                  (void)                                 G_GNUC_MALLOC;
void       gwy_master_set_worker_func      (GwyMaster *master,
                                            GwyMasterWorkerFunc worker);
void       gwy_master_set_create_data_func (GwyMaster *master,
                                            GwyMasterCreateDataFunc createdata,
                                            gpointer user_data);
void       gwy_master_set_destroy_data_func(GwyMaster *master,
                                            GwyMasterDestroyDataFunc destroydata);
void       gwy_master_set_task_func        (GwyMaster *master,
                                            GwyMasterTaskFunc provide_task,
                                            gpointer user_data);
void       gwy_master_set_result_func      (GwyMaster *master,
                                            GwyMasterResultFunc consume_result,
                                            gpointer user_data);
gboolean   gwy_master_create_workers       (GwyMaster *master,
                                            guint nworkers,
                                            GError **error);
void       gwy_master_manage_tasks         (GwyMaster *master);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
