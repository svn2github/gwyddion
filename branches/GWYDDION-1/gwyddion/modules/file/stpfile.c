/*
 *  $Id$
 *  Copyright (C) 2005 David Necas (Yeti), Petr Klstptek.
 *  E-mail: yeti@gwyddion.net, klstptek@gwyddion.net.
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
#include <libgwyddion/gwyutils.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "get.h"

#define MAGIC "UK SOFT\r\n"
#define MAGIC_SIZE (sizeof(MAGIC) - 1)

#define DATA_MAGIC "Data_section  \r\n"
#define DATA_MAGIC_SIZE (sizeof(DATA_MAGIC) - 1)

#define KEY_LEN 14

typedef struct {
    gint id;
    gint xres;
    gint yres;
    const guint16 *data;
    GHashTable *meta;
} STPData;

typedef struct {
    guint n;
    STPData *buffers;
    GHashTable *meta;
} STPFile;

typedef struct {
    STPFile *file;
    GwyContainer *data;
    GtkWidget *data_view;
} STPControls;

static gboolean      module_register       (const gchar *name);
static gint          stpfile_detect        (const gchar *filename,
                                            gboolean only_name);
static GwyContainer* stpfile_load          (const gchar *filename);
static guint         find_data_start       (const guchar *buffer,
                                            gsize size);
static void          stpfile_free          (STPFile *stpfile);
static void          read_data_field       (GwyDataField *dfield,
                                            STPData *stpdata);
static guint         file_read_header      (STPFile *stpfile,
                                            gchar *buffer);
static void          store_metadata        (GHashTable *meta,
                                            GwyContainer *container);
static guint         select_which_data     (STPFile *stpfile);
static void          selection_changed     (GtkWidget *button,
                                            STPControls *controls);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "stpfile",
    N_("Import Molecular Imaging STP data files."),
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
    static GwyFileFuncInfo stpfile_func_info = {
        "stpfile",
        N_("STP files (.stp)"),
        (GwyFileDetectFunc)&stpfile_detect,
        (GwyFileLoadFunc)&stpfile_load,
        NULL
    };

    gwy_file_func_register(name, &stpfile_func_info);

    return TRUE;
}

static gint
stpfile_detect(const gchar *filename, gboolean only_name)
{
    gint score = 0;
    FILE *fh;
    gchar magic[MAGIC_SIZE];

    if (only_name) {
        if (g_str_has_suffix(filename, ".stp"))
            score = 20;

        return score;
    }

    if (!(fh = fopen(filename, "rb")))
        return 0;

    if (fread(magic, 1, MAGIC_SIZE, fh) == MAGIC_SIZE
        && !memcmp(magic, MAGIC, MAGIC_SIZE))
        score = 100;
    fclose(fh);

    return score;
}

static GwyContainer*
stpfile_load(const gchar *filename)
{
    STPFile *stpfile;
    GObject *object = NULL;
    guchar *buffer = NULL;
    gsize size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    guint header_size;
    gchar *p;
    gboolean ok;
    guint i, pos;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        g_warning("Cannot read file %s", filename);
        g_clear_error(&err);
        return NULL;
    }
    if (strncmp(buffer, MAGIC, MAGIC_SIZE)
        || size <= DATA_MAGIC_SIZE
        || !(header_size = find_data_start(buffer, size))) {
        g_warning("File %s is not a STP file", filename);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    stpfile = g_new0(STPFile, 1);
    p = g_strndup(buffer, header_size);
    ok = file_read_header(stpfile, p);
    g_free(p);

    /* TODO: check size */
    if (ok) {
        pos = header_size;
        for (i = 0; i < stpfile->n; i++) {
            stpfile->buffers[i].data = (const guint16*)(buffer + pos);
            gwy_debug("buffer %i pos: %u", i, pos);
            pos += 2*stpfile->buffers[i].xres * stpfile->buffers[i].yres;
        }

        i = select_which_data(stpfile);
        if (i == (guint)-1)
            ok = FALSE;
    }

    if (ok) {
        dfield = GWY_DATA_FIELD(gwy_data_field_new(stpfile->buffers[i].xres,
                                                   stpfile->buffers[i].yres,
                                                   1.0, 1.0, FALSE));
        read_data_field(dfield, stpfile->buffers + i);
    }
    gwy_file_abandon_contents(buffer, size, NULL);

    if (dfield) {
        object = gwy_container_new();
        gwy_container_set_object_by_name(GWY_CONTAINER(object), "/0/data",
                                         G_OBJECT(dfield));
        /*store_metadata(meta, GWY_CONTAINER(object));*/
    }
    stpfile_free(stpfile);

    return (GwyContainer*)object;
}

static guint
find_data_start(const guchar *buffer,
                gsize size)
{
    const guchar *p;

    size -= DATA_MAGIC_SIZE;

    for (p = buffer;
         p && strncmp(p, DATA_MAGIC, DATA_MAGIC_SIZE);
         p = memchr(p+1, 'D', size - (p - buffer) - 1))
        ;

    return p ? (p - buffer) + DATA_MAGIC_SIZE : 0;
}

static void
stpfile_free(STPFile *stpfile)
{
    guint i;

    for (i = 0; i < stpfile->n; i++)
        g_hash_table_destroy(stpfile->buffers[i].meta);

    g_free(stpfile->buffers);
    g_hash_table_destroy(stpfile->meta);
    g_free(stpfile);
}

static guint
file_read_header(STPFile *stpfile,
                 gchar *buffer)
{
    STPData *data = NULL;
    GHashTable *meta;
    gchar *line, *key, *value = NULL;

    stpfile->meta = g_hash_table_new_full(g_str_hash, g_str_equal,
                                          g_free, g_free);
    meta = stpfile->meta;
    while ((line = gwy_str_next_line(&buffer))) {
        if (!strncmp(line, "buffer_id     ", KEY_LEN)) {
            gwy_debug("buffer id = %s", line + KEY_LEN);
            stpfile->n++;
            stpfile->buffers = g_renew(STPData, stpfile->buffers, stpfile->n);
            data = stpfile->buffers + (stpfile->n - 1);
            data->meta = g_hash_table_new_full(g_str_hash, g_str_equal,
                                               g_free, g_free);
            data->data = NULL;
            data->xres = data->yres = 0;
            data->id = atol(line + KEY_LEN);
            meta = data->meta;
        }
        key = g_strstrip(g_strndup(line, KEY_LEN));
        value = g_strstrip(g_strdup(line + KEY_LEN));
        g_hash_table_replace(meta, key, value);

        if (data) {
            if (!strcmp(key, "samples_x"))
                data->xres = atol(value);
            if (!strcmp(key, "samples_y"))
                data->yres = atol(value);
        }
    }

    return stpfile->n;
}

#if 0
static void
process_metadata(GHashTable *meta,
                 GwyContainer *container)
{
    GwyDataField *dfield;
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

static void
read_data_field(GwyDataField *dfield,
                STPData *stpdata)
{
    gdouble *data;
    guint i;

    gwy_data_field_resample(dfield, stpdata->xres, stpdata->yres,
                            GWY_INTERPOLATION_NONE);
    data = gwy_data_field_get_data(dfield);
    for (i = 0; i < stpdata->xres * stpdata->yres; i++)
        data[i] = GUINT16_FROM_LE(stpdata->data[i]);
}

static guint
select_which_data(STPFile *stpfile)
{
    STPControls controls;
    GtkWidget *dialog, *label, *vbox, *hbox, *align;
    GwyDataField *dfield;
    GwyEnum *choices;
    GtkObject *layer;
    GSList *radio, *rl;
    guint i, b;

    if (!stpfile->n)
        return (guint)-1;

    if (stpfile->n == 1)
        return 0;

    controls.file = stpfile;
    choices = g_new(GwyEnum, stpfile->n);
    for (i = 0; i < stpfile->n; i++) {
        choices[i].value = i;
        choices[i].name = g_strdup_printf(_("Buffer %u"),
                                          stpfile->buffers[i].id);
    }

    dialog = gtk_dialog_new_with_buttons(_("Select Data"), NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    hbox = gtk_hbox_new(FALSE, 20);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 6);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, TRUE, TRUE, 0);

    align = gtk_alignment_new(0.0, 0.0, 0.0, 0.0);
    gtk_box_pack_start(GTK_BOX(hbox), align, TRUE, TRUE, 0);

    vbox = gtk_vbox_new(TRUE, 0);
    gtk_container_add(GTK_CONTAINER(align), vbox);

    label = gtk_label_new(_("Data to load:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);

    radio = gwy_radio_buttons_create(choices, stpfile->n, "data",
                                     G_CALLBACK(selection_changed), &controls,
                                     0);
    for (i = 0, rl = radio; rl; i++, rl = g_slist_next(rl))
        gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(rl->data), TRUE, TRUE, 0);

    /* preview */
    align = gtk_alignment_new(1.0, 0.0, 0.0, 0.0);
    gtk_box_pack_start(GTK_BOX(hbox), align, TRUE, TRUE, 0);

    controls.data = GWY_CONTAINER(gwy_container_new());
    dfield = GWY_DATA_FIELD(gwy_data_field_new(stpfile->buffers[0].xres,
                                               stpfile->buffers[0].yres,
                                               1.0, 1.0, FALSE));
    read_data_field(dfield, stpfile->buffers);
    gwy_container_set_object_by_name(controls.data, "/0/data",
                                     (GObject*)dfield);
    g_object_unref(dfield);

    controls.data_view = gwy_data_view_new(controls.data);
    g_object_unref(controls.data);
    gwy_data_view_set_zoom(GWY_DATA_VIEW(controls.data_view),
                           120.0/MAX(stpfile->buffers[0].xres,
                                     stpfile->buffers[0].yres));
    layer = gwy_layer_basic_new();
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.data_view),
                                 GWY_PIXMAP_LAYER(layer));
    gtk_container_add(GTK_CONTAINER(align), controls.data_view);

    gtk_widget_show_all(dialog);
    gtk_window_present(GTK_WINDOW(dialog));
    switch (gtk_dialog_run(GTK_DIALOG(dialog))) {
        case GTK_RESPONSE_CANCEL:
        case GTK_RESPONSE_DELETE_EVENT:
        gtk_widget_destroy(dialog);
        case GTK_RESPONSE_NONE:
        b = (guint)-1;
        break;

        case GTK_RESPONSE_OK:
        b = GPOINTER_TO_UINT(gwy_radio_buttons_get_current(radio, "data"));
        gtk_widget_destroy(dialog);
        break;

        default:
        g_assert_not_reached();
        break;
    }

    for (i = 0; i < stpfile->n; i++)
        g_free((gpointer)choices[i].name);
    g_free(choices);

    return b;
}

static void
selection_changed(GtkWidget *button,
                  STPControls *controls)
{
    STPFile *stpfile;
    GwyDataField *dfield;
    guint i;

    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
        return;

    i = gwy_radio_buttons_get_current_from_widget(button, "data");
    g_assert(i != (guint)-1);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->data,
                                                             "/0/data"));
    stpfile = controls->file;
    read_data_field(dfield, stpfile->buffers + i);

    gwy_data_view_update(GWY_DATA_VIEW(controls->data_view));
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

