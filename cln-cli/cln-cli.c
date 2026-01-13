#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>

// Same definitions as kernel module
#define CORSAIR_IOC_MAGIC 'C'
#define CORSAIR_IOC_SET_BRIGHTNESS _IOW(CORSAIR_IOC_MAGIC, 1, unsigned char)
#define CORSAIR_IOC_SET_EFFECT _IOW(CORSAIR_IOC_MAGIC, 2, struct effect_config)
#define CORSAIR_IOC_GET_FIRMWARE _IOR(CORSAIR_IOC_MAGIC, 3, char[16])

struct effect_config {
    unsigned char channel;
    unsigned char num_leds;
    unsigned char mode;
    unsigned char speed;
    unsigned char direction;
    unsigned char random;
    unsigned char red1, grn1, blu1;
    unsigned char red2, grn2, blu2;
    unsigned char red3, grn3, blu3;
};

#define DEVICE_PATH "/dev/corsair-lncore0"
#define MAX_LEDS 204

void usage(const char *prog_name) {
    printf("Usage: %s <command> [arguments]\n\n", prog_name);
    printf("Commands:\n");
    printf("  set-color <channel> <r> <g> <b>              Set all LEDs on channel to RGB color\n");
    printf("  set-led <channel> <led> <r> <g> <b>          Set specific LED to RGB color\n");
    printf("  set-brightness <brightness>                 Set brightness (0-100)\n");
    printf("  set-effect <channel> <num_leds> <mode> <speed> <direction> <random> <r1> <g1> <b1> <r2> <g2> <b2> <r3> <g3> <b3>\n");
    printf("                                              Set hardware effect on channel\n");
    printf("  get-firmware                                 Get firmware version\n");
    printf("  help                                         Show this help\n");
    printf("\nExamples:\n");
    printf("  %s set-color 0 255 0 0                       Set channel 0 to red\n", prog_name);
    printf("  %s set-brightness 50                          Set brightness to 50%%\n", prog_name);
    printf("  %s get-firmware                               Show device firmware version\n", prog_name);
}

int open_device() {
    int fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "Error: Cannot open %s: %s\n", DEVICE_PATH, strerror(errno));
        fprintf(stderr, "Make sure the device is connected and the module is loaded.\n");
        return -1;
    }
    return fd;
}

int set_color(int fd, int channel, int r, int g, int b) {
    if (channel < 0 || channel > 1) {
        fprintf(stderr, "Error: Channel must be 0 or 1\n");
        return -1;
    }
    if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
        fprintf(stderr, "Error: RGB values must be 0-255\n");
        return -1;
    }

    // Assume 60 LEDs per channel (common for Lighting Node Core)
    unsigned char buf[2 + 60 * 3];
    buf[0] = channel;
    buf[1] = 60;  // num_leds

    for (int i = 0; i < 60; i++) {
        buf[2 + i * 3] = r;
        buf[3 + i * 3] = g;
        buf[4 + i * 3] = b;
    }

    if (write(fd, buf, sizeof(buf)) < 0) {
        fprintf(stderr, "Error: Failed to set color: %s\n", strerror(errno));
        return -1;
    }

    printf("Set channel %d to RGB(%d,%d,%d)\n", channel, r, g, b);
    return 0;
}

int set_led(int fd, int channel, int led, int r, int g, int b) {
    if (channel < 0 || channel > 1) {
        fprintf(stderr, "Error: Channel must be 0 or 1\n");
        return -1;
    }
    if (led < 0 || led >= 60) {
        fprintf(stderr, "Error: LED index must be 0-59\n");
        return -1;
    }
    if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
        fprintf(stderr, "Error: RGB values must be 0-255\n");
        return -1;
    }

    unsigned char buf[2 + 60 * 3];
    buf[0] = channel;
    buf[1] = 60;  // num_leds

    // Set all LEDs to off first
    memset(&buf[2], 0, 60 * 3);

    // Set the specific LED
    buf[2 + led * 3] = r;
    buf[3 + led * 3] = g;
    buf[4 + led * 3] = b;

    if (write(fd, buf, sizeof(buf)) < 0) {
        fprintf(stderr, "Error: Failed to set LED: %s\n", strerror(errno));
        return -1;
    }

    printf("Set channel %d LED %d to RGB(%d,%d,%d)\n", channel, led, r, g, b);
    return 0;
}

int set_brightness(int fd, int brightness) {
    if (brightness < 0 || brightness > 100) {
        fprintf(stderr, "Error: Brightness must be 0-100\n");
        return -1;
    }

    if (ioctl(fd, CORSAIR_IOC_SET_BRIGHTNESS, &brightness) < 0) {
        fprintf(stderr, "Error: Failed to set brightness: %s\n", strerror(errno));
        return -1;
    }

    printf("Set brightness to %d%%\n", brightness);
    return 0;
}

int set_effect(int fd, int argc, char *argv[]) {
    if (argc < 14) {
        fprintf(stderr, "Error: Not enough arguments for set-effect\n");
        return -1;
    }

    struct effect_config cfg;
    cfg.channel = atoi(argv[2]);
    cfg.num_leds = atoi(argv[3]);
    cfg.mode = atoi(argv[4]);
    cfg.speed = atoi(argv[5]);
    cfg.direction = atoi(argv[6]);
    cfg.random = atoi(argv[7]);
    cfg.red1 = atoi(argv[8]);
    cfg.grn1 = atoi(argv[9]);
    cfg.blu1 = atoi(argv[10]);
    cfg.red2 = atoi(argv[11]);
    cfg.grn2 = atoi(argv[12]);
    cfg.blu2 = atoi(argv[13]);
    cfg.red3 = atoi(argv[14]);
    cfg.grn3 = atoi(argv[15]);
    cfg.blu3 = atoi(argv[16]);

    if (cfg.channel < 0 || cfg.channel > 1) {
        fprintf(stderr, "Error: Channel must be 0 or 1\n");
        return -1;
    }

    if (ioctl(fd, CORSAIR_IOC_SET_EFFECT, &cfg) < 0) {
        fprintf(stderr, "Error: Failed to set effect: %s\n", strerror(errno));
        return -1;
    }

    printf("Set effect on channel %d\n", cfg.channel);
    return 0;
}

int get_firmware(int fd) {
    char firmware[16];

    if (ioctl(fd, CORSAIR_IOC_GET_FIRMWARE, firmware) < 0) {
        fprintf(stderr, "Error: Failed to get firmware: %s\n", strerror(errno));
        return -1;
    }

    printf("Firmware version: %s\n", firmware);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    int fd = open_device();
    if (fd < 0) {
        return 1;
    }

    int result = 0;

    if (strcmp(argv[1], "set-color") == 0) {
        if (argc != 6) {
            fprintf(stderr, "Error: set-color requires 3 RGB values\n");
            result = 1;
        } else {
            result = set_color(fd, atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), atoi(argv[5]));
        }
    } else if (strcmp(argv[1], "set-led") == 0) {
        if (argc != 7) {
            fprintf(stderr, "Error: set-led requires channel, LED index, and 3 RGB values\n");
            result = 1;
        } else {
            result = set_led(fd, atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), atoi(argv[5]), atoi(argv[6]));
        }
    } else if (strcmp(argv[1], "set-brightness") == 0) {
        if (argc != 3) {
            fprintf(stderr, "Error: set-brightness requires brightness value\n");
            result = 1;
        } else {
            result = set_brightness(fd, atoi(argv[2]));
        }
    } else if (strcmp(argv[1], "set-effect") == 0) {
        result = set_effect(fd, argc, argv);
    } else if (strcmp(argv[1], "get-firmware") == 0) {
        result = get_firmware(fd);
    } else if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        usage(argv[0]);
    } else {
        fprintf(stderr, "Error: Unknown command '%s'\n", argv[1]);
        usage(argv[0]);
        result = 1;
    }

    close(fd);
    return result;
}