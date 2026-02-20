#pragma once
#include <vector>
#include <cmath> // NAN, std::isnan
#include <cstdint>

namespace features
{

    // ---------------------------
    // EMA (exponential moving avg)
    // ---------------------------
    struct EMAStream
    {
        int period = 0;
        float alpha = 0.0f;
        float value = NAN;
        bool ready = false;

        EMAStream() = default;
        explicit EMAStream(int p) { init(p); }

        void init(int p)
        {
            period = p;
            alpha = 2.0f / (p + 1.0f);
            value = NAN;
            ready = false;
        }

        inline void update(float x)
        {
            if (!ready)
            {
                value = x;
                ready = true;
            }
            else
            {
                value += alpha * (x - value);
            }
        }
    };

    // ---------------------------
    // ATR (Wilder)
    // ---------------------------
    struct ATRStream
    {
        int period = 0;
        float value = NAN;
        bool ready = false;

        float prev_close = NAN;
        float wilder = 0.0f;
        int warm_count = 0;
        float warm_sum = 0.0f;

        ATRStream() = default;
        explicit ATRStream(int p) { init(p); }

        void init(int p)
        {
            period = p;
            value = NAN;
            ready = false;
            prev_close = NAN;
            wilder = 0.0f;
            warm_count = 0;
            warm_sum = 0.0f;
        }

        inline void update(float high, float low, float close)
        {
            float tr;
            if (std::isnan(prev_close))
            {
                tr = high - low;
                prev_close = close;
            }
            else
            {
                const float tr1 = high - low;
                const float tr2 = std::fabs(high - prev_close);
                const float tr3 = std::fabs(low - prev_close);
                tr = tr1;
                if (tr2 > tr)
                    tr = tr2;
                if (tr3 > tr)
                    tr = tr3;
                prev_close = close;
            }

            if (!ready)
            {
                warm_sum += tr;
                warm_count++;
                if (warm_count >= period)
                {
                    wilder = warm_sum / (float)period;
                    value = wilder;
                    ready = true;
                }
                return;
            }

            wilder = (wilder * (period - 1) + tr) / (float)period;
            value = wilder;
        }
    };

    struct EMAHandle
    {
        int period = 0;
        const EMAStream *stream = nullptr;
    };
    struct ATRHandle
    {
        int period = 0;
        const ATRStream *stream = nullptr;
    };

    class FeatureManager
    {
    public:
        FeatureManager() = default;

        EMAHandle require_ema(int period);
        ATRHandle require_atr(int period);

        void update(float open, float high, float low, float close, float volume);

        const EMAStream *find_ema(int period) const;
        const ATRStream *find_atr(int period) const;

        void reset();

    private:
        // ✅ Make these explicit. No CTAD. No missing template args.
        std::vector<EMAStream> emas_;
        std::vector<ATRStream> atrs_;
    };

} // namespace features
