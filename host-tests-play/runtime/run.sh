#!/bin/sh
set -eu
cd "$(dirname "$0")"
PLAY_TEST_BUILD_DIR=$(mktemp -d "${TMPDIR:-/tmp}/crossink-play-runtime.XXXXXX")
trap 'rm -rf "$PLAY_TEST_BUILD_DIR"' EXIT
SDK=../../freeink-sdk
python3 ../../scripts/apply_play_sdk_patches.py
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -pthread \
  -I../../lib/GfxRenderer -I"$SDK/libs/ui/FreeInkUI/include" \
  test_runtime.cpp -o "$PLAY_TEST_BUILD_DIR/test_runtime"
"$PLAY_TEST_BUILD_DIR/test_runtime"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -DCROSSINK_PLAY \
  -Istubs -I"$SDK/libs/hardware/SDCardManager/test/host/stubs" \
  -I"$SDK/libs/hardware/SDCardManager/include" \
  test_free_space.cpp "$SDK/libs/hardware/SDCardManager/src/SDCardManager.cpp" \
  -o "$PLAY_TEST_BUILD_DIR/test_free_space"
"$PLAY_TEST_BUILD_DIR/test_free_space"
python3 test_sdk_patch.py
