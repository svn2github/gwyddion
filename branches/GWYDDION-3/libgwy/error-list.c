/*
 *  $Id$
 *  Copyright (C) 2009 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdarg.h>
#include "libgwy/error-list.h"
#include "libgwy/libgwy-aliases.h"

/**
 * gwy_error_list_add:
 * @list: Pointer to error list or %NULL.
 * @domain: Error domain.
 * @code: Error code.
 * @format: Error message printf()-style format.
 * @...: arguments for @format.
 *
 * Adds a newly created error to an error list.
 *
 * Does nothing if @list is %NULL.  See also g_set_error().
 **/
void
gwy_error_list_add(GwyErrorList **list,
                   GQuark domain,
                   gint code,
                   const gchar *format,
                   ...)
{
    va_list ap;
    gchar *message;

    if (!list)
        return;

    va_start(ap, format);
    message = g_strdup_vprintf(format, ap);
    va_end(ap);
    *list = g_slist_prepend(*list, g_error_new_literal(domain, code, message));
    g_free(message);
}

/**
 * gwy_error_list_clear:
 * @list: Pointer to error list or %NULL.
 *
 * Clears all errors in an error list and destroys the list.
 *
 * If @list is not %NULL, it is reset to an empty list, i.e. %NULL is assigned
 * to the location it points to.
 **/
void
gwy_error_list_clear(GwyErrorList **list)
{
    GSList *l;

    if (!list || !*list)
        return;

    for (l = *list; l; l = l->next)
        g_clear_error((GError**)&l->data);

    g_slist_free(*list);
    *list = NULL;
}

#define __GWY_ERROR_LIST_C__
#include "libgwy/libgwy-aliases.c"

/**
 * SECTION: error-list
 * @title: GwyErrorList
 * @short_description: List of #GError<!-- -->s
 *
 * GwyErrorList is a plain #GSList containing pointers to #GErrors as the
 * @data members.  You can use all #GSList functions on #GwyErrorList, a few
 * additional functions are provided for convenient error list handling 
 * Notice the calling convention differs from the usual #GSList functions and
 * resembles more #GError functions.
 *
 * Errors are added to the list with gwy_error_list_add() in a stack-like
 * fashion, i.e. the most recent error is the first item in the list.
 *
 * There is no function to create a #GwyErrorList because %NULL represents an
 * empty list.  All items in the list hold some error, there are never any
 * items with %NULL data members.
 *
 * To clear all errors in the list and destroy the list, call
 * gwy_error_list_clear().
 **/

/**
 * GwyErrorList:
 *
 * Representation of an error list.
 *
 * This is an alias of #GSList.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
