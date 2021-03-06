/*
 *  $Id$
 *  Copyright (C) 2005 David Necas (Yeti), Petr Klapetek.
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <libgwyddion/gwywin32unistd.h>

#if (defined(HAVE_SYS_STAT_H) || defined(_WIN32))
#include <sys/stat.h>
/* And now we are in a deep s... */
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#include "get.h"

#define EXTENSION ".sdf"
#define MICROMAP_EXTENSION ".sdfa"

#define Micrometer (1e-6)

enum {
    SDF_HEADER_SIZE_BIN = 8 + 10 + 2*12 + 2*2 + 4*8 + 3*1
};

enum {
    SDF_PING_SIZE = 4096,
    SDF_MIN_TEXT_SIZE = 160
};

typedef enum {
    SDF_UINT8  = 0,
    SDF_UINT16 = 1,
    SDF_UINT32 = 2,
    SDF_FLOAT  = 3,
    SDF_SINT8  = 4,
    SDF_SINT16 = 5,
    SDF_SINT32 = 6,
    SDF_DOUBLE = 7,
    SDF_NTYPES
} SDFDataType;

typedef struct {
    gchar version[8];
    gchar manufacturer[10];
    gchar creation[12];
    gchar modification[12];
    gint xres;
    gint yres;
    gdouble xscale;
    gdouble yscale;
    gdouble zscale;
    gdouble zres;
    gint compression;
    SDFDataType data_type;
    gint check_type;
    GHashTable *extras;
    gchar *data;

    gint expected_size;
} SDFile;

static gboolean      module_register        (const gchar *name);
static gint          sdfile_detect_bin      (const gchar *filename,
                                             gboolean only_name);
static gint          sdfile_detect_text     (const gchar *filename,
                                             gboolean only_name);
static gint          micromap_detect        (const gchar *filename,
                                             gboolean only_name);
static GwyContainer* sdfile_load_bin        (const gchar *filename);
static GwyContainer* sdfile_load_text       (const gchar *filename);
static GwyContainer* micromap_load          (const gchar *filename);
static gboolean      check_params           (const SDFile *sdfile,
                                             guint len);
static gboolean      sdfile_read_header_bin (const guchar **p,
                                             gsize *len,
                                             SDFile *sdfile);
static gboolean      sdfile_read_header_text(gchar **buffer,
                                             gsize *len,
                                             SDFile *sdfile);
static gchar*        sdfile_next_line       (gchar **buffer,
                                             const gchar *key);
static GwyDataField* sdfile_read_data_bin   (SDFile *sdfile);
static GwyDataField* sdfile_read_data_text  (SDFile *sdfile);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "sdfile",
    N_("Imports Surfstand group SDF (Surface Data File) files."),
    "Yeti <yeti@gwyddion.net>",
    "0.1.3",
    "David Nečas (Yeti) & Petr Klapetek",
    "2005",
};

static const guint type_sizes[] = { 1, 2, 4, 4, 1, 2, 4, 8 };

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyFileFuncInfo sdfile_bin_func_info = {
        "sdfile-bin",
        N_("Surfstand SDF files, binary (.sdf)"),
        (GwyFileDetectFunc)&sdfile_detect_bin,
        (GwyFileLoadFunc)&sdfile_load_bin,
        NULL
    };
    static GwyFileFuncInfo sdfile_text_func_info = {
        "sdfile-txt",
        N_("Surfstand SDF files, text (.sdf)"),
        (GwyFileDetectFunc)&sdfile_detect_text,
        (GwyFileLoadFunc)&sdfile_load_text,
        NULL
    };
    /* A specific variant reported by a guy from Layertec */
    static GwyFileFuncInfo micromap_func_info = {
        "micromap",
        N_("Micromap SDF files (.sdfa)"),
        (GwyFileDetectFunc)&micromap_detect,
        (GwyFileLoadFunc)&micromap_load,
        NULL
    };

    gwy_file_func_register(name, &sdfile_bin_func_info);
    gwy_file_func_register(name, &sdfile_text_func_info);
    gwy_file_func_register(name, &micromap_func_info);

    return TRUE;
}

static gint
sdfile_detect_bin(const gchar *filename,
                  gboolean only_name)
{
    SDFile sdfile;
    struct stat st;
    guchar *header;
    const guchar *p;
    gsize len;
    FILE *fh;
    gint score = 0;

    if (only_name) {
        gchar *filename_lc;

        filename_lc = g_ascii_strdown(filename, -1);
        score = g_str_has_suffix(filename_lc, EXTENSION) ? 10 : 0;
        g_free(filename_lc);

        return score;
    }

    if (stat(filename, &st))
        return 0;

    if (!(fh = fopen(filename, "rb")))
        return 0;

    header = g_new(guchar, SDF_HEADER_SIZE_BIN);
    if (fread(header, 1, SDF_HEADER_SIZE_BIN, fh) != SDF_HEADER_SIZE_BIN
        || header[0] != 'b') {
        g_free(header);
        fclose(fh);
        return 0;
    }
    fclose(fh);

    len = SDF_HEADER_SIZE_BIN;
    p = header;
    if (sdfile_read_header_bin(&p, &len, &sdfile)
        && SDF_HEADER_SIZE_BIN + sdfile.expected_size == st.st_size
        && !sdfile.compression
        && !sdfile.check_type)
        score = 90;

    g_free(header);

    return score;
}

static gint
sdfile_detect_text(const gchar *filename,
                   gboolean only_name)
{
    SDFile sdfile;
    gchar *header, *p;
    gsize len;
    FILE *fh;
    gint score = 0;

    if (only_name) {
        gchar *filename_lc;

        filename_lc = g_ascii_strdown(filename, -1);
        score = g_str_has_suffix(filename_lc, EXTENSION) ? 15 : 0;
        g_free(filename_lc);

        return score;
    }

    if (!(fh = fopen(filename, "rb")))
        return 0;

    header = g_new0(gchar, SDF_PING_SIZE);
    len = fread(header, 1, SDF_PING_SIZE-1, fh);
    fclose(fh);
    if (len < SDF_MIN_TEXT_SIZE-1 || header[0] != 'a') {
        g_free(header);
        return 0;
    }

    p = header;
    if (sdfile_read_header_text(&p, &len, &sdfile)
        && !sdfile.compression
        && !sdfile.check_type)
        score = 90;

    g_free(header);

    return score;
}

/* This perform a generic SDF test first, then check for Micromap */
static gint
micromap_detect(const gchar *filename,
                gboolean only_name)
{
    SDFile sdfile;
    gchar *header, *p;
    gsize len;
    FILE *fh;
    gint score = 0;

    if (only_name) {
        gchar *filename_lc;

        filename_lc = g_ascii_strdown(filename, -1);
        score = g_str_has_suffix(filename_lc, MICROMAP_EXTENSION) ? 18 : 0;
        g_free(filename_lc);

        return score;
    }

    if (!(fh = fopen(filename, "rb")))
        return 0;

    header = g_new0(gchar, SDF_PING_SIZE);
    len = fread(header, 1, SDF_PING_SIZE-1, fh);
    fclose(fh);
    if (len < SDF_MIN_TEXT_SIZE-1 || header[0] != 'a') {
        g_free(header);
        return 0;
    }

    p = header;
    if (sdfile_read_header_text(&p, &len, &sdfile)
        && !sdfile.compression
        && !sdfile.check_type) {
        /* FIXME: This is gross.  But I'm lazy to read file footer, in 2.0
         * we can do that more easily */
        if (strncmp(sdfile.manufacturer, "Micromap", sizeof("Micromap")-1) == 0)
            score = 100;
    }

    g_free(header);

    return score;
}

static void
sdfile_set_units(SDFile *sdfile,
                 GwyDataField *dfield)
{
    GwySIUnit *siunit;

    gwy_data_field_multiply(dfield, sdfile->zscale);

    siunit = GWY_SI_UNIT(gwy_si_unit_new("m"));
    gwy_data_field_set_si_unit_xy(dfield, siunit);
    g_object_unref(siunit);

    siunit = GWY_SI_UNIT(gwy_si_unit_new("m"));
    gwy_data_field_set_si_unit_z(dfield, siunit);
    g_object_unref(siunit);
}

static GwyContainer*
sdfile_load_bin(const gchar *filename)
{
    SDFile sdfile;
    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    const guchar *p;
    gsize len, size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        g_clear_error(&err);
        g_warning("Cannot get file contents");
        return NULL;
    }

    len = size;
    p = buffer;
    if (sdfile_read_header_bin(&p, &len, &sdfile)) {
        if (check_params(&sdfile, len))
            dfield = sdfile_read_data_bin(&sdfile);
    }

    gwy_file_abandon_contents(buffer, size, NULL);
    if (!dfield)
        return NULL;

    sdfile_set_units(&sdfile, dfield);

    container = GWY_CONTAINER(gwy_container_new());
    gwy_container_set_object_by_name(container, "/0/data", (GObject*)dfield);
    g_object_unref(dfield);

    return container;
}

static GwyContainer*
sdfile_load_text(const gchar *filename)
{
    SDFile sdfile;
    GwyContainer *container = NULL;
    gchar *p, *buffer = NULL;
    gsize len, size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;

    if (!g_file_get_contents(filename, &buffer, &size, &err)) {
        g_clear_error(&err);
        g_warning("Cannot get file contents");
        return NULL;
    }
    len = size;
    p = buffer;
    if (sdfile_read_header_text(&p, &len, &sdfile)) {
        if (check_params(&sdfile, len))
            dfield = sdfile_read_data_text(&sdfile);
    }

    if (!dfield) {
        g_free(buffer);
        return NULL;
    }

    sdfile_set_units(&sdfile, dfield);

    container = GWY_CONTAINER(gwy_container_new());
    gwy_container_set_object_by_name(container, "/0/data", (GObject*)dfield);
    g_object_unref(dfield);

    g_free(buffer);
    if (sdfile.extras)
        g_hash_table_destroy(sdfile.extras);

    return container;
}

/* Perform a generic SDF load, then tweak the sizes:
 *
 * X [um] = NumPoints x OBJECTIVEMAG x TUBEMAG x CAMERAXPIXEL
 * Y [um] = NumProfiles x OBJECTIVEMAG x TUBEMAG x CAMERAXPIXEL
 */
static GwyContainer*
micromap_load(const gchar *filename)
{
    SDFile sdfile;
    GwyContainer *container = NULL;
    gchar *p, *buffer = NULL;
    gsize len, size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    const gchar *val;
    gdouble objectivemag = 0.0 + sizeof("Die, die, gcc");
    gdouble tubemag = 0.0 + sizeof("Die, die, gcc");
    gdouble cameraxpixel = 0.0 + sizeof("Die, die, gcc");
    gdouble cameraypixel = 0.0 + sizeof("Die, die, gcc");
    gboolean ok;

    if (!g_file_get_contents(filename, &buffer, &size, &err)) {
        g_clear_error(&err);
        g_warning("Cannot get file contents");
        return NULL;
    }
    len = size;
    p = buffer;
    if (sdfile_read_header_text(&p, &len, &sdfile)) {
        if (check_params(&sdfile, len))
            dfield = sdfile_read_data_text(&sdfile);
    }
    ok = (dfield != NULL);

    if (ok && (val = g_hash_table_lookup(sdfile.extras, "OBJECTIVEMAG")))
        objectivemag = g_ascii_strtod(val, NULL);
    else
        ok = FALSE;

    if (ok && (val = g_hash_table_lookup(sdfile.extras, "TUBEMAG")))
        tubemag = g_ascii_strtod(val, NULL);
    else
        ok = FALSE;

    if (ok && (val = g_hash_table_lookup(sdfile.extras, "CAMERAXPIXEL")))
        cameraxpixel = g_ascii_strtod(val, NULL);
    else
        ok = FALSE;

    if (ok && (val = g_hash_table_lookup(sdfile.extras, "CAMERAYPIXEL")))
        cameraypixel = g_ascii_strtod(val, NULL);
    else
        ok = FALSE;

    if (!ok) {
        gwy_object_unref(dfield);
        g_free(buffer);
        return NULL;
    }

    sdfile_set_units(&sdfile, dfield);
    gwy_data_field_set_xreal(dfield,
                             Micrometer * sdfile.xres * objectivemag
                             * tubemag * cameraxpixel);
    gwy_data_field_set_yreal(dfield,
                             Micrometer * sdfile.yres * objectivemag
                             * tubemag * cameraypixel);

    container = GWY_CONTAINER(gwy_container_new());
    gwy_container_set_object_by_name(container, "/0/data", (GObject*)dfield);
    g_object_unref(dfield);

    g_free(buffer);
    if (sdfile.extras)
        g_hash_table_destroy(sdfile.extras);

    return container;
}

static gboolean
check_params(const SDFile *sdfile,
             guint len)
{
    if (sdfile->data_type >= SDF_NTYPES) {
        g_warning("Unsupported DataType");
        return FALSE;
    }
    if (sdfile->expected_size > len) {
        g_warning("Data size mismatch");
        return FALSE;
    }
    if (sdfile->compression) {
        g_warning("Unsupported Compression");
        return FALSE;
    }
    if (sdfile->check_type) {
        g_warning("Unsupported CheckType");
        return FALSE;
    }

    return TRUE;
}

static gboolean
sdfile_read_header_bin(const guchar **p,
                       gsize *len,
                       SDFile *sdfile)
{
    if (*len < SDF_HEADER_SIZE_BIN) {
        g_warning("File is too short");
        return FALSE;
    }

    memset(sdfile, 0, sizeof(SDFile));
    get_CHARARRAY(sdfile->version, p);
    get_CHARARRAY(sdfile->manufacturer, p);
    get_CHARARRAY(sdfile->creation, p);
    get_CHARARRAY(sdfile->modification, p);
    sdfile->xres = get_WORD(p);
    sdfile->yres = get_WORD(p);
    sdfile->xscale = get_DOUBLE(p);
    sdfile->yscale = get_DOUBLE(p);
    sdfile->zscale = get_DOUBLE(p);
    sdfile->zres = get_DOUBLE(p);
    sdfile->compression = **p;
    (*p)++;
    sdfile->data_type = **p;
    (*p)++;
    sdfile->check_type = **p;
    (*p)++;
    sdfile->data = (gchar*)*p;

    if (sdfile->data_type < SDF_NTYPES)
        sdfile->expected_size = type_sizes[sdfile->data_type]
                                * sdfile->xres * sdfile->yres;
    else
        sdfile->expected_size = -1;

    *len -= SDF_HEADER_SIZE_BIN;
    return TRUE;
}

#define NEXT(line, key, val) \
    if (!(val = sdfile_next_line(&line, key))) { \
        g_warning("Missing field %s", key); \
        return FALSE; \
    }

#define READ_STRING(line, key, val, field) \
    NEXT(line, key, val) \
    strncpy(field, val, sizeof(field));

#define READ_INT(line, key, val, field, check) \
    NEXT(line, key, val) \
    field = atoi(val); \
    if (check && field <= 0) { \
        g_warning("Invalid `%s' value: %d.", key, field); \
        return FALSE; \
    }

#define READ_FLOAT(line, key, val, field, check) \
    NEXT(line, key, val) \
    field = g_ascii_strtod(val, NULL); \
    if (check && field <= 0.0) { \
        g_warning("Invalid `%s' value: %g.", key, field); \
        return FALSE; \
    }

/* NB: Buffer must be writable and nul-terminated, its initial part is
 * overwritten */
static gboolean
sdfile_read_header_text(gchar **buffer,
                        gsize *len,
                        SDFile *sdfile)
{
    gchar *val, *p;

    /* We do not need exact lenght of the minimum file */
    if (*len < SDF_MIN_TEXT_SIZE) {
        g_warning("File is too short");
        return FALSE;
    }

    memset(sdfile, 0, sizeof(SDFile));
    p = *buffer;

    val = g_strstrip(gwy_str_next_line(&p));
    strncpy(sdfile->version, val, sizeof(sdfile->version));

    READ_STRING(p, "ManufacID", val, sdfile->manufacturer)
    READ_STRING(p, "CreateDate", val, sdfile->creation)
    READ_STRING(p, "ModDate", val, sdfile->modification)
    READ_INT(p, "NumPoints", val, sdfile->xres, TRUE)
    READ_INT(p, "NumProfiles", val, sdfile->yres, TRUE)
    READ_FLOAT(p, "Xscale", val, sdfile->xscale, TRUE)
    READ_FLOAT(p, "Yscale", val, sdfile->yscale, TRUE)
    READ_FLOAT(p, "Zscale", val, sdfile->zscale, TRUE)
    READ_FLOAT(p, "Zresolution", val, sdfile->zres, FALSE)
    READ_INT(p, "Compression", val, sdfile->compression, FALSE)
    READ_INT(p, "DataType", val, sdfile->data_type, FALSE)
    READ_INT(p, "CheckType", val, sdfile->check_type, FALSE)

    /* at least */
    if (sdfile->data_type < SDF_NTYPES)
        sdfile->expected_size = 2*sdfile->xres * sdfile->yres;
    else
        sdfile->expected_size = -1;

    /* Skip possible extra header lines */
    do {
        val = gwy_str_next_line(&p);
        if (!val)
            break;
        val = g_strstrip(val);
        if (g_ascii_isalpha(val[0])) {
            gwy_debug("Extra header line: <%s>\n", val);
        }
    } while (val[0] == ';' || g_ascii_isalpha(val[0]));

    if (!val || *val != '*') {
        g_warning("Missing data start marker (*).");
        return FALSE;
    }

    *buffer = p;
    *len -= p - *buffer;
    sdfile->data = (gchar*)*buffer;
    return TRUE;
}

static gchar*
sdfile_next_line(gchar **buffer,
                 const gchar *key)
{
    guint klen;
    gchar *value, *line;

    do {
        line = gwy_str_next_line(buffer);
    } while (line && line[0] == ';');

    if (!line) {
        g_warning("End of file reached when looking for `%s' field.", key);
        return NULL;
    }

    klen = strlen(key);
    if (strncmp(line, key, klen) != 0
        || !g_ascii_isspace(line[klen])) {
        g_warning("Invalid line found when looking for `%s' field.", key);
        return NULL;
    }

    value = line + klen;
    g_strstrip(value);
    if (value[0] == '=') {
        value++;
        g_strstrip(value);
    }

    return value;
}

static GwyDataField*
sdfile_read_data_bin(SDFile *sdfile)
{
    gint i, n;
    GwyDataField *dfield;
    gdouble *data;
    const guchar *p;

    dfield = GWY_DATA_FIELD(gwy_data_field_new(sdfile->xres, sdfile->yres,
                                               sdfile->xres * sdfile->xscale,
                                               sdfile->yres * sdfile->yscale,
                                               FALSE));
    data = gwy_data_field_get_data(dfield);
    n = sdfile->xres * sdfile->yres;
    /* We assume Intel byteorder, although the format does not specify
     * any order.  But it was developed in PC context... */
    switch (sdfile->data_type) {
        case SDF_UINT8:
        for (i = 0; i < n; i++)
            data[i] = sdfile->data[i];
        break;

        case SDF_SINT8:
        for (i = 0; i < n; i++)
            data[i] = (signed char)sdfile->data[i];
        break;

        case SDF_UINT16:
        {
            const guint16 *pdata = (const guint16*)(sdfile->data);

            for (i = 0; i < n; i++)
                data[i] = GUINT16_FROM_LE(pdata[i]);
        }
        break;

        case SDF_SINT16:
        {
            const gint16 *pdata = (const gint16*)(sdfile->data);

            for (i = 0; i < n; i++)
                data[i] = GINT16_FROM_LE(pdata[i]);
        }
        break;

        case SDF_UINT32:
        {
            const guint32 *pdata = (const guint32*)(sdfile->data);

            for (i = 0; i < n; i++)
                data[i] = GUINT32_FROM_LE(pdata[i]);
        }
        break;

        case SDF_SINT32:
        {
            const gint32 *pdata = (const gint32*)(sdfile->data);

            for (i = 0; i < n; i++)
                data[i] = GINT32_FROM_LE(pdata[i]);
        }
        break;

        case SDF_FLOAT:
        p = sdfile->data;
        for (i = 0; i < n; i++)
            data[i] = get_FLOAT(&p);
        break;

        case SDF_DOUBLE:
        p = sdfile->data;
        for (i = 0; i < n; i++)
            data[i] = get_DOUBLE(&p);
        break;

        default:
        g_object_unref(dfield);
        g_return_val_if_reached(NULL);
        break;
    }

    return dfield;
}

static GwyDataField*
sdfile_read_data_text(SDFile *sdfile)
{
    gint i, n;
    GwyDataField *dfield;
    gdouble *data;
    gchar *p, *end, *line;

    dfield = GWY_DATA_FIELD(gwy_data_field_new(sdfile->xres, sdfile->yres,
                                               sdfile->xres * sdfile->xscale,
                                               sdfile->yres * sdfile->yscale,
                                               FALSE));
    data = gwy_data_field_get_data(dfield);
    n = sdfile->xres * sdfile->yres;
    switch (sdfile->data_type) {
        case SDF_UINT8:
        case SDF_SINT8:
        case SDF_UINT16:
        case SDF_SINT16:
        case SDF_UINT32:
        case SDF_SINT32:
        p = sdfile->data;
        for (i = 0; i < n; i++) {
            data[i] = strtol(p, (gchar**)&end, 10);
            if (p == end) {
                g_object_unref(dfield);
                g_warning("End of file reached when reading sample #%d of %d",
                          i, n);
                return NULL;
            }
            p = end;
        }
        break;

        case SDF_FLOAT:
        case SDF_DOUBLE:
        p = sdfile->data;
        for (i = 0; i < n; i++) {
            data[i] = g_ascii_strtod(p, (gchar**)&end);
            if (p == end) {
                g_object_unref(dfield);
                g_warning("End of file reached when reading sample #%d of %d",
                          i, n);
                return NULL;
            }
            p = end;
        }
        break;

        default:
        g_return_val_if_reached(NULL);
        break;
    }

    /* Find out if there is anything beyond the end-of-data-marker */
    while (*end && *end != '*')
        end++;
    if (!*end) {
        gwy_debug("Missing end-of-data marker `*' was ignored");
        return dfield;
    }

    do {
        end++;
    } while (g_ascii_isspace(*end));
    if (!*end)
        return dfield;

    /* Read the extra stuff */
    end--;
    sdfile->extras = g_hash_table_new(g_str_hash, g_str_equal);
    while ((line = gwy_str_next_line(&end))) {
        g_strstrip(line);
        if (!*line || *line == ';')
            continue;
        for (p = line; g_ascii_isalnum(*p); p++)
            ;
        if (!*p || (*p != '=' && !g_ascii_isspace(*p)))
            continue;
        *p = '\0';
        p++;
        while (*p == '=' || g_ascii_isspace(*p))
            p++;
        if (!*p)
            continue;
        g_strstrip(p);
        gwy_debug("extra: <%s> = <%s>", line, p);
        g_hash_table_insert(sdfile->extras, line, p);
    }

    return dfield;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

