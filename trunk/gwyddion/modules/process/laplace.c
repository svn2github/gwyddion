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

#include <math.h>
#include <stdio.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/app.h>
#include <app/gwyapp.h>
#include <app/wait.h>

#define LAPLACE_RUN_MODES \
    (GWY_RUN_MODAL | GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)


static gboolean    module_register            (const gchar *name);
static gboolean    laplace                        (GwyContainer *data,
                                               GwyRunType run);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "laplace",
    "Remove data under mask using laplace equation",
    "Petr Klapetek <petr@klapetek.cz>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo laplace_func_info = {
        "laplace",
        "/_Correct data/_Remove data under mask",
        (GwyProcessFunc)&laplace,
        LAPLACE_RUN_MODES,
    };

    gwy_process_func_register(name, &laplace_func_info);

    return TRUE;
}


static gboolean
laplace(GwyContainer *data, GwyRunType run)
{
    GtkWidget *dialog;
    GwyDataField *dfield, *maskfield, *buffer;
    gdouble error, cor, maxer;
    gint i;

    g_assert(run & LAPLACE_RUN_MODES);

    if (gwy_container_contains_by_name(data, "/0/mask"))
    {
        
        dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
        maskfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/mask"));
        buffer = gwy_data_field_new(dfield->xres, dfield->yres, dfield->xreal, dfield->yreal, TRUE);

        gwy_app_undo_checkpoint(data, "/0/data", NULL);
        
        cor = 0.2; 
        error = 0;
        maxer = (gwy_data_field_get_max(dfield) - gwy_data_field_get_min(dfield))/1.0e9;
        gwy_app_wait_start(GTK_WIDGET(gwy_app_data_window_get_current()),"Initializing...");
        
        gwy_data_field_correct_average(dfield, maskfield);
        
        for (i=0; i<10000; i++)
        {
            gwy_data_field_correct_laplace_iteration(dfield, maskfield, buffer,
                                                     &error, &cor);
            gwy_app_wait_set_message("Iterating...");
            if (error < maxer) break;
            if (!gwy_app_wait_set_fraction(i/(gdouble)(10000))) break;
        }
        printf("%d iterations\n", i);
        gwy_app_wait_finish();
        
        gwy_container_remove_by_name(data, "/0/mask");
        g_object_unref(buffer);
    }
    else
    {
        dialog = gtk_message_dialog_new(GTK_WINDOW(gwy_app_data_window_get_current()),
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_INFO,
                                        GTK_BUTTONS_CLOSE,
                                        _("There is no mask to be used for computation."));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return FALSE;
        
    }
    return TRUE;
}



/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
