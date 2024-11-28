/**
 * Copyright (2017) Baidu Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
// Author: Su Hao (suhao@baidu.com)
//
// Description: Encode & decode the int8 into base16/32/64.

/* This program is an encoder/decoder for Reed-Solomon codes. Encoding is in
   systematic form, decoding via the Berlekamp iterative algorithm.
   In the present form , the constants mm, nn, tt, and kk=nn-2tt must be
   specified  (the double letters are used simply to avoid clashes with
   other n,k,t used in other programs into which this was incorporated!)
   Also, the irreducible polynomial used to generate GF(2**mm) must also be
   entered -- these can be found in Lin and Costello, and also Clark and Cain.

   The representation of the elements of GF(2**m) is either in index form,
   where the number is the power of the primitive element alpha, which is
   convenient for multiplication (add the powers modulo 2**m-1) or in
   polynomial form, where the bits represent the coefficients of the
   polynomial representation of the number, which is the most convenient form
   for addition.  The two forms are swapped between via lookup tables.
   This leads to fairly messy looking expressions, but unfortunately, there
   is no easy alternative when working with Galois arithmetic.

   The code is not written in the most elegant way, but to the best
   of my knowledge, (no absolute guarantees!), it works.
   However, when including it into a simulation program, you may want to do
   some conversion of global variables (used here because I am lazy!) to
   local variables where appropriate, and passing parameters (eg array
   addresses) to the functions  may be a sensible move to reduce the number
   of global variables and thus decrease the chance of a bug being introduced.

   This program does not handle erasures at present, but should not be hard
   to adapt to do this, as it is just an adjustment to the Berlekamp-Massey
   algorithm. It also does not attempt to decode past the BCH bound -- see
   Blahut "Theory and practice of error control codes" for how to do this.

              Simon Rockliff, University of Adelaide   21/9/89

   26/6/91 Slight modifications to remove a compiler dependent bug which hadn't
           previously surfaced. A few extra comments added for clarity.
           Appears to all work fine, ready for posting to net!

                  Notice
                 --------
   This program may be freely modified and/or given to whoever wants it.
   A condition of such distribution is that the author's contribution be
   acknowledged by his name being left in the comments heading the program,
   however no responsibility is accepted for any financial or other loss which
   may result from some unforseen errors or malfunctioning of the program
   during use.
                                 Simon Rockliff, 26th June 1991
*/

#include "lightduer_rscode.h"
#include "lightduer_memory.h"
#include "lightduer_lib.h"
#include "lightduer_log.h"

typedef struct _duer_rscode_s {
    int             mm;
    int             nn;
    int             tt;
    int             kk;

    duer_rsunit_t *alpha_to;
    duer_rsunit_t *index_of;
    duer_rsunit_t *poly;

    duer_rsunit_t *decode_buff;
} duer_rscode_t;

/* generate GF(2**mm) from the irreducible polynomial p(X) in pp[0]..pp[mm]
   lookup tables:  index->polynomial form   alpha_to[] contains j=alpha**i;
                   polynomial form -> index form  index_of[j=alpha**i] = i
   alpha=2 is the primitive element of GF(2**mm)
*/
DUER_LOC_IMPL duer_status_t duer_rscode_gf(duer_rscode_t *rscode, int pp)
{
    int i;
    duer_rsunit_t mask = 1;

    if (rscode == NULL || rscode->alpha_to == NULL || rscode->index_of == NULL) {
        return DUER_ERR_FAILED;
    }

    rscode->alpha_to[rscode->mm] = 0;
    for (i = 0; i < rscode->mm; i++) {
        rscode->alpha_to[i] = mask;
        rscode->index_of[rscode->alpha_to[i]] = i;
        if ((pp >> (rscode->mm - i) & 1) != 0) {
            rscode->alpha_to[rscode->mm] ^= mask;
        }
        mask <<= 1;
    }
    rscode->index_of[rscode->alpha_to[rscode->mm]] = rscode->mm;
    mask >>= 1;
    for (i = rscode->mm + 1; i < rscode->nn; i++) {
        if (rscode->alpha_to[i - 1] >= mask) {
            rscode->alpha_to[i] = rscode->alpha_to[rscode->mm] ^ ((rscode->alpha_to[i - 1] ^ mask) << 1);
        } else {
            rscode->alpha_to[i] = rscode->alpha_to[i - 1] << 1;
        }
        rscode->index_of[rscode->alpha_to[i]] = i;
    }
    rscode->index_of[0] = -1;

    DUER_DUMPD("index_of", rscode->index_of, rscode->nn + 1);
    DUER_DUMPD("alpha_to", rscode->alpha_to, rscode->nn + 1);

    return DUER_OK;
}

/* Obtain the generator polynomial of the tt-error correcting, length
  nn=(2**mm -1) Reed Solomon code  from the product of (X+alpha**i), i=1..2*tt
*/
DUER_LOC_IMPL duer_status_t duer_rscode_gp(duer_rscode_t *rscode)
{
    int i;
    int j;

    if (rscode == NULL || rscode->alpha_to == NULL || rscode->index_of == NULL || rscode->poly == NULL) {
        return DUER_ERR_FAILED;
    }

    rscode->poly[0] = 2;    /* primitive element alpha = 2  for GF(2**mm)  */
    rscode->poly[1] = 1;    /* g(x) = (X+alpha) initially */
    for (i = 2; i <= rscode->nn - rscode->kk; i++) {
        rscode->poly[i] = 1;
        for (j = i - 1; j > 0; j--) {
            if (rscode->poly[j] != 0) {
                rscode->poly[j] = rscode->poly[j - 1] ^ rscode->alpha_to[(rscode->index_of[rscode->poly[j]] + i) % rscode->nn];
            } else {
                rscode->poly[j] = rscode->poly[j - 1];
            }
        }
        rscode->poly[0] = rscode->alpha_to[(rscode->index_of[rscode->poly[0]] + i) % rscode->nn]; /* poly[0] can never be zero */
    }
    /* convert poly[] to index form for quicker encoding */
    for (i = 0; i <= rscode->nn - rscode->kk; i++) {
        rscode->poly[i] = rscode->index_of[rscode->poly[i]];
    }

    DUER_DUMPD("poly", rscode->poly, rscode->nn - rscode->kk + 1);

    return DUER_OK;
}

DUER_INT_IMPL duer_rscode_handler duer_rscode_acquire(int mm, int tt, int pp)
{
    duer_rscode_t *rscode = NULL;
    duer_status_t rs = DUER_OK;

    do {
        int nn = (1 << mm) - 1;
        int kk = nn - 2 * tt;

        rscode = (duer_rscode_t *)DUER_MALLOC(sizeof(duer_rscode_t));
        if (rscode == NULL) {
            rs = DUER_ERR_MEMORY_OVERFLOW;
            break;
        }

        DUER_MEMSET(rscode, 0, sizeof(*rscode));

        rscode->alpha_to = DUER_MALLOC(sizeof(*(rscode->alpha_to)) * (nn + 1));
        if (rscode->alpha_to == NULL) {
            rs = DUER_ERR_MEMORY_OVERFLOW;
            break;
        }

        DUER_MEMSET(rscode->alpha_to, 0, sizeof(*(rscode->alpha_to)) * (nn + 1));

        rscode->index_of = DUER_MALLOC(sizeof(*(rscode->index_of)) * (nn + 1));
        if (rscode->index_of == NULL) {
            rs = DUER_ERR_MEMORY_OVERFLOW;
            break;
        }

        DUER_MEMSET(rscode->index_of, 0, sizeof(*(rscode->index_of)) * (nn + 1));

        rscode->poly = DUER_MALLOC(sizeof(*(rscode->poly)) * (nn - kk + 1));
        if (rscode->poly == NULL) {
            rs = DUER_ERR_MEMORY_OVERFLOW;
            break;
        }

        DUER_MEMSET(rscode->poly, 0, sizeof(*(rscode->poly)) * (nn - kk + 1));

        rscode->mm = mm;
        rscode->nn = nn;
        rscode->tt = tt;
        rscode->kk = kk;

        /* generate the Galois Field GF(2**mm) */
        duer_rscode_gf(rscode, pp);

        /* compute the generator polynomial for this RS code */
        duer_rscode_gp(rscode);
    } while (DUER_FALSE);

    if (rs < DUER_OK) {
        duer_rscode_release(rscode);
        rscode = NULL;
    }

    return rscode;
}

DUER_INT_IMPL duer_size_t duer_rscode_get_total_length(duer_rscode_handler handler)
{
    duer_size_t rs = 0;
    duer_rscode_t *rscode = (duer_rscode_t *)handler;

    if (rscode) {
        rs = rscode->nn;
    }

    return rs;
}

DUER_INT_IMPL duer_size_t duer_rscode_get_parity_length(duer_rscode_handler handler)
{
    duer_size_t rs = 0;
    duer_rscode_t *rscode = (duer_rscode_t *)handler;

    if (rscode) {
        rs = rscode->tt * 2;
    }

    return rs;
}

/* take the string of symbols in data[i], i=0..(k-1) and encode systematically
   to produce 2*tt parity symbols in parity[0]..parity[2*tt-1]
   data[] is input and parity[] is output in polynomial form.
   Encoding is done by using a feedback shift register with appropriate
   connections specified by the elements of poly[], which was generated above.
   Codeword is   c(X) = data(X)*X**(nn-kk)+ b(X)          */
DUER_INT_IMPL duer_status_t duer_rscode_encode(duer_rscode_handler handler, const duer_rsunit_t *data, duer_rsunit_t *parity)
{
    int i, j;
    duer_rsunit_t feedback;
    duer_rscode_t *rscode = (duer_rscode_t *)handler;

    if (rscode == NULL
        || rscode->poly == NULL
        || rscode->alpha_to == NULL
        || rscode->index_of == NULL
        || data == NULL
        || parity == NULL) {
        DUER_LOGE("DUER_ERR_INVALID_PARAMETER");
        return DUER_ERR_INVALID_PARAMETER;
    }

    for (i = 0; i < rscode->nn - rscode->kk; i++) {
        parity[i] = 0;
    }

    for (i = rscode->kk - 1; i >= 0; i--) {
        feedback = rscode->index_of[data[i] ^ parity[rscode->nn - rscode->kk - 1]];
        if (feedback != -1) {
            for (j = rscode->nn - rscode->kk - 1; j > 0; j--)
                if (rscode->poly[j] != -1) {
                    parity[j] = parity[j - 1] ^ rscode->alpha_to[(rscode->poly[j] + feedback) % rscode->nn];
                } else {
                    parity[j] = parity[j - 1];
                }
            parity[0] = rscode->alpha_to[(rscode->poly[0] + feedback) % rscode->nn];
        } else {
            for (j = rscode->nn - rscode->kk - 1; j > 0; j--) {
                parity[j] = parity[j - 1];
            }
            parity[0] = 0;
        }
    }

    return DUER_OK;
}

/* assume we have received bits grouped into mm-bit symbols in recd[i],
   i=0..(nn-1),  and recd[i] is index form (ie as powers of alpha).
   We first compute the 2*tt syndromes by substituting alpha**i into rec(X) and
   evaluating, storing the syndromes in s[i], i=1..2tt (leave s[0] zero) .
   Then we use the Berlekamp iteration to find the error location polynomial
   elp[i].   If the degree of the elp is >tt, we cannot correct all the errors
   and hence just put out the information symbols uncorrected. If the degree of
   elp is <=tt, we substitute alpha**i , i=1..n into the elp to get the roots,
   hence the inverse roots, the error location numbers. If the number of errors
   located does not equal the degree of the elp, we have more than tt errors
   and cannot correct them.  Otherwise, we then solve for the error value at
   the error location and correct the error.  The procedure is that found in
   Lin and Costello. For the cases where the number of errors is known to be too
   large to correct, the information symbols as received are output (the
   advantage of systematic encoding is that hopefully some of the information
   symbols will be okay and that if we are in luck, the errors are in the
   parity part of the transmitted codeword).  Of course, these insoluble cases
   can be returned as error flags to the calling routine if desired.   */
DUER_INT_IMPL duer_status_t duer_rscode_decode(duer_rscode_handler handler, duer_rsunit_t *recd)
{
    duer_rscode_t *rscode = (duer_rscode_t *)handler;
    duer_size_t size = 0;

    int i;
    int j;
    int count = 0;
    int syn_error = 0;

    duer_rsunit_t u;
    duer_rsunit_t q;
    duer_rsunit_t **elp;
    duer_rsunit_t *d;
    duer_rsunit_t *l;
    duer_rsunit_t *u_lu;
    duer_rsunit_t *s;
    duer_rsunit_t *root;
    duer_rsunit_t *loc;
    duer_rsunit_t *z;
    duer_rsunit_t *err;
    duer_rsunit_t *reg;

    if (rscode == NULL
        || rscode->poly == NULL
        || rscode->alpha_to == NULL
        || rscode->index_of == NULL
        || recd == NULL) {
        DUER_LOGE("DUER_ERR_INVALID_PARAMETER");
        return DUER_ERR_INVALID_PARAMETER;
    }

    if (rscode->decode_buff == NULL) {

        // elp
        size += (rscode->nn - rscode->kk + 2) * sizeof(duer_rsunit_t *);
        size += (rscode->nn - rscode->kk + 2) * (rscode->nn - rscode->kk) * sizeof(duer_rsunit_t);

        // d
        size += (rscode->nn - rscode->kk + 2) * sizeof(duer_rsunit_t);

        // l
        size += (rscode->nn - rscode->kk + 2) * sizeof(duer_rsunit_t);

        // u_lu
        size += (rscode->nn - rscode->kk + 2) * sizeof(duer_rsunit_t);

        // s
        size += (rscode->nn - rscode->kk + 2) * sizeof(duer_rsunit_t);

        // root
        size += (rscode->tt) * sizeof(duer_rsunit_t);

        // loc
        size += (rscode->tt) * sizeof(duer_rsunit_t);

        // z
        size += (rscode->tt + 1) * sizeof(duer_rsunit_t);

        // err
        size += (rscode->nn) * sizeof(duer_rsunit_t);

        // reg
        size += (rscode->tt + 1) * sizeof(duer_rsunit_t);

        rscode->decode_buff = DUER_MALLOC(size);
        if (rscode->decode_buff == NULL) {
            DUER_LOGE("Alloc decode buff failed!!!");
            return DUER_ERR_MEMORY_OVERFLOW;
        }
    }

    size = 0;

    // elp
    elp = (duer_rsunit_t **)(rscode->decode_buff + size);
    size += (rscode->nn - rscode->kk + 2) * sizeof(duer_rsunit_t *);

    for (i = 0; i < rscode->nn - rscode->kk + 2; i++) {
        elp[i] = rscode->decode_buff + size;
        size += (rscode->nn - rscode->kk) * sizeof(duer_rsunit_t);
    }

    // d
    d = rscode->decode_buff + size;
    size += (rscode->nn - rscode->kk + 2) * sizeof(duer_rsunit_t);

    // l
    l = rscode->decode_buff + size;
    size += (rscode->nn - rscode->kk + 2) * sizeof(duer_rsunit_t);

    // u_lu
    u_lu = rscode->decode_buff + size;
    size += (rscode->nn - rscode->kk + 2) * sizeof(duer_rsunit_t);

    // s
    s = rscode->decode_buff + size;
    size += (rscode->nn - rscode->kk + 2) * sizeof(duer_rsunit_t);

    // root
    root = rscode->decode_buff + size;
    size += (rscode->tt) * sizeof(duer_rsunit_t);

    // loc
    loc = rscode->decode_buff + size;
    size += (rscode->tt) * sizeof(duer_rsunit_t);

    // z
    z = rscode->decode_buff + size;
    size += (rscode->tt + 1) * sizeof(duer_rsunit_t);

    // err
    err = rscode->decode_buff + size;
    size += (rscode->nn) * sizeof(duer_rsunit_t);

    // reg
    reg = rscode->decode_buff + size;
    size += (rscode->tt + 1) * sizeof(duer_rsunit_t);

    for (i = 0; i < rscode->nn; i++) {
        recd[i] = rscode->index_of[recd[i]] ;          /* put recd[i] into index form */
    }

    /* first form the syndromes */
    for (i = 1; i <= rscode->nn - rscode->kk; i++) {
        s[i] = 0;
        for (j = 0; j < rscode->nn; j++) {
            if (recd[j] != -1) {
                s[i] ^= rscode->alpha_to[(recd[j] + i * j) % rscode->nn]; /* recd[j] in index form */
            }
        }
        /* convert syndrome from polynomial form to index form  */
        if (s[i] != 0) {
            syn_error = 1;    /* set flag if non-zero syndrome => error */
        }
        s[i] = rscode->index_of[s[i]];
    }

    if (syn_error) {     /* if errors, try and correct */
        /* compute the error location polynomial via the Berlekamp iterative algorithm,
           following the terminology of Lin and Costello :   d[u] is the 'mu'th
           discrepancy, where u='mu'+1 and 'mu' (the Greek letter!) is the step number
           ranging from -1 to 2*tt (see L&C),  l[u] is the
           degree of the elp at that step, and u_l[u] is the difference between the
           step number and the degree of the elp.
        */
        /* initialise table entries */
        d[0] = 0;           /* index form */
        d[1] = s[1];        /* index form */
        elp[0][0] = 0;      /* index form */
        elp[1][0] = 1;      /* polynomial form */
        for (i = 1; i < rscode->nn - rscode->kk; i++) {
            elp[0][i] = -1;   /* index form */
            elp[1][i] = 0;   /* polynomial form */
        }
        l[0] = 0;
        l[1] = 0;
        u_lu[0] = -1;
        u_lu[1] = 0;
        u = 0;

        do {
            u++;
            if (d[u] == -1) {
                l[u + 1] = l[u];
                for (i = 0; i <= l[u]; i++) {
                    elp[u + 1][i] = elp[u][i];
                    elp[u][i] = rscode->index_of[elp[u][i]];
                }
            } else { /* search for words with greatest u_lu[q] for which d[q]!=0 */
                q = u - 1;
                while ((d[q] == -1) && (q > 0)) {
                    q--;
                }

                /* have found first non-zero d[q]  */
                if (q > 0) {
                    j = q;
                    do {
                        j--;
                        if ((d[j] != -1) && (u_lu[q] < u_lu[j])) {
                            q = j;
                        }
                    } while (j > 0);
                }

                /* have now found q such that d[u]!=0 and u_lu[q] is maximum */
                /* store degree of new elp polynomial */
                if (l[u] > l[q] + u - q) {
                    l[u + 1] = l[u];
                } else {
                    l[u + 1] = l[q] + u - q;
                }

                /* form new elp(x) */
                for (i = 0; i < rscode->nn - rscode->kk; i++) {
                    elp[u + 1][i] = 0;
                }
                for (i = 0; i <= l[q]; i++) {
                    if (elp[q][i] != -1) {
                        elp[u + 1][i + u - q] = rscode->alpha_to[(d[u] + rscode->nn - d[q] + elp[q][i]) % rscode->nn];
                    }
                }
                for (i = 0; i <= l[u]; i++) {
                    elp[u + 1][i] ^= elp[u][i];
                    elp[u][i] = rscode->index_of[elp[u][i]];  /*convert old elp value to index*/
                }
            }
            u_lu[u + 1] = u - l[u + 1];

            /* form (u+1)th discrepancy */
            if (u < rscode->nn - rscode->kk) { /* no discrepancy computed on last iteration */
                if (s[u + 1] != -1) {
                    d[u + 1] = rscode->alpha_to[s[u + 1]];
                } else {
                    d[u + 1] = 0;
                }
                for (i = 1; i <= l[u + 1]; i++) {
                    if ((s[u + 1 - i] != -1) && (elp[u + 1][i] != 0)) {
                        d[u + 1] ^= rscode->alpha_to[(s[u + 1 - i] + rscode->index_of[elp[u + 1][i]]) % rscode->nn];
                    }
                }
                d[u + 1] = rscode->index_of[d[u + 1]]; /* put d[u+1] into index form */
            }
        } while ((u < rscode->nn - rscode->kk) && (l[u + 1] <= rscode->tt));

        u++;
        if (l[u] <= rscode->tt) {     /* can correct error */
            /* put elp into index form */
            for (i = 0; i <= l[u]; i++) {
                elp[u][i] = rscode->index_of[elp[u][i]];
            }

            /* find roots of the error location polynomial */
            for (i = 1; i <= l[u]; i++) {
                reg[i] = elp[u][i];
            }
            count = 0;
            for (i = 1; i <= rscode->nn; i++) {
                q = 1;
                for (j = 1; j <= l[u]; j++) {
                    if (reg[j] != -1) {
                        reg[j] = (reg[j] + j) % rscode->nn;
                        q ^= rscode->alpha_to[reg[j]];
                    }
                }
                if (!q) {      /* store root and error location number indices */
                    root[count] = i;
                    loc[count] = rscode->nn - i;
                    count++;
                }
            }
            if (count == l[u]) { /* no. roots = degree of elp hence <= tt errors */
                /* form polynomial z(x) */
                for (i = 1; i <= l[u]; i++) {  /* Z[0] = 1 always - do not need */
                    if ((s[i] != -1) && (elp[u][i] != -1)) {
                        z[i] = rscode->alpha_to[s[i]] ^ rscode->alpha_to[elp[u][i]];
                    } else if ((s[i] != -1) && (elp[u][i] == -1)) {
                        z[i] = rscode->alpha_to[s[i]];
                    } else if ((s[i] == -1) && (elp[u][i] != -1)) {
                        z[i] = rscode->alpha_to[elp[u][i]];
                    } else {
                        z[i] = 0;
                    }
                    for (j = 1; j < i; j++) {
                        if ((s[j] != -1) && (elp[u][i - j] != -1)) {
                            z[i] ^= rscode->alpha_to[(elp[u][i - j] + s[j]) % rscode->nn];
                        }
                    }
                    z[i] = rscode->index_of[z[i]];         /* put into index form */
                }

                /* evaluate errors at locations given by error location numbers loc[i] */
                for (i = 0; i < rscode->nn; i++) {
                    err[i] = 0;
                    if (recd[i] != -1) {     /* convert recd[] to polynomial form */
                        recd[i] = rscode->alpha_to[recd[i]];
                    } else {
                        recd[i] = 0;
                    }
                }
                for (i = 0; i < l[u]; i++) { /* compute numerator of error term first */
                    err[loc[i]] = 1;       /* accounts for z[0] */
                    for (j = 1; j <= l[u]; j++) {
                        if (z[j] != -1) {
                            err[loc[i]] ^= rscode->alpha_to[(z[j] + j * root[i]) % rscode->nn];
                        }
                    }
                    if (err[loc[i]] != 0) {
                        err[loc[i]] = rscode->index_of[err[loc[i]]];
                        q = 0;     /* form denominator of error term */
                        for (j = 0; j < l[u]; j++) {
                            if (j != i) {
                                q += rscode->index_of[1 ^ rscode->alpha_to[(loc[j] + root[i]) % rscode->nn]];
                            }
                        }
                        q = q % rscode->nn;
                        err[loc[i]] = rscode->alpha_to[(err[loc[i]] - q + rscode->nn) % rscode->nn];
                        recd[loc[i]] ^= err[loc[i]];  /*recd[i] must be in polynomial form */
                    }
                }

                syn_error = 0;
            } else { /* no. roots != degree of elp => >tt errors and cannot solve */
                for (i = 0; i < rscode->nn; i++) {    /* could return error flag if desired */
                    if (recd[i] != -1) {      /* convert recd[] to polynomial form */
                        recd[i] = rscode->alpha_to[recd[i]];
                    } else {     /* just output received codeword as is */
                        recd[i] = 0;
                    }
                }
            }
        } else {       /* elp has degree has degree >tt hence cannot solve */
            for (i = 0; i < rscode->nn; i++) {   /* could return error flag if desired */
                if (recd[i] != -1) {    /* convert recd[] to polynomial form */
                    recd[i] = rscode->alpha_to[recd[i]];
                } else {
                    recd[i] = 0;     /* just output received codeword as is */
                }
            }
        }
    } else {     /* no non-zero syndromes => no errors: output received codeword */
        for (i = 0; i < rscode->nn; i++) {
            if (recd[i] != -1) {      /* convert recd[] to polynomial form */
                recd[i] = rscode->alpha_to[recd[i]];
            } else {
                recd[i] = 0;
            }
        }
    }

    return syn_error == 0 ? DUER_OK : DUER_ERR_FAILED;
}

DUER_INT_IMPL void duer_rscode_release(duer_rscode_handler handler)
{
    duer_rscode_t *rscode = (duer_rscode_t *)handler;

    if (rscode != NULL) {
        if (rscode->alpha_to != NULL) {
            DUER_FREE(rscode->alpha_to);
            rscode->alpha_to = NULL;
        }

        if (rscode->index_of != NULL) {
            DUER_FREE(rscode->index_of);
            rscode->index_of = NULL;
        }

        if (rscode->poly != NULL) {
            DUER_FREE(rscode->poly);
            rscode->poly = NULL;
        }

        if (rscode->decode_buff != NULL) {
            DUER_FREE(rscode->decode_buff);
            rscode->decode_buff = NULL;
        }

        DUER_FREE(rscode);
    }
}
