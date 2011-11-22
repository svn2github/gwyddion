/*
 *  $Id$
 *  Copyright (C) 2009,2011 David Nečas (Yeti).
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

#include "config.h"
#include <stdarg.h>
#include "libgwy/macros.h"
#include "libgwy/strfuncs.h"

#define TOLOWER(c) ((c) >= 'A' && (c) <= 'Z' ? (c) - 'A' + 'a' : (c))

struct _GwyStrLineIter {
    gchar *buffer;
    gchar *pos;
    guint lineno;
};

/**
 * gwy_ascii_strisident:
 * @s: A NUL-terminated string.
 * @more: List of additional ASCII characters allowed inside identifier, empty
 *        list can be passed as %NULL.
 * @startmore: List of additional ASCII characters allowed as the first
 *             identifier characters, empty list can be passed as %NULL.
 *
 * Checks whether a string is valid identifier.
 *
 * Valid identifier must start with an alphabetic character or a character from
 * @startmore, and it must continue with alphanumeric characters or characters
 * from @more.
 *
 * Note underscore is not allowed by default, you have to pass it in @more
 * and/or @startmore.
 *
 * Returns: %TRUE if @s is valid identifier, %FALSE otherwise.
 **/
gboolean
gwy_ascii_strisident(const gchar *s,
                     const gchar *more,
                     const gchar *startmore)
{
    g_return_val_if_fail(s, FALSE);

    const gchar *m;
    if (!g_ascii_isalpha(*s)) {
        if (!startmore)
            return FALSE;
        for (m = startmore; *m; m++) {
            if (*s == *m)
                break;
        }
        if (!*m)
            return FALSE;
    }
    s++;

    while (*s) {
        if (!g_ascii_isalnum(*s)) {
            if (!more)
                return FALSE;
            for (m = more; *m; m++) {
                if (*s == *m)
                    break;
            }
            if (!*m)
                return FALSE;
        }
        s++;
    }

    return TRUE;
}

/**
 * gwy_utf8_strisident:
 * @s: A NUL-terminated string.
 * @more: List of additional characters allowed inside identifier, empty
 *        list can be passed as %NULL.
 * @startmore: List of additional characters allowed as the first
 *             identifier characters, empty list can be passed as %NULL.
 *
 * Checks whether a string is valid identifier.
 *
 * Valid identifier must start with an alphabetic character or a character
 * from @startmore, and it must continue with alphanumeric characters or
 * characters from @more.  Both @more and @startmore are passed as UCS-4
 * strings, not UTF-8.
 *
 * Note underscore is not allowed by default, you have to pass it in @more
 * and/or @startmore.
 *
 * Returns: %TRUE if @s is valid identifier, %FALSE otherwise.
 **/
gboolean
gwy_utf8_strisident(const gchar *s,
                    const gunichar *more,
                    const gunichar *startmore)
{
    g_return_val_if_fail(s, FALSE);
    if (!g_utf8_validate(s, -1, NULL))
        return FALSE;

    const gunichar *m;
    gunichar c;
    if (!g_unichar_isalpha((c = g_utf8_get_char(s)))) {
        if (!startmore)
            return FALSE;
        for (m = startmore; *m; m++) {
            if (c == *m)
                break;
        }
        if (!*m)
            return FALSE;
    }
    s = g_utf8_next_char(s);

    while (*s) {
        if (!g_unichar_isalnum((c = g_utf8_get_char(s)))) {
            if (!more)
                return FALSE;
            for (m = more; *m; m++) {
                if (c == *m)
                    break;
            }
            if (!*m)
                return FALSE;
        }
        s = g_utf8_next_char(s);
    }

    return TRUE;
}

/**
 * gwy_ascii_strcase_equal:
 * @v1: String key.
 * @v2: String key to compare with @v1.
 *
 * Compares two strings for equality, ignoring case.
 *
 * The case folding is performed only on ASCII letters.
 *
 * This function is intended to be passed to g_hash_table_new() as
 * @key_equal_func, namely in conjuction with gwy_ascii_strcase_hash() hashing
 * function.
 *
 * Returns: %TRUE if the two string keys match, ignoring case.
 */
gboolean
gwy_ascii_strcase_equal(gconstpointer v1,
                        gconstpointer v2)
{
    const gchar *s1 = (const gchar*)v1, *s2 = (const gchar*)v2;

    while (*s1 && *s2) {
        if (TOLOWER(*s1) != TOLOWER(*s2))
            return FALSE;
        s1++, s2++;
    }
    return !*s1 && !*s2;
}

/**
 * gwy_ascii_strcase_hash:
 * @v: String key.
 *
 * Converts a string to a hash value, ignoring case.
 *
 * The case folding is performed only on ASCII letters.
 *
 * This function is intended to be passed to g_hash_table_new() as @hash_func,
 * namely in conjuction with gwy_ascii_strcase_equal() comparison function.
 *
 * Returns: The hash value corresponding to the key @v.
 */
guint
gwy_ascii_strcase_hash(gconstpointer v)
{
    guint32 h = 5381;
    for (const signed char *p = v; *p; p++)
        h = (h << 5) + h + TOLOWER(*p);

    return h;
}

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
 * arbitrary memory blocks instead of %NULL-terminated strings.
 *
 * If @needle_len is zero, @haystack is always returned.
 *
 * On GNU systems with glibc at least 2.1 this is a just a trivial memmem()
 * wrapper.  On other systems it emulates memmem() behaviour.
 *
 * Returns: (transfer none):
 *          Pointer to the first byte of memory block in @haystack that matches
 *          @needle; %NULL if no such block exists.
 **/
gpointer
gwy_memmem(gconstpointer haystack,
           gsize haystack_len,
           gconstpointer needle,
           gsize needle_len)
{
#ifdef HAVE_WORKING_MEMMEM
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
 * @buffer: Text buffer (writable).
 *
 * Extracts a next line from a character buffer, modifying it in place.
 *
 * @buffer is updated to point after the end of the line and the "\n"
 * (or "\r" or "\r\n") is replaced with "\0", if present.
 *
 * The final line may or may not be terminated with an EOL marker, its contents
 * is returned in either case.  Note, however, that the empty string "" is not
 * interpreted as an empty unterminated line.  Instead, %NULL is immediately
 * returned.
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
    if (!*q) {
        *buffer = NULL;
        return NULL;
    }

    gchar *p;
    for (p = q; *p != '\n' && *p != '\r' && *p; p++)
        ;
    if (p[0] == '\r' && p[1] == '\n') {
        *(p++) = '\0';
        *(p++) = '\0';
    }
    else if (p[0]) {
        *(p++) = '\0';
    }

    *buffer = p;
    return q;
}

/**
 * gwy_str_line_iter_new:
 * @buffer: (allow-none) (transfer none):
 *          Writable NUL-terminated string, typically consisting of several
 *          lines.  %NULL is permitted and means the same as an empty @buffer.
 *
 * Creates a new text line iterator for a caller-owned string buffer.
 *
 * The buffer does not become owned by the newly created iterator, even though
 * the iterator modifies its contents along the way.  The caller needs to free
 * it afterwards.
 *
 * Returns: (transfer full):
 *          A new text line iterator for @buffer.
 **/
GwyStrLineIter*
gwy_str_line_iter_new(gchar *buffer)
{
    GwyStrLineIter *iter = g_slice_new0(GwyStrLineIter);
    iter->pos = buffer;
    return iter;
}

/**
 * gwy_str_line_iter_new_take:
 * @buffer: (allow-none) (transfer full):
 *          Writable NUL-terminated string, typically consisting of several
 *          lines.  %NULL is permitted and means the same as an empty @buffer.
 *
 * Creates a new text line iterator for a string buffer, taking its ownership.
 *
 * The buffer becomes owned by the newly created iterator that will also
 * modifies its contents along the way.
 *
 * Returns: A new text line iterator for @buffer.
 **/
GwyStrLineIter*
gwy_str_line_iter_new_take(gchar *buffer)
{
    GwyStrLineIter *iter = g_slice_new0(GwyStrLineIter);
    iter->pos = iter->buffer = buffer;
    return iter;
}

/**
 * gwy_str_line_iter_free:
 * @iter: A text line iterator.
 *
 * Frees a text line iterator.
 **/
void
gwy_str_line_iter_free(GwyStrLineIter *iter)
{
    g_return_if_fail(iter);
    GWY_FREE(iter->buffer);
    g_slice_free(GwyStrLineIter, iter);
}

/**
 * gwy_str_line_iter_next:
 * @iter: A text line iterator.
 *
 * Fetches the next line from a text line iterator.
 *
 * The iterator immediately moves on so it is not possible to go back to the
 * same line again.  The contents of the buffer is modified and the caller must
 * treat it as undefined.
 *
 * This method can be called any number of times after returning the last line;
 * it will simply continue returning %NULL until the caller finally gets bored
 * and frees the iterator.
 *
 * The last line of the buffer needs not be terminated with any end-of-line
 * character.
 *
 * A buffer with no characters is considered completely empty (i.e. not as a
 * single empty line) and this method returns %NULL immediately in the first
 * iteration.
 *
 * Example of typical use:
 * |[
 * GwyStrLineIter *iter = gwy_str_line_iter_new_take(buffer);
 * gchar *line;
 * while ((line = gwy_str_line_iter_next(iter))) {
 *     // Process line.
 * }
 * gwy_str_line_iter_free(iter);
 * ]|
 *
 * Returns: (allow-none) (transfer none):
 *          The next line, with any trailing end-of-line characters removed.
 *          The returned string is just a pointer somewhere into the buffer.
 *          %NULL is returned if there are no more lines.
 **/
gchar*
gwy_str_line_iter_next(GwyStrLineIter *iter)
{
    g_return_val_if_fail(iter, NULL);
    if (!iter->pos)
        return NULL;

    gchar *q = iter->pos;
    if (!*q) {
        iter->pos = NULL;
        return NULL;
    }

    gchar *p;
    for (p = q; *p != '\n' && *p != '\r' && *p; p++)
        ;
    if (p[0] == '\r' && p[1] == '\n') {
        *(p++) = '\0';
        *(p++) = '\0';
    }
    else if (p[0]) {
        *(p++) = '\0';
    }

    iter->pos = p;
    iter->lineno++;
    return q;
}

/**
 * gwy_str_line_iter_lineno:
 * @iter: A text line iterator.
 *
 * Obtains the current line number of a text line iterator.
 *
 * Returns: Zero if the iteration has not started yet.  The line number,
 *          starting from one, of the last line returned by
 *          gwy_str_line_iter_next() otherwise.
 **/
guint
gwy_str_line_iter_lineno(const GwyStrLineIter *iter)
{
    g_return_val_if_fail(iter, 0);
    return iter->lineno;
}

/**
 * SECTION: strfuncs
 * @title: String functions
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
 * a macro, it can reduce to a compiler builtin.
 **/

/**
 * GwyStrLineIter:
 *
 * Text line iterator.
 *
 * The #GwyStrLineIter struct is opaque and can only be accessed through the
 * gwy_str_line_iter_foo() functions.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
