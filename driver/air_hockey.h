#ifndef AIR_HOCKEY_H
#define AIR_HOCKEY_H

/*
 * air_hockey.h
 *
 * Shared interface between:
 *   1. userspace programs (test app / game logic)
 *   2. kernel driver (air_hockey.c)
 *
 * This file should be included by BOTH sides so they agree on:
 *   - ioctl command numbers
 *   - struct layouts
 *   - semantic meaning of each request
 *
 * Design rule:
 *   Userspace deals in logical game data (x, y, scores, sound event IDs).
 *   The driver is responsible for packing those values into 32-bit hardware
 *   register writes.
 */

#include <linux/ioctl.h>
#include <stdint.h>

/*
 * AIR_HOCKEY_MAGIC
 *
 * ioctl command families need a "magic" identifying character so command
 * numbers do not collide with unrelated drivers.
 *
 * This can be almost any value, but it should stay fixed once chosen.
 */
#define AIR_HOCKEY_MAGIC 'q'

/*
 * Logical 2D position in screen pixel coordinates.
 *
 * Coordinates are measured relative to the top-left corner of the visible
 * display region:
 *   - x increases to the right
 *   - y increases downward
 *
 * These are logical values used by userspace and driver code.
 * The driver packs them into one 32-bit register:
 *
 *   bits [15:0]  = x
 *   bits [31:16] = y
 *
 * We use uint16_t because the hardware register map allocates 16 bits
 * per coordinate.
 */
typedef struct {
    uint16_t x;
    uint16_t y;
} air_hockey_pos_t;

/*
 * Score structure used by userspace.
 *
 * The hardware SCORE register is packed as:
 *   bits [2:0] = player 1 score
 *   bits [5:3] = player 2 score
 *
 * We keep this as a logical struct here so userspace does not need to
 * manually pack bitfields.
 */
typedef struct {
    uint8_t p1;
    uint8_t p2;
} air_hockey_score_t;

/*
 * Status returned by the driver when userspace wants to read hardware state.
 *
 * For the current design, we only expose the raw STATUS register value.
 * Userspace can inspect bit 0 to check VSYNC_READY.
 *
 * Later, if you want, you can expand this struct to also include:
 *   - frame counter
 *   - sound busy
 *   - debug flags
 * without changing the general driver pattern.
 */
typedef struct {
    uint32_t status;
} air_hockey_status_t;

/*
 * Separate argument wrappers for ioctl commands.
 *
 * Why wrap instead of passing air_hockey_pos_t directly?
 * Mostly for consistency and future-proofing:
 *   - easier to expand later
 *   - makes each ioctl's payload semantics clearer
 *   - mirrors the lab3 style where one argument struct was passed
 */
typedef struct {
    air_hockey_pos_t pos;
} air_hockey_pos_arg_t;

typedef struct {
    air_hockey_score_t score;
} air_hockey_score_arg_t;

typedef struct {
    uint8_t sound_event;
} air_hockey_sound_arg_t;

/*
 * Sound event IDs understood by both userspace and driver.
 *
 * These values are what the hardware sound block expects in the
 * SOUND_CONTROL register's low bits.
 *
 * Keep these synchronized with the hardware documentation.
 */
#define AIR_HOCKEY_SOUND_NONE        0
#define AIR_HOCKEY_SOUND_HIT_WALL    1
#define AIR_HOCKEY_SOUND_HIT_PADDLE  2
#define AIR_HOCKEY_SOUND_GOAL        3

/*
 * ioctl command definitions
 *
 * _IOW = userspace writes data into the driver
 * _IOR = userspace reads data back from the driver
 *
 * Each command number should stay stable once userspace and driver are both
 * using it.
 */

/* Write player 1 paddle position to hardware */
#define AIR_HOCKEY_WRITE_P1_POS \
    _IOW(AIR_HOCKEY_MAGIC, 1, air_hockey_pos_arg_t)

/* Write player 2 paddle position to hardware */
#define AIR_HOCKEY_WRITE_P2_POS \
    _IOW(AIR_HOCKEY_MAGIC, 2, air_hockey_pos_arg_t)

/* Write puck position to hardware */
#define AIR_HOCKEY_WRITE_PUCK_POS \
    _IOW(AIR_HOCKEY_MAGIC, 3, air_hockey_pos_arg_t)

/* Write score display values to hardware */
#define AIR_HOCKEY_WRITE_SCORE \
    _IOW(AIR_HOCKEY_MAGIC, 4, air_hockey_score_arg_t)

/* Write one sound event code to hardware */
#define AIR_HOCKEY_WRITE_SOUND \
    _IOW(AIR_HOCKEY_MAGIC, 5, air_hockey_sound_arg_t)

/* Read back hardware STATUS register through the driver */
#define AIR_HOCKEY_READ_STATUS \
    _IOR(AIR_HOCKEY_MAGIC, 6, air_hockey_status_t)

/*
 * STATUS register bit definitions
 *
 * Userspace can use these helpers after reading status.
 */
#define AIR_HOCKEY_STATUS_VSYNC_READY  (1u << 0)

#endif