#!/bin/bash
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Charles Vestal (from fm1-x0x); modified for HAWK CHUA on FM1 Open (github.com/Plieuw/hawk-chua)
# FM-1 rescue for an FM-1 whose FM1 Open firmware crashes at start-up (HAWK CHUA FM1 Open beta 1): through the chip's
# own update mode, it puts a working firmware back: M-VAVE's FM1 Open V14 (built unchanged from their source) when the
# FM-1's flash head is FM1 Open's, else M-VAVE's stock V15 / V16 (as the original script). macOS, Linux.
#   bash -c "$(curl -fsSL https://raw.githubusercontent.com/Plieuw/hawk-chua/main/open/rescue/fm1_open_rescue.sh)"
set -e
DIR="$HOME/fm1-rescue"
RAW="https://raw.githubusercontent.com/Plieuw/hawk-chua/main/open/rescue"
OPEN_PKG="https://github.com/Plieuw/hawk-chua/releases/download/fm1-open-rescue/mvave-fm1-open-v14.fwsc"
V15=db1642b2b6fa5c2cccb11ffd13878068bb28601678d3644049f99dc40e7edb8a
V16=d017fb65e70aca97ee29d944470c3515a74581df5c0576bf70de917c7b2b61d5
MVAVE="https://yms-file-store.oss-cn-hongkong.aliyuncs.com/software/firmware/FM-1.fwsc"
URL15="https://web.archive.org/web/20261009203224id_/$MVAVE"
sha() { if command -v shasum >/dev/null; then shasum -a 256 "$1"; else sha256sum "$1"; fi | cut -d' ' -f1; }
echo
echo "=== FM-1 rescue (FM1 Open) ==="
if [ "$(uname)" = Darwin ] && ! xcode-select -p >/dev/null 2>&1; then
    echo "This needs Apple's command line tools (free). A window will open: click Install."
    echo "When it has finished, run this same command again."
    xcode-select --install >/dev/null 2>&1 || true
    exit 1
fi
mkdir -p "$DIR"
cd "$DIR"
if [ ! -x env/bin/python ] || ! env/bin/python -c "import usb, libusb_package" 2>/dev/null; then
    echo "Setting up (once) ..."
    rm -rf env
    if [ "$(uname)" = Darwin ]; then PY=/usr/bin/python3; else PY=python3; fi
    if ! "$PY" -m venv env; then
        echo "Python's venv is missing: install it (on Ubuntu: sudo apt install python3-venv) and run this again."
        exit 1
    fi
    env/bin/pip install --quiet --disable-pip-version-check pyusb libusb-package
fi
curl -fsSL -o fm1_open_rescue.py "$RAW/fm1_open_rescue.py"
PKGS=()
echo "Downloading M-VAVE's FM1 Open (V14, built unchanged from their source) ..."
if curl -fsSL -o "$DIR/mvave-fm1-open-v14.fwsc" "$OPEN_PKG"; then
    PKGS+=("$DIR/mvave-fm1-open-v14.fwsc")
else
    echo "  (could not get it)"
fi
get() {
    local name=$1 want=$2 url=$3 f
    for f in "$DIR/FM-1-$name.fwsc" "$HOME"/Downloads/*.fwsc "$HOME"/Desktop/*.fwsc; do
        [ -f "$f" ] || continue
        if [ "$(sha "$f")" = "$want" ]; then
            [ "$f" = "$DIR/FM-1-$name.fwsc" ] || cp "$f" "$DIR/FM-1-$name.fwsc"
            return 0
        fi
    done
    echo "Downloading the stock firmware (M-VAVE's FM-1 $name) ..."
    curl -fsSL -o "$DIR/FM-1.download" "$url" || true
    if [ "$(sha "$DIR/FM-1.download" 2>/dev/null)" != "$want" ]; then
        rm -f "$DIR/FM-1.download"
        echo "  (could not get $name: not the expected file)"
        return 1
    fi
    mv "$DIR/FM-1.download" "$DIR/FM-1-$name.fwsc"
}
get V16 "$V16" "$MVAVE" && PKGS+=("$DIR/FM-1-V16.fwsc") || true
get V15 "$V15" "$URL15" && PKGS+=("$DIR/FM-1-V15.fwsc") || true
if [ ${#PKGS[@]} -eq 0 ]; then
    echo "Could not get any firmware. Nothing was done; send this output."
    exit 1
fi
echo "Firmware to choose from: ${PKGS[*]}"
echo
echo "If it asks for a password, type your computer's password (nothing shows while you type) and press Return."
echo "When it says it is waiting: plug the FM-1 in and switch it on (try holding REC while you switch it on)."
echo "If it restarts over and over: open https://plieuw.github.io/hawk-chua/open/rescue/ in Chrome and press Start:"
echo "it sends the FM-1 the update command the moment it appears, and this catches it. (Ctrl-C stops it.)"
echo "It reads and saves the whole flash first (a backup in $DIR), and asks before it writes anything."
echo
exec sudo "$DIR/env/bin/python" "$DIR/fm1_open_rescue.py" "${PKGS[@]}" --ask --wait 600
