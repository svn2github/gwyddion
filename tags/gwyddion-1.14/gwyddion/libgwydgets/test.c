/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include <libgwydgets/gwygrapher.h>
#include <libgwydgets/gwygraphermodel.h>
#include <libgwydgets/gwygraphercurvemodel.h>


static void destroy( GtkWidget *widget, gpointer data )
{
        gtk_main_quit ();
}

int
main(int argc, char *argv[])
{
    GtkWidget *window;
    GtkWidget *axis, *label, *area, *graph, *foo;
    GObject *gmodel;
    GwyGrapherCurveModel *model;
    gint i;
    GString *str1, *str2, *str3, *str4, *str5;
    GwyDataLine *dln;

    double xs[100];
    double ys[100];
    double xr[10];
    double yr[10];
    double xp[100];
    double yp[100];
    double xu[10];
    double yu[10];
    double xv[20];
    double yv[20];
         
    for (i=0; i<100; i++){xs[i]=i-7; xp[i]=i; ys[i]= 100 + (double)i*i/40; 
        yp[i]=50 + 20*sin((double)i*15/100);
        
        if (i<20) {
            xv[i]=5.0*i + 12;
            yv[i]=20*sin((double)i*5.0*15/100)-15*cos((double)(i*5.0-3)*15/100) - 30;
           }
        if (i<10) {
            xr[i]=20+i*3;
            yr[i]=150+4*i;
            xu[i]=20+i*7;
            yu[i]=50 - (double)i*4;
           }
        }
    
    gtk_init(&argc, &argv);
    
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

    g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (destroy), NULL);

    gtk_container_set_border_width (GTK_CONTAINER (window), 0);
 
    /*
    label = gwy_graph_label_new();
    gtk_container_add (GTK_CONTAINER (window), label);
    gtk_widget_show (label);
    */

    /*
    area = gwy_graph_area_new(NULL,NULL);
    gtk_layout_set_size(GTK_LAYOUT(area), 320, 240);
    gtk_container_add (GTK_CONTAINER (window), area);

    foo = gtk_label_new("Foo!");
    gtk_layout_put(GTK_LAYOUT(area), foo, 10, 20);
    */

  
    dln = (GwyDataLine *) gwy_data_line_new(200, 200, 1);
    
    str1 = g_string_new("parabola");
    str2 = g_string_new("kousek");
    str3 = g_string_new("sinus");
    str4 = g_string_new("cosi");
    str5 = g_string_new("jiny sinus");

    gmodel = gwy_grapher_model_new(NULL);
    graph = gwy_grapher_new(gmodel);
    
    model = gwy_grapher_curve_model_new();
    model->xdata = xp;
    model->ydata = yp;
    model->description = str1;
    model->n = 100;
    gwy_grapher_model_add_curve(gmodel, model);
    
    model->xdata = xp;
    model->ydata = ys;
    model->description = str2;
    model->n = 100;
     
    gtk_container_add (GTK_CONTAINER (window), graph);
    gtk_widget_show (graph);

    gtk_widget_show_all(window);

    gwy_grapher_model_add_curve(gmodel, model);
    gtk_main();

    return 0;
}
/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
