# Local ZMK Firmware Builds (Dockerized)

This setup provides a fast, containerized build environment for [ZMK Firmware](https://zmk.dev), allowing you to compile firmware locally without pushing changes to GitHub or installing any toolchains on your system.

- Rapid feedback loop for keymap and overlay changes
- No GitHub Actions delay
- No toolchain installation required
- Cross-platform, reproducible builds
- Automatic shield and keymap discovery from build.yaml
- Isolated build sandboxes per shield
- ZMK Studio USB-UART support on central shields
- Artifacts published by format and keymap to a persistent folder for easy access


## Quick Start

### 1. Prerequisites

Make sure you have:

- [Docker](https://docs.docker.com/get-docker/)
- [Docker Compose](https://docs.docker.com/compose/install/)
- This repository cloned on your host machine


### 2. Start the Build Container

```bash
docker-compose -f local-build/docker-compose.yml run --rm builder
```

### 3. Flash the Firmware

Firmware files are placed under the `firmwares/` directory at the root of the repo.


## Quick incremental builds

`build_setup.sh` copies the whole repo into a fresh sandbox and builds from scratch every time, which is very slow on a Windows bind mount. For iterating on one variant, keep a container running and use `quick_build.sh`, which builds straight from the mount and keeps each variant's build directory in the container:

```bash
# once (the container restarts with Docker)
docker run -d --name cakerybdis-builder --restart unless-stopped \
  -v "C:/projects/cakerybdis:/workspaces/zmk" -w /workspaces/zmk \
  zmkfirmware/zmk-dev-arm:stable sleep infinity

# LED keyboard ("Cakerybdis L")
docker exec cakerybdis-builder bash local-build/quick_build.sh cakerybdis_l qwerty charybdis_left_bt charybdis_right_bt
# No-LED keyboard ("Cakerybdis 0")
docker exec cakerybdis-builder bash local-build/quick_build.sh cakerybdis_0 qwerty charybdis_left_bt charybdis_right_bt -- no_led/no_led.conf
```

Files after `--` are relative to `config/`; `.overlay` files are passed as devicetree overlays, e.g. `-- no_dcdc/reg1_ldo.overlay` for a left half with a faulty REG1. Output goes to `firmwares/<name>/<name>_left.uf2` and `<name>_right.uf2`. The first build of a name takes several minutes; later builds only recompile what changed. Run `west update` yourself (`docker exec cakerybdis-builder west update`) after changing `config/west.yml`. Note that `build_setup.sh` empties `firmwares/` when it runs.

## Troubleshooting

### Drop into a shell in the build container

This will give you an interactive bash prompt with all dependencies loaded, so you can run or debug the script manually:

```bash
docker-compose run --rm --entrypoint bash builder
```

Once inside the shell, you can manually test/troubleshoot or you can execute the script manually:

```bash
bash ./local-build/build_setup.sh
```

### Runtime USB Logging

Enable USB logging to troubleshoot the firmware while it's running on the keyboard. This is disabled by default due to the significant negative impact on battery life, but can easily be enabled for testing through an environment variable in the build command:

```bash
ENABLE_USB_LOGGING=true docker-compose -f local-build/docker-compose.yml run --rm builder
```

Follow instructions [here](https://zmk.dev/docs/development/usb-logging) to see the log stream on your computer.

Since this is a split keyboard you'll have to change the tty device depending on what you want log output from (e.g. `ttyACM0` for dongle, `ttyACM1` for right side, and `ttyACM2` for left side.

To enable PMW3610 sensor debug logging, also uncomment `CONFIG_PMW3610_ALT_LOG_LEVEL_DBG=y` in the relevant shield conf.

### Local Module Testing

When you deliberately want to build whatever is already checked out in your local module folders use the `SKIP_WEST_UPDATE=true` environment variable:

```bash
SKIP_WEST_UPDATE=true docker-compose -f local-build/docker-compose.yml run --rm builder
```

Normal builds should not use this so `west update` can pull the configured module revisions.

### Patched APDS9960 Zephyr Driver

The Prospector APDS9960 sensor builds expect Zephyr's stock APDS9960 driver to be disabled and the Prospector module replacement driver to be enabled.

The build script prints a resolved APDS9960 config block after each build so this is visible in the terminal output.

### Build Logs

Check the build script output for any warnings or errors. Most of the times it's a simple missing shield/keymaps, but sometimes it'll be a valid build failures. To save the output to a file for parsing and review:

  ```bash
  docker-compose run --rm builder > logs.txt 2>&1
  ```


## More Resources

- [ZMK Documentation](https://zmk.dev/docs)
- [ZMK Dev Container Setup](https://zmk.dev/docs/development/local-toolchain/setup/container)
