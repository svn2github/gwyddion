/*
 *  $Id$
 *  Copyright (C) 2006 David Necas (Yeti), Petr Klapetek, Markus Pristovsek
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net,
 *  prissi@gift.physik.tu-berlin.de.
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

/* TODO: metadata */

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule-file.h>
#include <libprocess/datafield.h>

#include "err.h"

#define MAGIC "Omicron SPM Control"
#define MAGIC_SIZE (sizeof(MAGIC)-1)

#define EXTENSION_HEADER ".par"

#define Nanometer 1e-9

typedef enum {
    SCAN_UNKNOWN = 0,
    SCAN_FORWARD = 1,
    SCAN_BACKWARD = -1
} ScanDirection;

typedef struct {
    gchar type;    /* Z or I */
    ScanDirection scandir;
    gint min_raw;
    gint max_raw;
    gdouble min_phys;
    gdouble max_phys;
    gdouble resolution;
    const gchar *units;
    const gchar *filename;
    const gchar *name;
} OmicronTopoChannel;

typedef struct {
    const gchar *filename;
    gint xres;
    gint yres;
    gdouble xreal;
    gdouble yreal;
    GHashTable *meta;
    GPtrArray *topo_channels;
} OmicronFile;

static gboolean      module_register         (void);
static gint          omicron_detect          (const GwyFileDetectInfo *fileinfo,
                                              gboolean only_name);
static GwyContainer* omicron_load            (const gchar *filename,
                                              GwyRunType mode,
                                              GError **error);
static gboolean      omicron_read_header     (gchar *buffer,
                                              OmicronFile *ofile,
                                              GError **error);
static gboolean      omicron_read_topo_header(gchar **buffer,
                                              OmicronTopoChannel *channel,
                                              GError **error);
static GwyDataField* omicron_read_data       (OmicronFile *ofile,
                                              OmicronTopoChannel *channel,
                                              GError **error);
static void          omicron_file_free       (OmicronFile *ofile);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Omicron data files (two-part .par + .tf*, .tb*, .sf*, .sb*)."),
    "Yeti <yeti@gwyddion.net>",
    "0.3",
    "David Nečas (Yeti) & Petr Klapetek & Markus Pristovsek",
    "2006",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("omicron",
                           N_("Omicron files (.par + data)"),
                           (GwyFileDetectFunc)&omicron_detect,
                           (GwyFileLoadFunc)&omicron_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
omicron_detect(const GwyFileDetectInfo *fileinfo,
               gboolean only_name)
{
    guint i;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION_HEADER)
               ? 15 : 0;

    /* Quick check to skip most non-matching files */
    if (fileinfo->buffer_len < 100
        || fileinfo->head[0] != ';')
        return 0;

    for (i = 1; i + MAGIC_SIZE+1 < fileinfo->buffer_len; i++) {
        if (fileinfo->head[i] != ';' && !g_ascii_isspace(fileinfo->head[i]))
            break;
    }
    if (memcmp(fileinfo->head + i, MAGIC, MAGIC_SIZE) == 0)
        return 100;

    return 0;
}

static GwyContainer*
omicron_load(const gchar *filename,
             G_GNUC_UNUSED GwyRunType mode,
             GError **error)
{
    OmicronFile ofile;
    GwyContainer *container = NULL;
    gchar *text = NULL;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    guint i, idx;
    gchar key[32];

    /* @text must not be destroyed while @ofile is still in used because
     * all strings are only references there */
    if (!g_file_get_contents(filename, &text, NULL, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    memset(&ofile, 0, sizeof(OmicronFile));
    ofile.filename = filename;
    if (!omicron_read_header(text, &ofile, error))
        goto fail;

    if (!ofile.topo_channels || !ofile.topo_channels->len) {
        err_NO_DATA(error);
        goto fail;
    }

    container = gwy_container_new();
    idx = 0;
    for (i = 0; i < ofile.topo_channels->len; i++) {
        OmicronTopoChannel *channel;

        channel = g_ptr_array_index(ofile.topo_channels, i);
        dfield = omicron_read_data(&ofile, channel, error);
        if (!dfield) {
            gwy_object_unref(container);
            goto fail;
        }

        g_snprintf(key, sizeof(key), "/%u/data", idx);
        gwy_container_set_object_by_name(container, key, dfield);
        g_object_unref(dfield);

        if (channel->name) {
            gchar *s;

            g_snprintf(key, sizeof(key), "/%u/data/title", idx);
            if (channel->scandir == SCAN_FORWARD)
                s = g_strdup_printf("%s (Forward)", channel->name);
            else if (channel->scandir == SCAN_BACKWARD)
                s = g_strdup_printf("%s (Backward)", channel->name);
            else
                s = g_strdup(channel->name);
            gwy_container_set_string_by_name(container, key, s);
        }

        idx++;
    }

fail:
    omicron_file_free(&ofile);
    g_free(text);

    return container;
}

#define GET_FIELD(hash, val, field, err) \
    do { \
        if (!(val = g_hash_table_lookup(hash, field))) { \
            err_MISSING_FIELD(err, field); \
            return FALSE; \
        } \
    } while (FALSE)

static gboolean
omicron_read_header(gchar *buffer,
                    OmicronFile *ofile,
                    GError **error)
{
    gchar *line, *val, *comment;

    ofile->meta = g_hash_table_new(g_str_hash, g_str_equal);

    while ((line = gwy_str_next_line(&buffer))) {
        /* FIXME: This strips 2nd and following lines from possibly multiline
         * fields like Comment. */
        if (!line[0] || line[0] == ';' || g_ascii_isspace(line[0]))
            continue;

        val = strchr(line, ':');
        if (!val) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Missing colon in header line."));
            return FALSE;
        }
        if (val == line) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Header line starts with a colon."));
            return FALSE;
        }
        *val = '\0';
        val++;
        g_strstrip(line);
        /* FIXME: what about `;[units]' comments? */
        comment = strchr(val, ';');
        if (comment) {
            *comment = '\0';
            comment++;
            g_strstrip(comment);
        }
        g_strstrip(val);

        if (gwy_strequal(line, "Topographic Channel")) {
            OmicronTopoChannel *channel;

            gwy_debug("Topographic Channel found (type %c)", val[0]);
            channel = g_new0(OmicronTopoChannel, 1);
            channel->type = val[0];
            if (!omicron_read_topo_header(&buffer, channel, error)) {
                g_free(channel);
                return FALSE;
            }
            if (!ofile->topo_channels)
                ofile->topo_channels = g_ptr_array_new();
            g_ptr_array_add(ofile->topo_channels, channel);
        }
        else if (gwy_strequal(line, "Spectroscopic Channel")) {
            gwy_debug("Spectroscopic Channel found");
            /* FIXME */
        }
        else {
            gwy_debug("<%s> = <%s>", line, val);
            g_hash_table_insert(ofile->meta, line, val);
        }
    }

    GET_FIELD(ofile->meta, val, "Image Size in X", error);
    ofile->xres = abs(atoi(val));
    GET_FIELD(ofile->meta, val, "Image Size in Y", error);
    ofile->yres = abs(atoi(val));

    GET_FIELD(ofile->meta, val, "Field X Size in nm", error);
    ofile->xreal = fabs(g_ascii_strtod(val, NULL)) * Nanometer;
    GET_FIELD(ofile->meta, val, "Field Y Size in nm", error);
    ofile->yreal = fabs(g_ascii_strtod(val, NULL)) * Nanometer;

    return TRUE;
}

#define NEXT_LINE(buffer, line, optional, err) \
    if (!(line = gwy_str_next_line(buffer))) { \
        g_set_error(error, GWY_MODULE_FILE_ERROR, \
                    GWY_MODULE_FILE_ERROR_DATA, \
                    _("File header ended unexpectedly.")); \
        return FALSE; \
    } \
    g_strstrip(line); \
    if (!*line) { \
        if (optional) \
            return TRUE; \
        g_set_error(error, GWY_MODULE_FILE_ERROR, \
                    GWY_MODULE_FILE_ERROR_DATA, \
                    _("Channel information ended unexpectedly.")); \
        return FALSE; \
    } \
    if ((p = strchr(line, ';'))) \
        *p = '\0'; \
    g_strstrip(line)

static gboolean
omicron_read_topo_header(gchar **buffer,
                         OmicronTopoChannel *channel,
                         GError **error)
{
    gchar *line, *p;

    /* Direction */
    NEXT_LINE(buffer, line, FALSE, error);
    gwy_debug("Scan direction: %s", line);
    if (gwy_strequal(line, "Forward"))
        channel->scandir = SCAN_FORWARD;
    else if (gwy_strequal(line, "Backward"))
        channel->scandir = SCAN_BACKWARD;
    else
        channel->scandir = SCAN_UNKNOWN;

    /* Raw range */
    NEXT_LINE(buffer, line, FALSE, error);
    channel->min_raw = atoi(line);
    NEXT_LINE(buffer, line, FALSE, error);
    channel->max_raw = atoi(line);
    gwy_debug("Raw range: [%d, %d]", channel->min_raw, channel->max_raw);

    /* Physical range */
    NEXT_LINE(buffer, line, FALSE, error);
    channel->min_phys = g_ascii_strtod(line, NULL);
    NEXT_LINE(buffer, line, FALSE, error);
    channel->max_phys = g_ascii_strtod(line, NULL);
    gwy_debug("Physical range: [%g, %g]", channel->min_phys, channel->max_phys);

    /* Resolution */
    NEXT_LINE(buffer, line, FALSE, error);
    channel->resolution = g_ascii_strtod(line, NULL);
    gwy_debug("Physical Resolution: %g", channel->resolution);

    /* Units */
    NEXT_LINE(buffer, line, FALSE, error);
    channel->units = line;
    gwy_debug("Units: <%s>", channel->units);

    /* Filename */
    NEXT_LINE(buffer, line, FALSE, error);
    channel->filename = line;
    gwy_debug("Filename: <%s>", channel->filename);

    /* Name */
    NEXT_LINE(buffer, line, TRUE, error);
    channel->name = line;
    gwy_debug("Channel name: <%s>", channel->name);

    return TRUE;
}

/* In most Omicron files, the letter case is arbitrary.  Try miscellaneous
 * variations till we finally give up */
static gchar*
omicron_fix_file_name(const gchar *parname,
                      const gchar *orig,
                      GError **error)
{
    gchar *filename, *dirname, *base;
    guint len, i;

    if (!g_path_is_absolute(orig)) {
        dirname = g_path_get_dirname(parname);
        filename = g_build_filename(dirname, orig, NULL);
    }
    else {
        dirname = g_path_get_dirname(orig);
        base = g_path_get_basename(orig);
        filename = g_build_filename(dirname, base, NULL);
        g_free(base);
    }
    g_free(dirname);
    base = filename + strlen(filename) - strlen(orig);
    len = strlen(base);

    gwy_debug("Trying <%s> (original)", filename);
    if (g_file_test(filename, G_FILE_TEST_IS_REGULAR | G_FILE_TEST_IS_SYMLINK))
        return filename;

    /* All upper */
    for (i = 0; i < len; i++)
        base[i] = g_ascii_toupper(base[i]);
    gwy_debug("Trying <%s>", filename);
    if (g_file_test(filename, G_FILE_TEST_IS_REGULAR | G_FILE_TEST_IS_SYMLINK))
        return filename;

    /* All lower */
    for (i = 0; i < len; i++)
        base[i] = g_ascii_tolower(base[i]);
    gwy_debug("Trying <%s>", filename);
    if (g_file_test(filename, G_FILE_TEST_IS_REGULAR | G_FILE_TEST_IS_SYMLINK))
        return filename;

    /* Capitalize */
    base[0] = g_ascii_toupper(base[0]);
    gwy_debug("Trying <%s>", filename);
    if (g_file_test(filename, G_FILE_TEST_IS_REGULAR | G_FILE_TEST_IS_SYMLINK))
        return filename;

    g_free(filename);
    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_IO,
                _("No data file corresponding to `%s' was found."), orig);
    return NULL;
}

static GwyDataField*
omicron_read_data(OmicronFile *ofile,
                  OmicronTopoChannel *channel,
                  GError **error)
{
    GError *err = NULL;
    GwyDataField *dfield;
    GwySIUnit *siunit;
    gchar *filename;
    gdouble *data;
    guchar *buffer;
    const gint16 *d;
    gdouble scale;
    gsize size;
    guint i, j, n;
    gint power10 = 0;

    filename = omicron_fix_file_name(ofile->filename, channel->filename, error);
    if (!filename)
        return NULL;

    gwy_debug("Succeeded with <%s>", filename);
    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        g_free(filename);
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }
    g_free(filename);

    n = ofile->xres*ofile->yres;
    if (size != 2*n) {
        err_SIZE_MISMATCH(error, 2*n, size);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    scale = (channel->max_phys - channel->min_phys)
            /(channel->max_raw - channel->min_raw);
    dfield = gwy_data_field_new(ofile->xres, ofile->yres,
                                ofile->xreal, ofile->yreal,
                                FALSE);
    data = gwy_data_field_get_data(dfield);
    d = (const gint16*)buffer;
    for (i = 0; i < ofile->yres; i++) {
        for (j = 0; j < ofile->xres; j++)
            data[(ofile->yres-1 - i)*ofile->xres + j]
                = scale*GINT16_FROM_BE(d[i*ofile->xres + j]);
    }
    gwy_file_abandon_contents(buffer, size, NULL);

    siunit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_xy(dfield, siunit);
    g_object_unref(siunit);

    siunit = gwy_si_unit_new_parse(channel->units, &power10);
    gwy_data_field_set_si_unit_z(dfield, siunit);
    g_object_unref(siunit);
    if (power10)
        gwy_data_field_multiply(dfield, pow10(power10));

    return dfield;
}

static void
omicron_file_free(OmicronFile *ofile)
{
    guint i;

    if (ofile->meta) {
        g_hash_table_destroy(ofile->meta);
        ofile->meta = NULL;
    }
    if (ofile->topo_channels) {
        for (i = 0; i < ofile->topo_channels->len; i++)
            g_free(g_ptr_array_index(ofile->topo_channels, i));
        g_ptr_array_free(ofile->topo_channels, TRUE);
        ofile->topo_channels = NULL;
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

