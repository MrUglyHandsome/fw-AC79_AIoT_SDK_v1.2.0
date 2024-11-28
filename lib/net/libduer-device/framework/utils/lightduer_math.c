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

#include "lightduer_math.h"
#include "lightduer_log.h"

DUER_INT_IMPL const duer_coef_t PI         = DUER_QCONST(3.141592654);
DUER_INT_IMPL const duer_coef_t PI_DOUBLE  = DUER_QCONST(3.141592654 * 2);
DUER_INT_IMPL const duer_coef_t PI_HALF    = DUER_QCONST(3.141592654 / 2);

#define DUER_PERCISION      (12)

DUER_INT_IMPL duer_coef_t duer_coef_sin(duer_coef_t x)
{
    duer_coef_t n = x;
    duer_coef_t sum = 0;

    int i = 1;

    // Eliminate the (2 * k * PI) as below:
    //      sin(x) = sin(2 * k * PI + x)     {k = 0, 1, 2...}
    n = (duer_coef_t)((int)(x / PI_DOUBLE));
    n = x - n * PI_DOUBLE;
    x = n;

    do {
        sum += n;
        i++;

        n = DUER_COEF_MUL(n, x);
        n = DUER_COEF_MUL(n, x);
        n *= -1;
        n /= 2 * i - 1;
        n /= 2 * i - 2;
    } while (i < DUER_PERCISION);

    return DUER_SAT(sum);
}

duer_status_t duer_conv(duer_coef_t *y, duer_size_t O, int ystart,
                        const duer_coef_t *x, duer_size_t N, int xstart,
                        const duer_coef_t *h, duer_size_t M)
{
    int n, i;

    DUER_LOGV("===> O: %d, N: %d, xstart: %d, M: %d", O, N, xstart, M);
    for (n = ystart; n < O + ystart; n++) {
        i = (n < M) ? 0 : (n - M + 1);
        i = i < xstart ? xstart : i;
        DUER_LOGV("\t+ n: %d, i: %d", n, i);
        for (; i < n + 1 && i < N + xstart; i++) {
            duer_coef_t temp = DUER_COEF_MUL(x[i - xstart], h[n - i]);
            DUER_LOGV("\t| x[%d] * h[%d] = %d * %d = %d; ", i, n - i, x[i - xstart], h[n - i], temp);
            y[n - ystart] += temp;
        }
    }

    return DUER_OK;
}

DUER_INT_IMPL duer_coef_t duer_window_cos(int n, int N, const duer_coef_t a[], duer_size_t order)
{
    duer_coef_t base = 0;
    duer_size_t i = 0;
    duer_coef_t rs = 0;

    if (a == NULL || order == 0 || N == 1) {
        return rs;
    }

    base = PI_DOUBLE * n / (N - 1);

    rs = a[0];
    i = 1;

    while (i < order) {
        rs += DUER_COEF_MUL(DUER_SIN(PI_HALF - i * base), a[i]);
        i++;
    }

    return rs;
}

DUER_INT_IMPL duer_coef_t duer_window_hamming(int index, int length)
{
    static const duer_coef_t coefs[] = {
        DUER_QCONST(0.54),
        DUER_QCONST(-0.46)
    };

    return duer_window_cos(index, length, coefs, 2);
}

DUER_INT_IMPL duer_coef_t duer_window_hanning(int index, int length)
{
    static const duer_coef_t coefs[] = {
        DUER_QCONST(0.5),
        DUER_QCONST(-0.5)
    };

    return duer_window_cos(index, length, coefs, 2);
}

DUER_INT_IMPL duer_coef_t duer_window_blackman(int index, int length)
{
    static const duer_coef_t coefs[] = {
        DUER_QCONST(0.62),
        DUER_QCONST(-0.48),
        DUER_QCONST(0.38)
    };

    return duer_window_cos(index, length, coefs, 3);
}

DUER_INT_IMPL duer_coef_t duer_window_blackman_harris(int index, int length)
{
    static const duer_coef_t coefs[] = {
        DUER_QCONST(0.35875),
        DUER_QCONST(-0.48829),
        DUER_QCONST(0.14128),
        DUER_QCONST(-0.01168)
    };

    return duer_window_cos(index, length, coefs, 4);
}

DUER_INT_IMPL duer_coef_t duer_window_blackman_nuttall(int index, int length)
{
    static const duer_coef_t coefs[] = {
        DUER_QCONST(0.3635819),
        DUER_QCONST(-0.4891775),
        DUER_QCONST(0.1365995),
        DUER_QCONST(-0.0106411)
    };

    return duer_window_cos(index, length, coefs, 4);
}

DUER_INT_IMPL duer_coef_t duer_sample_generate(int index, int freq, duer_size_t length, int fs)
{
    duer_coef_t coef = DUER_QCONST(1.0f * freq * index / fs);
    return DUER_SIN(DUER_COEF_MUL(PI_DOUBLE, coef));
}

DUER_INT_IMPL duer_coef_t duer_short2coef(short val)
{
    duer_coef_t rs = 0;

#if !defined(DUER_FIXED_POINT) || (DUER_FIXED_POINT == 0)
    rs = DUER_FCONST_BITS(val, 16);
#else
    rs = val;
#endif

    return rs;
}

DUER_INT_IMPL short duer_coef2short(duer_coef_t val)
{
    short rs = 0;

#if !defined(DUER_FIXED_POINT) || (DUER_FIXED_POINT == 0)
    rs = DUER_QCONST_BITS(val, 16);
#else
    rs = val;
#endif

    return rs;
}

DUER_INT_IMPL duer_coef_t *duer_short2coef_array(duer_coef_t dst[], const short src[], duer_size_t length)
{
    int i;

    for (i = 0; i < length; i++) {
        dst[i] = duer_short2coef(src[i]);
    }

    return dst;
}

DUER_INT_IMPL short *duer_coef2short_array(short dst[], const duer_coef_t src[], duer_size_t length)
{
    int i;

    for (i = 0; i < length; i++) {
        dst[i] = duer_coef2short(src[i]);
    }

    return dst;
}
