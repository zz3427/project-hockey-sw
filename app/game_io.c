#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "game_io.h"

/*
 * File descriptor for /dev/air_hockey.
 *
 * Hidden inside this file so the rest of the app does not need to care
 * about device file management.
 */
static int g_air_hockey_fd = -1;

/*
 * Internal helper:
 * send one position ioctl using the shared air_hockey_pos_arg_t wrapper.
 */
static void write_pos(unsigned long cmd, unsigned short x, unsigned short y)
{
    air_hockey_pos_arg_t arg;

    arg.pos.x = x;
    arg.pos.y = y;

    if (ioctl(g_air_hockey_fd, cmd, &arg)) {
        perror("game_io: position ioctl failed");
        exit(1);
    }
}

/*
 * Internal helper:
 * send one score ioctl.
 */
static void write_score(unsigned char p1, unsigned char p2)
{
    air_hockey_score_arg_t arg;

    arg.score.p1 = p1;
    arg.score.p2 = p2;

    if (ioctl(g_air_hockey_fd, AIR_HOCKEY_WRITE_SCORE, &arg)) {
        perror("game_io: score ioctl failed");
        exit(1);
    }
}

/*
 * Internal helper:
 * send one sound ioctl.
 */
static void write_sound(unsigned char event_id)
{
    air_hockey_sound_arg_t arg;

    arg.sound_event = event_id;

    if (ioctl(g_air_hockey_fd, AIR_HOCKEY_WRITE_SOUND, &arg)) {
        perror("game_io: sound ioctl failed");
        exit(1);
    }
}

int game_io_open(const char *device_path)
{
    if (device_path == NULL)
        device_path = "/dev/air_hockey";

    g_air_hockey_fd = open(device_path, O_RDWR);
    if (g_air_hockey_fd == -1) {
        perror("game_io: open(/dev/air_hockey) failed");
        return -1;
    }

    return 0;
}

void game_io_close(void)
{
    if (g_air_hockey_fd != -1) {
        close(g_air_hockey_fd);
        g_air_hockey_fd = -1;
    }
}

unsigned int game_io_read_status(void)
{
    air_hockey_status_t st;

    if (g_air_hockey_fd == -1) {
        fprintf(stderr, "game_io: device not open\n");
        exit(1);
    }

    if (ioctl(g_air_hockey_fd, AIR_HOCKEY_READ_STATUS, &st)) {
        perror("game_io: AIR_HOCKEY_READ_STATUS failed");
        exit(1);
    }

    return st.status;
}

void game_io_wait_for_vsync(void)
{
    //If already in VBlank, wait for it to end first (prevents accidental double-sync if this is called multiple times per frame)
    while ((game_io_read_status() & AIR_HOCKEY_STATUS_VSYNC_READY) != 0) {
        usleep(50);
    }
    //wait for the next VBlank to begin.
    while ((game_io_read_status() & AIR_HOCKEY_STATUS_VSYNC_READY) == 0) {
        usleep(50);
    }
}

void game_io_set_p1(unsigned short x, unsigned short y)
{
    write_pos(AIR_HOCKEY_WRITE_P1_POS, x, y);
}

void game_io_set_p2(unsigned short x, unsigned short y)
{
    write_pos(AIR_HOCKEY_WRITE_P2_POS, x, y);
}

void game_io_set_puck(unsigned short x, unsigned short y)
{
    write_pos(AIR_HOCKEY_WRITE_PUCK_POS, x, y);
}

void game_io_set_score(unsigned char p1, unsigned char p2)
{
    write_score(p1, p2);
}

void game_io_set_sound(unsigned char event_id)
{
    write_sound(event_id);
}

void game_io_submit_frame(const game_io_frame_t *frame)
{
    if (frame == NULL)
        return;

    /*
     * The intent is that main.c prepares one frame of logical state, then
     * calls this once during VSYNC.
     *
     * Order mostly does not matter much for the current hardware, but grouping
     * all writes here keeps the main loop clean and makes the per-frame update
     * policy explicit.
     */
    game_io_set_p1(frame->p1_x, frame->p1_y);
    game_io_set_p2(frame->p2_x, frame->p2_y);
    game_io_set_puck(frame->puck_x, frame->puck_y);
    game_io_set_score(frame->score_p1, frame->score_p2);
    game_io_set_sound(frame->sound_event);
}