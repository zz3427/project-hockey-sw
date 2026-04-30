/*
 * air_hockey.c
 *
 * Kernel driver for the FPGA air hockey peripheral.
 * 
 * Author: Gerald Zhao  Apr-30-2026
 *
 * Role of this file:
 *   - bind to the FPGA peripheral through the device tree
 *   - map the peripheral's memory-mapped register region
 *   - expose a simple device file: /dev/air_hockey
 *   - receive ioctl requests from userspace
 *   - translate logical userspace data into packed 32-bit hardware register
 *     writes
 *
 * This driver hides:
 *   - the peripheral's physical base address
 *   - the raw register packing details
 *      so the userspace program can be neater
 *
 * Userspace only needs to know:
 *   - /dev/air_hockey
 *   - ioctl command numbers
 *   - shared structs from air_hockey.h
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/uaccess.h>

#include "air_hockey.h"

/*
 * The device name is used in two places:
 *   1. misc device registration -> creates /dev/air_hockey
 *   2. kernel log messages
 */
#define DRIVER_NAME "air_hockey"

/*
 * Register offsets relative to the peripheral base address.
 * THIS MUST MATCH HARDWARE SIDE
 *
 * Hardware register map:
 *   0x00 STATUS
 *   0x04 SOUND_CONTROL
 *   0x08 P1_POS
 *   0x0C P2_POS
 *   0x10 PUCK_POS
 *   0x14 SCORE
 */
#define REG_STATUS         0x00
#define REG_SOUND_CONTROL  0x04
#define REG_P1_POS         0x08
#define REG_P2_POS         0x0C
#define REG_PUCK_POS       0x10
#define REG_SCORE          0x14

/*
 * Helper macro: convert register offset into mapped I/O address.
 *
 * dev.virtbase is the kernel's virtual mapping of the device register region.
 * Adding an offset gives the location of one specific register.
 *
 * dev.virtbase is memory-mapped I/O space, so access must go through iowrite32/ioread32.
 */
#define REG32_ADDR(base, off) ((base) + (off))

/*
 * Pack logical position from user program into one 32-bit hardware register.
 * note: this is used for both puck and paddle positions, they share the same format
 * Register format:
 *   bits [15:0]  = x
 *   bits [31:16] = y
 */
static inline uint32_t pack_pos(const air_hockey_pos_t *p)
{
    return (((uint32_t)p->y) << 16) | ((uint32_t)p->x);
}

/*
 * Pack logical scores into the SCORE register.
 *
 * SCORE register:
 *   bits [2:0] = player 1 score
 *   bits [5:3] = player 2 score
 *   bits [31:6] = 0
 */
static inline uint32_t pack_score(const air_hockey_score_t *s)
{
    return (((uint32_t)(s->p2 & 0x7)) << 3) |
           ((uint32_t)(s->p1 & 0x7));
}

/*
 * Pack one sound event into the SOUND_CONTROL register.
 *
 * SOUND_CONTROL register:
 *   bits [2:0] = sound event ID
 *   bits [31:3] = 0
 */
static inline uint32_t pack_sound(uint8_t sound_event)
{
    return ((uint32_t)(sound_event & 0x7));
}

/*
 * Driver-private state - only driver can see this instance
 * This stores both:
 *   1. the physical resource description (dev.res)
 *   2. the mapped I/O base pointer used for actual register access
 */
struct air_hockey_dev {
    struct resource res; // stores the physical addr of the registers, for ownership, allocation, release
    void __iomem *virtbase; // used for actual iowrite32 and ioread32 register access
    // this maps the physical addr to kernel virtual memory
} dev; // this instantiate a shared file scope object 

/*
 * Write helpers: write position registers.
 */
static void write_p1_pos(const air_hockey_pos_t *p)
{
    iowrite32(pack_pos(p), REG32_ADDR(dev.virtbase, REG_P1_POS));
}

static void write_p2_pos(const air_hockey_pos_t *p)
{
    iowrite32(pack_pos(p), REG32_ADDR(dev.virtbase, REG_P2_POS));
}

static void write_puck_pos(const air_hockey_pos_t *p)
{
    iowrite32(pack_pos(p), REG32_ADDR(dev.virtbase, REG_PUCK_POS));
}

static void write_score(const air_hockey_score_t *s)
{
    iowrite32(pack_score(s), REG32_ADDR(dev.virtbase, REG_SCORE));
}

static void write_sound(uint8_t sound_event)
{
    iowrite32(pack_sound(sound_event), REG32_ADDR(dev.virtbase, REG_SOUND_CONTROL));
}

/*
 * Read helper: read STATUS register.
 * This is exposed through AIR_HOCKEY_READ_STATUS so userspace can poll
 * VSYNC_READY and synchronize updates to vertical blank.
 */
static uint32_t read_status(void)
{
    return ioread32(REG32_ADDR(dev.virtbase, REG_STATUS));
}

/*
 * ioctl handler
 *   1. userspace calls ioctl(fd, cmd, arg)
 *   2. Linux dispatches here through .unlocked_ioctl
 */
static long air_hockey_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
    air_hockey_pos_arg_t pos_arg;
    air_hockey_score_arg_t score_arg;
    air_hockey_sound_arg_t sound_arg;
    air_hockey_status_t status_arg;

    switch (cmd) {

    case AIR_HOCKEY_WRITE_P1_POS:
        /*
         * Copy logical position from userspace into a kernel-local struct.
         * copy_from_user is in uaccess.h and returns 0 on success, nonzero on failure.
         */
        if (copy_from_user(&pos_arg,
                           (air_hockey_pos_arg_t *)arg,
                           sizeof(air_hockey_pos_arg_t)))
            return -EACCES; // failure and return immediately

        write_p1_pos(&pos_arg.pos);
        break;

    case AIR_HOCKEY_WRITE_P2_POS:
        if (copy_from_user(&pos_arg,
                           (air_hockey_pos_arg_t *)arg,
                           sizeof(air_hockey_pos_arg_t)))
            return -EACCES;

        write_p2_pos(&pos_arg.pos);
        break;

    case AIR_HOCKEY_WRITE_PUCK_POS:
        if (copy_from_user(&pos_arg,
                           (air_hockey_pos_arg_t *)arg,
                           sizeof(air_hockey_pos_arg_t)))
            return -EACCES;

        write_puck_pos(&pos_arg.pos);
        break;

    case AIR_HOCKEY_WRITE_SCORE:
        if (copy_from_user(&score_arg,
                           (air_hockey_score_arg_t *)arg,
                           sizeof(air_hockey_score_arg_t)))
            return -EACCES;

        write_score(&score_arg.score);
        break;

    case AIR_HOCKEY_WRITE_SOUND:
        if (copy_from_user(&sound_arg,
                           (air_hockey_sound_arg_t *)arg,
                           sizeof(air_hockey_sound_arg_t)))
            return -EACCES;

        write_sound(sound_arg.sound_event);
        break;

    case AIR_HOCKEY_READ_STATUS:
        /*
         * reads hardware register
         * fills a kernel-local result struct
         * copies that result back to userspace
         */
        status_arg.status = read_status();

        if (copy_to_user((air_hockey_status_t *)arg,
                         &status_arg,
                         sizeof(air_hockey_status_t)))
            return -EACCES;
        break;

    default:
        /*
         * if ioctl command is not valid
         */
        return -EINVAL;
    }
    return 0;
}

/*
 * File operations table for /dev/air_hockey
 * This binds the air_hockey_ioctl to linux when userspace calls ioctl()
 */
static const struct file_operations air_hockey_fops = {
    .owner          = THIS_MODULE,
    .unlocked_ioctl = air_hockey_ioctl,
};

/*
 * misc device descriptor
 *  This register the driver as a misc device to linux
 * This is the object that tells Linux:
 *   - create a misc char device
 *   - use this device name
 *   - use this file_operations table
 *
 * That essentially creates /dev/air_hockey gets
 */
static struct miscdevice air_hockey_misc_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = DRIVER_NAME,
    .fops  = &air_hockey_fops,
};

/*
 * Probe callback
 *
 * Called by Linux when the driver is matched to a platform device node
 * from the device tree.
 *
 *   1. create /dev/air_hockey
 *   2. fetch the hardware resource address range from the device tree
 *   3. reserve the region
 *   4. map it into kernel virtual address space
 *
 * After this succeeds, the driver can perform register reads/writes.
 */
static int __init air_hockey_probe(struct platform_device *pdev)
{
    int ret;

    /*
     * Register misc device first so userspace can open /dev/air_hockey
     * once probe succeeds.
     */
    ret = misc_register(&air_hockey_misc_device);
    if (ret)
        return ret;

    /*
     * Fetch the physical resource range from the device tree node
     * corresponding to this peripheral.
     */
    ret = of_address_to_resource(pdev->dev.of_node, 0, &dev.res);
    if (ret) {
        ret = -ENOENT;
        goto out_deregister; // if failed, return immediatly and redo previous step
    }

    /*
     * Reserve ownership of the physical I/O memory region.
     */
    if (request_mem_region(dev.res.start,
                           resource_size(&dev.res),
                           DRIVER_NAME) == NULL) {
        ret = -EBUSY;
        goto out_deregister; // redo previous steps
    }

    /*
     * Map the device's register space into kernel virtual memory so
     * iowrite32/ioread32 can use it.
     */
    dev.virtbase = of_iomap(pdev->dev.of_node, 0);
    if (dev.virtbase == NULL) {
        ret = -ENOMEM;
        goto out_release_mem_region; // redo previous steps
    }

    /*
     * Initialization writes.
     *
     * Defaults so the display starts in a known state.
     */
    {
        air_hockey_pos_t p1   = { .x = 100, .y = 240 };
        air_hockey_pos_t p2   = { .x = 540, .y = 240 };
        air_hockey_pos_t puck = { .x = 320, .y = 240 };
        air_hockey_score_t score = { .p1 = 0, .p2 = 0 };

        write_p1_pos(&p1);
        write_p2_pos(&p2);
        write_puck_pos(&puck);
        write_score(&score);
        write_sound(AIR_HOCKEY_SOUND_NONE);
    }

    return 0;

out_release_mem_region:
    release_mem_region(dev.res.start, resource_size(&dev.res));
out_deregister:
    misc_deregister(&air_hockey_misc_device);
    return ret;
}

/*
 * Remove callback
 *
 * Undo what probe() did.
 */
static int air_hockey_remove(struct platform_device *pdev)
{
    iounmap(dev.virtbase);
    release_mem_region(dev.res.start, resource_size(&dev.res));
    misc_deregister(&air_hockey_misc_device);
    return 0;
}

/*
 * Device tree match table.
 *
 * TODO: IMPORTANT:
 * The compatible string here must match the one generated by hardware
 * side (through the DT metadata in vga_ball_hw.tcl or its renamed equivalent).
 *
 * If the strings do not match, Linux will not bind this driver to the device.
 *
 * Replace this string if hardware side changed name.
 */

//  Which "compatible" string(s) to search for in the Device Tree
#ifdef CONFIG_OF
static const struct of_device_id air_hockey_of_match[] = {
    { .compatible = "csee4840,vga_ball-1.0" },
    {},
};
MODULE_DEVICE_TABLE(of, air_hockey_of_match);
#endif

/*
 * Platform driver object
 */
static struct platform_driver air_hockey_driver = {
    .driver = {
        .name = DRIVER_NAME,
        .owner = THIS_MODULE,
        .of_match_table = of_match_ptr(air_hockey_of_match),
    },
    .remove = __exit_p(air_hockey_remove),
};

/*
 * Called when the module is loaded.
 *
 * This does NOT directly initialize hardware registers itself.
 * It registers the platform driver and lets Linux call the
 * probe callback when a matching device tree node is found.
 */
static int __init air_hockey_init(void)
{
    pr_info(DRIVER_NAME ": init\n");
    return platform_driver_probe(&air_hockey_driver, air_hockey_probe);
    /* passing in function name as argument let's it decays into a 
     * pointer to the function
     */ 
}

/*
 * Called when the module is unloaded.
 */
static void __exit air_hockey_exit(void)
{
    platform_driver_unregister(&air_hockey_driver);
    pr_info(DRIVER_NAME ": exit\n");
}

module_init(air_hockey_init);
module_exit(air_hockey_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Gerald Zhao / adapted from CSEE 4840 lab skeleton");
MODULE_DESCRIPTION("FPGA air hockey driver");