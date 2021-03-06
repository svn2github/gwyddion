/*
 *  $Id$
 *  Copyright (C) 2009,2011-2014 David Nečas (Yeti).
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
 * @...: %NULL-terminated list of strings to compare @str with.
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
    g_return_val_if_fail(str, 0);

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
 * gwy_str_remove_prefix:
 * @str: String to remove a prefix from.
 * @...: %NULL-terminated list of string prefixes to removed from @str.
 *
 * Removes any of given list of prefixes from a string.
 *
 * Only the first matching prefix is removed.  The string is modified in place
 * by moving the rest of characters towards the begining.  An empty prefix is
 * considered to match any string (but, of course, nothing is removed then).
 *
 * Returns: A non-zero value if a prefix was removed, specifically the index of
 *          the removed prefix in the list, 1-based.  Zero is returned if no
 *          prefix matched.
 **/
guint
gwy_str_remove_prefix(gchar *str,
                      ...)
{
    g_return_val_if_fail(str, 0);

    va_list ap;
    const gchar *prefix;
    guint i = 1, len = strlen(str);

    va_start(ap, str);
    while ((prefix = va_arg(ap, const gchar*))) {
        guint plen = strlen(prefix);
        if (plen <= len && memcmp(str, prefix, plen) == 0) {
            va_end(ap);
            memmove(str, str+plen, len+1-plen);
            return i;
        }
        i++;
    }
    va_end(ap);

    return 0;
}

/**
 * gwy_str_remove_suffix:
 * @str: String to remove a suffix from.
 * @...: %NULL-terminated list of string suffixes to removed from @str.
 *
 * Removes any of given list of suffixes from a string.
 *
 * Only the first matching suffix is removed.  The string is modified in place
 * by placing a terminating '\0' as needed.  An empty suffix is considered to
 * match any string (but, of course, nothing is removed then).
 *
 * Returns: A non-zero value if a suffix was removed, specifically the index of
 *          the removed suffix in the list, 1-based.  Zero is returned if no
 *          suffix matched.
 **/
guint
gwy_str_remove_suffix(gchar *str,
                      ...)
{
    g_return_val_if_fail(str, 0);

    va_list ap;
    const gchar *suffix;
    guint i = 1, len = strlen(str);

    va_start(ap, str);
    while ((suffix = va_arg(ap, const gchar*))) {
        guint slen = strlen(suffix);
        if (slen <= len && memcmp(str + len-slen, suffix, slen) == 0) {
            va_end(ap);
            str[len-slen] = '\0';
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
 * wrapper.  On other systems it emulates memmem() behaviour, including the
 * obscure cases when @haystack or @needle is %NULL.
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
 * gwy_utf8_append_exponent:
 * @str: A #GString string to which the exponent should be appended.
 * @power: Integer exponent.
 *
 * Renders an integer exponent to a GString using Unicode superscript
 * characters.
 **/
void
gwy_utf8_append_exponent(GString *str,
                         gint power)
{
    g_return_if_fail(str);

    gchar buf[32];

    g_snprintf(buf, sizeof(buf), "%d", power);
    for (guint i = 0; buf[i]; i++) {
        if (buf[i] == '0' || (buf[i] >= '4' && buf[i] <= '9'))
            g_string_append_unichar(str, 0x2070 + buf[i] - '0');
        else if (buf[i] == '1')
            g_string_append_len(str, "¹", sizeof("¹")-1);
        else if (buf[i] == '2')
            g_string_append_len(str, "²", sizeof("²")-1);
        else if (buf[i] == '3')
            g_string_append_len(str, "³", sizeof("³")-1);
        else if (buf[i] == '-')
            g_string_append_len(str, "⁻", sizeof("⁻")-1);
        else {
            g_warning("Weird digits in number %s\n", buf);
            g_string_append_c(str, buf[i]);
        }
    }
}

/**
 * gwy_gstring_replace:
 * @str: A #GString string to modify in place.
 * @old: (allow-none):
 *       The character sequence to find and replace.  Passing %NULL is the same
 *       as passing the empty string.
 * @replacement: (allow-none):
 *               The character sequence that should replace @old.  Passing
 *               %NULL is the same as passing the empty string.
 * @count: The maximum number of replacements to make.  A negative number means
 *         replacing all occurrences of @old.  Note zero means just zero, i.e.
 *         no replacements are made.
 *
 * Replaces non-overlapping occurrences of one string with another in a
 * #GString.
 *
 * Passing %NULL or the empty string for @replacement will cause the occurrences
 * of @old to be removed.
 *
 * Passing %NULL or the empty string for @old means a match occurs at every
 * position in the string, including after the last character.  So @replacement
 * will be inserted at every position in this case.
 *
 * Returns: The number of replacements made.  A non-zero value means
 *          the string has been modified, no-op replacements do not count.
 **/
guint
gwy_gstring_replace(GString *str,
                    const gchar *old,
                    const gchar *replacement,
                    gint count)
{
    g_return_val_if_fail(str, 0);

    if (!old)
        old = "";
    guint oldlen = strlen(old);

    /* Do we need to do anywork at all? */
    if (!count)
        return 0;
    guint ucount = (count < 0) ? G_MAXUINT : (guint)count;

    gchar *p = str->str;
    if (oldlen && !(p = strstr(str->str, old)))
        return 0;

    if (!replacement)
        replacement = "";
    guint repllen = strlen(replacement);

    /* Equal lengths, we can do the replacement in place easily. */
    if (oldlen == repllen) {
        if (gwy_strequal(old, replacement))
            return 0;

        guint n = 0;
        while (p) {
            memcpy(p, replacement, repllen);
            if (++n == ucount)
                break;
            p = strstr(p + oldlen, old);
        }

        return n;
    }

    /* Empty old string: the slightly silly case.  It has a different oldlen
     * semantics so handle it specially. */
    if (!oldlen) {
        ucount = MIN(ucount, str->len + 1);

        if (ucount == 1) {
            g_string_prepend(str, replacement);
            return 1;
        }

        gchar *oldcopy = g_strdup(str->str);
        guint len = str->len;
        g_string_set_size(str, str->len + ucount*repllen);
        g_string_truncate(str, 0);
        p = str->str;
        for (guint i = 0; i < ucount; i++) {
            memcpy(p, replacement, repllen);
            p += repllen;
            *p = oldcopy[i];
            p++;
        }

        if (ucount < len)
            memcpy(p, oldcopy + ucount, len - ucount);

        g_free(oldcopy);

        return ucount;
    }

    /* The general case.  Count the actual replacement number. */
    guint n = 0;
    for (gchar *q = p; q; q = strstr(q + oldlen, old)) {
        if (++n == ucount)
            break;
    }
    ucount = MIN(n, ucount);

    guint newlen = str->len;
    if (repllen >= oldlen)
        newlen += ucount*(repllen - oldlen);
    else
        newlen -= ucount*(oldlen - repllen);

    if (!newlen) {
        g_string_truncate(str, 0);
        return ucount;
    }

    /* For just one replacement, do the operation directly. */
    if (ucount == 1) {
        guint pos = p - str->str;
        if (repllen > oldlen) {
            g_string_insert_len(str, pos, replacement, repllen - oldlen);
            memcpy(str->str + pos + (repllen - oldlen),
                   replacement + (repllen - oldlen), oldlen);
        }
        else {
            g_string_erase(str, pos, oldlen - repllen);
            memcpy(str->str + pos, replacement, repllen);
        }
        return 1;
    }

    /* For more replacements, rebuild the string from scratch in a buffer. */
    gchar *newstr = g_new(gchar, newlen);

    memcpy(newstr, str->str, p - str->str);
    gchar *newp = newstr + (p - str->str);

    n = 0;
    for (gchar *q = p; q; p = q) {
        if (repllen) {
            memcpy(newp, replacement, repllen);
            newp += repllen;
        }

        if (++n == ucount)
            break;
        if (!(q = strstr(q + oldlen, old)))
            break;

        memcpy(newp, p + oldlen, (q - p) - oldlen);
        newp += (q - p) - oldlen;
    }

    memcpy(newp, p + oldlen, str->len - oldlen - (p - str->str));

    g_string_truncate(str, 0);
    g_string_append_len(str, newstr, newlen);
    g_free(newstr);

    return ucount;
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
 * @section_id: libgwy4-strfuncs
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
 * <function>gwy_str_line_iter_foo<!-- -->()</function> functions.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
