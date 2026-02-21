#!/usr/bin/env python3
import struct
import sys
import os

import numpy as np
import matplotlib.pyplot as plt

# ---- RunPack binary layout (must match RunPackFormat.h) ----

FILE_HEADER_FMT = "<QIIQQQQIIQQQ"
TOC_ENTRY_FMT   = "<32sIIQQ"

FILE_HEADER_SIZE = struct.calcsize(FILE_HEADER_FMT)
TOC_ENTRY_SIZE   = struct.calcsize(TOC_ENTRY_FMT)

DTYPE_I32 = 1
DTYPE_I64 = 2
DTYPE_F32 = 3
DTYPE_F64 = 4

DTYPE_TO_NP = {
    DTYPE_I32: np.int32,
    DTYPE_I64: np.int64,
    DTYPE_F32: np.float32,
    DTYPE_F64: np.float64,
}


def read_header(f):
    data = f.read(FILE_HEADER_SIZE)
    if len(data) != FILE_HEADER_SIZE:
        raise RuntimeError("File too small for header")
    (magic, version, endian, created_ms,
     meta_off, meta_bytes,
     toc_off, toc_count, reserved0,
     trades_off, trades_count,
     file_bytes) = struct.unpack(FILE_HEADER_FMT, data)

    print(f"magic={hex(magic)} version={version} toc_count={toc_count}")
    return {
        "magic": magic,
        "version": version,
        "endian": endian,
        "created_ms": created_ms,
        "meta_off": meta_off,
        "meta_bytes": meta_bytes,
        "toc_off": toc_off,
        "toc_count": toc_count,
        "trades_off": trades_off,
        "trades_count": trades_count,
        "file_bytes": file_bytes,
    }


def read_toc(f, hdr):
    f.seek(hdr["toc_off"])
    entries = []
    for _ in range(hdr["toc_count"]):
        data = f.read(TOC_ENTRY_SIZE)
        if len(data) != TOC_ENTRY_SIZE:
            raise RuntimeError("Unexpected EOF reading TOC")
        raw_name, dtype, elem_size, length, offset = struct.unpack(TOC_ENTRY_FMT, data)
        name = raw_name.split(b"\x00", 1)[0].decode("ascii", errors="ignore")
        entries.append({
            "name": name,
            "dtype": dtype,
            "elem_size": elem_size,
            "len": length,
            "offset": offset,
        })
    print("Series in runpack:")
    for e in entries:
        print(f"  {e['name']}: len={e['len']} dtype={e['dtype']}")
    return entries


def read_series(f, entry):
    np_dtype = DTYPE_TO_NP.get(entry["dtype"])
    if np_dtype is None:
        raise RuntimeError(f"Unsupported dtype {entry['dtype']} for series {entry['name']}")

    expected_bytes = entry["len"] * entry["elem_size"]
    f.seek(entry["offset"])
    buf = f.read(expected_bytes)
    if len(buf) != expected_bytes:
        raise RuntimeError(f"Unexpected EOF reading series {entry['name']}")

    arr = np.frombuffer(buf, dtype=np_dtype)
    return arr


def main(path):
    with open(path, "rb") as f:
        hdr = read_header(f)
        toc = read_toc(f, hdr)

        ts_entry = next((e for e in toc if e["name"] == "ts"), None)
        ts = None
        if ts_entry is not None:
            ts = read_series(f, ts_entry)
            print(f"'ts' length = {ts.shape[0]}")
        else:
            print("No 'ts' series found; will use index for x-axis.")

        for e in toc:
            name = e["name"]
            if name == "ts":
                continue

            arr = read_series(f, e)
            print(f"Plotting {name}: len={arr.shape[0]}")

            plt.figure()
            if ts is not None and arr.shape[0] == ts.shape[0]:
                plt.plot(ts, arr)
                plt.xlabel("ts (ns)")
            else:
                plt.plot(np.arange(arr.shape[0]), arr)
                plt.xlabel("index")

            plt.title(name)
            plt.ylabel(name)
            plt.grid(True)

        if len(plt.get_fignums()) == 0:
            print("No series were plotted (check lengths printed above).")
        else:
            plt.show()


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(f"Usage: {os.path.basename(sys.argv[0])} path/to/run_000001.rpack")
        sys.exit(1)
    main(sys.argv[1])