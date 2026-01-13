# Corsair Lighting Node Core DKMS Driver

[![CI](https://github.com/rossigee/corsair-lncore-dkms/actions/workflows/ci.yml/badge.svg)](https://github.com/rossigee/corsair-lncore-dkms/actions/workflows/ci.yml)
[![Release](https://github.com/rossigee/corsair-lncore-dkms/actions/workflows/release.yml/badge.svg)](https://github.com/rossigee/corsair-lncore-dkms/actions/workflows/release.yml)
[![Latest Release](https://img.shields.io/github/v/release/rossigee/corsair-lncore-dkms)](https://github.com/rossigee/corsair-lncore-dkms/releases)

This is a DKMS driver for the Corsair Lighting Node Core USB device, allowing control of RGB lighting.

## Installation

1. Download the latest source package from the [GitHub Releases](https://github.com/rossigee/corsair-lncore-dkms/releases) page (e.g., `.dsc`, `.orig.tar.gz`, `.debian.tar.xz`).
2. Build and install: `dpkg-buildpackage -us -uc && sudo dpkg -i ../corsair-lncore-dkms_*.deb`
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

### Direct LED Control (Write Interface)

Write to the device file with format: channel (1 byte), num_leds (1 byte), then RGB bytes (3 per LED).

**Format:** `[channel: u8][num_leds: u8][led1_r: u8][led1_g: u8][led1_b: u8][led2_r: u8]...`

**Parameters:**
- `channel`: Channel number (0-1)
- `num_leds`: Number of LEDs to control (1-204)
- RGB values: 0-255 for each color component

Example: To set 2 LEDs on channel 0 to red and green:
```
echo -e '\x00\x02\xff\x00\x00\x00\xff\x00' > /dev/corsair-lncore0
```

### ioctl Commands

The driver supports several ioctl commands for advanced control:

#### CORSAIR_IOC_SET_BRIGHTNESS
Set brightness for channel 0.

```c
unsigned char brightness; // 0-100
ioctl(fd, CORSAIR_IOC_SET_BRIGHTNESS, &brightness);
```

#### CORSAIR_IOC_SET_EFFECT
Configure hardware effects on a channel.

```c
struct effect_config {
    unsigned char channel;      // 0-1
    unsigned char num_leds;     // 1-204
    unsigned char mode;         // Effect mode
    unsigned char speed;        // 0-4 (slow to fast)
    unsigned char direction;    // 0-1
    unsigned char random;       // 0-1
    unsigned char red1, grn1, blu1;  // Color 1 (RGB)
    unsigned char red2, grn2, blu2;  // Color 2 (RGB)
    unsigned char red3, grn3, blu3;  // Color 3 (RGB)
};

ioctl(fd, CORSAIR_IOC_SET_EFFECT, &config);
```

#### CORSAIR_IOC_GET_FIRMWARE
Get firmware version string.

```c
char firmware[16];
ioctl(fd, CORSAIR_IOC_GET_FIRMWARE, firmware);
```

## Udev Rules

The package installs udev rules for non-root access to `/dev/corsair-lncore0`. The device is accessible by users in the `plugdev` group.

To add your user to the plugdev group:
```bash
sudo usermod -a -G plugdev $USER
# Log out and back in for the group change to take effect
```

Reload udev rules if needed: `sudo udevadm control --reload-rules && sudo udevadm trigger`.

## Secure Boot

### Signing the Kernel Module

For Secure Boot compatibility, the kernel module must be signed with a key enrolled in your system's MOK (Machine Owner Key) database.

#### 1. Generate a signing key (one-time setup):
```bash
# Create private key
openssl req -new -x509 -newkey rsa:2048 -keyout MOK.priv -outform DER -out MOK.der -nodes -subj "/CN=Owner/"

# Enroll the key in MOK
sudo mokutil --import MOK.der
# Reboot and follow the MOK enrollment process
```

#### 2. Sign the module after building:
```bash
# Find the module
find /lib/modules/$(uname -r) -name "corsair-lncore.ko"

# Sign it
sudo /usr/src/linux-headers-$(uname -r)/scripts/sign-file sha256 MOK.priv MOK.der /path/to/corsair-lncore.ko
```

#### 3. Alternative: Disable Secure Boot
You can disable Secure Boot in your UEFI firmware settings, but this reduces system security.

### Verification
Check if the module loads properly:
```bash
sudo modprobe corsair-lncore
dmesg | tail
```

## Troubleshooting

### Module won't load
- **Secure Boot issue**: Sign the module as described above
- **Missing dependencies**: Ensure DKMS is installed
- **Kernel version mismatch**: Rebuild the module for your current kernel

### Device not detected
- Check USB connection: `lsusb | grep 1b1c:0c1a`
- Verify permissions: `ls -la /dev/corsair-lncore0`
- Reload udev rules: `sudo udevadm control --reload-rules && sudo udevadm trigger`

### ioctl commands fail
- **Permission denied**: Ensure you have write access to the device
- **Invalid parameters**: Check parameter ranges in the API documentation
- **Device busy**: Wait for previous operations to complete

### Common Issues
- **No /dev/corsair-lncore0**: Check that the device is plugged in and module loaded
- **Permission denied**: Add user to appropriate group or use sudo
- **Invalid argument**: Verify ioctl parameters match the expected ranges