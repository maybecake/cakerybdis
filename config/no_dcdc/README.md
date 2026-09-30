# No-DC/DC firmware (faulty nice!nano fix)

One nice!nano from a batch of identical boards (USB serial `92D0F4962CD6DBFB`) won't run ZMK with the stock board config. Its bootloader and Arduino sketches work fine. ZMK enables the nRF52840's DC/DC regulators, and on this board they appear to be broken, most likely a missing or bad inductor. Running both regulators in LDO mode fixes it.

## Symptoms

- After flashing, no COM port shows up and nothing appears in the Bluetooth device list.
- Getting into the bootloader needs a triple tap instead of a double tap. This went away once a complete firmware was on the board.
- Every ZMK build fails the same way, including `settings_reset`. The bootloader and the PlatformIO sk6812 tester still work.

## Fix

[`no_dcdc.overlay`](no_dcdc.overlay) disables `&reg0` and sets `&reg1` to LDO mode.

### Turning it on or off

The variant is a single entry in [`build.yaml`](../../build.yaml):

```yaml
  - name: bt_no_dcdc
    board: nice_nano//zmk
    shield:
      - charybdis_left_bt
      - charybdis_right_bt
    keymap:
      - qwerty
    extra_dtc_overlay_files:
      - no_dcdc/no_dcdc.overlay
    artifact_suffix: _no_dcdc
```

- **Enable:** leave the entry uncommented (the default). The normal `bt` builds still get built next to it.
- **Disable:** comment out the whole entry.
- **Other keymaps or shields:** add them to the entry's lists. The same entry pattern works for dongle builds too.

`artifact_suffix` is a field specific to this fork. Both the GitHub Actions workflow and `local-build/build_setup.sh` append it to every output filename, so faulty-board firmware can't be mixed up with normal builds:

| Build | Output |
|---|---|
| GitHub Actions | `firmware-bt_no_dcdc-qwerty` artifact, containing `charybdis_right_bt-qwerty_no_dcdc.uf2` etc. |
| Local Docker build | `firmwares/charybdis_bt_no_dcdc/qwerty/charybdis_right_bt_no_dcdc.uf2` etc. |

For a one-off build, pass the overlay by hand: `west build ... -- -DEXTRA_DTC_OVERLAY_FILE=<path>/no_dcdc.overlay`.

Flash `*_no_dcdc.uf2` **only to the faulty board**. Keep the normal builds for every other board.

**Trade-off:** LDO mode roughly doubles the chip's active current (radio and CPU). Sleep current barely changes. Expect about 20–40% shorter battery life on that half. On USB power it makes no difference.

## Debugging recap

1. **Checked the bootloader.** `INFO_UF2.TXT` showed a genuine nice!nano bootloader: UF2 0.11.0, S140 6.1.1, USB `239A:00B3`. The v6 UF2 matched the board's flash layout: start `0x26000`, family `0xADA52840`.
2. **Compared `CURRENT.UF2` against the v6 UF2.** The first ~156 KB (`0x26000–0x4D000`) still held an old ZMK build on Zephyr 3.2. The drag-and-drop copy had written only part of the file, which explains the triple-tap symptom.
3. **Erased the app region and reflashed.** A UF2 filled with `0xFF` (covering `0x26000–0xF4000`) cleared the app region. Reflashing v6 over serial DFU (`adafruit-nrfutil`, COM port) gave a verified, complete image, but ZMK still didn't start.
4. **Flashed the sk6812 tester to isolate the hardware.** Its COM port came up and `millis()` kept accurate time, so USB, the CPU and the 32 kHz crystal all work.
5. **Tested a bare `settings_reset` build (ZMK only, no Charybdis code).** It didn't enumerate either, so the fault is in the base nice!nano board config.
6. **Diffed ZMK against Arduino.** ZMK's `nice_nano.dts` puts `reg1` in DC/DC mode and the v2 overlay enables `reg0`. The Arduino core enables neither.
7. **Rebuilt `settings_reset` with this overlay.** It enumerated immediately as ZMK (`1D50:615E`) with the same chip serial. The full left and right builds with the overlay work.

Not yet narrowed down: whether REG0, REG1 or both are at fault. Building with only one of them in LDO mode would show which one, and which inductor to inspect: REG1 uses 10 µH + 15 nH on `DCC`/`DEC4`, REG0 uses 10 µH on `DCCH`. If REG0 turns out to be fine, it can be re-enabled to recover some battery life.
