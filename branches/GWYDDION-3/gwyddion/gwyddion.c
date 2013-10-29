#include <stdlib.h>
#include <gtk/gtk.h>
#include "libgwy/libgwy.h"
#include "libgwyui/libgwyui.h"
#include "libgwyapp/libgwyapp.h"

G_GNUC_NORETURN
static gboolean
print_version(G_GNUC_UNUSED const gchar *name,
              G_GNUC_UNUSED const gchar *value,
              G_GNUC_UNUSED gpointer data,
              G_GNUC_UNUSED GError **error)
{
    g_print("Gwyddion %s (running with libgwy %s).\n",
            GWY_VERSION_STRING, gwy_version_string());
    exit(EXIT_SUCCESS);
}

int
main(int argc, char *argv[])
{
    static GOptionEntry entries[] = {
        { "version", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, &print_version, "Print version", NULL },
        { NULL, 0, 0, 0, NULL, NULL, NULL }
    };
    GOptionContext *context = g_option_context_new("");
    g_option_context_add_main_entries(context, entries, GETTEXT_PACKAGE);
    g_option_context_add_group(context, gtk_get_option_group(TRUE));
    GError *error = NULL;
    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_printerr("%s\n", error->message);
        g_clear_error(&error);
        exit(EXIT_FAILURE);
    }

    gwy_resources_load(NULL);
    gwy_register_stock_items();
    gwy_register_modules(NULL);

    return 0;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
