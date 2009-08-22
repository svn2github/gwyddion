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

#include <config.h>
#include <string.h>
#include <stdarg.h>
#include "libgwy/macros.h"
#include "libgwy/strfuncs.h"
#include "libgwy/libgwy-aliases.h"

/**
 * gwy_stramong:
 * @str: String to find.
 * @...: %NULL-terminated list of string to compare @str with.
 *
 * Checks whether a string is equal to any from given list.
 *
 * Returns: Zero if @str does not equal to any string from the list, nozero
 *          othwerise.  More precisely, the position + 1 of the first string
 *          @str equals to is returned in the latter case.
 **/
guint
gwy_stramong(const gchar *str,
             ...)
{
    va_list ap;
    const gchar *s;
    guint i = 1;

    va_start(ap, str);
    while ((s = va_arg(ap, const gchar*))) {
        if (gwy_strequal(str, s)) {
            va_end(ap);
            return i;
        }
        i++;
    }
    va_end(ap);

    return 0;
}

/**
 * gwy_memmem:
 * @haystack: Memory block to search in.
 * @haystack_len: Size of @haystack, in bytes.
 * @needle: Memory block to find.
 * @needle_len: Size of @needle, in bytes.
 *
 * Find a block of memory in another block of memory.
 *
 * This function is very similar to strstr(), except that it works with
 * arbitrary memory blocks instead of %NUL-terminated strings.
 *
 * If @needle_len is zero, @haystack is always returned.
 *
 * On GNU systems with glibc at least 2.1 this is a just a trivial memmem()
 * wrapper.  On other systems it emulates memmem() behaviour.
 *
 * Returns: Pointer to the first byte of memory block in @haystack that matches
 *          @needle; %NULL if no such block exists.
 **/
gpointer
gwy_memmem(gconstpointer haystack,
           gsize haystack_len,
           gconstpointer needle,
           gsize needle_len)
{
#ifdef HAVE_MEMMEM
    return memmem(haystack, haystack_len, needle, needle_len);
#else
    const guchar *h = haystack;
    const guchar *s, *p;
    guchar n0;

    /* Empty needle can be found anywhere */
    if (!needle_len)
        return (gpointer)haystack;

    /* Needle that doesn't fit cannot be found anywhere */
    if (needle_len > haystack_len)
        return NULL;

    /* The general case */
    n0 = *(const guchar*)needle;
    s = h + haystack_len - needle_len;
    for (p = h; p && p <= s; p = memchr(p, n0, s-p + 1)) {
        if (memcmp(p, needle, needle_len) == 0)
            return (gpointer)p;
        p++;
    }
    return NULL;
#endif
}

/**
 * gwy_str_next_line:
 * @buffer: Text buffer.
 *
 * Extracts a next line from a character buffer, modifying it in place.
 *
 * @buffer is updated to point after the end of the line and the "\n"
 * (or "\r" or "\r\n") is replaced with "\0", if present.
 *
 * The typical usage of gwy_str_next_line() is:
 * |[
 * gchar *p = text;
 * for (gchar *line = gwy_str_next_line(&p);
 *      line;
 *      line = gwy_str_next_line(&p)) {
 *     g_strstrip(line);
 *     // Do something more with line
 * }
 * ]|
 *
 * Returns: The start of the line.  %NULL if the buffer is empty or %NULL.
 *          The return value is <emphasis>not</emphasis> a new string; the
 *          normal return value is the previous value of @buffer.
 **/
gchar*
gwy_str_next_line(gchar **buffer)
{
    if (!buffer || !*buffer)
        return NULL;

    gchar *q = *buffer;
    gsize n = strcspn(*buffer, "\n\r");

    if (n || *q) {
        gchar *p = q + n;

        if (p[0] == '\r' && p[1] == '\n') {
            p[0] = p[1] = '\0';
            *buffer = p+2;
        }
        else {
            p[0] = '\0';
            *buffer = p+1;
        }
    }
    else
        *buffer = NULL;

    return q;
}

#define __LIBGWY_STRFUNCS_C__
#include "libgwy/libgwy-aliases.c"

/**
 * SECTION: strfuncs
 * @title: strfuncs
 * @short_description: String utility functions
 **/

/**
 * gwy_strequal:
 * @a: String.
 * @b: Another string.
 *
 * Expands to %TRUE if strings are equal, to %FALSE otherwise.
 *
 * This is a shorthand for strcmp() which does not require mentally inverting
 * the result to test two strings for equality.
 *
 * Compared to g_str_equal(), the advantage of gwy_strequal() is that, being
 * a macro, it can reduce to a GCC builtin.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
