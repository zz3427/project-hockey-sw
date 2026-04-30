#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/input.h>

#include "../driver/air_hockey.h"

/* Area bounds (wall + puck radius = 10+4+10 = 24) */
#define X_MIN 24
#define X_MAX 615
#define Y_MIN 24
#define Y_MAX 455

/*
 * Global file descriptor for the device.
 *
 * This is the handle returned by open("/dev/air_hockey", ...).
 * It is how userspace refers to the driver after opening the device.
 */
static int air_hockey_fd;

/*
 * Helper: read STATUS register through the driver.
 *
 * Returns the raw status value on success.
 * Exits on failure because this is a simple test utility, not production code.
 */
static uint32_t read_status(void)
{
    air_hockey_status_t st;

    if (ioctl(air_hockey_fd, AIR_HOCKEY_READ_STATUS, &st)) {
        perror("ioctl(AIR_HOCKEY_READ_STATUS) failed");
        exit(1);
    }

    return st.status;
}

static int clamp_mouse(int val, int lo, int hi)
{
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}

/*
 * Helper: wait until VSYNC_READY is asserted.
 *
 * Why do this?
 *   The design intent is for software to update visible game-state registers
 *   during vertical blanking, which reduces the chance of visible tearing.
 *
 * For an MVP, polling is simple and good enough.
 */
static void wait_for_vsync_ready(void)
{
    while ((read_status() & AIR_HOCKEY_STATUS_VSYNC_READY) == 0) {
        /* busy-wait */
    }
}

/*
 * Helper: write one logical position to a specific ioctl command.
 *
 * This keeps the repeated userspace pattern simple:
 *   fill wrapper struct
 *   call ioctl
 *   check error
 */
static void write_pos(unsigned long cmd, uint16_t x, uint16_t y)
{
    air_hockey_pos_arg_t arg;

    arg.pos.x = x;
    arg.pos.y = y;

    if (ioctl(air_hockey_fd, cmd, &arg)) {
        perror("position ioctl failed");
        exit(1);
    }
}

/*
 * Helper: write score values.
 */
static void write_score(uint8_t p1, uint8_t p2)
{
    air_hockey_score_arg_t arg;

    arg.score.p1 = p1;
    arg.score.p2 = p2;

    if (ioctl(air_hockey_fd, AIR_HOCKEY_WRITE_SCORE, &arg)) {
        perror("ioctl(AIR_HOCKEY_WRITE_SCORE) failed");
        exit(1);
    }
}

/*
 * Helper: write sound event.
 */
static void write_sound(uint8_t event_id)
{
    air_hockey_sound_arg_t arg;

    arg.sound_event = event_id;

    if (ioctl(air_hockey_fd, AIR_HOCKEY_WRITE_SOUND, &arg)) {
        perror("ioctl(AIR_HOCKEY_WRITE_SOUND) failed");
        exit(1);
    }
}

/*
 * Convenience wrappers so call sites are easier to read.
 */
static void set_p1(uint16_t x, uint16_t y)
{
    write_pos(AIR_HOCKEY_WRITE_P1_POS, x, y);
}

static void set_p2(uint16_t x, uint16_t y)
{
    write_pos(AIR_HOCKEY_WRITE_P2_POS, x, y);
}

static void set_puck(uint16_t x, uint16_t y)
{
    write_pos(AIR_HOCKEY_WRITE_PUCK_POS, x, y);
}


int main(int argc, char *argv[])
{
    static const char filename[] = "/dev/air_hockey";

    const char *dev_mouse = (argc > 1) ? argv[1] : "/dev/input/event0";


    printf("air_hockey test_positions starting\n");

    /*
     * Open the driver-exposed device file.
     *
     * This is not a normal data file.
     * It is the userspace handle to the kernel driver.
     */
    air_hockey_fd = open(filename, O_RDWR);
    if (air_hockey_fd == -1) {
        perror("open(/dev/air_hockey) failed");
        return 1;
    }

    /*
     * Initialize a visible starting scene.
     */
    wait_for_vsync_ready();
    set_p1(100, 240);
    set_p2(540, 240);
    set_puck(320, 240);
    write_score(0, 0);
    write_sound(AIR_HOCKEY_SOUND_NONE);

    sleep(1);
    
    struct input_event ev;
    int mouse_fd;
    int x = 320, y = 240;

    mouse_fd = open(dev_mouse, O_RDONLY);
    if (mouse_fd == -1) {
        fprintf(stderr, "could not open %s\n", dev_mouse);
        return -1;
    }

    printf("Air Hockey started — move mouse to control puck\n");
    printf("Using mouse device: %s\n", dev_mouse);
    set_p1(x, y);


    while (read(mouse_fd, &ev, sizeof(ev)) == sizeof(ev)) {
        if (ev.type == EV_REL) {
            if (ev.code == REL_X)
                x = clamp_mouse(x + ev.value, X_MIN, X_MAX);
            else if (ev.code == REL_Y)
                y = clamp_mouse(y + ev.value, Y_MIN, Y_MAX);
            set_p1(x, y);
        }
    }


    close(mouse_fd);
    close(air_hockey_fd);
    return 0;
}

