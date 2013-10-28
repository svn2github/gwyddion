/*
 *  $Id$
 *  Copyright (C) 2013 David Nečas (Yeti).
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

#ifndef __LIBGWYAPP_MODULE_H__
#define __LIBGWYAPP_MODULE_H__

#include <libgwy/error-list.h>
#include <glib-object.h>
#include <gmodule.h>

G_BEGIN_DECLS

#define GWY_MODULE_ABI_VERSION 3

#ifdef  __cplusplus
#define __GWY_MODULE_EXTERN_C extern "C"
#else
#define __GWY_MODULE_EXTERN_C /* */
#endif

#define GWY_MODULE_INFO_SYMBOL gwy_module_info

#ifndef GWY_MODULE_BUILDING_LIBRARY
#define GWY_DEFINE_MODULE(mod_info,name) \
    __GWY_MODULE_EXTERN_C G_MODULE_EXPORT const GwyModuleInfo* \
    GWY_MODULE_INFO_SYMBOL##_##name = mod_info
#else
#define GWY_DEFINE_MODULE(mod_info,name) \
    __GWY_MODULE_EXTERN_C G_GNUC_INTERNAL const GwyModuleInfo* \
    GWY_MODULE_INFO_SYMBOL##_##name = mod_info
#endif

#define GWY_DEFINE_MODULE_LIBRARY(mod_info_list) \
    __GWY_MODULE_EXTERN_C G_MODULE_EXPORT const GwyModuleLibraryRecord* \
    GWY_MODULE_INFO_SYMBOL = mod_info_list

#define GWY_MODULE_INFO_SENTINEL \
    ((GwyModuleInfo){ 0, 0, NULL, NULL, NULL, NULL, NULL, NULL })

#define GWY_MODULE_ERROR gwy_module_error_quark()

typedef enum {
    GWY_MODULE_ERROR_MODULE_NAME,
    GWY_MODULE_ERROR_DUPLICATE_MODULE,
    GWY_MODULE_ERROR_EMPTY_LIBRARY,
    GWY_MODULE_ERROR_OVERRIDEN,
    GWY_MODULE_ERROR_OPEN,
    GWY_MODULE_ERROR_INFO,
    GWY_MODULE_ERROR_ABI,
    GWY_MODULE_ERROR_TYPES,
    GWY_MODULE_ERROR_METADATA,
    GWY_MODULE_ERROR_DUPLICATE_TYPE,
    GWY_MODULE_ERROR_TYPE_NAME,
    GWY_MODULE_ERROR_GET_TYPE,
} GwyModuleError;

typedef GType (*GwyGetTypeFunc)(void);

typedef struct {
    const gchar *name;
    GwyGetTypeFunc get_type;
} GwyModuleProvidedType;

typedef struct {
    guint32 abi_version;
    guint32 ntypes;
    const gchar *description;
    const gchar *author;
    const gchar *version;
    const gchar *copyright;
    const gchar *date;
    const GwyModuleProvidedType *types;
} GwyModuleInfo;

typedef struct {
    const gchar *name;
    const GwyModuleInfo *info;
} GwyModuleLibraryRecord;

GQuark               gwy_module_error_quark   (void)                      G_GNUC_CONST;
guint                gwy_register_modules     (GwyErrorList **errorlist);
const GwyModuleInfo* gwy_module_load          (const gchar *filename,
                                               GError **error);
guint                gwy_module_load_library  (const gchar *filename,
                                               GwyErrorList **errorlist);
guint                gwy_module_load_directory(const gchar *path,
                                               GwyErrorList **errorlist);
guint                gwy_module_register_types(GwyErrorList **errorlist);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
