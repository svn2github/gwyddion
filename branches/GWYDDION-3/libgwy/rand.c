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
   The 64bit Mersenne Twister generator code was adapted from:

   A C-program for MT19937-64 (2004/9/29 version).
   Coded by Takuji Nishimura and Makoto Matsumoto.

   This is a 64-bit version of Mersenne Twister pseudorandom number
   generator.

   Before using, initialize the state by using init_genrand64(seed)
   or init_by_array64(init_key, key_length).

   Copyright (C) 2004, Makoto Matsumoto and Takuji Nishimura,
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

     1. Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.

     2. Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.

     3. The names of its contributors may not be used to endorse or promote
        products derived from this software without specific prior written
        permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE.

   References:
   T. Nishimura, ``Tables of 64-bit Mersenne Twisters''
     ACM Transactions on Modeling and
     Computer Simulation 10. (2000) 348--357.
   M. Matsumoto and T. Nishimura,
     ``Mersenne Twister: a 623-dimensionally equidistributed
       uniform pseudorandom number generator''
     ACM Transactions on Modeling and
     Computer Simulation 8. (Jan. 1998) 3--30.

   Any feedback is very welcome.
   http://www.math.hiroshima-u.ac.jp/~m-mat/MT/emt.html
   email: m-mat @ math.sci.hiroshima-u.ac.jp (remove spaces)
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

#define NN 312
#define MM 156
#define ONE G_GUINT64_CONSTANT(1)
#define MATRIX_A G_GUINT64_CONSTANT(0xb5026f5aa96619e9)
/* Most significant 33 bits */
#define UM G_GUINT64_CONSTANT(0xffffffff80000000)
/* Least significant 31 bits */
#define LM G_GUINT64_CONSTANT(0x7fffffff)

struct _GwyRand {
    // The array for the state vector.
    guint64 mt[NN];
    guint mti;
    // Auxiliary data for 32bit number generation.
    gboolean has_spare32;
    guint32 spare32;
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
    guint64 *mt = rng->mt;

    // Zero seed would produce only zeroes, fix it to some other value
    // (the default seed of the original generator).
    if (!seed)
        seed = G_GUINT64_CONSTANT(5489);

    mt[0] = seed;
    for (guint mti = 1; mti < NN; mti++)
        mt[mti] = (G_GUINT64_CONSTANT(6364136223846793005)
                   *(mt[mti-1] ^ (mt[mti-1] >> 62)) + mti);
    rng->mti = NN;
    rng->has_spare32 = FALSE;
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

    guint64 *mt = rng->mt;
    guint64 i = 1, j = 0, k = MAX(NN, seed_length);

    for (; k; k--) {
        mt[i] = ((mt[i] ^ ((mt[i-1] ^ (mt[i-1] >> 62))
                          *G_GUINT64_CONSTANT(3935559000370003845)))
                 + seed[j] + j); // non linear
        i++;
        j++;
        if (i >= NN) {
            mt[0] = mt[NN-1];
            i = 1;
        }
        if (j >= seed_length)
            j = 0;
    }
    for (k = NN-1; k; k--) {
        mt[i] = ((mt[i] ^ ((mt[i-1] ^ (mt[i-1] >> 62))
                          *G_GUINT64_CONSTANT(2862933555777941757)))
                 - i); // non linear
        i++;
        if (i >= NN) {
            mt[0] = mt[NN-1];
            i = 1;
        }
    }

    // MSB is 1; assuring non-zero initial array.
    // FIXME: This looks a bit weird.
    mt[0] = ONE << 63;
    rng->has_spare32 = FALSE;
}

static void
generate_block(GwyRand *rng)
{
    static const guint64 mag01[2] = { 0, MATRIX_A };

    guint64 *mt = rng->mt;
    guint i;

    for (i = 0; i < NN-MM; i++) {
        guint64 x = (mt[i] & UM) | (mt[i+1] & LM);
        mt[i] = mt[i+MM] ^ (x >> 1) ^ mag01[(guint)(x & ONE)];
    }
    for (; i < NN-1; i++) {
        guint64 x = (mt[i] & UM) | (mt[i+1] & LM);
        mt[i] = mt[i + (MM-NN)] ^ (x >> 1) ^ mag01[(guint)(x & ONE)];
    }
    guint64 x = (mt[NN-1] & UM) | (mt[0] & LM);
    mt[NN-1] = mt[MM-1] ^ (x >> 1) ^ mag01[(guint)(x & ONE)];

    rng->mti = 0;
}

static guint64
generate_uint64(GwyRand *rng)
{
    // Generate NN words at one time.
    if (G_UNLIKELY(rng->mti >= NN))
        generate_block(rng);

    guint64 x = rng->mt[rng->mti++];
    x ^= (x >> 29) & G_GUINT64_CONSTANT(0x5555555555555555);
    x ^= (x << 17) & G_GUINT64_CONSTANT(0x71d67fffeda60000);
    x ^= (x << 37) & G_GUINT64_CONSTANT(0xfff7eee000000000);
    x ^= (x >> 43);

    return x;
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
    rng->has_spare32 = FALSE;
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
    if (rng->has_spare32) {
        rng->has_spare32 = FALSE;
        return rng->spare32;
    }
    guint64 x = generate_uint64(rng);
    rng->spare32 = (guint32)(x >> 32);
    rng->has_spare32 = TRUE;
    return (guint32)(x & G_GUINT64_CONSTANT(0xffffffff));
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
