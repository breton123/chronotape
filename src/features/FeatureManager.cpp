#include "FeatureManager.h"

namespace features
{

    EMAHandle FeatureManager::require_ema(int period)
    {
        for (auto &s : emas_)
        {
            if (s.period == period)
                return {period, &s};
        }
        emas_.emplace_back(period);
        return {period, &emas_.back()};
    }

    ATRHandle FeatureManager::require_atr(int period)
    {
        for (auto &s : atrs_)
        {
            if (s.period == period)
                return {period, &s};
        }
        atrs_.emplace_back(period);
        return {period, &atrs_.back()};
    }

    void FeatureManager::update(float open, float high, float low, float close, float volume)
    {
        (void)open;
        (void)volume;

        for (auto &e : emas_)
            e.update(close);
        for (auto &a : atrs_)
            a.update(high, low, close);
    }

    const EMAStream *FeatureManager::find_ema(int period) const
    {
        for (auto &s : emas_)
            if (s.period == period)
                return &s;
        return nullptr;
    }

    const ATRStream *FeatureManager::find_atr(int period) const
    {
        for (auto &s : atrs_)
            if (s.period == period)
                return &s;
        return nullptr;
    }

    void FeatureManager::reset()
    {
        emas_.clear();
        atrs_.clear();
    }

} // namespace features
