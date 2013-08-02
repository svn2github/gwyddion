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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libgwyddion/gwyutils.h>
#include <app/settings.h>
#include <app/gwytool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const gchar *name;
    const GwyModuleInfo *info;
} ModInfo;

typedef struct {
    gchar *target;
    guint use_count;
} MapInfo;

static GHashTable *user_guide = NULL;
static GSList *tag_stack = NULL;

static void
read_user_guide(const gchar *filename)
{
    gchar *buf = NULL;

    g_file_get_contents(filename, &buf, NULL, NULL);
    user_guide = g_hash_table_new(g_str_hash, g_str_equal);
    while (buf && *buf) {
        MapInfo *minfo;
        gchar *key = gwy_str_next_line(&buf);
        gchar *val = strchr(key, ' ');

        if (!val)
            continue;
        *val = '\0';
        minfo = g_new0(MapInfo, 1);
        minfo->target = val+1;
        g_hash_table_insert(user_guide, key, minfo);
    };
}

/* For module list sorting */
static gint
compare_modules(gconstpointer a, gconstpointer b)
{
    return strcmp(((const ModInfo*)a)->name,
                  ((const ModInfo*)b)->name);
}

/* For finding module of given name */
static gint
find_module(gconstpointer a, gconstpointer b)
{
    return strcmp(((const ModInfo*)a)->name, (const gchar*)b);
}

/* Print a single tag with content, handling sensitive characters */
static void
tag_print(const gchar *tag, const gchar *contents)
{
    gchar *s;

    if (!contents || !*contents) {
        printf("<%s/>\n", tag);
        return;
    }

    s = g_markup_escape_text(contents, strlen(contents));
    printf("<%s>%s</%s>\n", tag, s, tag);
    g_free(s);
}

/* Print an opening tag and push it onto stack */
static void
tag_open(const gchar *tag)
{
    printf("<%s>\n", tag);
    tag_stack = g_slist_prepend(tag_stack, g_strdup(tag));
}

/* Pop currently open tag and close it */
static void
tag_close(void)
{
    gchar *s;

    g_return_if_fail(tag_stack);
    s = tag_stack->data;
    printf("</%s>\n", s);
    tag_stack = g_slist_delete_link(tag_stack, tag_stack);
    g_free(s);
}

static void
menu_path_print(const gchar *path, const gchar *tag)
{
    gchar *s;
    gint i;

    s = gwy_strkill(gwy_strreplace(path + (path[0] == '/'),
                                   "/", " → ", -1), "_");
    i = strlen(s);
    do {
        s[i] = '\0';
        i--;
    } while (s[i] == '.');
    tag_print(tag, s);
    g_free(s);
}

/* Remove <...> substrings from a string. */
static gchar*
kill_mail(const gchar *authors)
{
    const gchar *a;
    gchar *s, *p;

    p = s = g_new0(gchar, strlen(authors) + 1);
    while (authors) {
        a = strchr(authors, '<');
        if (!a) {
            strcpy(p, authors);
            break;
        }
        else {
            while (a > authors && !g_ascii_isspace(*a))
                a--;
            strncpy(p, authors, a - authors);
            p += a - authors;
            authors = strchr(a+1, '>');
            if (authors)
                authors++;
        }
    }

    p = s + strlen(s);
    while (p >= s && (!*p || g_ascii_isspace(*p))) {
        *p = '\0';
        p--;
    }

    return s;
}

static void
add_info(const gchar *name,
         const GwyModuleInfo *info,
         GSList **list)
{
    ModInfo *i;

    i = g_new(ModInfo, 1);
    i->name = name;
    i->info = info;
    *list = g_slist_prepend(*list, i);
}

static void
warn_unused(const gchar *name,
            const MapInfo *minfo)
{
    if (!minfo->use_count)
        fprintf(stderr, "Unused map entry %s -> %s\n", name, minfo->target);
}

/* Main */
int
main(int argc,
     char *argv[])
{
    /* modules we know we don't like to see in the list (XXX: hack) */
    const gchar *filter_modules[] = {
        "threshold-example",
    };
    /* modules we know we don't want to see functions of (XXX: hack) */
    const gchar *no_func_modules[] = {
        "plugin-proxy", "pygwy",
    };

    gchar **module_dirs;
    GSList *m, *modules = NULL;
    gsize i;

    gwy_type_init();
    if (argc < 2) {
        g_printerr("Need user guide map.\n");
        return 1;
    }
    read_user_guide(argv[1]);

    module_dirs = gwy_app_settings_get_module_dirs();
    gwy_module_register_modules((const gchar**)module_dirs);
    gwy_module_foreach((GHFunc)add_info, &modules);
    modules = g_slist_sort(modules, &compare_modules);

    for (i = 0; i < G_N_ELEMENTS(filter_modules); i++) {
        m = g_slist_find_custom(modules, filter_modules[i], find_module);
        if (!m)
            continue;
        modules = g_slist_delete_link(modules, m);
    }

    puts("<?xml version=\"1.0\" encoding=\"utf-8\"?>");
    tag_open("modulelist");
    for (m = modules; m; m = g_slist_next(m)) {
        ModInfo *info = (ModInfo*)m->data;
        GSList *f = gwy_module_get_functions(info->name);
        MapInfo *minfo;
        gchar *s;

        tag_open("module");
        tag_print("name", info->name);
        tag_print("version", info->info->version);
        s = kill_mail(info->info->author);
        tag_print("author", s);
        g_free(s);
        tag_print("copyright", info->info->copyright);
        tag_print("date", info->info->date);
        if ((minfo = g_hash_table_lookup(user_guide, info->name))) {
            tag_print("userguide", minfo->target);
            minfo->use_count++;
        }
        else
            fprintf(stderr, "No user guide entry for %s\n", info->name);
        tag_print("description", info->info->blurb);
        for (i = 0; i < G_N_ELEMENTS(no_func_modules); i++) {
            if (gwy_strequal(info->name, no_func_modules[i]))
                break;
        }
        if (i < G_N_ELEMENTS(no_func_modules)) {
            tag_print("funclist", NULL);
            tag_close();
            continue;
        }

        tag_open("funclist");
        for ( ; f; f = g_slist_next(f)) {
            gchar *name, *class = g_strdup(f->data);

            tag_open("func");
            /* parse class::name string */
            name = strchr(class, ':');
            g_assert(name != NULL && name > class && name[1] && name[2]);
            *name = '\0';
            name += 2;
            tag_print("class", class);
            tag_print("name", name);

            /* dig more info about particular function types */
            if (gwy_strequal(class, "proc"))
                menu_path_print(gwy_process_func_get_menu_path(name), "info");
            else if (gwy_strequal(class, "graph"))
                menu_path_print(gwy_graph_func_get_menu_path(name), "info");
            else if (gwy_strequal(class, "file"))
                menu_path_print(gwy_file_func_get_description(name), "info");
            else if (gwy_strequal(class, "tool")) {
                GwyToolClass *tool_class;

                tool_class = g_type_class_peek(g_type_from_name(name));
                tag_print("info", gwy_tool_class_get_tooltip(tool_class));
            }

            g_free(class);
            tag_close();
        }
        tag_close();
        tag_close();
    }
    tag_close();

    g_hash_table_foreach(user_guide, (GHFunc)warn_unused, NULL);

    g_slist_free(modules);
    g_strfreev(module_dirs);

    return 0;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

