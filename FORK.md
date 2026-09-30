# Cakerybdis: changes from upstream

Cakerybdis is a fork of [280Zo/charybdis-wireless-mini-zmk-firmware](https://github.com/280Zo/charybdis-wireless-mini-zmk-firmware). Every change from upstream lives in its own commit, so a feature can be reviewed, reverted or cherry-picked on its own. Each section below is one of those commits.

| # | Feature | Files |
|---|---|---|
| 1 | [Keyboard name](#1-keyboard-name-cakerybdis-0) | `boards/shields/charybdis_common/Kconfig.bt.defconfig` |
| 2 | [Bluetooth pairing stability + passkey](#2-bluetooth-pairing-stability--passkey-entry) | `boards/shields/charybdis_right_bt/charybdis_right_bt.conf` |
| 3 | [Trackball orientation](#3-trackball-orientation) | `boards/shields/charybdis_trackball/charybdis_pmw3610.dtsi` |
| 4 | [Corne-style qwerty keymap](#4-corne-style-qwerty-keymap) | `config/keymaps/qwerty.keymap`, `config/_qwerty_dev.keymap` |
| 5 | [No-DC/DC variant for a faulty nice!nano](#5-no-dcdc-variant-for-a-faulty-nicenano) | `config/no_dcdc/`, `build.yaml`, `.github/workflows/build.yml`, `local-build/build_setup.sh` |
| 6 | [Forked + pinned dependencies](#dependencies) | `config/west.yml` |

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

## Dependencies

This repo only holds configuration. The firmware source (ZMK, Zephyr, the trackball driver and the Prospector module) is downloaded at build time by [west](https://docs.zephyrproject.org/latest/develop/west/index.html), as listed in [`config/west.yml`](config/west.yml). To make sure the keyboard always builds from code under my control:

- every external repo is **forked** to my account with a `cakerybdis-` prefix, and
- every project is **pinned to an exact commit SHA** instead of a branch, so a build next year produces the same firmware as today.

| west project | Fork (built from) | Forked from | Pinned commit | Branch | Local changes |
|---|---|---|---|---|---|
| `zmk` | [maybecake/cakerybdis-zmk](https://github.com/maybecake/cakerybdis-zmk) | [zmkfirmware/zmk](https://github.com/zmkfirmware/zmk) | `8e99aa8f` | `cakerybdis` | Split-central disconnect fix + Zephyr pin (see below) |
| `zmk-pmw3610-driver` | [maybecake/cakerybdis-zmk-pmw3610-driver](https://github.com/maybecake/cakerybdis-zmk-pmw3610-driver) | [280Zo/zmk-pmw3610-driver](https://github.com/280Zo/zmk-pmw3610-driver) (itself from [badjeff](https://github.com/badjeff/zmk-pmw3610-driver)) | `f6e23436` | `main` | None. Includes 280Zo's cursor-jump-on-wake patch. |
| `prospector-zmk-module` | [maybecake/cakerybdis-prospector-zmk-module](https://github.com/maybecake/cakerybdis-prospector-zmk-module) | [280Zo/prospector-zmk-module](https://github.com/280Zo/prospector-zmk-module) | `45f174c6` | `main` | None. Only used by Prospector dongle builds. |
| `zephyr` (+ HALs) | not forked: [zmkfirmware/zephyr](https://github.com/zmkfirmware/zephyr) | — | `10ba6d0c` | — | Pinned from inside `cakerybdis-zmk`'s `app/west.yml`. Zephyr's own modules (hal_nordic etc.) are pinned by Zephyr's manifest. |

The project `name`s in `west.yml` (`zmk`, `zmk-pmw3610-driver`, `prospector-zmk-module`) must stay the same, because the build script and CI use them as folder names. `repo-path:` is what points each one at its `cakerybdis-*` repo.

### Changes in `cakerybdis-zmk`

The `cakerybdis` branch is upstream ZMK `9ebbeff0` (2026-09-14) plus two commits:

1. **`fix(split): ignore host disconnects in split central disconnect handler`**. `split_central_disconnected()` is a global connection callback, so it also fired when the *host* disconnected. That tore down split state for no reason. It now returns early unless this device is the BLE central on that connection. It was originally a local edit made while debugging Bluetooth pairing, and every firmware since v7 was built with it.
2. **`chore(west): pin zephyr to 10ba6d0cb38b`**. ZMK tracks the `v4.1.0+zmk-fixes` branch of its Zephyr fork, and this pins the exact commit the firmware was tested with.

### Updating a dependency

Every update is a deliberate commit to `config/west.yml` (plus the fork), so it can be reverted in one step if a build breaks.

**Trackball driver or Prospector module** (plain forks, no local changes):

```bash
# 1. Sync the fork with its upstream (or click "Sync fork" on GitHub)
gh repo sync maybecake/cakerybdis-zmk-pmw3610-driver --source 280Zo/zmk-pmw3610-driver

# 2. Get the new commit SHA
gh api repos/maybecake/cakerybdis-zmk-pmw3610-driver/commits/main --jq .sha

# 3. Put that SHA in the project's `revision:` in config/west.yml, update the
#    date/subject comment above it, then build, flash, test and commit.
```

**ZMK** (fork with local commits on the `cakerybdis` branch):

```bash
cd zmk                                          # west's checkout
git remote add upstream https://github.com/zmkfirmware/zmk.git   # once
git remote add fork https://github.com/maybecake/cakerybdis-zmk.git  # once
git fetch upstream main
git switch cakerybdis
git rebase upstream/main          # replays the 2 cakerybdis commits on new ZMK
```

- If upstream's `app/west.yml` changed the Zephyr revision, update the pinned SHA in the Zephyr-pin commit to the current head of that branch: `gh api repos/zmkfirmware/zephyr/commits/<branch> --jq .sha`.
- If upstream fixed the disconnect bug itself, drop that commit during the rebase.

```bash
git push --force-with-lease fork cakerybdis
git rev-parse HEAD                # -> new `revision:` for zmk in config/west.yml
```

Then run a normal build (`west update` runs automatically), flash, test and commit `config/west.yml`.

> Run a **plain** `west update`, without project names, after changing ZMK's pin. West can only update Zephyr and the other projects that ZMK brings in through a full update.

**Adding another external module:** fork it as `cakerybdis-<name>`, add a project with `remote: maybecake`, `repo-path: cakerybdis-<name>` and a SHA `revision:`, and add it to the table above.

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
