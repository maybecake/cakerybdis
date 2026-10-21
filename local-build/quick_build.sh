#!/usr/bin/env bash
# quick_build.sh - Incremental build of one firmware variant inside a long-running container.
#
# Unlike build_setup.sh, this skips the sandbox copy of the whole repo and keeps each
# variant's build directory in the container (/build/<name>/<shield>), so rebuilds only
# recompile what changed. Start the container once (see local-build/README.md), then:
#
#   docker exec cakerybdis-builder bash local-build/quick_build.sh <name> <keymap> <shield>... [-- extra.conf|extra.overlay...]
#
# Example: quick_build.sh cakerybdis_0 qwerty charybdis_left_bt charybdis_right_bt -- no_led/no_led.conf
#          quick_build.sh cakerybdis_l_reg1_ldo qwerty charybdis_left_bt -- no_dcdc/reg1_ldo.overlay
# Output:  firmwares/<name>/<name>_<left|right>.uf2
# The shields, keymap and extra confs are fixed on a name's first build; to change them,
# use a new name or delete /build/<name> in the container.

set -euo pipefail

REPO_ROOT=/workspaces/zmk
BOARD=nice_nano//zmk

name="$1"; keymap="$2"; shift 2
shields=()
while [[ $# -gt 0 && "$1" != "--" ]]; do shields+=("$1"); shift; done
[[ "${1:-}" == "--" ]] && shift
extra_files=("$@")

# One-time setup per container: register Zephyr's CMake package.
if [ ! -d ~/.cmake/packages/Zephyr ]; then
  (cd "$REPO_ROOT" && west zephyr-export)
fi

for shield in "${shields[@]}"; do
  side=left
  [[ "$shield" == *right* ]] && side=right

  # Mirror the config layout build_setup.sh creates. cp -p keeps mtimes, so unchanged
  # files don't trigger a rebuild.
  cfg="/build/$name/$shield/config"
  mkdir -p "$cfg"
  cp -rp "$REPO_ROOT/config/." "$cfg/"
  cp -p "$REPO_ROOT"/config/charybdis/*.conf "$cfg/"
  cp -p "$cfg/keymaps/$keymap.keymap" "$cfg/$shield.keymap"

  snippet=()
  [[ "$shield" == "charybdis_right_bt" ]] && snippet=(-S studio-rpc-usb-uart)

  # Files after "--" are config-relative: .overlay files are devicetree overlays, the rest Kconfig.
  extra_conf_arg=()
  confs=(); overlays=()
  for c in "${extra_files[@]}"; do
    if [[ "$c" == *.overlay ]]; then overlays+=("$cfg/$c"); else confs+=("$cfg/$c"); fi
  done
  [ ${#confs[@]} -gt 0 ] && extra_conf_arg+=("-DEXTRA_CONF_FILE=$(IFS=';'; echo "${confs[*]}")")
  [ ${#overlays[@]} -gt 0 ] && extra_conf_arg+=("-DEXTRA_DTC_OVERLAY_FILE=$(IFS=';'; echo "${overlays[*]}")")

  printf '\n=== %s: %s (%s) ===\n' "$name" "$shield" "$keymap"
  out="/build/$name/$shield/out"
  if [ -f "$out/build.ninja" ]; then
    # Already configured: passing CMake args again would force a full re-configure.
    # Ninja re-runs CMake by itself when a .conf, overlay or keymap changes.
    west build -d "$out"
  else
    west build -s "$REPO_ROOT/zmk/app" -d "$out" -b "$BOARD" "${snippet[@]}" -- \
      -DZMK_CONFIG="$cfg" \
      -DSHIELD="$shield" \
      "${extra_conf_arg[@]}" \
      "-DZMK_EXTRA_MODULES=$REPO_ROOT/boards;$REPO_ROOT/zmk-pmw3610-driver;$REPO_ROOT/prospector-zmk-module"
  fi

  dest="$REPO_ROOT/firmwares/$name"
  mkdir -p "$dest"
  cp "/build/$name/$shield/out/zephyr/zmk.uf2" "$dest/${name}_${side}.uf2"
  chmod -R a+rwX "$dest"
  echo "-> firmwares/$name/${name}_${side}.uf2"
done
