#!/bin/sh
# Compile every imported freestanding game screen against CrossInk's pinned
# FreeInkSDK, then exercise layout adapters and all 25 shelf rows.
set -eu
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-play-ui-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
SDK=../../freeink-sdk/libs/ui/FreeInkUI
ICONS=../../freeink-sdk/libs/assets/Icons
JSONSAX=../../freeink-sdk/libs/network/JsonSax
mkdir -p "$BUILD_DIR"
set -- ../../src/apps_local/ShelfScreen.cpp ../../src/apps_local/*/*Screens.cpp \
  ../../src/apps_local/player/PlayerScreen.cpp ../../src/apps_local/toybattle/ToyBattleMenus.cpp \
  ../../src/apps_local/toybattle/ToyBattleHowTo.cpp
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -Wno-comment \
  -I"$SDK/include" -I"$ICONS/include" -I"$JSONSAX/include" -fsyntax-only "$@"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -Wno-comment \
  -I"$SDK/include" -I"$ICONS/include" -I"$JSONSAX/include" "$SDK/src/FreeInkUI.cpp" \
  ../../src/apps_local/ShelfScreen.cpp ../../src/apps_local/player/PlayerName.cpp \
  ../../src/apps_local/player/PlayerAvatar.cpp test_ui.cpp -o "$BUILD_DIR/test_ui"
"$BUILD_DIR/test_ui"
