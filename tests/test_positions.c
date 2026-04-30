#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "air_hockey.h"

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

int main(void)
{
    static const char filename[] = "/dev/air_hockey";

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

    /*
     * Move puck to a few fixed points so we can confirm the entire path:
     * userspace -> ioctl -> driver -> hardware register -> renderer.
     */
    wait_for_vsync_ready();
    set_puck(160, 120);
    sleep(1);

    wait_for_vsync_ready();
    set_puck(480, 120);
    sleep(1);

    wait_for_vsync_ready();
    set_puck(480, 360);
    sleep(1);

    wait_for_vsync_ready();
    set_puck(160, 360);
    sleep(1);

    wait_for_vsync_ready();
    set_puck(320, 240);
    sleep(1);

    /*
     * Move paddles too, so you confirm each independent register path.
     */
    wait_for_vsync_ready();
    set_p1(140, 200);
    set_p2(500, 280);
    sleep(1);

    wait_for_vsync_ready();
    set_p1(140, 280);
    set_p2(500, 200);
    sleep(1);

    /*
     * Test score and sound path once.
     */
    wait_for_vsync_ready();
    write_score(1, 2);
    write_sound(AIR_HOCKEY_SOUND_GOAL);
    sleep(1);

    close(air_hockey_fd);
    printf("air_hockey test_positions done\n");

    return 0;
}