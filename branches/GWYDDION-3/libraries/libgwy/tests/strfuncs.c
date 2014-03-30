/*
 *  $Id$
 *  Copyright (C) 2009,2012-2014 David Nečas (Yeti).
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

// A helper for other tests.
void
base62_format(guint x, gchar *out, guint outsize)
{
    out += outsize-1;
    *out = '\0';
    out--;
    for (guint n = outsize-1; n; n--, out--) {
        guint digit = x % 62;
        if (digit < 10)
            *out = '0' + x;
        else if (digit < 36)
            *out = 'a' + (x - 10);
        else
            *out = 'A' + (x - 36);
        x /= 62;
    }
}

/* FIXME: On GNU systems we test the libc memmem() here.  On the other hand,
 * this is the implementation we are going to use, so let's test it. */
void
test_str_memmem(void)
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
next_line_check(const gchar *text, ...)
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
test_str_next_line(void)
{
    next_line_check(NULL, NULL);
    next_line_check("", NULL);
    next_line_check("\r", "", NULL);
    next_line_check("\n", "", NULL);
    next_line_check("\r\n", "", NULL);
    next_line_check("\r\r", "", "", NULL);
    next_line_check("\n\n", "", "", NULL);
    next_line_check("\r\n\r\n", "", "", NULL);
    next_line_check("\n\r\n", "", "", NULL);
    next_line_check("\r\r\n", "", "", NULL);
    next_line_check("\r\n\r", "", "", NULL);
    next_line_check("\n\r\r", "", "", "", NULL);
    next_line_check("X", "X", NULL);
    next_line_check("X\n", "X", NULL);
    next_line_check("X\r", "X", NULL);
    next_line_check("X\r\n", "X", NULL);
    next_line_check("\nX", "", "X", NULL);
    next_line_check("\rX", "", "X", NULL);
    next_line_check("\r\nX", "", "X", NULL);
    next_line_check("X\r\r", "X", "", NULL);
    next_line_check("X\n\n", "X", "", NULL);
    next_line_check("X\nY\rZ", "X", "Y", "Z", NULL);
}

G_GNUC_NULL_TERMINATED
static void
next_line_check_iter(const gchar *text, ...)
{
    gchar *buf = g_strdup(text);
    va_list ap;
    const gchar *expected = NULL;

    va_start(ap, text);
    GwyStrLineIter *iter = gwy_str_line_iter_new_take(buf);
    gchar *line;
    guint lineno = 0;
    g_assert_cmpuint(gwy_str_line_iter_lineno(iter), ==, lineno);
    while ((line = gwy_str_line_iter_next(iter))) {
        expected = va_arg(ap, const gchar*);
        g_assert(expected != NULL);
        g_assert_cmpstr(line, ==, expected);
        lineno++;
        g_assert_cmpuint(gwy_str_line_iter_lineno(iter), ==, lineno);
    }
    expected = va_arg(ap, const gchar*);
    g_assert(expected == NULL);
    va_end(ap);
    gwy_str_line_iter_free(iter);
}

void
test_str_line_iter(void)
{
    next_line_check_iter(NULL, NULL);
    next_line_check_iter("", NULL);
    next_line_check_iter("\r", "", NULL);
    next_line_check_iter("\n", "", NULL);
    next_line_check_iter("\r\n", "", NULL);
    next_line_check_iter("\r\r", "", "", NULL);
    next_line_check_iter("\n\n", "", "", NULL);
    next_line_check_iter("\r\n\r\n", "", "", NULL);
    next_line_check_iter("\n\r\n", "", "", NULL);
    next_line_check_iter("\r\r\n", "", "", NULL);
    next_line_check_iter("\r\n\r", "", "", NULL);
    next_line_check_iter("\n\r\r", "", "", "", NULL);
    next_line_check_iter("X", "X", NULL);
    next_line_check_iter("X\n", "X", NULL);
    next_line_check_iter("X\r", "X", NULL);
    next_line_check_iter("X\r\n", "X", NULL);
    next_line_check_iter("\nX", "", "X", NULL);
    next_line_check_iter("\rX", "", "X", NULL);
    next_line_check_iter("\r\nX", "", "X", NULL);
    next_line_check_iter("X\r\r", "X", "", NULL);
    next_line_check_iter("X\n\n", "X", "", NULL);
    next_line_check_iter("X\nY\rZ", "X", "Y", "Z", NULL);
}

void
test_str_isident_ascii(void)
{
    g_assert(gwy_ascii_strisident("a", NULL, NULL));
    g_assert(gwy_ascii_strisident("a0", NULL, NULL));
    g_assert(gwy_ascii_strisident("A", NULL, NULL));
    g_assert(gwy_ascii_strisident("A123", NULL, NULL));
    g_assert(gwy_ascii_strisident("a", "a", "a"));
    g_assert(gwy_ascii_strisident("A_", "_", NULL));
    g_assert(gwy_ascii_strisident("_A", NULL, "_"));
    g_assert(gwy_ascii_strisident("__", "_", "_"));
    g_assert(gwy_ascii_strisident("_12345_", "_", "_"));
    g_assert(gwy_ascii_strisident("+-", "-", "+"));
    g_assert(gwy_ascii_strisident("+a-b", "-", "+"));

    g_assert(!gwy_ascii_strisident("", NULL, NULL));
    g_assert(!gwy_ascii_strisident("123a", NULL, NULL));
    g_assert(!gwy_ascii_strisident("_", NULL, NULL));
    g_assert(!gwy_ascii_strisident("_", "_", NULL));
    g_assert(!gwy_ascii_strisident("_A", "_", NULL));
    g_assert(!gwy_ascii_strisident("A_", NULL, "_"));
    g_assert(!gwy_ascii_strisident("z+a-b", "-", "+"));
}

void
test_str_isident_utf8(void)
{
    // ASCII
    g_assert(gwy_utf8_strisident("a", NULL, NULL));
    g_assert(gwy_utf8_strisident("a0", NULL, NULL));
    g_assert(gwy_utf8_strisident("A", NULL, NULL));
    g_assert(gwy_utf8_strisident("A123", NULL, NULL));

    g_assert(!gwy_utf8_strisident("", NULL, NULL));
    g_assert(!gwy_utf8_strisident("123a", NULL, NULL));
    g_assert(!gwy_utf8_strisident("_", NULL, NULL));
    g_assert(!gwy_utf8_strisident("×", NULL, NULL));
    g_assert(!gwy_utf8_strisident("a×b", NULL, NULL));

    // Non-ASCII
    g_assert(gwy_utf8_strisident("π", NULL, NULL));
    g_assert(gwy_utf8_strisident("Γ2", NULL, NULL));
    gunichar underscore[] = { '_', '\0' };
    g_assert(gwy_utf8_strisident("Γ_α", underscore, NULL));
    g_assert(gwy_utf8_strisident("_Γα", NULL, underscore));
    g_assert(!gwy_utf8_strisident("_Γα", underscore, NULL));
    g_assert(!gwy_utf8_strisident("Γ_α", NULL, underscore));
    g_assert(gwy_utf8_strisident("_Γα_", underscore, underscore));
    gunichar times[] = { g_utf8_get_char("×"), '\0' };
    g_assert(gwy_utf8_strisident("Γ×α", times, NULL));
    g_assert(gwy_utf8_strisident("×Γα", NULL, times));
    g_assert(!gwy_utf8_strisident("×Γα", times, NULL));
    g_assert(!gwy_utf8_strisident("Γ×α", NULL, times));
    g_assert(gwy_utf8_strisident("×Γα×", times, times));
    g_assert(gwy_utf8_strisident("×Γ_α_", underscore, times));
    g_assert(!gwy_utf8_strisident("×Γ_α_", times, underscore));
}

void
test_str_among(void)
{
    g_assert_cmpuint(gwy_stramong("test", NULL), ==, 0);
    g_assert_cmpuint(gwy_stramong("", NULL), ==, 0);
    g_assert_cmpuint(gwy_stramong("a", NULL, "a", NULL), ==, 0);

    g_assert_cmpuint(gwy_stramong("", "", NULL), ==, 1);
    g_assert_cmpuint(gwy_stramong("", "a", "b", "", NULL), ==, 3);
    g_assert_cmpuint(gwy_stramong("a", "", "b", "a", NULL), ==, 3);
    g_assert_cmpuint(gwy_stramong("a", "a ", "ahoy", "a", NULL), ==, 3);
    g_assert_cmpuint(gwy_stramong("ahoy", "a ", "ahoy ", "ahoy", NULL), ==, 3);
    g_assert_cmpuint(gwy_stramong("ahoy", "ahoy", "ahoy", "ahoy", NULL), ==, 1);
}

void
test_str_remove_prefix(void)
{
    gchar *str = g_new0(gchar, 20);

    strcpy(str, "");
    g_assert_cmpuint(gwy_str_remove_prefix(str, NULL), ==, 0);
    g_assert_cmpstr(str, ==, "");

    strcpy(str, "test");
    g_assert_cmpuint(gwy_str_remove_prefix(str, NULL), ==, 0);
    g_assert_cmpstr(str, ==, "test");

    strcpy(str, "test");
    g_assert_cmpuint(gwy_str_remove_prefix(str, "a", "b", "c", NULL), ==, 0);
    g_assert_cmpstr(str, ==, "test");

    strcpy(str, "test");
    g_assert_cmpuint(gwy_str_remove_prefix(str, "testing", NULL), ==, 0);
    g_assert_cmpstr(str, ==, "test");

    strcpy(str, "test");
    g_assert_cmpuint(gwy_str_remove_prefix(str, "test test", NULL), ==, 0);
    g_assert_cmpstr(str, ==, "test");

    strcpy(str, "test");
    g_assert_cmpuint(gwy_str_remove_prefix(str, "t", "tes", "test", NULL),
                     ==, 1);
    g_assert_cmpstr(str, ==, "est");

    strcpy(str, "test");
    g_assert_cmpuint(gwy_str_remove_prefix(str, "test", "tes", "t", NULL),
                     ==, 1);
    g_assert_cmpstr(str, ==, "");

    strcpy(str, "test");
    g_assert_cmpuint(gwy_str_remove_prefix(str, "tes", "t", NULL), ==, 1);
    g_assert_cmpstr(str, ==, "t");

    strcpy(str, "test");
    g_assert_cmpuint(gwy_str_remove_prefix(str, "x", "te", NULL), ==, 2);
    g_assert_cmpstr(str, ==, "st");

    g_free(str);
}

void
test_str_remove_suffix(void)
{
    gchar *str = g_new0(gchar, 20);

    strcpy(str, "");
    g_assert_cmpuint(gwy_str_remove_suffix(str, NULL), ==, 0);
    g_assert_cmpstr(str, ==, "");

    strcpy(str, "test");
    g_assert_cmpuint(gwy_str_remove_suffix(str, NULL), ==, 0);
    g_assert_cmpstr(str, ==, "test");

    strcpy(str, "test");
    g_assert_cmpuint(gwy_str_remove_suffix(str, "a", "b", "c", NULL), ==, 0);
    g_assert_cmpstr(str, ==, "test");

    strcpy(str, "test");
    g_assert_cmpuint(gwy_str_remove_suffix(str, "detest", NULL), ==, 0);
    g_assert_cmpstr(str, ==, "test");

    strcpy(str, "test");
    g_assert_cmpuint(gwy_str_remove_suffix(str, "test test", NULL), ==, 0);
    g_assert_cmpstr(str, ==, "test");

    strcpy(str, "test");
    g_assert_cmpuint(gwy_str_remove_suffix(str, "t", "est", "test", NULL),
                     ==, 1);
    g_assert_cmpstr(str, ==, "tes");

    strcpy(str, "test");
    g_assert_cmpuint(gwy_str_remove_suffix(str, "test", "est", "t", NULL),
                     ==, 1);
    g_assert_cmpstr(str, ==, "");

    strcpy(str, "test");
    g_assert_cmpuint(gwy_str_remove_suffix(str, "est", "t", NULL), ==, 1);
    g_assert_cmpstr(str, ==, "t");

    strcpy(str, "test");
    g_assert_cmpuint(gwy_str_remove_suffix(str, "x", "st", NULL), ==, 2);
    g_assert_cmpstr(str, ==, "te");

    g_free(str);
}


static void
randomize_string(GString *str,
                 GRand *rng,
                 gboolean asciionly)
{
    guint n = g_rand_int(rng);
    n = ((n & 0xff) + ((n >> 8) & 0xff) + ((n >> 16) & 0xff) + (n >> 24))/100;
    g_string_set_size(str, n);

    guint x = 0, remaining = 0;
    for (guint i = 0; i < n; i++) {
        guchar c;
        do {
            if (!remaining) {
                x = g_rand_int(rng);
                remaining = 4;
            }
            c = x & (asciionly ? 0x7f : 0xff);
            x >>= 8;
            remaining--;
        } while (!c);
        str->str[i] = c;
    }
}

void
test_str_case_equal_ascii(void)
{
    enum { niter = 20000 };
    GRand *rng = g_rand_new_with_seed(42);
    GString *str1 = g_string_new(NULL), *str2 = g_string_new(NULL);

    for (guint iter = 0; iter < niter; iter++) {
        randomize_string(str1, rng, FALSE);
        randomize_string(str2, rng, FALSE);
        gconstpointer s1 = str1->str, s2 = str2->str;
        g_assert_cmpuint(gwy_ascii_strcase_equal(s1, s2),
                         ==,
                         !g_ascii_strcasecmp(s1, s2));
    }

    g_string_free(str2, TRUE);
    g_string_free(str1, TRUE);
    g_rand_free(rng);
}

void
test_str_case_hash_ascii(void)
{
    enum { niter = 20000 };
    GRand *rng = g_rand_new_with_seed(42);
    GString *str1 = g_string_new(NULL), *str2 = g_string_new(NULL);

    for (guint iter = 0; iter < niter; iter++) {
        randomize_string(str1, rng, FALSE);
        randomize_string(str2, rng, FALSE);
        gconstpointer s1 = str1->str, s2 = str2->str;
        if (gwy_ascii_strcase_equal(s1, s2))
            g_assert_cmpuint(gwy_ascii_strcase_hash(s1),
                             ==,
                             gwy_ascii_strcase_hash(s2));
        if (gwy_ascii_strcase_hash(s1) != gwy_ascii_strcase_hash(s2))
            g_assert(!gwy_ascii_strcase_equal(s1, s2));
    }

    g_string_free(str2, TRUE);
    g_string_free(str1, TRUE);
    g_rand_free(rng);
}

void
test_str_utf8_append_exponent(void)
{
    GString *str = g_string_new(NULL);

    g_string_truncate(str, 0);
    gwy_utf8_append_exponent(str, 0);
    g_assert_cmpstr(str->str, ==, "⁰");

    g_string_truncate(str, 0);
    gwy_utf8_append_exponent(str, 123456789);
    g_assert_cmpstr(str->str, ==, "¹²³⁴⁵⁶⁷⁸⁹");

    g_string_truncate(str, 0);
    gwy_utf8_append_exponent(str, -123456789);
    g_assert_cmpstr(str->str, ==, "⁻¹²³⁴⁵⁶⁷⁸⁹");

    g_string_free(str, TRUE);
}

static void
str_gstring_replace_one(const gchar *string,
                        const gchar *old,
                        const gchar *replacement,
                        gint count,
                        const gchar *expected_result,
                        guint expected_nrepl)
{
    //g_printerr("<<<%s>>>\n", string);
    GString *str = g_string_new(string);
    guint nrepl = gwy_gstring_replace(str, old, replacement, count);
    g_assert_cmpstr(str->str, ==, expected_result);
    g_assert_cmpuint(nrepl, ==, expected_nrepl);
    g_string_free(str, TRUE);
}

void
test_str_gstring_replace_null_old(void)
{
    str_gstring_replace_one("", NULL, NULL, 0, "", 0);
    str_gstring_replace_one("", NULL, "a", 0, "", 0);
    str_gstring_replace_one("", NULL, "a", -1, "a", 1);
    str_gstring_replace_one("", NULL, "a", 1, "a", 1);
    str_gstring_replace_one("b", NULL, "a", -1, "aba", 2);
    str_gstring_replace_one("b", NULL, "a", 2, "aba", 2);
    str_gstring_replace_one("b", NULL, "a", 1, "ab", 1);
    str_gstring_replace_one("test", NULL, "::", 2, "::t::est", 2);
    str_gstring_replace_one("test", NULL, "::", 4, "::t::e::s::t", 4);
    str_gstring_replace_one("test", NULL, "::", 5, "::t::e::s::t::", 5);
    str_gstring_replace_one("test", NULL, "::", -1, "::t::e::s::t::", 5);
}

void
test_str_gstring_replace_null_replacement(void)
{
    str_gstring_replace_one("", NULL, NULL, 0, "", 0);
    str_gstring_replace_one("aba", "a", NULL, 0, "aba", 0);
    str_gstring_replace_one("aba", "a", NULL, 1, "ba", 1);
    str_gstring_replace_one("aba", "a", NULL, 2, "b", 2);
    str_gstring_replace_one("aba", "a", NULL, -1, "b", 2);
    str_gstring_replace_one("aaaaa", "a", NULL, -1, "", 5);
    str_gstring_replace_one("aaaaa", "aa", NULL, -1, "a", 2);
    str_gstring_replace_one("abaaaa", "aa", NULL, -1, "ab", 2);
    str_gstring_replace_one("abaabaa", "aa", NULL, -1, "abb", 2);
    str_gstring_replace_one("abaabaa", "baa", NULL, -1, "a", 2);
    str_gstring_replace_one("abaabaa", "baa", NULL, 3, "a", 2);
    str_gstring_replace_one("abaabaa", "baa", NULL, 1, "abaa", 1);
    str_gstring_replace_one("abaabaa", "a", NULL, -1, "bb", 5);
    str_gstring_replace_one("abaabaa", "a", NULL, 2, "babaa", 2);
}

void
test_str_gstring_replace_general(void)
{
    str_gstring_replace_one("test", "a", "b", -1, "test", 0);
    str_gstring_replace_one("test", "t", "T", -1, "TesT", 2);
    str_gstring_replace_one("test", "t", "T", 1, "Test", 1);
    str_gstring_replace_one("test", "t", "TT", -1, "TTesTT", 2);
    str_gstring_replace_one("test", "t", "TT", 1, "TTest", 1);
    str_gstring_replace_one("testte", "te", "Zz", 1, "Zzstte", 1);
    str_gstring_replace_one("testte", "te", "Zz", -1, "ZzstZz", 2);
    str_gstring_replace_one("aaaaaa", "aa", "b", -1, "bbb", 3);
    str_gstring_replace_one("aaaaaa", "aa", "b", 1, "baaaa", 1);
    str_gstring_replace_one("aaaaaa", "aa", "bob", 1, "bobaaaa", 1);
    str_gstring_replace_one("aaaaaa", "aa", "bob", -1, "bobbobbob", 3);
    str_gstring_replace_one("xaaaaaaz", "aa", "b", -1, "xbbbz", 3);
    str_gstring_replace_one("xaaaaaaz", "aa", "b", 1, "xbaaaaz", 1);
    str_gstring_replace_one("xaaaaaaz", "aa", "bob", 1, "xbobaaaaz", 1);
    str_gstring_replace_one("xaaaaaaz", "aa", "bob", -1, "xbobbobbobz", 3);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
