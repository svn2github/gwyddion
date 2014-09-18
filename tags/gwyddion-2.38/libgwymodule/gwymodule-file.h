/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#ifndef __GWY_MODULE_FILE_H__
#define __GWY_MODULE_FILE_H__

#include <libgwyddion/gwycontainer.h>
#include <libgwymodule/gwymoduleenums.h>
#include <libgwymodule/gwymoduleloader.h>

G_BEGIN_DECLS

#define GWY_FILE_DETECT_BUFFER_SIZE 4096U

#define GWY_MODULE_FILE_ERROR gwy_module_file_error_quark()

typedef enum {
    GWY_MODULE_FILE_ERROR_CANCELLED,
    GWY_MODULE_FILE_ERROR_UNIMPLEMENTED,
    GWY_MODULE_FILE_ERROR_IO,
    GWY_MODULE_FILE_ERROR_DATA,
    GWY_MODULE_FILE_ERROR_INTERACTIVE,
    GWY_MODULE_FILE_ERROR_SPECIFIC
} GwyModuleFileError;

typedef struct {
    const gchar *name;
    const gchar *name_lowercase;
    gsize file_size;
    guint buffer_len;
    const guchar *head;
    const guchar *tail;
} GwyFileDetectInfo;

typedef gint           (*GwyFileDetectFunc) (const GwyFileDetectInfo *fileinfo,
                                             gboolean only_name,
                                             const gchar *name);
typedef GwyContainer*  (*GwyFileLoadFunc)   (const gchar *filename,
                                             GwyRunType mode,
                                             GError **error,
                                             const gchar *name);
typedef gboolean       (*GwyFileSaveFunc)   (GwyContainer *data,
                                             const gchar *filename,
                                             GwyRunType mode,
                                             GError **error,
                                             const gchar *name);

/* low-level interface */
gboolean      gwy_file_func_register  (const gchar *name,
                                       const gchar *description,
                                       GwyFileDetectFunc detect,
                                       GwyFileLoadFunc load,
                                       GwyFileSaveFunc save,
                                       GwyFileSaveFunc export_);
gint          gwy_file_func_run_detect(const gchar *name,
                                       const gchar *filename,
                                       gboolean only_name);
GwyContainer* gwy_file_func_run_load  (const gchar *name,
                                       const gchar *filename,
                                       GwyRunType mode,
                                       GError **error);
gboolean      gwy_file_func_run_save  (const gchar *name,
                                       GwyContainer *data,
                                       const gchar *filename,
                                       GwyRunType mode,
                                       GError **error);
gboolean      gwy_file_func_run_export(const gchar *name,
                                       GwyContainer *data,
                                       const gchar *filename,
                                       GwyRunType mode,
                                       GError **error);

gboolean             gwy_file_func_exists         (const gchar *name);
GwyFileOperationType gwy_file_func_get_operations (const gchar *name);
const gchar*         gwy_file_func_get_description(const gchar *name);
void                 gwy_file_func_foreach        (GFunc function,
                                                   gpointer user_data);
const gchar*         gwy_file_func_current        (void);

/* high-level interface */
const gchar*        gwy_file_detect           (const gchar *filename,
                                               gboolean only_name,
                                               GwyFileOperationType operations);
const gchar*        gwy_file_detect_with_score(const gchar *filename,
                                               gboolean only_name,
                                               GwyFileOperationType operations,
                                               gint *score);
GwyContainer*       gwy_file_load             (const gchar *filename,
                                               GwyRunType mode,
                                               GError **error);
GwyContainer*       gwy_file_load_with_func   (const gchar *filename,
                                               GwyRunType mode,
                                               const gchar **name,
                                               GError **error);
GwyFileOperationType gwy_file_save            (GwyContainer *data,
                                               const gchar *filename,
                                               GwyRunType mode,
                                               GError **error);
GwyFileOperationType gwy_file_save_with_func  (GwyContainer *data,
                                               const gchar *filename,
                                               GwyRunType mode,
                                               const gchar **name,
                                               GError **error);
gboolean            gwy_file_func_get_is_detectable(const gchar *name);
void                gwy_file_func_set_is_detectable(const gchar *name,
                                                    gboolean is_detectable);
gboolean            gwy_file_get_data_info    (GwyContainer *data,
                                               const gchar **name,
                                               const gchar **filename_sys);
const gchar*        gwy_file_get_filename_sys (GwyContainer *data);

GQuark gwy_module_file_error_quark(void);

G_END_DECLS

#endif /* __GWY_MODULE_FILE_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
