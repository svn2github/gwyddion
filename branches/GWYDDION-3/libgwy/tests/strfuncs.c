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

#include "testlibgwy.h"
#include <string.h>

/***************************************************************************
 *
 * String functions
 *
 ***************************************************************************/

/* FIXME: On GNU systems we test the libc memmem() here.  On the other hand,
 * this is the implementation we are going to use, so let's test it. */
void
test_memmem(void)
{
    static const gchar haystack[] = {
        1, 2, 0, 1, 2, 0, 0, 2, 2, 1, 2, 2,
    };
    static const gchar *needle = haystack;
    static const gchar eye[3] = { 1, 2, 2 };
    static const gchar pin[3] = { 2, 0, 2 };

    /* Successes */
    g_assert(gwy_memmem(NULL, 0, NULL, 0) == NULL);
    g_assert(gwy_memmem(haystack, 0, NULL, 0) == haystack);
    for (gsize n = 0; n <= sizeof(haystack); n++)
        g_assert(gwy_memmem(haystack, sizeof(haystack), needle, n) == haystack);

    g_assert(gwy_memmem(haystack, sizeof(haystack), needle + 4, 2)
             == haystack + 1);
    g_assert(gwy_memmem(haystack, sizeof(haystack), needle + 4, 3)
             == haystack + 4);
    g_assert(gwy_memmem(haystack, sizeof(haystack), eye, sizeof(eye))
             == haystack + sizeof(haystack) - sizeof(eye));

    /* Failures */
    g_assert(gwy_memmem(NULL, 0, needle, 1) == NULL);
    g_assert(gwy_memmem(haystack, sizeof(haystack)-1, needle, sizeof(haystack))
             == NULL);
    g_assert(gwy_memmem(haystack, sizeof(haystack), pin, sizeof(pin)) == NULL);
    g_assert(gwy_memmem(haystack, sizeof(haystack)-1, eye, sizeof(eye))
             == NULL);
}

G_GNUC_NULL_TERMINATED
static void
test_next_line_check(const gchar *text, ...)
{
    gchar *buf = g_strdup(text);
    gchar *p = buf;
    va_list ap;
    const gchar *expected = NULL;

    va_start(ap, text);
    for (gchar *line = gwy_str_next_line(&p);
         line;
         line = gwy_str_next_line(&p)) {
        expected = va_arg(ap, const gchar*);
        g_assert(expected != NULL);
        g_assert_cmpstr(line, ==, expected);
    }
    expected = va_arg(ap, const gchar*);
    g_assert(expected == NULL);
    va_end(ap);
    g_free(buf);
}

void
test_next_line(void)
{
    test_next_line_check(NULL, NULL);
    test_next_line_check("", NULL);
    test_next_line_check("\r", "", NULL);
    test_next_line_check("\n", "", NULL);
    test_next_line_check("\r\n", "", NULL);
    test_next_line_check("\r\r", "", "", NULL);
    test_next_line_check("\n\n", "", "", NULL);
    test_next_line_check("\r\n\r\n", "", "", NULL);
    test_next_line_check("\n\r\n", "", "", NULL);
    test_next_line_check("\r\r\n", "", "", NULL);
    test_next_line_check("\r\n\r", "", "", NULL);
    test_next_line_check("\n\r\r", "", "", "", NULL);
    test_next_line_check("X", "X", NULL);
    test_next_line_check("X\n", "X", NULL);
    test_next_line_check("X\r", "X", NULL);
    test_next_line_check("X\r\n", "X", NULL);
    test_next_line_check("\nX", "", "X", NULL);
    test_next_line_check("\rX", "", "X", NULL);
    test_next_line_check("\r\nX", "", "X", NULL);
    test_next_line_check("X\r\r", "X", "", NULL);
    test_next_line_check("X\n\n", "X", "", NULL);
    test_next_line_check("X\nY\rZ", "X", "Y", "Z", NULL);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
