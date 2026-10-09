#!/usr/bin/env python3
"""Pre-relocate a convbin ELF -> 8ek application for the first app slot.

convbin handles ELF symbols, RELA addends, VMA/LMA mapping and RAM references.
Its app relocation records contain only Flash references, relative to the
payload. Apply these to the final installed address, then remove the records.
The output is raw app data for make_8ek.py and make_segments.py.
"""

import argparse
from pathlib import Path
import sys


FLASH_END = 0x3B0000
FILE_HEADER_SIZE = 0x4E
APP_HEADER_SIZE = 0x100
METADATA_SIZE = 0x2A
SIGNATURE_SIZE = 0x104
INSTALLER_TRAILER_SIZE = 3


def read_int(data, offset, size=3, byteorder="little"):
    return int.from_bytes(data[offset:offset + size], byteorder)


def write_int(data, offset, value, size=3, byteorder="little"):
    if not 0 <= value < 1 << (size * 8):
        raise ValueError("app field does not fit in {} bytes".format(size))
    data[offset:offset + size] = value.to_bytes(size, byteorder)


def relocate(file_data, french=False):
    """Return (raw app bytes, relocation count, payload Flash address)."""
    minimum = FILE_HEADER_SIZE + APP_HEADER_SIZE + METADATA_SIZE + SIGNATURE_SIZE
    if len(file_data) < minimum or file_data[:8] != b"**TIFL**":
        raise ValueError("expected a CE .8ek from convbin -j elf -k 8ek")
    app = bytearray(file_data[FILE_HEADER_SIZE:])
    if read_int(file_data, 0x4A, 4) != len(app):
        raise ValueError("8ek file length does not match its header")
    if app[:2] != b"\x81\x0f" or app[0xFA:0xFC] != b"\x81\x7f":
        raise ValueError("unsupported application header layout")
    if app[0x100:0x103] != b"eZ8":
        raise ValueError("missing CE application metadata")
    if app[-SIGNATURE_SIZE:] != b"\x02\x3e\x01\x00" + b"\xff" * 0x100:
        raise ValueError("expected an unsigned convbin application")

    extended_size = read_int(app, 0xFC, 4, "big")
    if extended_size != len(app) - APP_HEADER_SIZE - SIGNATURE_SIZE:
        raise ValueError("invalid extended application length")
    if read_int(app, 2, 4, "big") != len(app) - 6 - SIGNATURE_SIZE:
        raise ValueError("invalid master application length")

    code_start = read_int(app, 0x112)
    if not METADATA_SIZE <= code_start < extended_size:
        raise ValueError("invalid code offset")
    saved = code_start - METADATA_SIZE
    if saved % 6:
        raise ValueError("relocation table size is not a multiple of six")
    payload_size = extended_size - code_start
    final_size = len(app) - saved
    app_start = FLASH_END - INSTALLER_TRAILER_SIZE - final_size
    if app_start < 0:
        raise ValueError("application exceeds the app Flash region")
    payload_start = app_start + APP_HEADER_SIZE + METADATA_SIZE

    # Offsets are relative to eZ8. Zero means no such section; description
    # offset 3 points to the name when no description was requested.
    for offset in (0x115, 0x11B, 0x121, 0x124):
        value = read_int(app, offset)
        if value == 0 or (offset == 0x124 and value == 3):
            continue
        if not code_start <= value < extended_size:
            raise ValueError("invalid metadata offset at 0x{:x}".format(offset))
        if offset == 0x115 and value + read_int(app, 0x118) > extended_size:
            raise ValueError("initialized data extends past app payload")
        write_int(app, offset, value - saved)

    patches = {}
    for offset in range(APP_HEADER_SIZE + METADATA_SIZE,
                        APP_HEADER_SIZE + code_start, 6):
        hole = read_int(app, offset)
        value = read_int(app, offset + 3)
        if hole + 3 > payload_size:
            raise ValueError("relocation hole extends past app payload")
        if value >= 0x400000:
            raise ValueError("unsupported relocation target (expected Flash offset)")
        if hole in patches and patches[hole] != value:
            raise ValueError("conflicting relocation records")
        patches[hole] = value
        write_int(app, APP_HEADER_SIZE + code_start + hole, payload_start + value)

    del app[APP_HEADER_SIZE + METADATA_SIZE:APP_HEADER_SIZE + code_start]
    write_int(app, 0x112, METADATA_SIZE)
    write_int(app, 2, read_int(app, 2, 4, "big") - saved, 4, "big")
    write_int(app, 0xFC, extended_size - saved, 4, "big")
    if french:
        # Preserve the legacy French-app metadata bit.
        app[0x10E] |= 0x08
    return bytes(app), saved // 6, payload_start


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="unsigned .8ek from convbin")
    parser.add_argument("output", type=Path, help="pre-relocated raw .bin")
    parser.add_argument("--french", action="store_true",
                        help="preserve the legacy French-app metadata bit")
    args = parser.parse_args()
    try:
        app, count, base = relocate(args.input.read_bytes(), french=args.french)
        args.output.write_bytes(app)
    except (OSError, ValueError) as error:
        print("error: {}".format(error), file=sys.stderr)
        return 1
    print("Pre-relocated {} Flash references at 0x{:06X}; saved {} bytes; wrote {}"
          .format(count, base, count * 6, args.output))
    return 0


if __name__ == "__main__":
    sys.exit(main())
