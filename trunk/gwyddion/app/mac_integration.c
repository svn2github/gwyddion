/*
 *  @(#) $Id$
 *  Copyright (C) 2009 Andrey Gruzdev.
 *  E-mail: gruzdev@ntmdt.ru.
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

#ifdef __APPLE__
#include <AppKit/AppKit.h>
#include <CoreFoundation/CoreFoundation.h>
#include <file.h>
#include "config.h"

#ifdef HAVE_GTK_MAC_INTEGRATION
#include <gtkmacintegration/gtkosxapplication.h>
#endif

#define USE_MAC_INTEGRATION
#define USED_ON_MAC /* */
#else
#define USED_ON_MAC G_GNUC_UNUSED
#endif

#ifdef HAVE_GTK_MAC_INTEGRATION
#define USED_ON_MAC_QUARTZ /* */
#else
#define USED_ON_MAC_QUARTZ G_GNUC_UNUSED
#endif

#include "mac_integration.h"

#ifdef USE_MAC_INTEGRATION
int fileModulesReady = 0;
GPtrArray *files_array = NULL;
#ifdef HAVE_GTK_MAC_INTEGRATION
GtkosxApplication *theApp = NULL;
#endif
#endif

void
gwy_osx_get_menu_from_widget(USED_ON_MAC_QUARTZ GtkWidget *container)
{
#ifdef HAVE_GTK_MAC_INTEGRATION
    GList *children;            //,*subchildren,*subsubchildren;
    GList *l, *ll, *lll;
    GtkWidget *menubar = gtk_menu_bar_new();

    gtk_widget_set_name(menubar, "toolboxmenubar");
    children = gtk_container_get_children(GTK_CONTAINER(container));
    for (l = children; l; l = l->next) {
        GtkWidget *widget = l->data;

        if (GTK_IS_CONTAINER(widget)) {
            children = gtk_container_get_children(GTK_CONTAINER(widget));
            for (ll = children; ll; ll = ll->next) {
                GtkWidget *subwidget = ll->data;

                if (GTK_IS_CONTAINER(subwidget)) {
                    children =
                        gtk_container_get_children(GTK_CONTAINER(subwidget));
                    for (lll = children; lll; lll = lll->next) {
                        GtkWidget *subsubwidget = lll->data;

                        if (GTK_IS_MENU_ITEM(subsubwidget)) {
                            gtk_widget_hide(widget);
                            g_object_ref(subsubwidget);
                            gtk_container_remove(GTK_CONTAINER(subwidget), subsubwidget);
                            gtk_menu_shell_append(GTK_MENU_SHELL(menubar),
                                                  subsubwidget);
                            g_object_unref(subsubwidget);
                        }
                    }
                }
            }
        }
    }
    gtk_container_add(GTK_CONTAINER(container), menubar);
    gtk_widget_hide(menubar);
    gtkosx_application_set_menu_bar ( theApp, GTK_MENU_SHELL(menubar));
    gtkosx_application_ready (theApp);
#endif
}


#ifdef USE_MAC_INTEGRATION

static void
gwy_osx_open_file(gpointer data,
                  G_GNUC_UNUSED gpointer user_data)
{
    gwy_app_file_load((const gchar*)data, (const gchar*)data, NULL);
}

@interface GwyOSXEventHandler:NSObject
@end

@implementation GwyOSXEventHandler
- (void)handleOpenEvent:(NSAppleEventDescriptor *)event withReplyEvent: (NSAppleEventDescriptor *)replyEvent
{
    #pragma unused (replyEvent)
    NSAppleEventDescriptor *descr = [event descriptorForKeyword:keyDirectObject];
    NSInteger i,count = [descr numberOfItems];

    for(i=0;i<count;i++)
    {
        NSAppleEventDescriptor *descr1 = i==0?descr:[descr descriptorAtIndex:i];
        NSString *url = [descr1 stringValue];
        NSString *filename = [[NSURL URLWithString:url]  path];


        char * strBuffer = (char*)[filename UTF8String];

        if (fileModulesReady)
            gwy_osx_open_file(strBuffer, NULL);
        else {
            if (!files_array)
                files_array = g_ptr_array_new();
            g_ptr_array_add(files_array, g_strdup((gchar*)strBuffer));
        }
    }
}

- (void)handleQuitEvent:(NSAppleEventDescriptor *)event withReplyEvent: (NSAppleEventDescriptor *)replyEvent
{
   #pragma unused (event)
   #pragma unused (replyEvent)
   gtk_main_quit();
}

@end

GwyOSXEventHandler *eventHandler;

#endif

void
gwy_osx_init_handler(USED_ON_MAC int *argc)
{
#ifdef USE_MAC_INTEGRATION
    NSAppleEventManager *appleEventManager;
    CFURLRef res_url_ref = NULL, bundle_url_ref = NULL;
    res_url_ref = CFBundleCopyResourcesDirectoryURL(CFBundleGetMainBundle());
    bundle_url_ref = CFBundleCopyBundleURL(CFBundleGetMainBundle());

    if (res_url_ref
        && bundle_url_ref && !CFEqual(res_url_ref, bundle_url_ref))
        *argc = 1;        // command line options not available in app bundles

    if (res_url_ref)
        CFRelease(res_url_ref);
    if (bundle_url_ref)
        CFRelease(bundle_url_ref);


    eventHandler = [[GwyOSXEventHandler alloc] init];


    appleEventManager = [NSAppleEventManager sharedAppleEventManager];
    [appleEventManager setEventHandler:eventHandler
                           andSelector:@selector(handleOpenEvent:withReplyEvent:)
                         forEventClass:kCoreEventClass
                            andEventID:kAEOpenDocuments];

    [appleEventManager setEventHandler:eventHandler
                           andSelector:@selector(handleQuitEvent:withReplyEvent:)
                         forEventClass:kCoreEventClass
                            andEventID:kAEQuitApplication];

#ifdef HAVE_GTK_MAC_INTEGRATION
    theApp  = g_object_new (GTKOSX_TYPE_APPLICATION, NULL);
#endif
#endif
}

void
gwy_osx_remove_handler(void)
{
#ifdef USE_MAC_INTEGRATION
    NSAppleEventManager *appleEventManager = [NSAppleEventManager sharedAppleEventManager];
    [appleEventManager removeEventHandlerForEventClass:kCoreEventClass andEventID:kAEOpenDocuments];
    [appleEventManager removeEventHandlerForEventClass:kCoreEventClass andEventID:kAEQuitApplication];
    [eventHandler release];
    eventHandler = nil;
#ifdef HAVE_GTK_MAC_INTEGRATION
    g_object_unref (theApp);
#endif
#endif
}

void
gwy_osx_open_files(void)
{
#ifdef USE_MAC_INTEGRATION
    if (files_array) {
        g_ptr_array_foreach(files_array, gwy_osx_open_file, NULL);
        g_ptr_array_foreach(files_array, (GFunc)g_free, NULL);
        g_ptr_array_free(files_array, TRUE);
        files_array = NULL;
    }
    fileModulesReady = 1;
#endif
}

void
gwy_osx_set_locale()
{
#ifdef USE_MAC_INTEGRATION
    static const struct {
        const gchar *locale;
        const gchar *lang;
    }
    locales[] = {
/* The following generated part is updated by running utils/update-langs.py */
/* @@@ GENERATED LANG OS X BEGIN @@@ */
        { "en_US.UTF-8", "en" },
        { "cs_CZ.UTF-8", "cs" },
        { "de_DE.UTF-8", "de" },
        { "fr_FR.UTF-8", "fr" },
        { "it_IT.UTF-8", "it" },
        { "ru_RU.UTF-8", "ru" },
        { "es_ES.UTF-8", "es" },
/* @@@ GENERATED LANG OS X END @@@ */
    };

    CFTypeRef preferences = CFPreferencesCopyAppValue(CFSTR("AppleLanguages"),
                                                      kCFPreferencesCurrentApplication);

    if (preferences != NULL && CFGetTypeID(preferences) == CFArrayGetTypeID()) {
        CFArrayRef prefArray = (CFArrayRef) preferences;
        int n = CFArrayGetCount(prefArray);
        static char buf[256];
        int i;

        for (i = 0; i < n; i++) {
            CFTypeRef element = CFArrayGetValueAtIndex(prefArray, i);

            if (element != NULL && CFGetTypeID(element) == CFStringGetTypeID()
                && CFStringGetCString((CFStringRef) element,
                                      buf, sizeof(buf),
                                      kCFStringEncodingASCII)) {
                int j;

                for (j = 0; j < G_N_ELEMENTS(locales); j++) {
                    if (strcmp(locales[j].lang, buf) == 0) {
                        g_setenv("LANG", locales[j].locale, TRUE);
                        goto exit;
                    }
                }
            }
        }
      exit:
        CFRelease(preferences);
    }
#endif
}
