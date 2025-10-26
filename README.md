# Corsair Lighting Node Core DKMS Driver

This is a DKMS driver for the Corsair Lighting Node Core USB device, allowing control of RGB lighting.

## Installation

1. Download the latest `.deb` package from the [GitHub Releases](https://github.com/rossigee/corsair-lncore-dkms/releases) page.
2. Install the package: `sudo dpkg -i corsair-lncore-dkms_*.deb`
3. Plug in the device; it should create `/dev/corsair-lncore0` and load the module automatically.

### Dependencies

The package depends on DKMS, which will be installed automatically. For Secure Boot, you may need to sign the kernel module.

### Building from Source (for Developers)

If you want to build the package yourself:

1. Install build dependencies: `sudo apt-get install dkms debhelper devscripts`
2. Clone the repo: `git clone https://github.com/rossigee/corsair-lncore-dkms.git`
3. Build: `dpkg-buildpackage -us -uc`
4. Install: `sudo dpkg -i ../corsair-lncore-dkms_*.deb`

## Usage

Write to the device file with format: channel (1 byte), num_leds (1 byte), then RGB bytes (3 per LED).

Example: To set 2 LEDs on channel 0 to red and green:
```
echo -e '\x00\x02\xff\x00\x00\x00\xff\x00' > /dev/corsair-lncore0
```

## Udev Rules

The package installs udev rules for non-root access to `/dev/corsair-lncore0`. Reload udev if needed: `sudo udevadm control --reload-rules && sudo udevadm trigger`.

## Secure Boot

For Secure Boot, sign the module or disable it.