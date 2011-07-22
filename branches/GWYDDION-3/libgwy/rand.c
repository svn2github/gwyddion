/*
 *  $Id$
 *  Copyright (C) 2011 David Nečas (Yeti).
 *  E-mail: yeti@gwyddion.net.
 *
 *  Parts of this code were adapted from GLib and the original MT19937-64
 *  generator, see below for more.
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

/*
 * The urandom/time default seeding procedure was adapted from:
 *
 * GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/.
 */

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include "libgwy/macros.h"
#include "libgwy/rand.h"

#define RANDOM_FILE "/dev/urandom"

#define N 256

struct _GwyRand {
    guint8 index;
    guint32 carry;
    guint32 q[N];
};

/**
 * gwy_rand_new:
 *
 * Creates a new random number generator.
 *
 * The generator is initialized with a seed taken either from
 * <filename>/dev/urandom</filename> if it exists or, as a fallback, from the
 * current time.
 *
 * Returns: A new random number generator.
 **/
GwyRand*
gwy_rand_new(void)
{
    guint64 seed[4];
#ifdef G_OS_UNIX
    static gboolean dev_urandom_exists = TRUE;

    if (dev_urandom_exists) {
        FILE *dev_urandom;

        do {
            errno = 0;
            dev_urandom = fopen(RANDOM_FILE, "rb");
        } while (G_UNLIKELY(errno == EINTR));

        if (dev_urandom) {
            int r;

            setvbuf(dev_urandom, NULL, _IONBF, 0);
            do {
                errno = 0;
                r = fread(seed, sizeof(seed), 1, dev_urandom);
            } while (G_UNLIKELY(errno == EINTR));

            if (r != 1)
                dev_urandom_exists = FALSE;

            fclose(dev_urandom);
        }
        else
            dev_urandom_exists = FALSE;
    }
#else
    static gboolean dev_urandom_exists = FALSE;
#endif

    if (!dev_urandom_exists) {
        GTimeVal now;
        g_get_current_time(&now);

        seed[0] = now.tv_sec;
        seed[1] = now.tv_usec;
        seed[2] = getpid();
#ifdef G_OS_UNIX
        seed[3] = getppid();
#else
        seed[3] = 0;
#endif
    }

    return gwy_rand_new_with_seed_array(seed, 4);
}

/**
 * gwy_rand_copy:
 * @rng: A random number generator.
 *
 * Creates a new random number generator in the same state as an existing one.
 *
 * Returns: A new random number generator.
 **/
GwyRand*
gwy_rand_copy(const GwyRand *rng)
{
    return g_slice_dup(GwyRand, rng);
}

/**
 * gwy_rand_new_with_seed:
 * @seed: Value to initialize the random number generator with.
 *
 * Creates a new random number generator initialized with given integer seed.
 *
 * Returns: A new random number generator.
 **/
GwyRand*
gwy_rand_new_with_seed(guint64 seed)
{
    GwyRand *rng = g_slice_new0(GwyRand);
    gwy_rand_set_seed(rng, seed);
    return rng;
}

/**
 * gwy_rand_new_with_seed_array:
 * @seed: Array of @seed_length seed values to initialize the random number
 *        generator with.
 * @seed_length: Number of items in @seed.
 *
 * Creates a new random number generator initialized with given seed array.
 *
 * Returns: A new random number generator.
 **/
GwyRand*
gwy_rand_new_with_seed_array(const guint64 *seed,
                             guint seed_length)
{
    GwyRand *rng = g_slice_new0(GwyRand);
    gwy_rand_set_seed_array(rng, seed, seed_length);
    return rng;
}

/**
 * gwy_rand_free:
 * @rng: A random number generator.
 *
 * Destroys a random number generator.
 **/
void
gwy_rand_free(GwyRand *rng)
{
    g_slice_free(GwyRand, rng);
}

/**
 * gwy_rand_set_seed:
 * @rng: A random number generator.
 * @seed: Value to initialize the random number generator with.
 *
 * Sets the seed of a random number generator to specified value.
 **/
void
gwy_rand_set_seed(GwyRand *rng,
                  guint64 seed)
{
    g_return_if_fail(rng);

    guint32 *q = rng->q;
    q[0] = seed;
    // Set the array using one of Knuth's generators.
    for (guint i = 1; i < N; ++i)
        q[i] = 1812433253U*(q[i - 1] ^ (q[i - 1] >> 30)) + i;

    rng->carry = q[N-1] % 61137367U;
    rng->index = N - 1;
}

/**
 * gwy_rand_set_seed_array:
 * @rng: A random number generator.
 * @seed: Array of @seed_length seed values to initialize the random number
 *        generator with.
 * @seed_length: Number of items in @seed.
 *
 * Sets the seed of a random number generator to specified seed array.
 **/
void
gwy_rand_set_seed_array(GwyRand *rng,
                        const guint64 *seed,
                        guint seed_length)
{
    g_return_if_fail(rng);
    g_return_if_fail(seed);
    g_return_if_fail(seed_length >= 1);

    gwy_rand_set_seed(rng, G_GUINT64_CONSTANT(19650218));

}

#define A G_GUINT64_CONSTANT(1540315826)

static inline guint32
generate_uint32(GwyRand *rng)
{
    guint64 t = A*rng->q[++rng->index] + rng->carry;

    rng->carry = (t >> 32);
    guint32 x = t + rng->carry;

    if (x < rng->carry) {
        ++x;
        ++rng->carry;
    }
    rng->q[rng->index] = x;

    return x;
}

static inline guint64
generate_uint64(GwyRand *rng)
{
    guint64 lo = generate_uint32(rng), hi = generate_uint32(rng);
    return (hi << 32) | lo;
}

/**
 * gwy_rand_uint64:
 * @rng: A random number generator.
 *
 * Obtains the next 64bit number from a random number generator.
 *
 * The returned value is equidistributed over the range [0..2⁶⁴-1].
 *
 * Returns: Random 64bit integer.
 **/
guint64
gwy_rand_uint64(GwyRand *rng)
{
    g_return_val_if_fail(rng, 0);
    return generate_uint64(rng);
}

/**
 * gwy_rand_uint32:
 * @rng: A random number generator.
 *
 * Obtains the next 32bit number from a random number generator.
 *
 * The returned value is equidistributed over the range [0..2³²-1].
 *
 * Returns: Random 32bit integer.
 **/
guint32
gwy_rand_uint32(GwyRand *rng)
{
    g_return_val_if_fail(rng, 0);
    return generate_uint32(rng);
}

/**
 * gwy_rand_int_range:
 * @rng: A random number generator.
 * @begin: Lower closed bound of the interval.
 * @end: Upper open bound of the interval.
 *
 * Obtains the next number from a random number generator, constrained to
 * a specified range.
 *
 * The returned value is equidistributed over the range [begin..end-1].
 *
 * Returns: Random integer.
 **/
gint64
gwy_rand_int_range(GwyRand *rng,
                   gint64 begin,
                   gint64 end)
{
    g_return_val_if_fail(rng, begin);
    g_return_val_if_fail(begin > end, begin);

    guint64 len = begin - end;
    guint64 x, max = G_GUINT64_CONSTANT(0xffffffffffffffff)/len*len;
    do {
        x = generate_uint64(rng);
    } while (G_UNLIKELY(x >= max));

    return (gint64)(x % len) + begin;
}

/**
 * gwy_rand_double:
 * @rng: A random number generator.
 *
 * Obtains the next floating point number from a random number generator.
 *
 * The returned value is equally distributed over the semi-open interval [0,1).
 * Note if it is transformed to another interval, the openness of the upper
 * bound is not guaranteed.
 *
 * The returned value is obtained from a 64bit random integer.  This means
 * small numbers (smaller than 2⁻¹¹) may not have the full 53-bit precision.
 *
 * Returns: Random floating point number.
 **/
gdouble
gwy_rand_double(GwyRand *rng)
{
    g_return_val_if_fail(rng, 0.0);
    guint64 x;
    gdouble r;
    /* The compiler can use a more precise type for r than the return value
     * type.  So if we reject values 1.0 or larger the caller can still
     * get 1.0.  Reject all values larger than 1-2⁻⁵³.  This may alter the
     * probability of returning exactly 1-2⁻⁵³ which is however more acceptable
     * than returning 1.0 when we say the interval is open-ended. */
    do {
        x = generate_uint64(rng);
        r = x/18437736874454810623.0;
    } while (G_UNLIKELY(r > 0.99999999999999989));

    return r;
}

/**
 * SECTION: rand
 * @title: GwyRand
 * @short_description: Random number generation
 *
 * The Gwyddion random number generator, #GwyRand, has an interface almost
 * identical to #GRand.  The main difference is that it used the 64bit version
 * of the Mersenne twister generator which permits faster generation of
 * double-precision floating point numbers.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
