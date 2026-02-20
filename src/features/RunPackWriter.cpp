#include "RunPackWriter.h"

static constexpr uint64_t ALIGN8(uint64_t x) { return (x + 7ull) & ~7ull; }

void RunPackWriter::set_name32(char out[32], const char *name)
{
    std::memset(out, 0, 32);
    std::strncpy(out, name, 31); // leave room for null
}

uint64_t RunPackWriter::tellp_u64(std::ofstream &f)
{
    return static_cast<uint64_t>(f.tellp());
}

void RunPackWriter::write_u64(std::ofstream &f, uint64_t v)
{
    f.write(reinterpret_cast<const char *>(&v), sizeof(v));
}

template <typename T>
runpack::DType RunPackWriter::dtype_of()
{
    if constexpr (std::is_same_v<T, int32_t>)
        return runpack::DType::I32;
    if constexpr (std::is_same_v<T, int64_t>)
        return runpack::DType::I64;
    if constexpr (std::is_same_v<T, float>)
        return runpack::DType::F32;
    if constexpr (std::is_same_v<T, double>)
        return runpack::DType::F64;
    // If you need more, add them.
    return (runpack::DType)0;
}

template <typename T>
RunPackWriter::SeriesDesc RunPackWriter::make_desc(const char *name, const std::vector<T> &v)
{
    SeriesDesc d{};
    set_name32(d.name, name);
    d.dtype = dtype_of<T>();
    d.elem_size = static_cast<uint32_t>(sizeof(T));
    d.len = static_cast<uint64_t>(v.size());
    d.data = v.empty() ? nullptr : (const void *)v.data();
    return d;
}

std::vector<runpack::TradeDiskV1> RunPackWriter::convert_trades(const TradeLog &trades)
{
    std::vector<runpack::TradeDiskV1> out;
    out.reserve(trades.closed().size());

    for (const auto &t : trades.closed())
    {
        runpack::TradeDiskV1 x{};
        x.entry_ts = t.entry_ts;
        x.exit_ts = t.exit_ts;
        x.entry_i = t.entry_i;
        x.exit_i = t.exit_i;
        x.side = (t.side == TradeSide::Long) ? int8_t(+1) : int8_t(-1);
        x.lots = t.lots;
        x.entry_price = t.entry_price;
        x.exit_price = t.exit_price;
        x.pnl = t.pnl;
        x.pnl_r = t.pnl_r;
        x.mae = t.mae;
        x.mfe = t.mfe;
        out.push_back(x);
    }
    return out;
}

std::vector<RunPackWriter::SeriesDesc> RunPackWriter::build_series_desc(const RunSeries &s)
{
    // IMPORTANT: names are what your reader/chart will use.
    // Keep stable and consistent.
    std::vector<SeriesDesc> v;
    v.reserve(64);

    v.push_back(make_desc("ts", s.ts));

    v.push_back(make_desc("balance", s.balance));
    v.push_back(make_desc("equity", s.equity));
    v.push_back(make_desc("dd_equity", s.dd_equity));
    v.push_back(make_desc("dd_balance", s.dd_balance));

    v.push_back(make_desc("avg_equity_dd", s.avg_equity_dd));
    v.push_back(make_desc("avg_balance_dd", s.avg_balance_dd));

    v.push_back(make_desc("pct_in_equity_dd", s.pct_in_equity_drawdown));
    v.push_back(make_desc("pct_in_balance_dd", s.pct_in_balance_drawdown));
    v.push_back(make_desc("bars_in_equity_dd", s.bars_in_equity_drawdown));
    v.push_back(make_desc("bars_in_balance_dd", s.bars_in_balance_drawdown));

    v.push_back(make_desc("unrealized_pnl", s.unrealized_pnl));
    v.push_back(make_desc("max_equity", s.max_equity));
    v.push_back(make_desc("max_balance", s.max_balance));
    v.push_back(make_desc("max_equity_dd", s.max_equity_dd));
    v.push_back(make_desc("max_balance_dd", s.max_balance_dd));

    v.push_back(make_desc("max_equity_daily_dd", s.max_equity_daily_dd));
    v.push_back(make_desc("max_balance_daily_dd", s.max_balance_daily_dd));

    v.push_back(make_desc("net_profit", s.net_profit));

    v.push_back(make_desc("total_trades", s.total_trades));
    v.push_back(make_desc("winning_trades", s.winning_trades));
    v.push_back(make_desc("losing_trades", s.losing_trades));

    v.push_back(make_desc("win_rate", s.win_rate));
    v.push_back(make_desc("gross_profit", s.gross_profit));
    v.push_back(make_desc("gross_loss", s.gross_loss));
    v.push_back(make_desc("profit_factor", s.profit_factor));

    v.push_back(make_desc("expected_value", s.expected_value));
    v.push_back(make_desc("avg_win", s.avg_win));
    v.push_back(make_desc("avg_loss", s.avg_loss));
    v.push_back(make_desc("profit_loss_ratio", s.profit_loss_ratio));

    v.push_back(make_desc("expectancy_r", s.expectancy_r));
    v.push_back(make_desc("median_pnl", s.median_pnl));
    v.push_back(make_desc("top10_contrib", s.top_10_percent_contribution));
    v.push_back(make_desc("trades_per_day", s.trades_per_day));

    v.push_back(make_desc("time_in_market", s.time_in_market));

    v.push_back(make_desc("ret_vol", s.return_volatility));
    v.push_back(make_desc("sharpe", s.sharpe_ratio));
    v.push_back(make_desc("calmar", s.calmar_ratio));
    v.push_back(make_desc("sortino", s.sortino_ratio));

    return v;
}

bool RunPackWriter::write(const std::string &path,
                          const Meta &meta,
                          const RunSeries &series,
                          const TradeLog &trades,
                          std::string *err)
{
    auto fail = [&](const std::string &e)
    {
        if (err)
            *err = e;
        return false;
    };

    // Basic consistency: all series should have same length as ts (except trades)
    const size_t n = series.ts.size();
    if (n == 0)
        return fail("RunSeries.ts is empty (no bars).");

    // Build series descriptors
    auto desc = build_series_desc(series);

    // Convert trades to packed disk format
    auto tvec = convert_trades(trades);

    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f)
        return fail("Failed to open file for writing: " + path);

    // 1) Write placeholder header
    runpack::FileHeader hdr{};
    hdr.created_unix_ms = meta.created_unix_ms;
    f.write(reinterpret_cast<const char *>(&hdr), sizeof(hdr));
    if (!f)
        return fail("Failed writing header placeholder.");

    // 2) Write metadata blob (length + bytes), aligned to 8
    hdr.meta_offset = tellp_u64(f);
    const uint64_t meta_len = static_cast<uint64_t>(meta.meta_json.size());
    hdr.meta_bytes = meta_len;

    // store meta_len (u64) then bytes
    write_u64(f, meta_len);
    if (meta_len)
        f.write(meta.meta_json.data(), (std::streamsize)meta_len);

    // pad to 8
    uint64_t pos = tellp_u64(f);
    uint64_t pad_to = ALIGN8(pos);
    if (pad_to > pos)
    {
        static const char zeros[8] = {0};
        f.write(zeros, (std::streamsize)(pad_to - pos));
    }

    // 3) Write TOC placeholder
    hdr.toc_offset = tellp_u64(f);
    hdr.toc_count = static_cast<uint32_t>(desc.size());

    std::vector<runpack::TocEntry> toc;
    toc.resize(desc.size());

    // placeholder TOC on disk
    f.write(reinterpret_cast<const char *>(toc.data()),
            (std::streamsize)(toc.size() * sizeof(runpack::TocEntry)));
    if (!f)
        return fail("Failed writing TOC placeholder.");

    // 4) Write series blobs, fill toc entries with offsets
    for (size_t i = 0; i < desc.size(); ++i)
    {
        const auto &d = desc[i];

        // align to 8
        uint64_t p = tellp_u64(f);
        uint64_t a = ALIGN8(p);
        if (a > p)
        {
            static const char zeros[8] = {0};
            f.write(zeros, (std::streamsize)(a - p));
        }

        runpack::TocEntry e{};
        std::memset(&e, 0, sizeof(e));
        std::memcpy(e.name, d.name, 32);
        e.dtype = d.dtype;
        e.elem_size = d.elem_size;
        e.len = d.len;
        e.offset = tellp_u64(f);

        // write raw contiguous array
        const uint64_t bytes = d.len * (uint64_t)d.elem_size;
        if (bytes > 0 && d.data)
        {
            f.write(reinterpret_cast<const char *>(d.data), (std::streamsize)bytes);
        }
        else
        {
            // ok: empty vector or null
        }
        if (!f)
            return fail(std::string("Failed writing series blob: ") + d.name);

        toc[i] = e;
    }

    // 5) Write trades blob (aligned)
    {
        uint64_t p = tellp_u64(f);
        uint64_t a = ALIGN8(p);
        if (a > p)
        {
            static const char zeros[8] = {0};
            f.write(zeros, (std::streamsize)(a - p));
        }

        hdr.trades_offset = tellp_u64(f);
        hdr.trades_count = static_cast<uint64_t>(tvec.size());

        if (!tvec.empty())
        {
            f.write(reinterpret_cast<const char *>(tvec.data()),
                    (std::streamsize)(tvec.size() * sizeof(runpack::TradeDiskV1)));
            if (!f)
                return fail("Failed writing trades blob.");
        }
    }

    // 6) Finalize header + write TOC back into placeholder region
    hdr.file_bytes = tellp_u64(f);

    // write TOC entries
    f.seekp((std::streamoff)hdr.toc_offset, std::ios::beg);
    f.write(reinterpret_cast<const char *>(toc.data()),
            (std::streamsize)(toc.size() * sizeof(runpack::TocEntry)));
    if (!f)
        return fail("Failed rewriting TOC.");

    // write header
    f.seekp(0, std::ios::beg);
    f.write(reinterpret_cast<const char *>(&hdr), sizeof(hdr));
    if (!f)
        return fail("Failed rewriting header.");

    f.flush();
    return true;
}