// Anti-aliasing filters for common sample rates
// Copyright (C) 2020 Tyler Coy
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "sos.hpp"

namespace vostok_ripples
{

template <typename T>
class AAFilter
{
public:
    void Init(float sample_rate)
    {
        InitFilter(sample_rate);
    }

    T ProcessUp(T in)
    {
        return up_filter_.Process(in);
    }

    T ProcessDown(T in)
    {
        return down_filter_.Process(in);
    }

    int GetOversamplingFactor(void)
    {
        return oversampling_factor_;
    }

protected:
    struct CascadedSOS
    {
        float sample_rate;
        int oversampling_factor;
        int num_sections;
        const SOSCoefficients* coeffs;
    };

    /*[[[cog
    from scipy import signal
    import math

    oversampling_factor = 2

    common_rates = [
        24000,
        32000, 
        48000,
        96000        
    ]

    fp = 20000 # target passband corner in Hz
    rp = 0.1 # passband ripple in dB
    rs = 100 # stopband attenuation in dB

    array_name = 'kFilter'
    cascades = []
    max_num_sections = 0

    for fs in common_rates:
        factor = oversampling_factor
        # With fixed 2x oversampling, very low sample rates cannot realize a
        # 20 kHz passband while still keeping wp < ws (required by ellipord).
        fp_eff = min(fp, 0.45 * fs)
        wp = fp_eff / fs
        ws = 0.5

        n, wc = signal.ellipord(wp*2/factor, ws*2/factor, rp, rs)

        # We are using second-order sections, so if the filter order would have
        # been odd, we can bump it up by 1 for 'free'
        n = 2 * int(math.ceil(n / 2))

        # Non-oversampled sampling rates result in 0-order filters, since there
        # is no spectral content above fs/2. Bump these up to order 2 so we
        # get some rolloff.
        n = max(2, n)
        z, p, k = signal.ellip(n, rp, rs, wc, output='zpk')

        if n % 2 == 0:
            # DC gain is -rp for even-order filters, so amplify by rp
            k *= math.pow(10, rp / 20)
        sos = signal.zpk2sos(z, p, k)
        max_num_sections = max(max_num_sections, len(sos))

        cascade = (fs, factor, n, wc, sos)
        cascades.append(cascade)

    cog.outl('static constexpr int kMaxNumSections = {};'
        .format(max_num_sections))
    ]]]*/
    static constexpr int kMaxNumSections = 6;
    //[[[end]]]

    SOSFilter<T, kMaxNumSections> up_filter_;
    SOSFilter<T, kMaxNumSections> down_filter_;
    int oversampling_factor_;

    void InitFilter(float sample_rate)
    {
        if (false) {}
        /*[[[cog
        for cascade in reversed(cascades):
            (fs, factor, order, wc, sos) = cascade
            num_sections = len(sos)
            name = '{:s}{:d}x{:d}'.format(array_name, fs, factor)
            cost = fs * factor * num_sections

            cog.outl('else if ({} <= sample_rate)'.format(fs))
            cog.outl('{')
            cog.outl('    const SOSCoefficients {:s}[{:d}] ='
                ' // n = {:d}, wc = {:f}, cost = {:d}'
                .format(name, num_sections, order, wc, cost))
            cog.outl('    {')
            for sec in sos:
                b = ''.join(['{:.8e},'.format(c).ljust(17) for c in sec[:3]])
                a = ''.join(['{:.8e},'.format(c).ljust(17) for c in sec[4:]])
                cog.outl('        { {' + b + '}, {' + a + '} },')
            cog.outl('    };')
            cog.outl('    up_filter_.Init({}, {});'.format(num_sections, name))
            cog.outl('    down_filter_.Init({}, {});'.format(num_sections, name))
            cog.outl('    oversampling_factor_ = {};'.format(factor))
            cog.outl('}')
        cog.outl('else {{ InitFilter({}); }}'.format(*cascades[0]))
        ]]]*/
        else if (96000 <= sample_rate)
        {
            const SOSCoefficients kFilter96000x2[4] = // n = 8, wc = 0.208333, cost = 768000
            {
                { {1.61642425e-04,  2.48570126e-04,  1.61642425e-04,  }, {-1.55380069e+00, 6.19246726e-01,  } },
                { {1.00000000e+00,  -3.58596848e-03, 1.00000000e+00,  }, {-1.52398387e+00, 7.01782723e-01,  } },
                { {1.00000000e+00,  -7.04289597e-01, 1.00000000e+00,  }, {-1.49925872e+00, 8.20194069e-01,  } },
                { {1.00000000e+00,  -9.36239381e-01, 1.00000000e+00,  }, {-1.51854777e+00, 9.39912815e-01,  } },
            };
            up_filter_.Init(4, kFilter96000x2);
            down_filter_.Init(4, kFilter96000x2);
            oversampling_factor_ = 2;
        }
        else if (48000 <= sample_rate)
        {
            const SOSCoefficients kFilter48000x2[6] = // n = 12, wc = 0.416667, cost = 576000
            {
                { {1.13607815e-03,  2.09710115e-03,  1.13607815e-03,  }, {-1.21931845e+00, 4.08933254e-01,  } },
                { {1.00000000e+00,  1.04601876e+00,  1.00000000e+00,  }, {-1.03580025e+00, 5.39844154e-01,  } },
                { {1.00000000e+00,  3.80916327e-01,  1.00000000e+00,  }, {-8.08830338e-01, 7.03595041e-01,  } },
                { {1.00000000e+00,  1.72815250e-02,  1.00000000e+00,  }, {-6.37029110e-01, 8.32367841e-01,  } },
                { {1.00000000e+00,  -1.58681120e-01, 1.00000000e+00,  }, {-5.37547766e-01, 9.17642098e-01,  } },
                { {1.00000000e+00,  -2.29300230e-01, 1.00000000e+00,  }, {-4.97997645e-01, 9.75321067e-01,  } },
            };
            up_filter_.Init(6, kFilter48000x2);
            down_filter_.Init(6, kFilter48000x2);
            oversampling_factor_ = 2;
        }
        else if (32000 <= sample_rate)
        {
            const SOSCoefficients kFilter32000x2[6] = // n = 12, wc = 0.450000, cost = 384000
            {
                { {1.71561642e-03,  3.21625777e-03,  1.71561642e-03,  }, {-1.14542510e+00, 3.70279720e-01,  } },
                { {1.00000000e+00,  1.19285791e+00,  1.00000000e+00,  }, {-9.23753708e-01, 5.15033840e-01,  } },
                { {1.00000000e+00,  5.82488267e-01,  1.00000000e+00,  }, {-6.55907869e-01, 6.91723371e-01,  } },
                { {1.00000000e+00,  2.30478761e-01,  1.00000000e+00,  }, {-4.57266494e-01, 8.27375285e-01,  } },
                { {1.00000000e+00,  5.51959098e-02,  1.00000000e+00,  }, {-3.42839064e-01, 9.15674051e-01,  } },
                { {1.00000000e+00,  -1.60872969e-02, 1.00000000e+00,  }, {-2.95082208e-01, 9.74790386e-01,  } },
            };
            up_filter_.Init(6, kFilter32000x2);
            down_filter_.Init(6, kFilter32000x2);
            oversampling_factor_ = 2;
        }
        else if (24000 <= sample_rate)
        {
            const SOSCoefficients kFilter24000x2[6] = // n = 12, wc = 0.450000, cost = 288000
            {
                { {1.71561642e-03,  3.21625777e-03,  1.71561642e-03,  }, {-1.14542510e+00, 3.70279720e-01,  } },
                { {1.00000000e+00,  1.19285791e+00,  1.00000000e+00,  }, {-9.23753708e-01, 5.15033840e-01,  } },
                { {1.00000000e+00,  5.82488267e-01,  1.00000000e+00,  }, {-6.55907869e-01, 6.91723371e-01,  } },
                { {1.00000000e+00,  2.30478761e-01,  1.00000000e+00,  }, {-4.57266494e-01, 8.27375285e-01,  } },
                { {1.00000000e+00,  5.51959098e-02,  1.00000000e+00,  }, {-3.42839064e-01, 9.15674051e-01,  } },
                { {1.00000000e+00,  -1.60872969e-02, 1.00000000e+00,  }, {-2.95082208e-01, 9.74790386e-01,  } },
            };
            up_filter_.Init(6, kFilter24000x2);
            down_filter_.Init(6, kFilter24000x2);
            oversampling_factor_ = 2;
        }
        else { InitFilter(24000); }
        //[[[end]]]
    }
};

}
