/*
 *  $Id$
 *  Copyright (C) 2005 David Necas (Yeti), Petr Klrhktek.
 *  E-mail: yeti@gwyddion.net, klrhktek@gwyddion.net.
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */
#define DEBUG 1
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "get.h"

#define HEADER_SIZE 512

#define MAGIC "STiMage 3.1"
#define MAGIC_SIZE (sizeof(MAGIC) - 1)
#define EXTENSION ".sm2"

typedef enum {
    RHK_IMAGE_UNDEFINED     = 0,
    RHK_IMAGE_TOPOGAPHIC    = 1,
    RHK_IMAGE_CURRENT       = 2,
    RHK_IMAGE_AUX           = 3,
    RHK_IMAGE_FORCE         = 4,
    RHK_IMAGE_SIGNAL        = 5,
    RHK_IMAGE_FFT           = 6,
    RHK_IMAGE_LAST
} RHKImageType;

typedef struct {
    gdouble scale;
    gdouble offset;
    gchar *units;
} RHKRange;

typedef struct {
    gchar *date;
    guint xres;
    guint yres;
    guint type;
    guint data_type;
    guint line_type;
    guint size;
    guint image_type;
    RHKRange x;
    RHKRange y;
    RHKRange z;
    gdouble xyskew;
    gdouble alpha;
    RHKRange iv;
    guint id;
    guint data_offset;
    gchar *label;
    gchar *comment;
} RHKFile;

static gboolean      module_register       (const gchar *name);
static gint          rhkspm32_detect       (const gchar *filename,
                                            gboolean only_name);
static GwyContainer* rhkspm32_load         (const gchar *filename);
static gboolean      rhkspm32_read_header  (gchar *buffer,
                                            RHKFile *rhkfile);
static gboolean      rhkspm32_read_range   (const gchar *buffer,
                                            const gchar *name,
                                            RHKRange *range);
static void          rhkspm32_free         (RHKFile *rhkfile);
static GwyDataField* rhkspm32_read_data    (const guchar *buffer,
                                            RHKFile *rhkfile);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "rhk-spm32",
    N_("Imports RHK Technology SPM32 data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2005",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyFileFuncInfo rhkspm32_func_info = {
        "rhk-spm32",
        N_("RHK SPM32 files (.sm2)"),
        (GwyFileDetectFunc)&rhkspm32_detect,
        (GwyFileLoadFunc)&rhkspm32_load,
        NULL
    };

    gwy_file_func_register(name, &rhkspm32_func_info);

    return TRUE;
}

static gint
rhkspm32_detect(const gchar *filename, gboolean only_name)
{
    gint score = 0;
    FILE *fh;
    gchar magic[MAGIC_SIZE];

    if (only_name) {
        gchar *filename_lc;

        filename_lc = g_ascii_strdown(filename, -1);
        score = g_str_has_suffix(filename_lc, EXTENSION) ? 20 : 0;
        g_free(filename_lc);

        return score;
    }

    if (!(fh = fopen(filename, "rb")))
        return 0;
    if (fread(magic, 1, MAGIC_SIZE, fh) == MAGIC_SIZE
        && memcmp(magic, MAGIC, MAGIC_SIZE) == 0)
        score = 100;
    fclose(fh);

    return score;
}

static GwyContainer*
rhkspm32_load(const gchar *filename)
{
    RHKFile rhkfile;
    GObject *object = NULL;
    guchar *buffer = NULL;
    gsize size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    gboolean ok;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        g_warning("Cannot read file %s", filename);
        g_clear_error(&err);
        return NULL;
    }
    if (size < HEADER_SIZE) {
        g_warning("File %s is not a RHK SPM32 file", filename);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    memset(&rhkfile, 0, sizeof(rhkfile));
    if (!(ok = rhkspm32_read_header((gchar*)buffer, &rhkfile)))
        g_warning("Cannot parse file header");

    if (ok) {
        if (size < rhkfile.data_offset + rhkfile.xres*rhkfile.yres)
            g_warning("Truncated file");
        else
            dfield = rhkspm32_read_data(buffer + rhkfile.data_offset, &rhkfile);
    }

    rhkspm32_free(&rhkfile);
    gwy_file_abandon_contents(buffer, size, NULL);

    if (dfield) {
        object = gwy_container_new();
        gwy_container_set_object_by_name(GWY_CONTAINER(object), "/0/data",
                                         G_OBJECT(dfield));
        /*store_metadata(meta, GWY_CONTAINER(object));*/
    }

    return (GwyContainer*)object;
}

static gboolean
rhkspm32_read_header(gchar *buffer,
                     RHKFile *rhkfile)
{
    gchar *end;
    guint pos;

    rhkfile->date = g_strstrip(g_strndup(buffer + MAGIC_SIZE,
                                        0x20 - MAGIC_SIZE));
    if (sscanf(buffer + 0x20, "%d %d %d %d %d %d %d",
               &rhkfile->type, &rhkfile->data_type, &rhkfile->line_type,
               &rhkfile->xres, &rhkfile->yres, &rhkfile->size,
               &rhkfile->image_type) != 7
        || rhkfile->xres <= 0 || rhkfile->yres <= 0)
        return FALSE;
    gwy_debug("type = %u, data = %u, line = %u, image = %u",
              rhkfile->type, rhkfile->data_type, rhkfile->line_type,
              rhkfile->image_type);
    gwy_debug("xres = %d, yres = %d", rhkfile->xres, rhkfile->yres);

    if (!rhkspm32_read_range(buffer + 0x40, "X", &rhkfile->x)
        || !rhkspm32_read_range(buffer + 0x60, "Y", &rhkfile->y)
        || !rhkspm32_read_range(buffer + 0x80, "Z", &rhkfile->z))
        return FALSE;

    if (!g_str_has_prefix(buffer + 0xa0, "XY"))
        return FALSE;
    pos = 0xa0 + sizeof("XY");
    rhkfile->xyskew = g_ascii_strtod(buffer + pos, &end);
    if (end == buffer + pos)
        return FALSE;
    pos = (end - buffer) + 2;
    rhkfile->alpha = g_ascii_strtod(buffer + pos, &end);
    if (end == buffer + pos)
        return FALSE;

    if (!rhkspm32_read_range(buffer + 0xc0, "IV", &rhkfile->iv))
        return FALSE;

    /* FIXME: what is at 0xe0? */

    if (sscanf(buffer + 0x100, "id %u %u",
               &rhkfile->id, &rhkfile->data_offset) != 2)
        return FALSE;
    gwy_debug("data_offset = %u", rhkfile->data_offset);
    if (rhkfile->data_offset < HEADER_SIZE)
        return FALSE;

    rhkfile->label = g_strstrip(g_strndup(buffer + 0x140, 0x20));
    rhkfile->comment = g_strstrip(g_strndup(buffer + 0x160,
                                           HEADER_SIZE - 0x160));

    return TRUE;
}

static gboolean
rhkspm32_read_range(const gchar *buffer,
                    const gchar *name,
                    RHKRange *range)
{
    gchar *end;
    guint pos;

    if (!g_str_has_prefix(buffer, name))
        return FALSE;
    pos = strlen(name) + 1;

    range->scale = fabs(g_ascii_strtod(buffer + pos, &end));
    if (end == buffer + pos || pos > 0x20)
        return FALSE;
    pos = end - buffer;

    range->offset = g_ascii_strtod(buffer + pos, &end);
    if (end == buffer + pos || pos > 0x20)
        return FALSE;
    pos = end - buffer;

    range->units = g_strstrip(g_strndup(buffer + pos, 0x20 - pos));
    gwy_debug("<%s> %g %g <%s>",
              name, range->scale, range->offset, range->units);

    return TRUE;
}

static void
rhkspm32_free(RHKFile *rhkfile)
{
    g_free(rhkfile->date);
    g_free(rhkfile->x.units);
    g_free(rhkfile->y.units);
    g_free(rhkfile->z.units);
    g_free(rhkfile->iv.units);
    g_free(rhkfile->label);
    g_free(rhkfile->comment);
}

#if 0
static void
store_metadata(GHashTable *meta,
               GwyContainer *container)
{
    const struct {
        const gchar *id;
        const gchar *unit;
        const gchar *key;
    }
    metakeys[] = {
        { "scanspeed",   "nm/s",    "Scan speed"        },
        { "xoffset",     "nm",      "X offset"          },
        { "yoffset",     "nm",      "Y offset"          },
        { "bias",        "V",       "Bias voltage"      },
        { "current",     "nA",      "Tunneling current" },
        { "starttime",   NULL,      "Scan time"         },
        /* FIXME: I've seen other stuff, but don't know interpretation */
    };
    gchar *value;
    GString *key;
    guint i;

    key = g_string_new("");
    for (i = 0; i < G_N_ELEMENTS(metakeys); i++) {
        if (!(value = g_hash_table_lookup(meta, metakeys[i].id)))
            continue;

        g_string_printf(key, "/meta/%s", metakeys[i].key);
        if (metakeys[i].unit)
            gwy_container_set_string_by_name(container, key->str,
                                             g_strdup_printf("%s %s",
                                                             value,
                                                             metakeys[i].unit));
        else
            gwy_container_set_string_by_name(container, key->str,
                                             g_strdup(value));
    }
    g_string_free(key, TRUE);
}
#endif

static GwyDataField*
rhkspm32_read_data(const guchar *buffer,
                   RHKFile *rhkfile)
{
    GwyDataField *dfield;
    gdouble *data;
    guint i;

    dfield = GWY_DATA_FIELD(gwy_data_field_new(rhkfile->xres, rhkfile->yres,
                                               rhkfile->xres * rhkfile->x.scale,
                                               rhkfile->yres * rhkfile->y.scale,
                                               FALSE));
    data = gwy_data_field_get_data(dfield);
    for (i = 0; i < rhkfile->xres*rhkfile->yres; i++) {
        data[i] = (gint)(buffer)[0] + ((signed char)(buffer)[1]*256.0);
        buffer += 2;
    }
    gwy_data_field_multiply(dfield, rhkfile->z.scale);

    return dfield;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

