/*
 *  $Id$
 *  Copyright (C) 2011 David Nečas (Yeti).
 *  E-mail: yeti@gwyddion.net.
 *
 *  Parts of this code were adapted from GLib, see below for more.
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
 *
 *
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include "libgwy/macros.h"
#include "libgwy/math.h"
#include "libgwy/rand.h"

#define RANDOM_FILE "/dev/urandom"

// MWC8222
#define N 256
#define Q 2.3283064365386962890625e-10
#define Q2 5.42101086242752217003726400434970855712890625e-20
#define S 2.710505431213761085018632002174854278564453125e-20
#define A G_GUINT64_CONSTANT(1540315826)

// Ziggurat
enum {
    NZIGGURAT = 128,
    ZIGGMASK = NZIGGURAT - 1,
};

struct _GwyRand {
    // Generator
    guint8 index;  // The byte-type is tied to N=256 and ensures automated
                   // wrap-around when the end of q[] is reached.
    guint32 carry;
    guint nbool;
    guint nbyte;
    guint32 spare_bools;
    union {
        guint32 u;
        struct {
            guint8 x[4];
        } b;
    } spare_bytes;
    guint32 q[N];
    // For generation of smaller types
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

// Set the array using one of Knuth's generators.
static void
set_seed_knuth(GwyRand *rng)
{
    guint32 *q = rng->q;
    for (guint i = 1; i < N; ++i)
        q[i] ^= 1812433253U*(q[i - 1] ^ (q[i - 1] >> 30)) + i;

    rng->carry = q[N-1] % 61137367U;
    rng->index = N - 1;
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
    // Calling with a 32bit number results in simply setting the first item to
    // the seed and then using the Knuth's algorithm for the rest, as expected.
    gwy_rand_set_seed_array(rng, &seed, 1);
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

    guint32 *q = rng->q;
    gwy_clear(q, N);
    for (guint i = 0; i < seed_length; i++) {
        guint k = (2*i) % N;
        q[k] ^= (guint32)(seed[i] & G_GUINT64_CONSTANT(0xffffffff));
        q[k+1] ^= (guint32)(seed[i] >> 32);
    }
    set_seed_knuth(rng);
    // XXX: All code paths that reset the state must go through here.
    rng->nbool = rng->nbyte = 0;
}

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
#if 0
    // Funny this is slightly slower.
    union {
        guint64 q;
        struct {
            guint32 lo, hi;
        } u;
    } x;
    x.u.lo = generate_uint32(rng);
    x.u.hi = generate_uint32(rng);
    return x.q;
#endif
}

static inline gdouble
generate_double(GwyRand *rng)
{
    /* The compiler can use a more precise type for r than the return value
     * type.  So if we reject values 1.0 or larger the caller can still
     * get 1.0.  Reject all values larger than 1-2⁻⁵³.  This may alter the
     * probability of returning exactly 1-2⁻⁵³ which is however more acceptable
     * than returning 1.0 when we say the interval is open-ended. */
    while (TRUE) {
        // Two conversions from 32bit int seem to be still faster than one
        // conversion from 64bit int even on modern processors.
        // gdouble r = Q2*generate_uint64(rng) + S;
        guint32 hi = generate_uint32(rng), lo = generate_uint32(rng);
        gdouble r = Q*(Q*lo + hi) + S;
        if (G_LIKELY(r <= 0.99999999999999989))
            return r;
    }
}

static inline gboolean
generate_bool(GwyRand *rng)
{
    if (G_UNLIKELY(!rng->nbool--)) {
        rng->spare_bools = generate_uint32(rng);
        rng->nbool = 31;
    }
    gboolean retval = rng->spare_bools & 1;
    rng->spare_bools >>= 1;
    return retval;
}

static inline guint8
generate_byte(GwyRand *rng)
{
    if (!rng->nbyte--) {
        rng->spare_bytes.u = generate_uint32(rng);
        rng->nbyte = 3;
    }
    return rng->spare_bytes.b.x[rng->nbyte];
}

/**
 * gwy_rand_int64:
 * @rng: A random number generator.
 *
 * Obtains the next 64bit number from a random number generator.
 *
 * The returned value is equidistributed over the range [0..2⁶⁴-1].
 *
 * Returns: Random 64bit integer.
 **/
guint64
gwy_rand_int64(GwyRand *rng)
{
    g_return_val_if_fail(rng, 0);
    return generate_uint64(rng);
}

/**
 * gwy_rand_int:
 * @rng: A random number generator.
 *
 * Obtains the next 32bit number from a random number generator.
 *
 * The returned value is equidistributed over the range [0..2³²-1].
 *
 * Returns: Random 32bit integer.
 **/
guint32
gwy_rand_int(GwyRand *rng)
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

    /* TODO: generate small numbers using the 32 bit generator. */
    guint64 len = begin - end;
    guint64 x, max = G_GUINT64_CONSTANT(0xffffffffffffffff)/len*len;
    do {
        x = generate_uint64(rng);
    } while (G_UNLIKELY(x >= max));

    return (gint64)(x % len) + begin;
}

/**
 * gwy_rand_boolean:
 * @rng: A random number generator.
 *
 * Obtains the next boolean from a random number generator.
 *
 * Returns: Random boolean.
 **/
gboolean
gwy_rand_boolean(GwyRand *rng)
{
    return generate_bool(rng);
}

/**
 * gwy_rand_byte:
 * @rng: A random number generator.
 *
 * Obtains the next byte from a random number generator.
 *
 * Returns: Random byte.
 **/
guint8
gwy_rand_byte(GwyRand *rng)
{
    return generate_byte(rng);
}

/**
 * gwy_rand_double:
 * @rng: A random number generator.
 *
 * Obtains the next floating point number from a random number generator.
 *
 * The returned value is equally distributed over the open interval (0,1).
 * The smallest returned value is 2⁻⁶⁵ and the largest is the largerst IEEE
 * double values is smaller than 1.  Note if it is transformed to another
 * interval, the openness is, generally, not guaranteed.
 *
 * The returned value is obtained from a 64bit random integer.  This means
 * small numbers (smaller than 2⁻¹¹) may not have the full 53-bit precision.
 *
 * Returns: Random floating point number.
 **/
gdouble
gwy_rand_double(GwyRand *rng)
{
    return generate_double(rng);
}

// Ziggurat tables for the exponential distribution {{{
static const gdouble ziggurat_exp_x[NZIGGURAT] = {
    7.898315116615642606, 6.898315116615642606, 6.135192844501238473,
    5.665072466085739702, 5.324182373137499384, 5.056174148798510553,
    4.834984367324839779, 4.646421132862482529, 4.481902072262685058,
    4.335840994085857399, 4.204397939328747754, 4.084819616805754587,
    3.975064441378075306, 3.87357649759723393, 3.779142663756101485,
    3.690798693076618195, 3.60776540937241985, 3.529404126123905631,
    3.455184732396476672, 3.384662357961562611, 3.317459990569623274,
    3.25325531138591809, 3.19177057673762192, 3.132764737386632765,
    3.076027226488977801, 3.021373009291722741, 2.968638598903891525,
    2.917678820286674495, 2.86836415985974299, 2.820578577915053575,
    2.774217690067900186, 2.729187245421314858, 2.685401845136825785,
    2.642783857191138617, 2.601262492307359783, 2.56077301312943841,
    2.521256054197794076, 2.482657034573778486, 2.444925648337809207,
    2.408015420863446085, 2.371883320906367544, 2.336489420262991619,
    2.301796594139413828, 2.267770256497069992, 2.234378125560768155,
    2.201590015429123811, 2.169377650349496779, 2.137714498734808814,
    2.106575624428295004, 2.075937553080334795, 2.045778151801814649,
    2.016076520511259825, 1.986812893606513652, 1.957968550772741169,
    1.929525735892441045, 1.901467583154417294, 1.873778049570940175,
    1.846441853208617724, 1.81944441652128624, 1.792771814244565055,
    1.766410725373316598, 1.740348388796530901, 1.7145725622103042,
    1.689071483969600461, 1.663833837574208357, 1.638848718514436316,
    1.614105603228214454, 1.589594319943889589, 1.565305021202523086,
    1.541228157870277194, 1.517354454465790933, 1.493674885639536968,
    1.470180653652205693, 1.446863166707330549, 1.423714017999759576,
    1.400724965346267138, 1.377887911267626252, 1.355194883392831199,
    1.332638015055841014, 1.310209525953137995, 1.28790170272645016,
    1.265706879329014189, 1.243617417025542627, 1.221625683865328604,
    1.199724033454319249, 1.17790478283506834, 1.15616018926267901,
    1.134482425639467292, 1.112863554340239821, 1.091295499122686538,
    1.06977001477206407, 1.048278654074353167, 1.026812731645221303,
    1.005363284060623293, 0.9839210256351705929, 0.9624762990719225034,
    0.9410190200560897862, 0.9195386146775997245, 0.8980239483334632255,
    0.8764632444670772109, 0.8548439911302239479, 0.8331528328807094534,
    0.81137544492190183, 0.7894963856054505447, 0.7674989223936082809,
    0.74536482502650455, 0.7230741178398832422, 0.700604780753902613,
    0.6779323851462465009, 0.6550296462513996425, 0.6318658673168977289,
    0.6084062416121217155, 0.584610965138341206, 0.5604340933044594482,
    0.535822045250432114, 0.5107116137288675827, 0.4850272656948706141,
    0.4586773994899662058, 0.4315490220368352075, 0.4034999515155241413,
    0.3743469874287288057, 0.3438471885662818223, 0.3116666672586600087,
    0.2773250692759120968, 0.2400880527238222816, 0.1987336555292024457,
    0.1509526859368724309, 0.09133951810290360708,
};

static const gdouble ziggurat_exp_y[NZIGGURAT] = {
    0.001009484861243412576, 0.002165307609953275407, 0.003464896676122502728,
    0.00487233313651831522, 0.006369883179090378291, 0.007946812553931661003,
    0.009595882940474610156, 0.01131187667233022608, 0.01309086010637215077,
    0.01492977199230825339, 0.01682617420128337582, 0.01877809136960827542,
    0.02078390276138267666, 0.02284226653566188778, 0.02495206503996838376,
    0.02711236426043523456, 0.02932238310446546599, 0.03158146969660075734,
    0.03388908279317268768, 0.03624477700911079536, 0.03864819093489093702,
    0.04109903747978040572, 0.04359709595480498008, 0.04614220553306357475,
    0.04873425981362800594, 0.05137320227953114609, 0.05405902248765531462,
    0.05679175286363620681, 0.05957146600157450969, 0.06239827238874037615,
    0.06527231849121416642, 0.06819378514870714935, 0.07116288623649881295,
    0.07417986756013523486, 0.07724500595471574985, 0.08035860856559677534,
    0.08352101229142159601, 0.0867325833737407979, 0.08999371712027174721,
    0.09330483775117473198, 0.09666639835969011231, 0.1000788809801575466,
    0.1035427967578826796, 0.1070586862165747628, 0.1106271196201878715,
    0.1142486974269891913, 0.1179240508345754481, 0.1216538424153840123,
    0.1254387668430163457, 0.1292795517104236465, 0.1331769584417112426,
    0.1371317833000115351, 0.1411448584945661061, 0.1452170533908563133,
    0.149349275828338198, 0.1535424735510815932, 0.1577976357573947737,
    0.1621157947753449835, 0.1664980278719723729, 0.1709454592049517097,
    0.1754592619264950883, 0.1800406604504234275, 0.1846909328945799893,
    0.1894114137121325225, 0.1942034965268311852, 0.1990686371889790412,
    0.2040083570707556827, 0.2090242466216411775, 0.2141179692070502316,
    0.2192912652559435872, 0.2245459567461798576, 0.2298839520597581833,
    0.2353072512439410834, 0.2408179517186089161, 0.2464182544751663334,
    0.2521104708179959642, 0.257897029705952457, 0.2637804857588501161,
    0.2697635280024854898, 0.2758489894356506971, 0.282039857514071897,
    0.2883392856595366012, 0.2947506059179997617, 0.3012773429086019131,
    0.307923229226805006, 0.3146922224898792347, 0.3215885242425267446,
    0.3286166009754556525, 0.3357812075513987492, 0.3430874133828745711,
    0.3505406317657550693, 0.3581466528447533622, 0.3659116807742165085,
    0.3738423757438496994, 0.3819459016690187125, 0.390229980505289026,
    0.3987029543449445121, 0.4073738566999793698, 0.4162524946854696975,
    0.4253495442079531132, 0.4346766607605439075, 0.4442466090640085305,
    0.4540734156175651835, 0.4641725492993982725, 0.4745611365754393665,
    0.4852582197645608394, 0.4962850693541613808, 0.5076655648326160164,
    0.5194266633053397682, 0.5315989818945557644, 0.5442175295198774863,
    0.5573226375779842879, 0.5709611596294253183, 0.5851880413177632572,
    0.6000684099219086275, 0.6156804095812435907, 0.632119133965388683,
    0.6495022218359038909, 0.6679780591855548159, 0.6877382339008341317,
    0.7090372687730501066, 0.7322255623077419793, 0.7578081169315134915,
    0.7865585993901363869, 0.8197682049873180305, 0.8598883825099514965,
    0.9127077774751252199, 1,
};
// }}}

static gdouble
generate_exp_oneside(GwyRand *rng)
{
    const gdouble *zx = ziggurat_exp_x, *zy = ziggurat_exp_y;

    while (TRUE) {
        guint32 l = generate_byte(rng);
        guint level = l & ZIGGMASK;

        gdouble x = generate_double(rng) * zx[level];
        if (G_LIKELY(level != ZIGGMASK && x < zx[level+1]))
            return x;

        if (G_LIKELY(level != 0)) {
            gdouble y = generate_double(rng) * (zy[level] - zy[level-1])
                        + zy[level-1];
            if (y <= exp(-x))
                return x;
        }
        else {
            // Exponential distribution can be generated recursively.
            gdouble x1 = zx[1];
            return x1 + generate_exp_oneside(rng);
        }
    }
}

/**
 * gwy_rand_exp_positive:
 * @rng: A random number generator.
 *
 * Obtains the next one-side exponentially distributed number from a random
 * number generator.
 *
 * The probability distribution function is exp(-@x).
 *
 * Returns: Random floating point number.
 **/
gdouble
gwy_rand_exp_positive(GwyRand *rng)
{
    return generate_exp_oneside(rng);
}

/**
 * gwy_rand_exp:
 * @rng: A random number generator.
 *
 * Obtains the next two-side exponentially distributed number from a random
 * number generator.
 *
 * The probability distribution function is exp(-|@x|)/2.
 *
 * Returns: Random floating point number.
 **/
gdouble
gwy_rand_exp(GwyRand *rng)
{
    gdouble x = generate_exp_oneside(rng);
    return generate_bool(rng) ? x : -x;
}

// Ziggurat tables for the normal distribution {{{
static const gdouble ziggurat_normal_x[NZIGGURAT] = {
    3.713086246740363261, 3.442619855896652121, 3.223084984578618545,
    3.083228858214213701, 2.978696252645016961, 2.894344007018670621,
    2.823125350545966438, 2.761169372384153851, 2.706113573118722337,
    2.6564064112581925, 2.610972248428613203, 2.569033625921639133,
    2.530009672385466617, 2.493454522091950761, 2.459018177408350094,
    2.426420645530211593, 2.395434278007467342, 2.365871370113987544,
    2.337575241335530735, 2.310413683695002156, 2.284274059673656806,
    2.259059573865329525, 2.23468639558705698, 2.211081408874727811,
    2.188180432072020609, 2.165926793744840738, 2.144270182356261352,
    2.12316570866978996, 2.102573135184998884, 2.082456237987724644,
    2.062782274503963357, 2.043521536650669498, 2.024646973372933878,
    2.00613386995896684, 1.987959574123060724, 1.970103260849713224,
    1.952545729548888906, 1.935269228291900201, 1.91825730085973203,
    1.901494653100317614, 1.884967035702869238, 1.868661140989542009,
    1.852564511723087062, 1.836665460253384045, 1.820952996591005074,
    1.805416764214048742, 1.790046982594618986, 1.774834395580769246,
    1.759770224894231875, 1.744846128108376509, 1.730054160558243535,
    1.715386740708116548, 1.700836618564300944, 1.686396846773486326,
    1.672060754091852207, 1.657821920948207546, 1.643674156856982649,
    1.629611479464678396, 1.615628095037132964, 1.601718380215277059,
    1.587876864884400702, 1.574098216016749722, 1.560377222359840687,
    1.546708779853503461, 1.533087877667556079, 1.519509584759370781,
    1.50596903685655026, 1.492461423774615408, 1.478981976983097855,
    1.465525957335794628, 1.452088642882216493, 1.438665316677461314,
    1.425251254506861573, 1.411841712439760251, 1.398431914123606352,
    1.385017037725148645, 1.37159220241973227, 1.358152454322422874,
    1.344692751745713043, 1.331207949657676502, 1.317692783201342991,
    1.304141850120421539, 1.290549591917873151, 1.276910273551699718,
    1.263217961446028231, 1.249466499564333748, 1.235649483254481175,
    1.221760230530962568, 1.207791750406757603, 1.193736707823772199,
    1.179587384654460703, 1.165335636155046908, 1.150972842138976065,
    1.136489852003075535, 1.121876922572254066, 1.107123647523535398,
    1.092218876896553761, 1.077150624881937657, 1.0619059636836194,
    1.046470900752580263, 1.030830236056455591, 1.014967395239299472,
    0.998864233480643513, 0.9825008035027603848, 0.9658550793881305949,
    0.9489026254979119538, 0.9316161966013538105, 0.9139652510088017764,
    0.8959153525662385289, 0.8774274290977156914, 0.8584568431780508635,
    0.8389522142812074557, 0.8188539066833177233, 0.7980920606262748045,
    0.776583987876148386, 0.7542306644345100715, 0.7309119106218812815,
    0.7064796113136080346, 0.6807479186459042167, 0.653478638715042387,
    0.6243585973090882211, 0.5929629424419779791, 0.5586921783755179714,
    0.5206560387251449176, 0.4774378372537878768, 0.4265479863033051249,
    0.3628714310284183042, 0.2723208647046638506,
};

static const gdouble ziggurat_normal_y[NZIGGURAT] = {
    0.002669629083902503509, 0.005548995220816470539, 0.008624484412930470968,
    0.01183947865798231371, 0.01516729801067204247, 0.01859210273716581265,
    0.02210330461611159262, 0.02569329193614961657, 0.02935631744025382962,
    0.03308788614650515557, 0.03688438878696877413, 0.04074286807479060463,
    0.0446608622008724298, 0.04863629586028405188, 0.05266740190350316979,
    0.05675266348153858419, 0.06089077034856637597, 0.06508058521363187375,
    0.0693211173941802526, 0.07361150188475489339, 0.07795098251465471419,
    0.08233889824295740824, 0.086774671895542969, 0.0912578008276347102,
    0.09578784912257815216, 0.1003644410295455401, 0.1049872554103545398,
    0.109656021015817761, 0.1143705124498882745, 0.1191305467087185877,
    0.1239359802039817425, 0.1287867061971039611, 0.1336826525846476412,
    0.138623779985851037, 0.1436100800919329947, 0.1486415742436969657,
    0.1537183122095865707, 0.1588403711409350781, 0.1640078546849277479,
    0.1692208922389247518, 0.174479638332402323, 0.1797842721249621142,
    0.1851349970107134322, 0.1905320403209137211, 0.195975653118110414,
    0.2014661100762032412, 0.2070037094418738006, 0.2125887730737361006,
    0.218221646556370596, 0.2239026993871338875, 0.2296323252343027036,
    0.235410942265727656, 0.2412389935477513161, 0.2471169475146967358,
    0.2530452985097658593, 0.2590245673987107426, 0.2650553022581619403,
    0.2711380791410252734, 0.2772735029218977115, 0.2834622082260125178,
    0.2897048604458104977, 0.2960021568498558366, 0.3023548277894797627,
    0.3087636380092519228, 0.3152293880681575222, 0.3217529158792086203,
    0.3283350983761523961, 0.3349768533169711615, 0.3416791412350136841,
    0.3484429675498724693, 0.3552693848515471443, 0.3621594953730332116,
    0.3691144536682751395, 0.3761354695144544295, 0.3832238110598836459,
    0.3903808082413894892, 0.3976078564980425521, 0.404906420811488351,
    0.4122780401070246206, 0.4197243320540382347, 0.4272469983095623988,
    0.4348478302546618964, 0.4425287152802466148, 0.4502916436869269609,
    0.4581387162728719648, 0.4660721526945709792, 0.4740943006982496045,
    0.4822076463348386906, 0.4904148252893216374, 0.4987186354765843242,
    0.5071220510813045895, 0.5156282382498720519, 0.5242405726789927981,
    0.5329626593899875884, 0.5417983550317241231, 0.5507517931210552774,
    0.5598274127106948179, 0.5690299910747216123, 0.5783646811267023128,
    0.5878370544418205257, 0.5974531509518122821, 0.6072195366326048855,
    0.6171433708265624887, 0.6272324852578145658, 0.6374954773431448743,
    0.6479418211185508088, 0.6585820000586536801, 0.6694276673577061689,
    0.6804918410064143336, 0.6917891434460358528, 0.7033360990258174163,
    0.7151515074204770437, 0.7272569183545058779, 0.7396772436833381485,
    0.7524415591857038015, 0.7655841739092359948, 0.7791460859417031657,
    0.7931770117838592105, 0.8077382946961211134, 0.8229072113952620005,
    0.8387836053106472238, 0.8555006078850642842, 0.8732430489268535888,
    0.892281650802302723, 0.91304364799203806, 0.9362826817083710755,
    0.9635996931557675996, 1,
};
// }}}

static gdouble
generate_normal_oneside(GwyRand *rng)
{
    const gdouble *zx = ziggurat_normal_x, *zy = ziggurat_normal_y;

    while (TRUE) {
        guint32 l = generate_byte(rng);
        guint level = l & ZIGGMASK;

        gdouble x = generate_double(rng) * zx[level];
        if (G_LIKELY(level != ZIGGMASK && x < zx[level+1]))
            return x;

        if (G_LIKELY(level != 0)) {
            gdouble y = generate_double(rng) * (zy[level] - zy[level-1])
                        + zy[level-1];
            if (y <= exp(-x*x/2.0))
                return x;
        }
        else {
            // Normal distribution tail is generated using Marsaglia's method.
            gdouble y, x1 = zx[1];
            do {
                x = generate_exp_oneside(rng)/x1;
                y = generate_exp_oneside(rng);
            } while (G_LIKELY(2*y > x*x));
            return x1 + x;
        }
    }
}

/**
 * gwy_rand_normal_positive:
 * @rng: A random number generator.
 *
 * Obtains the next half-normally distributed number from a random number
 * generator.
 *
 * The probability distribution function is exp(-@x²/2)*sqrt(2/π).
 *
 * Returns: Random floating point number.
 **/
gdouble
gwy_rand_normal_positive(GwyRand *rng)
{
    return generate_normal_oneside(rng);
}

/**
 * gwy_rand_normal:
 * @rng: A random number generator.
 *
 * Obtains the next normally distributed number from a random number generator.
 *
 * The probability distribution function is exp(-@x²/2)/sqrt(2π).
 *
 * Returns: Random floating point number.
 **/
gdouble
gwy_rand_normal(GwyRand *rng)
{
    gdouble x = generate_normal_oneside(rng);
    return generate_bool(rng) ? x : -x;
}

/**
 * SECTION: rand
 * @title: GwyRand
 * @short_description: Random number generation
 *
 * The Gwyddion random number generator, #GwyRand, has an interface similar to
 * #GRand.  However, it is faster and provides more useful floating point
 * number guarantees.  It can also generate 64bit integers or normally
 * distributed floating point numbers.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
