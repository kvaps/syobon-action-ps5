#!/usr/bin/env bash
# syobon-action-ps5 - rebuild the home-screen art.
# Copyright (C) 2026 Andrei Kvapil
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Rebuild sce_sys/icon0.png, pic0.dds and pic1.dds from the game's sprites.
#
#   BC7ENC=/path/to/bc7enc art/make.sh
#
# bc7enc is richgel999/bc7enc_rdo; it stands in for texconv, which the
# template's own converter needs Windows for.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
bc7enc=${BC7ENC:-bc7enc}
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT

python3 -I "$root/art/compose.py" "$root/sce_sys"
cp "$root/sce_sys/background-source.png" "$work/pic0.png"
cp "$root/sce_sys/launch-background-source.png" "$work/pic1.png"
for name in pic0 pic1; do
    (cd "$work" && "$bc7enc" -q -g -u4 "$name.png" >/dev/null)
    # One mip level, declared as the console's loader expects (texconv's header)
    python3 -I - "$work/$name.dds" <<'PY'
import struct, sys
path = sys.argv[1]
with open(path, "r+b") as dds:
    flags = struct.unpack_from("<I", dds.read(12), 8)[0]
    dds.seek(8)
    dds.write(struct.pack("<I", flags | 0x20000))
    dds.seek(28)
    dds.write(struct.pack("<I", 1))
PY
    cp "$work/$name.dds" "$root/sce_sys/$name.dds"
done
