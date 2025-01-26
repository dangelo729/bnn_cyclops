#pragma once

#include "app/engine/sos.h"

namespace recorder
{

template <typename T>
class AAFilter
{
public:
    void Init(void)
    {
        filter_.Init(kNumSections, kCoeffs);
    }

    void Reset(void)
    {
        filter_.Reset();
    }

    T Process(T in)
    {
        return filter_.Process(in);
    }

    int GetOversamplingFactor(void)
    {
        return kOSFactor;
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

    fs = 16000
    min_oversampled_rate = 48000

    fp = 6000 # passband corner in Hz
    rp = 0.1 # passband ripple in dB
    rs = 80 # stopband attenuation in dB

    cascades = []
    max_num_sections = 0

    factor = math.ceil(min_oversampled_rate / fs)
    wp = fp / fs
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

    order = n
    num_sections = len(sos)
    name = 'kCoeffs'
    cost = fs * factor * num_sections

    cog.outl('static constexpr int kOSFactor = {};'.format(factor))
    cog.outl('static constexpr int kNumSections = {:d};'.format(num_sections))
    cog.outl('static constexpr SOSCoefficients {:s}[kNumSections] ='
        ' // n = {:d}, wc = {:f}, cost = {:d}'
        .format(name, order, wc, cost))
    cog.outl('{')
    for sec in sos:
        b = ''.join(['{:.8e},'.format(c).ljust(17) for c in sec[:3]])
        a = ''.join(['{:.8e},'.format(c).ljust(17) for c in sec[4:]])
        cog.outl('    { {' + b + '}, {' + a + '} },')
    cog.outl('};')
    ]]]*/
    static constexpr int kOSFactor = 3;
    static constexpr int kNumSections = 5;
    static constexpr SOSCoefficients kCoeffs[kNumSections] = // n = 10, wc = 0.250000, cost = 240000
    {
        { {7.49218660e-04,  1.02001710e-03,  7.49218660e-04,  }, {-1.47186222e+00, 5.63132134e-01,  } },
        { {1.00000000e+00,  -3.39627122e-01, 1.00000000e+00,  }, {-1.43791341e+00, 6.76234255e-01,  } },
        { {1.00000000e+00,  -9.56546925e-01, 1.00000000e+00,  }, {-1.40032243e+00, 8.10526929e-01,  } },
        { {1.00000000e+00,  -1.16644094e+00, 1.00000000e+00,  }, {-1.38033296e+00, 9.09094957e-01,  } },
        { {1.00000000e+00,  -1.23628586e+00, 1.00000000e+00,  }, {-1.38478499e+00, 9.73533102e-01,  } },
    };
    //[[[end]]]

    SOSFilter<T, kNumSections> filter_;
};

}
