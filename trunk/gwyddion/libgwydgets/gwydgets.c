/* @(#) $Id$ */

#include <string.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkimagemenuitem.h>

#include <libgwyddion/gwymacros.h>
#include <libdraw/gwypalette.h>
#include "gwydgets.h"

#define BITS_PER_SAMPLE 8
#define PALETTE_SAMPLE_HEIGHT 16
#define PALETTE_SAMPLE_WIDTH 80

static GtkWidget*
gwy_sample_palette_to_gtkimage(GwyPaletteDef *palette_def)
{
    static GwyPalette *palette = NULL;
    static guchar *samples = NULL;
    GdkPixbuf *pixbuf;
    GtkWidget *image;
    guint rowstride;
    guchar *data;
    gint i;

    if (!palette_def) {
        g_free(samples);
        gwy_object_unref(palette);
        return NULL;
    }

    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, 0, BITS_PER_SAMPLE,
                            PALETTE_SAMPLE_WIDTH, PALETTE_SAMPLE_HEIGHT);
    rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    data = gdk_pixbuf_get_pixels(pixbuf);

    if (!palette)
        palette = (GwyPalette*)gwy_palette_new(palette_def);
    else
        gwy_palette_set_palette_def(palette, palette_def);

    samples = gwy_palette_sample(palette, PALETTE_SAMPLE_WIDTH, samples);
    for (i = 0; i < PALETTE_SAMPLE_WIDTH; i++)
        memcpy(data + 3*i, samples + 4*i, 3);
    for (i = 1; i < PALETTE_SAMPLE_HEIGHT; i++)
        memcpy(data + i*rowstride, data, 3*PALETTE_SAMPLE_WIDTH);
    gwy_object_unref(palette);

    image = gtk_image_new_from_pixbuf(pixbuf);
    return image;
}

static void
gwy_hash_table_to_slist_cb(gpointer key,
                           gpointer value,
                           gpointer user_data)
{
    GSList **list = (GSList**)user_data;

    *list = g_slist_prepend(*list, value);
}

static gint
palette_def_compare(GwyPaletteDef *a,
                    GwyPaletteDef *b)
{
    /* XXX: should use gwy_palette_def_get_name() */
    return strcmp(a->name, b->name);
}

GtkWidget*
gwy_palette_option_menu(GCallback callback,
                        gpointer cbdata,
                        const gchar *current)
{
    GSList *l, *entries = NULL;
    GtkWidget *omenu, *menu, *image, *item;
    gint i, index;

    gwy_palette_def_foreach((GwyPaletteDefFunc)gwy_hash_table_to_slist_cb,
                            &entries);
    entries = g_slist_sort(entries, (GCompareFunc)palette_def_compare);

    omenu = gtk_option_menu_new();
    menu = gtk_menu_new();

    index = -1;
    i = 0;
    for (l = entries; l; l = g_slist_next(l)) {
        GwyPaletteDef *palette_def = (GwyPaletteDef*)l->data;
        const gchar *name = gwy_palette_def_get_name(palette_def);

        image = gwy_sample_palette_to_gtkimage(palette_def);
        item = gtk_image_menu_item_new_with_label(name);
        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
        g_object_set_data(G_OBJECT(item), "palette-name", name);
        if (callback)
            g_signal_connect(G_OBJECT(item), "activate", callback, cbdata);
        if (current && strcmp(current, name) == 0)
            index = i;
        i++;
    }
    gwy_sample_palette_to_gtkimage(NULL);
    g_slist_free(entries);

    gtk_option_menu_set_menu(GTK_OPTION_MENU(omenu), menu);
    if (index != -1)
        gtk_option_menu_set_history(GTK_OPTION_MENU(omenu), index);

    return omenu;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
