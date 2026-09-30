# Cakerybdis: changes from upstream

Cakerybdis is a fork of [280Zo/charybdis-wireless-mini-zmk-firmware](https://github.com/280Zo/charybdis-wireless-mini-zmk-firmware). Every change from upstream lives in its own commit, so a feature can be reviewed, reverted or cherry-picked on its own. Each section below is one of those commits.

| # | Feature | Files |
|---|---|---|
| 1 | [Keyboard name](#1-keyboard-name-cakerybdis-0) | `boards/shields/charybdis_common/Kconfig.bt.defconfig` |
| 2 | [Bluetooth pairing stability + passkey](#2-bluetooth-pairing-stability--passkey-entry) | `boards/shields/charybdis_right_bt/charybdis_right_bt.conf` |
| 3 | [Trackball orientation](#3-trackball-orientation) | `boards/shields/charybdis_trackball/charybdis_pmw3610.dtsi` |
| 4 | [Corne-style qwerty keymap](#4-corne-style-qwerty-keymap) | `config/keymaps/qwerty.keymap`, `config/_qwerty_dev.keymap` |
| 5 | [No-DC/DC variant for a faulty nice!nano](#5-no-dcdc-variant-for-a-faulty-nicenano) | `config/no_dcdc/`, `build.yaml`, `.github/workflows/build.yml`, `local-build/build_setup.sh` |

---

## 1. Keyboard name: "Cakerybdis 0"

The right (central) half advertises over Bluetooth and USB as **`Cakerybdis 0`** instead of `Charybdis`, which makes it easy to pick out of the OS pairing list when several boards are around. To rename it, change `ZMK_KEYBOARD_NAME` in `Kconfig.bt.defconfig`. ZMK limits the name to 16 characters.

## 2. Bluetooth pairing stability + passkey entry

These settings are added to the right half's `.conf`, the half that pairs with the host:

| Setting | Why |
|---|---|
| `CONFIG_BT_AUTO_PHY_UPDATE=n`<br>`CONFIG_BT_AUTO_DATA_LEN_UPDATE=n`<br>`CONFIG_BT_GAP_AUTO_UPDATE_CONN_PARAMS=n` | Pairing with Windows sometimes failed with disconnect reason `0x23` (`LL_PROC_COLLISION`). The keyboard's own automatic PHY, data-length and connection-parameter updates were colliding with the pairing handshake. With them off, the host drives those updates. |
| `CONFIG_ZMK_BLE_PASSKEY_ENTRY=y` | Without passkey entry the keyboard only supports "Just Works" pairing, and some Android builds reject that for keyboards (`Security failed err 1 AUTH_FAIL`). With it on, pairing gets MITM protection. |

**Pairing with passkey entry:** when the host shows a 6-digit code, type it on the keyboard and press **Enter**.

## 3. Trackball orientation

`swap-xy` and `invert-x` are removed from the PMW3610 node, and only `invert-y` is kept, to match how the sensor is mounted in this build. If the pointer moves the wrong way on a different build, re-add the properties you need. The options are `swap-xy`, `invert-x` and `invert-y`.

## 4. Corne-style qwerty keymap

`qwerty.keymap` is reworked to match my Corne layout ([corne-wireless-view-zmk-config](https://github.com/maybecake/corne-wireless-view-zmk-config)), so both keyboards behave the same.

**Layers**

| # | Layer | Purpose |
|---|---|---|
| 0 | **MAC** | Default base layer, with macOS-style modifiers |
| 1 | NUM | Unchanged from upstream |
| 2 | NAV | Unchanged from upstream |
| 3 | **SYM** | Redesigned: symbols on the left, numpad (7-8-9 / 4-5-6 / 1-2-3, `0` on a thumb) on the right |
| 4 | GAME | Unchanged, except the bottom-left key is now transparent |
| 5 | EXTRAS | Unchanged from upstream |
| 6 | SLOW | Precision pointer (from upstream) |
| 7 | SCROLL | Trackball becomes a scroll wheel (from upstream) |
| 8 | **WIN** | Same as MAC, but Cmd is replaced with Ctrl |
| 9 | **CONN** | Bluetooth: `BT_CLR` on the Esc position, `BT_SEL 0–4` on Q–T, bootloader on both bottom corners |
| 10 | **MOVE** | Home/Up/End/PgUp on W/E/R/T, Left/Down/Right/PgDn on S/D/F/G, word jumps (Alt+←/→) and line start/end (Ctrl+A / Ctrl+E) on the bottom row |
| 11 | **MOUSE** | S/D/F = right/middle/left click, hold A = SLOW, hold Z = SCROLL |

**Base layer (MAC / WIN)**

- **Home-row mods on the E/S/D/F and I/J/K/L columns.**
  - Left: E = Ctrl, S = Alt, D = Cmd, F = Shift.
  - Right: I = Ctrl, J = Shift, K = Cmd, L = Alt.
  - On WIN, D and K are Ctrl instead of Cmd.
- **Left thumbs:** hold for MOUSE / Backspace / tap Enter, hold for SYM.
- **Right thumbs:** `cap` (tap = Delete, double-tap = Caps Word) / tap Space, hold for MOVE.
- **Bottom-left outer key, `conn_or_os`:**
  - Hold for CONN.
  - Tap once for GUI, twice for MAC, three times for WIN, four times for GAME.
- **Bottom-right outer key:** double-tap to lock the screen. On MAC this sends Ctrl+Cmd+Q (`lockmac`). On WIN it sends Win+L (`lockwin`).
- **Outer columns:** Esc / Tab on the left, `\` / `'` on the right.

`config/_qwerty_dev.keymap` is a scratch copy used while developing the layout. It differs only in whitespace, and the build ignores it, because builds only read `config/keymaps/*.keymap`.

## 5. No-DC/DC variant for a faulty nice!nano

One nice!nano in a batch had a broken DC/DC regulator, so ZMK never started on it: no USB and no Bluetooth, even though the bootloader worked. A device-tree overlay switches the nRF52840's regulators to LDO mode, and an extra `build.yaml` entry (`bt_no_dcdc`) builds it as a separate, clearly named variant. To turn the variant on or off, comment out or uncomment that entry.

To support the naming, this commit adds an `artifact_suffix` field to `build.yaml` entries. Both the GitHub Actions workflow and the local Docker build append it to output filenames (`charybdis_right_bt_no_dcdc.uf2`).

The full write-up covers the symptoms, the trade-offs, the build toggle and the debugging steps. See **[config/no_dcdc/README.md](config/no_dcdc/README.md)**.

---

## Keeping up with upstream

Remotes in the local clone:

```
origin    https://github.com/maybecake/cakerybdis.git                               (this fork)
upstream  git@github.com:280Zo/charybdis-wireless-mini-zmk-firmware.git          (original)
```

To pull in upstream changes:

```bash
git fetch upstream
git rebase upstream/main    # keeps the per-feature commits on top
git push --force-with-lease origin main
```

If a rebase conflicts, the per-feature commits make it clear which change is involved.

> **Local build warning:** `local-build/build_setup.sh` deletes everything in `firmwares/` before building. Move hand-numbered test builds (e.g. `*_v6.uf2`) somewhere else first.
