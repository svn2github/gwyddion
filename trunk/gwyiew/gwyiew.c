/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>

#include <libgwymodule/gwymodule.h>
#include <libgwyddion/gwyddion.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>

static void print_help(void);
static void process_preinit_options(int *argc,
                                    char ***argv);

int
main(int argc, char *argv[])
{
    gchar *settings_file;

    process_preinit_options(&argc, &argv);

    gtk_init(&argc, &argv);

    settings_file = gwy_app_settings_get_settings_filename();
    if (g_file_test(settings_file, G_FILE_TEST_IS_REGULAR))
        gwy_app_settings_load(settings_file);

    gwy_module_register_module(GWY_MODULE_DIR "/file/gwyfile.so");

    /*gwy_app_file_open_initial(argv + 1, argc - 1);*/

    gtk_main();
    g_free(settings_file);

    return 0;
}

static void
process_preinit_options(int *argc,
                        char ***argv)
{
    if (*argc == 1)
        return;

    if (!strcmp((*argv)[1], "--help") || !strcmp((*argv)[1], "-h")) {
        print_help();
        exit(0);
    }

    if (!strcmp((*argv)[1], "--version") || !strcmp((*argv)[1], "-v")) {
        printf("%s %s\n", PACKAGE, VERSION);
        exit(0);
    }
}

static void
print_help(void)
{
    puts(
"Usage: gwyddion [OPTIONS...] FILE...\n"
"An SPM data analysis framework, written in Gtk+.\n"
        );
    puts(
"Gwyddion options:\n"
" -h, --help                 Print this help and terminate.\n"
" -v, --version              Print version info and terminate.\n"
"     --no-splash            Don't show splash screen.\n"
        );
    puts(
"Gtk+ and Gdk options:\n"
"     --display=DISPLAY      Set X display to use.\n"
"     --screen=SCREEN        Set X screen to use.\n"
"     --sync                 Make X calls synchronous.\n"
"     --name=NAME            Set program name as used by the window manager.\n"
"     --class=CLASS          Set program class as used by the window manager.\n"
"     --gtk-module=MODULE    Load an additional Gtk module MODULE.\n"
"They may be other Gtk+ and Gdk options, depending on platform, how it was\n"
"compiled, and loaded modules.  Please see Gtk+ documentation.\n"
        );
    puts("Please report bugs in Gwyddion bugzilla "
         "http://trific.ath.cx/bugzilla/");
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

