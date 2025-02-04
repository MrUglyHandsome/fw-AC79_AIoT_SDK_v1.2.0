//----------------------------------------------------------------------------
// Anti-Grain Geometry - Version 2.4
// Copyright (C) 2002-2005 Maxim Shemanarev (http://www.antigrain.com)
//
// Permission to copy, use, modify, sell and distribute this software
// is granted provided this copyright notice appears in all copies.
// This software is provided "as is" without express or implied
// warranty, and with no claim as to its suitability for any purpose.
//
//----------------------------------------------------------------------------
// Contact: mcseem@antigrain.com
//          mcseemagg@yahoo.com
//          http://www.antigrain.com
//----------------------------------------------------------------------------

#ifndef AGG_GAMMA_LUT_INCLUDED
#define AGG_GAMMA_LUT_INCLUDED

#include <math.h>
#include "agg_basics.h"
#include "agg_gamma_functions.h"

namespace agg
{
template<class LoResT = int8u,
         class HiResT = int8u,
         unsigned GammaShift = 8,
         unsigned HiResShift = 8> class gamma_lut
{
public:
    typedef gamma_lut<LoResT, HiResT, GammaShift, HiResShift> self_type;

    enum gamma_scale_e {
        gamma_shift = GammaShift,
        gamma_size  = 1 << gamma_shift,
        gamma_mask  = gamma_size - 1
    };

    enum hi_res_scale_e {
        hi_res_shift = HiResShift,
        hi_res_size  = 1 << hi_res_shift,
        hi_res_mask  = hi_res_size - 1
    };

    ~gamma_lut()
    {
        pod_allocator<LoResT>::deallocate(m_inv_gamma, hi_res_size);
        pod_allocator<HiResT>::deallocate(m_dir_gamma, gamma_size);
    }

    gamma_lut() :
        m_gamma(1.0),
        m_dir_gamma(pod_allocator<HiResT>::allocate(gamma_size)),
        m_inv_gamma(pod_allocator<LoResT>::allocate(hi_res_size))
    {
        unsigned i;
        for (i = 0; i < gamma_size; i++) {
            m_dir_gamma[i] = HiResT(i << (hi_res_shift - gamma_shift));
        }

        for (i = 0; i < hi_res_size; i++) {
            m_inv_gamma[i] = LoResT(i >> (hi_res_shift - gamma_shift));
        }
    }

    gamma_lut(double g) :
        m_gamma(1.0),
        m_dir_gamma(pod_allocator<HiResT>::allocate(gamma_size)),
        m_inv_gamma(pod_allocator<LoResT>::allocate(hi_res_size))
    {
        gamma(g);
    }

    void gamma(double g)
    {
        m_gamma = g;

        unsigned i;
        for (i = 0; i < gamma_size; i++) {
            m_dir_gamma[i] = (HiResT)
                             uround(pow(i / double(gamma_mask), m_gamma) * double(hi_res_mask));
        }

        double inv_g = 1.0 / g;
        for (i = 0; i < hi_res_size; i++) {
            m_inv_gamma[i] = (LoResT)
                             uround(pow(i / double(hi_res_mask), inv_g) * double(gamma_mask));
        }
    }

    double gamma() const
    {
        return m_gamma;
    }

    HiResT dir(LoResT v) const
    {
        return m_dir_gamma[unsigned(v)];
    }

    LoResT inv(HiResT v) const
    {
        return m_inv_gamma[unsigned(v)];
    }

private:
    gamma_lut(const self_type &);
    const self_type &operator = (const self_type &);

    double m_gamma;
    HiResT *m_dir_gamma;
    LoResT *m_inv_gamma;
};

//
// sRGB support classes
//

// sRGB_lut - implements sRGB conversion for the various types.
// Base template is undefined, specializations are provided below.
template<class LinearType>
class sRGB_lut;

template<>
class sRGB_lut<float>
{
public:
    sRGB_lut()
    {
        // Generate lookup tables.
        for (int i = 0; i <= 255; ++i) {
            m_dir_table[i] = float(sRGB_to_linear(i / 255.0));
        }
        for (int i = 0; i <= 65535; ++i) {
            m_inv_table[i] = uround(255.0 * linear_to_sRGB(i / 65535.0));
        }
    }

    float dir(int8u v) const
    {
        return m_dir_table[v];
    }

    int8u inv(float v) const
    {
        return m_inv_table[int16u(0.5 + v * 65535)];
    }

private:
    float m_dir_table[256];
    int8u m_inv_table[65536];
};

template<>
class sRGB_lut<int16u>
{
public:
    sRGB_lut()
    {
        // Generate lookup tables.
        for (int i = 0; i <= 255; ++i) {
            m_dir_table[i] = uround(65535.0 * sRGB_to_linear(i / 255.0));
        }
        for (int i = 0; i <= 65535; ++i) {
            m_inv_table[i] = uround(255.0 * linear_to_sRGB(i / 65535.0));
        }
    }

    int16u dir(int8u v) const
    {
        return m_dir_table[v];
    }

    int8u inv(int16u v) const
    {
        return m_inv_table[v];
    }

private:
    int16u m_dir_table[256];
    int8u m_inv_table[65536];
};

template<>
class sRGB_lut<int8u>
{
public:
    sRGB_lut()
    {
        // Generate lookup tables.
        for (int i = 0; i <= 255; ++i) {
            m_dir_table[i] = uround(255.0 * sRGB_to_linear(i / 255.0));
            m_inv_table[i] = uround(255.0 * linear_to_sRGB(i / 255.0));
        }
    }

    int8u dir(int8u v) const
    {
        return m_dir_table[v];
    }

    int8u inv(int8u v) const
    {
        return m_inv_table[v];
    }

private:
    int8u m_dir_table[256];
    int8u m_inv_table[256];
};

// Common base class for sRGB_conv objects. Defines an internal
// sRGB_lut object so that users don't have to.
template<class T>
class sRGB_conv_base
{
public:
    static T rgb_from_sRGB(int8u x)
    {
        return lut.dir(x);
    }

    static int8u rgb_to_sRGB(T x)
    {
        return lut.inv(x);
    }

private:
    static sRGB_lut<T> lut;
};

// Definition of sRGB_conv_base::lut. Due to the fact that this a template,
// we don't need to place the definition in a cpp file. Hurrah.
template<class T>
sRGB_lut<T> sRGB_conv_base<T>::lut;

// Wrapper for sRGB-linear conversion.
// Base template is undefined, specializations are provided below.
template<class T>
class sRGB_conv;

template<>
class sRGB_conv<float> : public sRGB_conv_base<float>
{
public:
    static float alpha_from_sRGB(int8u x)
    {
        static const double y = 1 / 255.0;
        return float(x * y);
    }

    static int8u alpha_to_sRGB(float x)
    {
        return int8u(0.5 + x * 255);
    }
};

template<>
class sRGB_conv<int16u> : public sRGB_conv_base<int16u>
{
public:
    static int16u alpha_from_sRGB(int8u x)
    {
        return (x << 8) | x;
    }

    static int8u alpha_to_sRGB(int16u x)
    {
        return x >> 8;
    }
};

template<>
class sRGB_conv<int8u> : public sRGB_conv_base<int8u>
{
public:
    static int8u alpha_from_sRGB(int8u x)
    {
        return x;
    }

    static int8u alpha_to_sRGB(int8u x)
    {
        return x;
    }
};
}

#endif
