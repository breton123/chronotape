#pragma once
#include "TradeTypes.h"
#include <vector>

class TradeLog
{
public:
    void reserve(size_t n) { closed_.reserve(n); }
    void add_closed(const ClosedTrade &t) { closed_.push_back(t); }

    const std::vector<ClosedTrade> &closed() const { return closed_; }
    size_t size() const { return closed_.size(); }

private:
    std::vector<ClosedTrade> closed_;
};
