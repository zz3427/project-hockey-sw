#ifndef GAME_IO_H
#define GAME_IO_H

/*
 * app/game_io.h
 *
 * Userspace wrapper around the kernel driver /dev/air_hockey.
 *
 * This is NOT the kernel driver.
 * This is a small helper layer used by app/main.c and tests so they do not
 * have to call ioctl() directly everywhere.
 *
 * Responsibilities:
 *   - open / close the device
 *   - read status register
 *   - wait for VSYNC
 *   - send logical puck / paddle / score / sound updates
 *
 * Non-responsibilities:
 *   - no physics
 *   - no mouse decoding
 *   - no game rules
 *   - no hardware register packing details
 */

#include "../driver/air_hockey.h"

/* Open /dev/air_hockey (or another device path if needed). Returns 0 on success. */
int game_io_open(const char *device_path);

/* Close the device if open. Safe to call more than once. */
void game_io_close(void);

/* Read raw STATUS register value through the kernel driver. */
unsigned int game_io_read_status(void);

/* Busy-wait until VSYNC_READY is asserted. */
void game_io_wait_for_vsync(void);

/* Write one logical paddle/puck position. */
void game_io_set_p1(unsigned short x, unsigned short y);
void game_io_set_p2(unsigned short x, unsigned short y);
void game_io_set_puck(unsigned short x, unsigned short y);

/* Write score register. */
void game_io_set_score(unsigned char p1, unsigned char p2);

/* Write one sound event code. */
void game_io_set_sound(unsigned char event_id);

/*
 * Push all visible game objects for one frame.
 *
 * This is just a convenience helper so main.c can update all visible state
 * together in one place.
 */
typedef struct {
    unsigned short p1_x, p1_y;
    unsigned short p2_x, p2_y;
    unsigned short puck_x, puck_y;
    unsigned char score_p1, score_p2;
    unsigned char sound_event;
} game_io_frame_t;

void game_io_submit_frame(const game_io_frame_t *frame);

#endif