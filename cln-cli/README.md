# Corsair Lighting Node Core CLI Tool

A simple command-line interface for controlling Corsair Lighting Node Core RGB devices.

## Requirements

- Linux system with the corsair-lncore-dkms kernel module installed and loaded
- Corsair Lighting Node Core device connected
- User must be in the `plugdev` group (see main README)

## Building

```bash
cd cln-cli
make
```

## Installation

```bash
sudo make install
```

Or manually copy the `cln-cli` binary to `/usr/local/bin/`.

## Usage

```bash
cln-cli <command> [arguments]
```

### Commands

#### Set All LEDs to a Color
```bash
cln-cli set-color <channel> <red> <green> <blue>
```
- `channel`: 0 or 1
- `red`, `green`, `blue`: 0-255

Example:
```bash
cln-cli set-color 0 255 0 0    # Set channel 0 to red
cln-cli set-color 1 0 255 0    # Set channel 1 to green
```

#### Set Individual LED
```bash
cln-cli set-led <channel> <led_index> <red> <green> <blue>
```
- `channel`: 0 or 1
- `led_index`: 0-59 (LED position)
- `red`, `green`, `blue`: 0-255

Example:
```bash
cln-cli set-led 0 0 255 255 255    # Set first LED on channel 0 to white
```

#### Set Brightness
```bash
cln-cli set-brightness <brightness>
```
- `brightness`: 0-100 (percentage)

Example:
```bash
cln-cli set-brightness 75    # Set brightness to 75%
```

#### Set Hardware Effect
```bash
cln-cli set-effect <channel> <num_leds> <mode> <speed> <direction> <random> <r1> <g1> <b1> <r2> <g2> <b2> <r3> <g3> <b3>
```

This is a low-level command that requires detailed knowledge of the hardware effects protocol. For most users, `set-color` and `set-led` are recommended instead.

#### Get Firmware Version
```bash
cln-cli get-firmware
```

Example output:
```
Firmware version: 1.2.3
```

#### Help
```bash
cln-cli help
```

## Examples

### Rainbow Effect (Simple)
Create a rainbow by setting individual LEDs:

```bash
# Red LEDs
for i in {0..19}; do cln-cli set-led 0 $i 255 0 0; done
# Orange LEDs
for i in {20..39}; do cln-cli set-led 0 $i 255 165 0; done
# Yellow LEDs
for i in {40..59}; do cln-cli set-led 0 $i 255 255 0; done
```

### Pulsing Effect
```bash
while true; do
    cln-cli set-brightness 100
    sleep 0.5
    cln-cli set-brightness 20
    sleep 0.5
done
```

### Gradient
Create a smooth gradient from red to blue:

```bash
for i in {0..59}; do
    r=$((255 - i*4))
    b=$((i*4))
    cln-cli set-led 0 $i $r 0 $b
done
```

## Troubleshooting

- **Permission denied**: Make sure you're in the `plugdev` group
- **Device not found**: Check that the module is loaded (`lsmod | grep corsair`) and device is connected (`ls /dev/corsair-lncore*`)
- **Invalid argument**: Check parameter ranges and command syntax
- **No effect**: Some effects require hardware support and may not work on all LED types

## Uninstallation

```bash
sudo make uninstall
```