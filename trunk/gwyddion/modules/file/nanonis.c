/*
 *  $Id$
 *  Copyright (C) 2006 David Necas (Yeti), Petr Klapetek.
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

/* TODO: metadata */

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-nanonis-spm">
 *   <comment>Nanonis SPM data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value=":NANONIS_VERSION:"/>
 *   </magic>
 *   <glob pattern="*.sxm"/>
 *   <glob pattern="*.SXM"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-FILEMAGIC]
 * # Nanonis
 * 0 string :NANONIS_VERSION:\x0a Nanonis SXM data
 * >&0 regex [0-9]+ version %s
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * Nanonis SXM
 * .sxm
 * Read
 **/

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"

#define MAGIC ":NANONIS_VERSION:"
#define MAGIC_SIZE (sizeof(MAGIC) - 1)

#define EXTENSION ".sxm"

typedef enum {
    DIR_FORWARD  = 1 << 0,
    DIR_BACKWARD = 1 << 1,
    DIR_BOTH     = (DIR_FORWARD | DIR_BACKWARD)
} SXMDirection;

typedef struct {
    gint channel;
    gchar *name;
    gchar *unit;
    SXMDirection direction;
    gdouble calibration;
    gdouble offset;
} SXMDataInfo;

typedef struct {
    GHashTable *meta;
    gchar **z_controller_headers;
    gchar **z_controller_values;
    gint ndata;
    SXMDataInfo *data_info;

    gboolean ok;
    gint xres;
    gint yres;
    gdouble xreal;
    gdouble yreal;
    /* Set if the times are set to N/A or NaN, this seems to be done in slice
     * files of 3D data.  We cannot trust direction filed then as it's set to
     * `both' although the file contains one direction only. */
    gboolean bogus_scan_time;
    /* TRUE if we have just read a comment header and its first line.  If the
     * next line is not tag, do not complain and consider it to be a part of
     * the comment as they apparently can be mutiline.  This is a kluge. */
    gboolean in_comment;
} SXMFile;

static gboolean      module_register(void);
static gint          sxm_detect     (const GwyFileDetectInfo *fileinfo,
                                     gboolean only_name);
static GwyContainer* sxm_load       (const gchar *filename,
                                     GwyRunType mode,
                                     GError **error);
static GwyContainer* sxm_build_meta (const SXMFile *sxmfile,
                                     guint id);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Nanonis SXM data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.11",
    "David Nečas (Yeti) & Petr Klapetek",
    "2006",
};

/* FIXME: I'm making this up, never seen anything except `both' */
static const GwyEnum directions[] = {
    { "forward",  DIR_FORWARD,  },
    { "backward", DIR_BACKWARD, },
    { "both",     DIR_BOTH,     },
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("nanonis",
                           N_("Nanonis SXM files (.sxm)"),
                           (GwyFileDetectFunc)&sxm_detect,
                           (GwyFileLoadFunc)&sxm_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
sxm_detect(const GwyFileDetectInfo *fileinfo,
           gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 20 : 0;

    if (fileinfo->buffer_len > MAGIC_SIZE
        && memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) == 0)
        score = 100;

    return score;
}

static gchar*
get_next_line_with_error(gchar **p,
                         GError **error)
{
    gchar *line;

    if (!(line = gwy_str_next_line(p))) {
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_DATA,
                    _("File header ended unexpectedly."));
        return NULL;
    }
    g_strstrip(line);

    return line;
}

static gchar**
split_line_in_place(gchar *line,
                    gchar delim)
{
    gchar **strs;
    guint i, n = 0;

    for (i = 0; line[i]; i++) {
        if ((!i || line[i-1] == delim) && (line[i] && line[i] != delim))
            n++;
    }

    strs = g_new(gchar*, n+1);
    n = 0;
    for (i = 0; line[i]; i++) {
        if ((!i || line[i-1] == delim || !line[i-1])
            && (line[i] && line[i] != delim))
            strs[n++] = line + i;
        else if (i && line[i] == delim && line[i-1] != delim)
            line[i] = '\0';
    }
    strs[n] = NULL;

#ifdef DEBUG
    for (i = 0; strs[i]; i++)
        gwy_debug("%u: <%s>", i, strs[i]);
#endif

    return strs;
}

static void
sxm_free_z_controller(SXMFile *sxmfile)
{
    g_free(sxmfile->z_controller_headers);
    sxmfile->z_controller_headers = NULL;
    g_free(sxmfile->z_controller_values);
    sxmfile->z_controller_values = NULL;
}

static gboolean
sxm_read_tag(SXMFile *sxmfile,
             gchar **p,
             GError **error)
{
    gchar *line, *tag;
    gchar **columns;
    guint len;

    if (!(line = get_next_line_with_error(p, error)))
        return FALSE;

    len = strlen(line);
    if (len < 3 || line[0] != ':' || line[len-1] != ':') {
        if (sxmfile->in_comment) {
            /* Add the line to the comment if we are inside a comment. */
            gchar *comment, *newcomment;

            comment = g_hash_table_lookup(sxmfile->meta, "COMMENT");
            g_assert(comment);

            newcomment = g_strconcat(comment, " ", line, NULL);
            g_hash_table_remove(sxmfile->meta, "COMMENT");
            g_free(comment);
            g_hash_table_insert(sxmfile->meta, "COMMENT", newcomment);
            return TRUE;
        }
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Garbage was found in place of tag header line."));
        return FALSE;
    }
    tag = line+1;
    line[len-1] = '\0';
    sxmfile->in_comment = FALSE;
    gwy_debug("tag: <%s>", tag);

    if (gwy_strequal(tag, "SCANIT_END")) {
        sxmfile->ok = TRUE;
        return TRUE;
    }

    if (gwy_strequal(tag, "Z-CONTROLLER")) {
        /* Headers */
        if (!(line = get_next_line_with_error(p, error)))
            return FALSE;

        if (sxmfile->z_controller_headers) {
            g_warning("Multiple Z-CONTROLLERs, keeping only the last");
            sxm_free_z_controller(sxmfile);
        }

        /* XXX: Documentation says tabs, but I see spaces in the file. */
        g_strdelimit(line, " ", '\t');
        sxmfile->z_controller_headers =  split_line_in_place(line, '\t');

        /* Values */
        if (!(line = get_next_line_with_error(p, error))) {
            sxm_free_z_controller(sxmfile);
            return FALSE;
        }

        sxmfile->z_controller_values = split_line_in_place(line, '\t');
        if (g_strv_length(sxmfile->z_controller_headers)
            != g_strv_length(sxmfile->z_controller_values)) {
            g_warning("The numbers of Z-CONTROLLER headers and values differ");
            sxm_free_z_controller(sxmfile);
        }
        return TRUE;
    }

    if (gwy_strequal(tag, "DATA_INFO")) {
        SXMDataInfo di;
        GArray *data_info;

        /* Headers */
        if (!(line = get_next_line_with_error(p, error)))
            return FALSE;
        /* XXX: Documentation says tabs, but I see spaces in the file. */
        g_strdelimit(line, " ", '\t');
        columns = split_line_in_place(line, '\t');

        if (g_strv_length(columns) < 6
            || !gwy_strequal(columns[0], "Channel")
            || !gwy_strequal(columns[1], "Name")
            || !gwy_strequal(columns[2], "Unit")
            || !gwy_strequal(columns[3], "Direction")
            || !gwy_strequal(columns[4], "Calibration")
            || !gwy_strequal(columns[5], "Offset")) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("DATA_INFO does not contain the expected "
                          "columns: %s."),
                        "Channel Name Unit Direction Calibration Offset");
            g_free(columns);
            return FALSE;
        }

        if (sxmfile->data_info) {
            g_warning("Multiple DATA_INFOs, keeping only the last");
            g_free(sxmfile->data_info);
            sxmfile->data_info = NULL;
        }

        data_info = g_array_new(FALSE, FALSE, sizeof(SXMDataInfo));
        while ((line = get_next_line_with_error(p, error)) && *line) {
            g_strstrip(line);
            if (gwy_strequal(line, ":SCANIT_END:")) {
                sxmfile->ok = TRUE;
                return TRUE;
            }

            columns = split_line_in_place(line, '\t');
            if (g_strv_length(columns) < 6) {
                g_set_error(error, GWY_MODULE_FILE_ERROR,
                            GWY_MODULE_FILE_ERROR_DATA,
                            _("DATA_INFO line contains less than %d fields."),
                            6);
                g_free(columns);
                g_array_free(data_info, TRUE);
                return FALSE;
            }

            di.channel = atoi(columns[0]);
            di.name = columns[1];
            di.unit = columns[2];
            di.direction = gwy_string_to_enum(columns[3],
                                              directions,
                                              G_N_ELEMENTS(directions));
            if (di.direction == (SXMDirection)-1) {
                err_INVALID(error, "Direction");
                g_free(columns);
                g_array_free(data_info, TRUE);
                return FALSE;
            }
            di.calibration = g_ascii_strtod(columns[4], NULL);
            di.offset = g_ascii_strtod(columns[5], NULL);
            g_array_append_val(data_info, di);

            g_free(columns);
            columns = NULL;
        }

        if (!line) {
            g_array_free(data_info, TRUE);
            return FALSE;
        }

        sxmfile->data_info = (SXMDataInfo*)data_info->data;
        sxmfile->ndata = data_info->len;
        g_array_free(data_info, FALSE);
        return TRUE;
    }

    if (gwy_strequal(tag, "Multipass-Config")) {
        /* Multipass-Config.  Don't know how to tell the number of lines in the
         * table :-/  Cross fingers and try to read lines until we hit ':' at
         * the begining.  We have to look at p as seeing ':' in line would be
         * too late. */
        while (*p && **p && **p != ':') {
            if (!(line = get_next_line_with_error(p, error)))
                return FALSE;
        }
        if (!*p || !**p) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Header ended within a Multipass-Config table."));
            return FALSE;
        }
        /* Uf. */
        return TRUE;
    }

    if (!(line = get_next_line_with_error(p, error)))
        return FALSE;

    if (gwy_strequal(tag, "COMMENT")) {
        gchar *comment;

        sxmfile->in_comment = TRUE;
        if ((comment = g_hash_table_lookup(sxmfile->meta, tag))) {
            g_hash_table_remove(sxmfile->meta, tag);
            g_free(comment);
        }
        /* XXX: Comments are built per partes and hence we have to allocate
         * them. */
        line = g_strdup(line);
    }

    g_hash_table_insert(sxmfile->meta, tag, line);
    gwy_debug("value: <%s>", line);

    return TRUE;
}

static void
read_data_field(GwyContainer *container,
                gint *id,
                const gchar *filename,
                const SXMFile *sxmfile,
                const SXMDataInfo *data_info,
                SXMDirection dir,
                const guchar **p)
{
    GwyContainer *meta;
    GwyDataField *dfield, *mfield = NULL;
    gdouble *data, *mdata;
    gint j;
    gchar key[32];
    gchar *s;
    gboolean flip_vertically = FALSE, flip_horizontally = FALSE;

    dfield = gwy_data_field_new(sxmfile->xres, sxmfile->yres,
                                sxmfile->xreal, sxmfile->yreal,
                                FALSE);
    data = gwy_data_field_get_data(dfield);

    for (j = 0; j < sxmfile->xres*sxmfile->yres; j++) {
        /* This is not a perfect NaN check, but Nanonis uses ff as the payload
         * so look only for these. */
        if (G_UNLIKELY(((*p)[0] & 0x7f) == 0x7f && (*p)[1] == 0xff))
            break;

        *(data++) = gwy_get_gfloat_be(p);
    }

    if (j < sxmfile->xres*sxmfile->yres) {
        mfield = gwy_data_field_new_alike(dfield, TRUE);
        mdata = gwy_data_field_get_data(mfield);
        while (j < sxmfile->xres*sxmfile->yres) {
            if (((*p)[0] & 0x7f) == 0x7f && (*p)[1] == 0xff) {
                mdata[j] = -1.0;
                *p += sizeof(gfloat);
            }
            else
                *(data++) = gwy_get_gfloat_be(p);
            j++;
        }
        gwy_data_field_add(mfield, 1.0);
        gwy_app_channel_remove_bad_data(dfield, mfield);
    }

    if (mfield) {
        gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_xy(mfield), "m");
        g_snprintf(key, sizeof(key), "/%d/mask", *id);
        gwy_container_set_object_by_name(container, key, mfield);
        g_object_unref(mfield);
    }

    gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_xy(dfield), "m");
    gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_z(dfield),
                                data_info->unit);
    g_snprintf(key, sizeof(key), "/%d/data", *id);
    gwy_container_set_object_by_name(container, key, dfield);
    g_object_unref(dfield);

    g_strlcat(key, "/title", sizeof(key));
    if (!dir)
        gwy_container_set_string_by_name(container, key,
                                         g_strdup(data_info->name));
    else {
        gchar *title;

        title = g_strdup_printf("%s (%s)", data_info->name,
                                dir == DIR_BACKWARD ? "Backward" : "Forward");
        gwy_container_set_string_by_name(container, key, title);
        /* Don't free title, container eats it */
    }

    if ((meta = sxm_build_meta(sxmfile, *id))) {
        g_snprintf(key, sizeof(key), "/%d/meta", *id);
        gwy_container_set_object_by_name(container, key, meta);
        g_object_unref(meta);
    }

    gwy_app_channel_check_nonsquare(container, *id);

    if (dir == DIR_BACKWARD)
        flip_horizontally = TRUE;

    if ((s = g_hash_table_lookup(sxmfile->meta, "SCAN_DIR"))
        && gwy_strequal(s, "up"))
        flip_vertically = TRUE;

    gwy_data_field_invert(dfield, flip_vertically, flip_horizontally, FALSE);
    gwy_file_channel_import_log_add(container, *id, NULL, filename);

    (*id)++;
}

static GwyContainer*
sxm_load(const gchar *filename,
         G_GNUC_UNUSED GwyRunType mode,
         GError **error)
{
    SXMFile sxmfile;
    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    gsize size1 = 0, size = 0;
    GError *err = NULL;
    const guchar *p;
    gchar *header, *hp, *s, *endptr;
    gchar **columns;
    G_GNUC_UNUSED gboolean rotated = FALSE;
    gint version;
    guint i;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    if (size < MAGIC_SIZE + 400) {
        err_TOO_SHORT(error);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    if (memcmp(buffer, MAGIC, MAGIC_SIZE) != 0) {
        err_FILE_TYPE(error, "Nanonis");
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    /* Extract header (we need it writable) */
    p = memchr(buffer, '\x1a', size);
    if (!p || p + 1 == buffer + size || p[1] != '\x04') {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Missing data start marker \\x1a\\x04."));
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    gwy_clear(&sxmfile, 1);
    sxmfile.meta = g_hash_table_new(g_str_hash, g_str_equal);

    header = g_memdup(buffer, p - buffer + 1);
    header[p - buffer] = '\0';
    hp = header;
    /* Move p to actual data start */
    p += 2;

    /* Parse header */
    do {
        if (!sxm_read_tag(&sxmfile, &hp, error)) {
            sxm_free_z_controller(&sxmfile);
            g_free(sxmfile.data_info);
            g_free(header);
            gwy_file_abandon_contents(buffer, size, NULL);
            return NULL;
        }
    } while (!sxmfile.ok);

    /* Data info */
    if (sxmfile.ok) {
        if (!sxmfile.data_info) {
            err_NO_DATA(error);
            sxmfile.ok = FALSE;
        }
    }

    /* Version */
    if ((s = g_hash_table_lookup(sxmfile.meta, "NANONIS_VERSION")))
        version = atoi(s);
    else {
        g_warning("Version is missing, assuming old files.  "
                  "How it can happen, anyway?");
        version = 0;
    }

    /* Data type */
    if (sxmfile.ok) {
        if ((s = g_hash_table_lookup(sxmfile.meta, "SCANIT_TYPE"))) {
            gwy_debug("s: <%s>", s);
            columns = split_line_in_place(s, ' ');
            if (g_strv_length(columns) == 2
                && gwy_strequal(columns[0], "FLOAT")
                /* XXX: No matter what they say, the files seems to be BE */
                && (gwy_strequal(columns[1], "LSBFIRST")
                    || gwy_strequal(columns[1], "MSBFIRST"))) {
                size1 = sizeof(gfloat);
            }
            else {
                err_UNSUPPORTED(error, "SCANIT_TYPE");
                sxmfile.ok = FALSE;
            }
            g_free(columns);
        }
        else {
            err_MISSING_FIELD(error, "SCANIT_TYPE");
            sxmfile.ok = FALSE;
        }
    }

    /* Check for rotated data */
    if (sxmfile.ok) {
        if ((s = g_hash_table_lookup(sxmfile.meta, "SCAN_ANGLE"))) {
            if (g_ascii_strtod(s, NULL) == 90.0) {
                gwy_debug("data is rotated");
                rotated = TRUE;
            }
        }
    }

    /* Pixel sizes */
    if (sxmfile.ok) {
        if ((s = g_hash_table_lookup(sxmfile.meta, "SCAN_PIXELS"))) {
            if (sscanf(s, "%d %d", &sxmfile.xres, &sxmfile.yres) == 2) {
                /* Version 1 files have y and x swapped just for fun. */
                if (version < 2)
                    GWY_SWAP(gint, sxmfile.xres, sxmfile.yres);
                size1 *= sxmfile.xres * sxmfile.yres;
                gwy_debug("xres: %d, yres: %d", sxmfile.xres, sxmfile.yres);
                gwy_debug("size1: %u", (guint)size1);
            }
            else {
                err_INVALID(error, "SCAN_PIXELS");
                sxmfile.ok = FALSE;
            }
        }
        else {
            err_MISSING_FIELD(error, "SCAN_PIXELS");
            sxmfile.ok = FALSE;
        }

        if (sxmfile.ok
            && (err_DIMENSION(error, sxmfile.xres)
                || err_DIMENSION(error, sxmfile.yres)))
            sxmfile.ok = FALSE;
    }

    /* Physical dimensions */
    if (sxmfile.ok) {
        if ((s = g_hash_table_lookup(sxmfile.meta, "SCAN_RANGE"))) {
            sxmfile.xreal = g_ascii_strtod(s, &endptr);
            if (endptr != s) {
                s = endptr;
                sxmfile.yreal = g_ascii_strtod(s, &endptr);
                gwy_debug("xreal: %g, yreal: %g", sxmfile.xreal, sxmfile.yreal);
            }
            if (s == endptr) {
                err_INVALID(error, "SCAN_RANGE");
                sxmfile.ok = FALSE;
            }
        }
        else {
            err_MISSING_FIELD(error, "SCAN_RANGE");
            sxmfile.ok = FALSE;
        }

        if (sxmfile.ok) {
            /* Use negated positive conditions to catch NaNs */
            if (!((sxmfile.xreal = fabs(sxmfile.xreal)) > 0)) {
                g_warning("Real x size is 0.0, fixing to 1.0");
                sxmfile.xreal = 1.0;
            }
            if (!((sxmfile.yreal = fabs(sxmfile.yreal)) > 0)) {
                g_warning("Real y size is 0.0, fixing to 1.0");
                sxmfile.yreal = 1.0;
            }
        }
    }

    /* Scan times, check for bogus values indicating generated slice files. */
    if (sxmfile.ok) {
        if ((s = g_hash_table_lookup(sxmfile.meta, "ACQ_TIME"))
            && gwy_strequal(s, "N/A"))
            sxmfile.bogus_scan_time = TRUE;
        else if ((s = g_hash_table_lookup(sxmfile.meta, "SCAN_TIME"))
                 && strncmp(s, "NaN", 3) == 0)
            sxmfile.bogus_scan_time = TRUE;
    }

    /* Check file size */
    if (sxmfile.ok) {
        gsize expected_size;

        expected_size = p - buffer;
        for (i = 0; i < sxmfile.ndata; i++) {
            guint d = sxmfile.data_info[i].direction;

            if (d == DIR_BOTH) {
                /* XXX: Assume generated files lie about the direction and
                 * they are always unidirectional. */
                if (sxmfile.bogus_scan_time) {
                    sxmfile.data_info[i].direction = DIR_FORWARD;
                    expected_size += size1;
                }
                else
                    expected_size += 2*size1;
            }
            else if (d == DIR_FORWARD || d == DIR_BACKWARD)
                expected_size += size1;
            else {
                g_assert_not_reached();
            }
        }
        if (err_SIZE_MISMATCH(error, expected_size, size, TRUE))
            sxmfile.ok = FALSE;
    }

    /* Read data */
    if (sxmfile.ok) {
        gint id = 0;

        container = gwy_container_new();
        for (i = 0; i < sxmfile.ndata; i++) {
            guint d = sxmfile.data_info[i].direction;

            if (d == DIR_BOTH) {
                read_data_field(container, &id, filename,
                                &sxmfile, sxmfile.data_info + i,
                                DIR_FORWARD, &p);
                read_data_field(container, &id, filename,
                                &sxmfile, sxmfile.data_info + i,
                                DIR_BACKWARD, &p);
            }
            else if (d == DIR_FORWARD || d == DIR_BACKWARD) {
                read_data_field(container, &id, filename,
                                &sxmfile, sxmfile.data_info + i, d, &p);
            }
            else {
                g_assert_not_reached();
            }
        }
    }

    sxm_free_z_controller(&sxmfile);
    g_free(sxmfile.data_info);
    if ((s = g_hash_table_lookup(sxmfile.meta, "COMMENT")))
        g_free(s);
    g_hash_table_destroy(sxmfile.meta);
    g_free(header);
    gwy_file_abandon_contents(buffer, size, NULL);

    return container;
}

static inline gchar*
reformat_float(const gchar *format,
               const gchar *value)
{
    gdouble v = g_ascii_strtod(value, NULL);
    return g_strdup_printf(format, v);
}

static GwyContainer*
sxm_build_meta(const SXMFile *sxmfile,
               G_GNUC_UNUSED guint id)
{
    GwyContainer *meta = gwy_container_new();
    const gchar *value;

    if ((value = g_hash_table_lookup(sxmfile->meta, "COMMENT")))
        gwy_container_set_string_by_name(meta, "Comment", g_strdup(value));
    if ((value = g_hash_table_lookup(sxmfile->meta, "REC_DATE")))
        gwy_container_set_string_by_name(meta, "Date", g_strdup(value));
    if ((value = g_hash_table_lookup(sxmfile->meta, "REC_TIME")))
        gwy_container_set_string_by_name(meta, "Time", g_strdup(value));
    if ((value = g_hash_table_lookup(sxmfile->meta, "REC_TEMP")))
        gwy_container_set_string_by_name(meta, "Temperature",
                                         reformat_float("%g K", value));
    if ((value = g_hash_table_lookup(sxmfile->meta, "ACQ_TIME")))
        gwy_container_set_string_by_name(meta, "Acquistion time",
                                         reformat_float("%g s", value));
    if ((value = g_hash_table_lookup(sxmfile->meta, "SCAN_FILE")))
        gwy_container_set_string_by_name(meta, "File name", g_strdup(value));
    if ((value = g_hash_table_lookup(sxmfile->meta, "BIAS")))
        gwy_container_set_string_by_name(meta, "Bias",
                                         reformat_float("%g V", value));
    if ((value = g_hash_table_lookup(sxmfile->meta, "SCAN_DIR")))
        gwy_container_set_string_by_name(meta, "Direction", g_strdup(value));

    if (sxmfile->z_controller_headers && sxmfile->z_controller_values) {
        gchar **cvalues = sxmfile->z_controller_values;
        gchar **cheaders = sxmfile->z_controller_headers;
        guint i;

        for (i = 0; cheaders[i] && cvalues[i]; i++) {
            gchar *key = g_strconcat("Z controller ", cheaders[i], NULL);
            gwy_container_set_string_by_name(meta, key, g_strdup(cvalues[i]));
            g_free(key);
        }
    }

    if (gwy_container_get_n_items(meta))
        return meta;

    g_object_unref(meta);
    return NULL;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
