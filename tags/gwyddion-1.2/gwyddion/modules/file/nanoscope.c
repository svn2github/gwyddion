/*
 *  @(#) $Id$
 *  Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule.h>
#include <libgwydgets/gwydgets.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#define MAGIC_BIN "\\*File list\r\n"
#define MAGIC_TXT "?*File list\r\n"
#define MAGIC_SIZE (sizeof(MAGIC_TXT)-1)

typedef enum {
    NANOSCOPE_FILE_TYPE_NONE = 0,
    NANOSCOPE_FILE_TYPE_BIN,
    NANOSCOPE_FILE_TYPE_TXT
} NanoscopeFileType;

typedef struct {
    GHashTable *hash;
    GwyDataField *data_field;
} NanoscopeData;

static gboolean       module_register     (const gchar *name);
static gint           nanoscope_detect    (const gchar *filename,
                                           gboolean only_name);
static GwyContainer*  nanoscope_load      (const gchar *filename);
static GwyDataField*  hash_to_data_field  (GHashTable *hash,
                                           NanoscopeFileType file_type,
                                           gsize bufsize,
                                           gchar *buffer,
                                           gdouble zscale,
                                           gdouble curscale,
                                           gchar **p);
static NanoscopeData* select_which_data   (GList *list);
static gboolean       read_ascii_data     (gint n,
                                           gdouble *data,
                                           gchar **buffer,
                                           gint bpp);
static gboolean       read_binary_data    (gint n,
                                           gdouble *data,
                                           gchar *buffer,
                                           gint bpp);
static GHashTable*    read_hash           (gchar **buffer);
static gchar*         next_line           (gchar **buffer);

static void           get_value_scales    (GHashTable *hash,
                                           gdouble *zscale,
                                           gdouble *curscale);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "nanoscope",
    "Load Nanoscope data.",
    "Yeti <yeti@gwyddion.net>",
    "0.4",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyFileFuncInfo nanoscope_func_info = {
        "nanoscope",
        "Nanoscope files",
        (GwyFileDetectFunc)&nanoscope_detect,
        (GwyFileLoadFunc)&nanoscope_load,
        NULL,
    };

    gwy_file_func_register(name, &nanoscope_func_info);

    return TRUE;
}

static gint
nanoscope_detect(const gchar *filename,
                 gboolean only_name)
{
    FILE *fh;
    gint score;
    gchar magic[MAGIC_SIZE];

    if (only_name)
        return 0;

    if (!(fh = fopen(filename, "rb")))
        return 0;
    score = 0;
    if (fread(magic, 1, MAGIC_SIZE, fh) == MAGIC_SIZE
        && (memcmp(magic, MAGIC_TXT, MAGIC_SIZE) == 0
            || memcmp(magic, MAGIC_BIN, MAGIC_SIZE) == 0))
        score = 100;
    fclose(fh);

    return score;
}

static GwyContainer*
nanoscope_load(const gchar *filename)
{
    GObject *object = NULL;
    GError *err = NULL;
    gchar *buffer = NULL;
    gchar *p;
    gsize size = 0;
    NanoscopeFileType file_type;
    NanoscopeData *ndata;
    GHashTable *hash;
    GList *l, *list = NULL;
    /* FIXME defaults */
    gdouble zscale = 9.583688e-9;
    gdouble curscale = 10.0e-9;
    gboolean ok;

    if (!g_file_get_contents(filename, &buffer, &size, &err)) {
        g_warning("Cannot read file %s", filename);
        g_clear_error(&err);
        return NULL;
    }
    file_type = NANOSCOPE_FILE_TYPE_NONE;
    if (size > MAGIC_SIZE) {
        if (!memcmp(buffer, MAGIC_TXT, MAGIC_SIZE))
            file_type = NANOSCOPE_FILE_TYPE_TXT;
        else if (!memcmp(buffer, MAGIC_BIN, MAGIC_SIZE))
            file_type = NANOSCOPE_FILE_TYPE_BIN;
    }
    if (!file_type) {
        g_warning("File %s doesn't seem to be a nanoscope file", filename);
        g_free(buffer);
        return NULL;
    }
    gwy_debug("File type: %d", file_type);
    /* as already know file_type, fix the first char for hash reading */
    *buffer = '\\';

    p = buffer;
    while ((hash = read_hash(&p))) {
        ndata = g_new0(NanoscopeData, 1);
        ndata->hash = hash;
        list = g_list_append(list, ndata);
    }
    ok = TRUE;
    for (l = list; ok && l; l = g_list_next(l)) {
        ndata = (NanoscopeData*)l->data;
        hash = ndata->hash;
        if (strcmp(g_hash_table_lookup(hash, "#self"), "Scanner list") == 0) {
            get_value_scales(hash, &zscale, &curscale);
            continue;
        }
        if (strcmp(g_hash_table_lookup(hash, "#self"), "Ciao image list"))
            continue;

        ndata->data_field = hash_to_data_field(hash, file_type,
                                               size, buffer,
                                               zscale, curscale, &p);
        ok = ok && ndata->data_field;
    }

    /* select (let user select) which data to load */
    ndata = ok ? select_which_data(list) : NULL;
    if (ndata) {
        object = gwy_container_new();
        gwy_container_set_object_by_name(GWY_CONTAINER(object), "/0/data",
                                         G_OBJECT(ndata->data_field));
    }

    /* unref all data fields, the container already keeps a reference to the
     * right one */
    g_free(buffer);
    for (l = list; l; l = g_list_next(l)) {
        ndata = (NanoscopeData*)l->data;
        g_hash_table_destroy(ndata->hash);
        gwy_object_unref(ndata->data_field);
        g_free(ndata);
    }
    g_list_free(list);

    return (GwyContainer*)object;
}

static void
get_value_scales(GHashTable *hash,
                 gdouble *zscale,
                 gdouble *curscale)
{
    gchar *s, un[6];

    /* z sensitivity */
    if (!(s = g_hash_table_lookup(hash, "@Sens. Zscan"))) {
        g_warning("`@Sens. Zscan' not found");
        return;
    }
    if (s[0] != 'V' || s[1] != ' '
        || sscanf(s+2, "%lf %5s", zscale, un) != 2) {
        g_warning("Cannot parse `@Sens. Zscan': <%s>", s+2);
        return;
    }
    if (strcmp(un, "nm/V") == 0)
        *zscale /= 1e9;
    else if (strcmp(un, "~m/V") == 0 || strcmp(un, "um/V") == 0)
        *zscale /= 1e6;
    else {
        g_warning("Cannot understand z units: <%s>", un);
        *zscale /= 1e9;
    }

    /* current sensitivity */
    if (!(s = g_hash_table_lookup(hash, "@Sens. Current"))) {
        g_warning("`@Sens. Current' not found");
        return;
    }
    if (s[0] != 'V' || s[1] != ' '
        || sscanf(s+2, "%lf %5s", curscale, un) != 2) {
        g_warning("Cannot parse `@Sens. Current': <%s>", s+2);
        return;
    }
    if (strcmp(un, "pA/V") == 0)
        *curscale /= 1e12;
    if (strcmp(un, "nA/V") == 0)
        *curscale /= 1e9;
    else if (strcmp(un, "~A/V") == 0 || strcmp(un, "uA/V") == 0)
        *curscale /= 1e6;
    else {
        g_warning("Cannot understand z units: <%s>", un);
        *curscale /= 1e9;
    }
}

static GwyDataField*
hash_to_data_field(GHashTable *hash,
                   NanoscopeFileType file_type,
                   gsize bufsize,
                   gchar *buffer,
                   gdouble zscale,
                   gdouble curscale,
                   gchar **p)
{
    GwyDataField *dfield;
    const gchar *s;
    gchar *t;
    gchar un[5];
    gint xres, yres, bpp, offset, size;
    gdouble xreal, yreal, q, zmagnify = 1.0, zscalesens = 1.0;
    gdouble *data;
    gboolean is_current;  /* assume height if not current */

    if (!(s = g_hash_table_lookup(hash, "@2:Image Data"))) {
        g_warning("No `@2 Image Data' found");
        return NULL;
    }
    is_current = strstr(s, "[Current]") || strstr(s, "\"Current\"");
    gwy_debug("is current = %d", is_current);

    if (!(s = g_hash_table_lookup(hash, "Samps/line"))) {
        g_warning("`Samps/line' not found");
        return NULL;
    }
    xres = atoi(s);

    if (!(s = g_hash_table_lookup(hash, "Number of lines"))) {
        g_warning("`Number of lines' not found");
        return NULL;
    }
    yres = atoi(s);

    if (!(s = g_hash_table_lookup(hash, "Bytes/pixel"))) {
        g_warning("`Bytes/pixel' not found");
        return NULL;
    }
    bpp = atoi(s);

    /* scan size */
    if (!(s = g_hash_table_lookup(hash, "Scan size"))) {
        g_warning("`Scan size' not found");
        return NULL;
    }
    if (sscanf(s, "%lf %lf %4s", &xreal, &yreal, un) != 3) {
        g_warning("Cannot parse `Scan size': <%s>", s);
        return NULL;
    }
    if (strcmp(un, "nm") == 0) {
        xreal /= 1e9;
        yreal /= 1e9;
    }
    else if (strcmp(un, "~m") == 0 || strcmp(un, "um") == 0) {
        xreal /= 1e6;
        yreal /= 1e6;
    }
    else {
        g_warning("Cannot understand size units: <%s>", un);
        xreal /= 1e9;
        yreal /= 1e9;
    }

    offset = size = 0;
    if (file_type == NANOSCOPE_FILE_TYPE_BIN) {
        if (!(s = g_hash_table_lookup(hash, "Data offset"))) {
            g_warning("`Data offset' not found");
            return NULL;
        }
        offset = atoi(s);

        if (!(s = g_hash_table_lookup(hash, "Data length"))) {
            g_warning("`Data length' not found");
            return NULL;
        }
        size = atoi(s);
        if (size != bpp*xres*yres) {
            g_warning("Data size %d != %d bpp*xres*yres", size, bpp*xres*yres);
            return NULL;
        }

        if (offset + size > (gint)bufsize) {
            g_warning("Data don't fit to the file");
            return NULL;
        }
    }

    /* XXX: now ignored */
    if (!(t = g_hash_table_lookup(hash, "@Z magnify")))
        g_warning("`@Z magnify' not found");
    else {
        if (!(s = strchr(t, ']')))
            g_warning("Cannot parse `@Z magnify': <%s>", t);
        else {
            zmagnify = strtod(s+1, &t);
            if (t == s+1) {
                g_warning("Cannot parse `@Z magnify' value: <%s>", s+1);
                zmagnify = 1.0;
            }
        }
    }

    if (!(t = g_hash_table_lookup(hash, "@2:Z scale")))
        g_warning("`@2:Z scale' not found");
    else {
        if (!(s = strchr(t, '(')))
            g_warning("Cannot parse `@2:Z scale': <%s>", t);
        else {
            zscalesens = strtod(s+1, &t);
            if (t == s+1) {
                g_warning("Cannot parse `@2:Z scale' value: <%s>", s+1);
                zscalesens = 1.0;
            }
        }
    }

    dfield = GWY_DATA_FIELD(gwy_data_field_new(xres, yres, xreal, yreal,
                                               FALSE));
    data = gwy_data_field_get_data(dfield);
    switch (file_type) {
        case NANOSCOPE_FILE_TYPE_TXT:
        if (!read_ascii_data(xres*yres, data, p, bpp)) {
            g_object_unref(dfield);
            return NULL;
        }
        break;

        case NANOSCOPE_FILE_TYPE_BIN:
        if (!read_binary_data(xres*yres, data, buffer + offset, bpp)) {
            g_object_unref(dfield);
            return NULL;
        }
        break;

        default:
        g_assert_not_reached();
        break;
    }
    /*q = (is_current ? curscale : zscale) * zscalesens/zmagnify*10;*/
    q = (is_current ? curscale : zscale) * zscalesens*10;
    gwy_debug("curscale = %fe-9, zscale = %fe-9, zscalesens = %f, zmagnify = %f",
              curscale*1e9, zscale*1e9, zscalesens, zmagnify);
    gwy_data_field_multiply(dfield, q);
    gwy_data_field_set_si_unit_xy(dfield, GWY_SI_UNIT(gwy_si_unit_new("m")));
    gwy_data_field_set_si_unit_z(dfield,
                                 GWY_SI_UNIT(gwy_si_unit_new(is_current
                                                             ? "A" : "m")));

    return dfield;
}

static NanoscopeData*
select_which_data(GList *list)
{
    NanoscopeData *ndata, *ndata0;
    GtkWidget *dialog, *omenu, *menu, *label, *table;
    GObject *item;
    GwyEnum *choices;
    GList *l;
    gint i, count, response;

    count = 0;
    ndata0 = NULL;
    l = NULL;
    while (list) {
        ndata = (NanoscopeData*)list->data;
        if (ndata->data_field) {
            count++;
            l = g_list_append(l, ndata);
            if (!ndata0)
                ndata0 = ndata;
        }
        list = g_list_next(list);
    }
    list = l;
    if (count == 0 || count == 1) {
        g_list_free(list);
        return ndata0;
    }

    choices = g_new(GwyEnum, count);
    i = 0;
    for (l = list; l; l = g_list_next(l)) {
        ndata = (NanoscopeData*)l->data;
        choices[i].name = g_hash_table_lookup(ndata->hash, "@2:Image Data");
        if (choices[i].name[0] && choices[i].name[1])
            choices[i].name += 2;
        choices[i].value = i;
        i++;
    }

    dialog = gtk_dialog_new_with_buttons(_("Select data"), NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    table = gtk_table_new(2, 1, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 6);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 0);

    label = gtk_label_new(_("Data to load:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    omenu = gwy_option_menu_create(choices, count, "data", NULL, NULL, -1);
    gtk_table_attach(GTK_TABLE(table), omenu, 0, 1, 1, 2,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    gtk_widget_show_all(dialog);
    gtk_window_present(GTK_WINDOW(dialog));
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            g_free(choices);
            g_list_free(list);
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(omenu));
    item = G_OBJECT(gtk_menu_get_active(GTK_MENU(menu)));
    response = GPOINTER_TO_INT(g_object_get_data(item, "data"));
    gtk_widget_destroy(dialog);

    l = g_list_nth(list, response);
    ndata0 = (NanoscopeData*)l->data;

    g_free(choices);
    g_list_free(list);

    return ndata0;
}

static gboolean
read_ascii_data(gint n, gdouble *data,
                gchar **buffer,
                gint bpp)
{
    gint i;
    gdouble q;
    gchar *end;
    long l, min, max;

    q = 1.0/(1 << (8*bpp));
    min = 10000000;
    max = -10000000;
    for (i = 0; i < n; i++) {
        /*data[i] = q*strtol(*buffer, &end, 10);*/
        l = strtol(*buffer, &end, 10);
        min = MIN(l, min);
        max = MAX(l, max);
        data[i] = q*l;
        if (end == *buffer) {
            g_warning("Garbage after data sample #%d", i);
            return FALSE;
        }
        *buffer = end;
    }
    gwy_debug("min = %ld, max = %ld", min, max);
    return TRUE;
}

static gboolean
read_binary_data(gint n, gdouble *data,
                 gchar *buffer,
                 gint bpp)
{
    gint i;
    gdouble q;

    q = 1.0/(1 << (8*bpp));
    switch (bpp) {
        case 1:
        for (i = 0; i < n; i++)
            data[i] = q*buffer[i];
        break;

        case 2:
        {
            gint16 *p = (gint16*)buffer;

            for (i = 0; i < n; i++)
                data[i] = q*p[i];
        }
        break;

        case 4:
        {
            gint32 *p = (gint32*)buffer;

            for (i = 0; i < n; i++)
                data[i] = q*p[i];
        }
        break;

        default:
        g_warning("bpp = %d unimplemented", bpp);
        return FALSE;
        break;
    }

    return TRUE;
}

static GHashTable*
read_hash(gchar **buffer)
{
    GHashTable *hash;
    gchar *line, *colon;

    line = next_line(buffer);
    if (line[0] != '\\' || line[1] != '*')
        return NULL;
    if (!strcmp(line, "\\*File list end")) {
        gwy_debug("FILE LIST END");
        return NULL;
    }

    hash = g_hash_table_new(g_str_hash, g_str_equal);
    g_hash_table_insert(hash, "#self", line + 2);    /* self */
    gwy_debug("hash table <%s>", line + 2);
    while ((*buffer)[0] == '\\' && (*buffer)[1] && (*buffer)[1] != '*') {
        line = next_line(buffer) + 1;
        if (!line || !line[0] || !line[1] || !line[2])
            goto fail;
        colon = strchr(line + 3, ':');
        if (!colon || !isspace(colon[1]))
            goto fail;
        *colon = '\0';
        g_hash_table_insert(hash, line, colon + 2);
        gwy_debug("<%s>: <%s>", line, colon + 2);
    }

    return hash;

fail:
    g_hash_table_destroy(hash);
    return NULL;
}


/**
 * next_line:
 * @buffer: A character buffer containing some text.
 *
 * Extracts a next line from @buffer.
 *
 * @buffer is updated to point after the end of the line and the "\n" 
 * (or "\r\n") is replaced with "\0", if present.
 *
 * Returns: The start of the line.  %NULL if the buffer is empty or %NULL.
 *          The line is not duplicated, the returned pointer points somewhere
 *          to @buffer.
 **/
static gchar*
next_line(gchar **buffer)
{
    gchar *p, *q;

    if (!buffer || !*buffer)
        return NULL;

    q = *buffer;
    p = strchr(*buffer, '\n');
    if (p) {
        if (p > *buffer && *(p-1) == '\r')
            *(p-1) = '\0';
        *buffer = p+1;
        *p = '\0';
    }
    else
        *buffer = NULL;

    return q;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

