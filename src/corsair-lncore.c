#include <linux/module.h>
#include <linux/usb.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>  // For copy_to/from_user if needed
#include <linux/ioctl.h>
#include <linux/mutex.h>

#define VENDOR_ID 0x1b1c
#define PRODUCT_ID 0x0c1a
#define MODULE_NAME "corsair-lncore"

#define CORSAIR_LIGHTING_NODE_WRITE_PACKET_SIZE 65
#define CORSAIR_LIGHTING_NODE_READ_PACKET_SIZE 17
#define CORSAIR_LIGHTING_NODE_READ_TIMEOUT 15

enum {
    CORSAIR_LIGHTING_NODE_PACKET_ID_FIRMWARE = 0x02,
    CORSAIR_LIGHTING_NODE_PACKET_ID_DIRECT = 0x32,
    CORSAIR_LIGHTING_NODE_PACKET_ID_COMMIT = 0x33,
    CORSAIR_LIGHTING_NODE_PACKET_ID_BEGIN = 0x34,
    CORSAIR_LIGHTING_NODE_PACKET_ID_EFFECT_CONFIG = 0x35,
    CORSAIR_LIGHTING_NODE_PACKET_ID_TEMPERATURE = 0x36,
    CORSAIR_LIGHTING_NODE_PACKET_ID_RESET = 0x37,
    CORSAIR_LIGHTING_NODE_PACKET_ID_PORT_STATE = 0x38,
    CORSAIR_LIGHTING_NODE_PACKET_ID_BRIGHTNESS = 0x39,
    CORSAIR_LIGHTING_NODE_PACKET_ID_LED_COUNT = 0x3A,
    CORSAIR_LIGHTING_NODE_PACKET_ID_PROTOCOL = 0x3B,
};

enum {
    CORSAIR_LIGHTING_NODE_DIRECT_CHANNEL_RED = 0x00,
    CORSAIR_LIGHTING_NODE_DIRECT_CHANNEL_GREEN = 0x01,
    CORSAIR_LIGHTING_NODE_DIRECT_CHANNEL_BLUE = 0x02,
};

enum {
    CORSAIR_LIGHTING_NODE_PORT_STATE_HARDWARE = 0x01,
    CORSAIR_LIGHTING_NODE_PORT_STATE_SOFTWARE = 0x02,
};

enum {
    CORSAIR_LIGHTING_NODE_CHANNEL_1 = 0x00,
    CORSAIR_LIGHTING_NODE_CHANNEL_2 = 0x01,
    CORSAIR_LIGHTING_NODE_NUM_CHANNELS = 0x02,
};

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

static struct usb_device_id id_table[] = {
    { USB_DEVICE(VENDOR_ID, PRODUCT_ID) },
    { }
};
MODULE_DEVICE_TABLE(usb, id_table);

static dev_t dev_num;
static struct cdev cdev;
static struct class *cl;  // For /dev node
static struct usb_interface *interface;
static char firmware_version[16] = {0};
static DEFINE_MUTEX(lncore_mutex);  // For thread-safe access

// Helper function to send packet
static int send_packet(struct usb_device *dev, unsigned char *buf, size_t len) {
    return usb_control_msg(dev, usb_sndctrlpipe(dev, 0), 0x09, 0x21, 0x0200, 0, buf, len, 5000);
}

// Send Direct packet
static void send_direct(struct usb_device *dev, unsigned char channel, unsigned char start, unsigned char count, unsigned char color_channel, unsigned char *color_data) {
    unsigned char buf[CORSAIR_LIGHTING_NODE_WRITE_PACKET_SIZE] = {0};
    buf[0] = 0x00; // report ID
    buf[1] = CORSAIR_LIGHTING_NODE_PACKET_ID_DIRECT;
    buf[2] = channel;
    buf[3] = start;
    buf[4] = count;
    buf[5] = color_channel;
    memcpy(&buf[6], color_data, count);
    send_packet(dev, buf, sizeof(buf));
}

// Send Commit packet
static void send_commit(struct usb_device *dev) {
    unsigned char buf[CORSAIR_LIGHTING_NODE_WRITE_PACKET_SIZE] = {0};
    buf[0] = 0x00;
    buf[1] = CORSAIR_LIGHTING_NODE_PACKET_ID_COMMIT;
    buf[2] = 0xFF;
    send_packet(dev, buf, sizeof(buf));
}

// Send Port State packet
static void send_port_state(struct usb_device *dev, unsigned char channel, unsigned char state) {
    unsigned char buf[CORSAIR_LIGHTING_NODE_WRITE_PACKET_SIZE] = {0};
    buf[0] = 0x00;
    buf[1] = CORSAIR_LIGHTING_NODE_PACKET_ID_PORT_STATE;
    buf[2] = channel;
    buf[3] = state;
    send_packet(dev, buf, sizeof(buf));
}

// Send Brightness packet
static void send_brightness(struct usb_device *dev, unsigned char channel, unsigned char brightness) {
    unsigned char buf[CORSAIR_LIGHTING_NODE_WRITE_PACKET_SIZE] = {0};
    buf[0] = 0x00;
    buf[1] = CORSAIR_LIGHTING_NODE_PACKET_ID_BRIGHTNESS;
    buf[2] = channel;
    buf[3] = brightness;
    send_packet(dev, buf, sizeof(buf));
}

// Send Begin packet
static void send_begin(struct usb_device *dev, unsigned char channel) {
    unsigned char buf[CORSAIR_LIGHTING_NODE_WRITE_PACKET_SIZE] = {0};
    buf[0] = 0x00;
    buf[1] = CORSAIR_LIGHTING_NODE_PACKET_ID_BEGIN;
    buf[2] = channel;
    send_packet(dev, buf, sizeof(buf));
}

// Send Reset packet
static void send_reset(struct usb_device *dev, unsigned char channel) {
    unsigned char buf[CORSAIR_LIGHTING_NODE_WRITE_PACKET_SIZE] = {0};
    buf[0] = 0x00;
    buf[1] = CORSAIR_LIGHTING_NODE_PACKET_ID_RESET;
    buf[2] = channel;
    send_packet(dev, buf, sizeof(buf));
}

// Send Effect Config packet
static void send_effect_config(struct usb_device *dev, struct effect_config *cfg) {
    unsigned char buf[CORSAIR_LIGHTING_NODE_WRITE_PACKET_SIZE] = {0};
    buf[0] = 0x00;
    buf[1] = CORSAIR_LIGHTING_NODE_PACKET_ID_EFFECT_CONFIG;
    buf[2] = cfg->channel;
    buf[3] = cfg->num_leds;
    buf[4] = 0; // led_type, assume 0
    buf[5] = cfg->mode;
    buf[6] = cfg->speed;
    buf[7] = cfg->direction;
    buf[8] = cfg->random;
    buf[9] = 0; // padding?
    buf[10] = cfg->red1;
    buf[11] = cfg->grn1;
    buf[12] = cfg->blu1;
    buf[13] = cfg->red2;
    buf[14] = cfg->grn2;
    buf[15] = cfg->blu2;
    buf[16] = cfg->red3;
    buf[17] = cfg->grn3;
    buf[18] = cfg->blu3;
    // temperatures 0
    send_packet(dev, buf, sizeof(buf));
}

// Send Firmware Request
static void send_firmware_request(struct usb_device *dev) {
    unsigned char buf[CORSAIR_LIGHTING_NODE_WRITE_PACKET_SIZE] = {0};
    buf[0] = 0x00;
    buf[1] = CORSAIR_LIGHTING_NODE_PACKET_ID_FIRMWARE;
    send_packet(dev, buf, sizeof(buf));
    // Read response
    unsigned char read_buf[CORSAIR_LIGHTING_NODE_READ_PACKET_SIZE];
    usb_control_msg(dev, usb_rcvctrlpipe(dev, 0), 0x01, 0xA1, 0x0100, 0, read_buf, sizeof(read_buf), 5000);
    if (read_buf[0] == 0x00 && read_buf[1] == CORSAIR_LIGHTING_NODE_PACKET_ID_FIRMWARE) {
        sprintf(firmware_version, "%d.%d.%d", read_buf[2], read_buf[3], read_buf[4]);
    }
}

// File operations (expand with full protocol)
static int lncore_open(struct inode *inode, struct file *file) { return 0; }
static int lncore_release(struct inode *inode, struct file *file) { return 0; }
static ssize_t lncore_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos) {
    char *kbuf;
    unsigned char *red_data, *grn_data, *blu_data;
    ssize_t ret = -ENOMEM;

    if (count < 2 || count > 1024) return -EINVAL;

    kbuf = kmalloc(count, GFP_KERNEL);
    if (!kbuf) return -ENOMEM;

    if (copy_from_user(kbuf, buf, count)) {
        ret = -EFAULT;
        goto out;
    }

    unsigned char channel = kbuf[0];
    unsigned char num_leds = kbuf[1];

    // Validate channel and LED count
    if (channel >= CORSAIR_LIGHTING_NODE_NUM_CHANNELS) {
        ret = -EINVAL;
        goto out;
    }
    if (num_leds == 0 || num_leds > 204) {  // Reasonable limit for LED count
        ret = -EINVAL;
        goto out;
    }

    // Check for integer overflow in size calculation
    if (num_leds > (SIZE_MAX - 2) / 3) {
        ret = -EINVAL;
        goto out;
    }

    if (count != 2 + 3 * num_leds) {
        ret = -EINVAL;
        goto out;
    }

    red_data = kmalloc(50, GFP_KERNEL);
    grn_data = kmalloc(50, GFP_KERNEL);
    blu_data = kmalloc(50, GFP_KERNEL);
    if (!red_data || !grn_data || !blu_data) goto out_arrays;

    struct usb_device *dev = interface_to_usbdev(interface);

    // Set port state to software
    send_port_state(dev, channel, CORSAIR_LIGHTING_NODE_PORT_STATE_SOFTWARE);

    unsigned int colors_remaining = num_leds;
    unsigned int pkt_offset = 0;

    while (colors_remaining > 0) {
        unsigned char pkt_size = (colors_remaining < 50) ? colors_remaining : 50;
        for (int i = 0; i < pkt_size; i++) {
            int idx = pkt_offset + i;
            red_data[i] = kbuf[2 + 3*idx];
            grn_data[i] = kbuf[3 + 3*idx];
            blu_data[i] = kbuf[4 + 3*idx];
        }

        // Send direct for each color channel
        send_direct(dev, channel, pkt_offset, pkt_size, CORSAIR_LIGHTING_NODE_DIRECT_CHANNEL_RED, red_data);
        send_direct(dev, channel, pkt_offset, pkt_size, CORSAIR_LIGHTING_NODE_DIRECT_CHANNEL_GREEN, grn_data);
        send_direct(dev, channel, pkt_offset, pkt_size, CORSAIR_LIGHTING_NODE_DIRECT_CHANNEL_BLUE, blu_data);

        colors_remaining -= pkt_size;
        pkt_offset += pkt_size;
    }

    // Commit
    send_commit(dev);

    ret = count;

out_arrays:
    kfree(blu_data);
    kfree(grn_data);
    kfree(red_data);
out:
    kfree(kbuf);
    return ret;
}
// Add read/ioctl as needed for queries/commit

static long lncore_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    struct usb_device *dev;
    long ret = 0;

    if (mutex_lock_interruptible(&lncore_mutex))
        return -ERESTARTSYS;

    dev = interface_to_usbdev(interface);
    switch (cmd) {
    case CORSAIR_IOC_SET_BRIGHTNESS: {
        unsigned char brightness;
        if (copy_from_user(&brightness, (void __user *)arg, sizeof(brightness))) {
            ret = -EFAULT;
            goto out;
        }
        // Validate brightness range (0-100)
        if (brightness > 100) {
            ret = -EINVAL;
            goto out;
        }
        // Assume channel 0 for now, or add channel to ioctl
        send_brightness(dev, 0, brightness);
        send_commit(dev);
        break;
    }
    case CORSAIR_IOC_SET_EFFECT: {
        struct effect_config cfg;
        if (copy_from_user(&cfg, (void __user *)arg, sizeof(cfg))) {
            ret = -EFAULT;
            goto out;
        }
        // Validate effect config parameters
        if (cfg.channel >= CORSAIR_LIGHTING_NODE_NUM_CHANNELS) {
            ret = -EINVAL;
            goto out;
        }
        if (cfg.num_leds == 0 || cfg.num_leds > 204) {
            ret = -EINVAL;
            goto out;
        }
        if (cfg.speed > 4 || cfg.direction > 1 || cfg.random > 1) {
            ret = -EINVAL;
            goto out;
        }
        send_reset(dev, cfg.channel);
        send_begin(dev, cfg.channel);
        send_port_state(dev, cfg.channel, CORSAIR_LIGHTING_NODE_PORT_STATE_HARDWARE);
        send_effect_config(dev, &cfg);
        send_commit(dev);
        break;
    }
    case CORSAIR_IOC_GET_FIRMWARE: {
        if (copy_to_user((void __user *)arg, firmware_version, sizeof(firmware_version))) {
            ret = -EFAULT;
            goto out;
        }
        break;
    }
    default:
        ret = -ENOTTY;
        goto out;
    }

out:
    mutex_unlock(&lncore_mutex);
    return ret;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = lncore_open,
    .release = lncore_release,
    .write = lncore_write,
    .unlocked_ioctl = lncore_ioctl,
};

static int lncore_probe(struct usb_interface *intf, const struct usb_device_id *id) {
    interface = intf;
    if (alloc_chrdev_region(&dev_num, 0, 1, MODULE_NAME) < 0) return -1;
    cdev_init(&cdev, &fops);
    if (cdev_add(&cdev, dev_num, 1) < 0) goto err_chrdev;
    cl = class_create(MODULE_NAME);
    device_create(cl, NULL, dev_num, NULL, MODULE_NAME "0");  // Creates /dev/corsair-lncore0
    struct usb_device *dev = interface_to_usbdev(interface);
    send_firmware_request(dev);
    printk(KERN_INFO "%s: Probed, firmware %s\n", MODULE_NAME, firmware_version);
    return 0;

err_chrdev:
    unregister_chrdev_region(dev_num, 1);
    return -1;
}

static void lncore_disconnect(struct usb_interface *intf) {
    device_destroy(cl, dev_num);
    class_destroy(cl);
    cdev_del(&cdev);
    unregister_chrdev_region(dev_num, 1);
    printk(KERN_INFO "%s: Disconnected\n", MODULE_NAME);
}

static struct usb_driver lncore_driver = {
    .name = MODULE_NAME,
    .id_table = id_table,
    .probe = lncore_probe,
    .disconnect = lncore_disconnect,
};

module_usb_driver(lncore_driver);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ross Golder");
MODULE_DESCRIPTION("Corsair Lighting Node Core USB driver");
MODULE_VERSION("0.1.0");
