#!/usr/bin/env python3
import os
import struct
import argparse
from datetime import datetime

TAPE_MAGIC = b"TAPEv001"
IDX_MAGIC = b"IDXv001\x00"

# -------- binary formats --------

# TapeHeader: 64 bytes
# 8s I I I I Q Q Q 24s
TAPE_HDR_FMT = "<8sIIIIQQQ24s"
TAPE_HDR_SIZE = struct.calcsize(TAPE_HDR_FMT)

# Bar1m: Q d d d d f = 40 bytes
REC_FMT = "<Qddddf"
REC_SIZE = struct.calcsize(REC_FMT)

# IndexHeader: 32 bytes
# 8s I I Q Q
IDX_HDR_FMT = "<8sIIQQ"
IDX_HDR_SIZE = struct.calcsize(IDX_HDR_FMT)

# IndexEntry: Q Q = 16 bytes
IDX_ENTRY_FMT = "<QQ"

RECORD_TYPE_BAR_1M = 2

# --------------------------------


def parse_ts_ns(date_str, time_str):
    dt = datetime.strptime(f"{date_str} {time_str}", "%Y.%m.%d %H:%M")
    return int(dt.timestamp() * 1_000_000_000)


def write_day(day, records, outdir, symbol, stride):
    if not records:
        return

    os.makedirs(outdir, exist_ok=True)

    tape_path = os.path.join(outdir, f"{symbol}_{day}.tape")
    idx_path = os.path.join(outdir, f"{symbol}_{day}.idx")

    index_entries = []
    next_index_at = 0

    with open(tape_path, "wb") as tf:
        tf.write(b"\x00" * TAPE_HDR_SIZE)

        start_ts = records[0][0]
        end_ts = records[-1][0]

        for i, rec in enumerate(records):
            if i == next_index_at:
                index_entries.append((rec[0], tf.tell()))
                next_index_at += stride

            tf.write(struct.pack(REC_FMT, *rec))

        header = struct.pack(
            TAPE_HDR_FMT,
            TAPE_MAGIC,
            1,                     # version
            RECORD_TYPE_BAR_1M,
            REC_SIZE,
            0,
            start_ts,
            end_ts,
            len(records),
            b"\x00" * 24
        )

        tf.seek(0)
        tf.write(header)

    with open(idx_path, "wb") as ix:
        ix.write(struct.pack(IDX_HDR_FMT, IDX_MAGIC,
                 1, stride, len(index_entries), 0))
        for ts, off in index_entries:
            ix.write(struct.pack(IDX_ENTRY_FMT, ts, off))

    print(f"Wrote {day}: {len(records)} bars")


def build(csv_path, outdir, symbol, stride):
    current_day = None
    day_records = []

    with open(csv_path, "r") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue

            date_s, time_s, o, h, l, c, v = line.split(",")

            ts_ns = parse_ts_ns(date_s, time_s)
            day = date_s.replace(".", "")

            record = (
                ts_ns,
                float(o),
                float(h),
                float(l),
                float(c),
                float(v)
            )

            if current_day is None:
                current_day = day

            if day != current_day:
                write_day(current_day, day_records, outdir, symbol, stride)
                day_records = []
                current_day = day

            day_records.append(record)

    # flush last day
    if day_records:
        write_day(current_day, day_records, outdir, symbol, stride)


def main():
    start_year = 2000
    end_year = 2025
    symbol = "EURUSD"
    stride = 720

    for i in range(start_year, end_year):
        csv = f"C:\\Users\\louis\\Desktop\\Project\\chronotape\\DAT_MT_EURUSD_M1_{i}.csv"
        print(csv)
        outdir = f"C:\\Users\\louis\\Desktop\\Project\\chronotape\\data\\tapes\\bars\\EURUSD\\1m\\{i}"
        print(outdir)
        if not os.path.exists(outdir):
            os.makedirs(outdir)
        print("Building tape...")
        build(csv, outdir, symbol, stride)
        print("Tape built successfully")

if __name__ == "__main__":
    main()