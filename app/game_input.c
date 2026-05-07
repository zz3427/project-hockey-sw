#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>

#include "game_input.h"
#include "game_config.h"

static double clamp_double(double val, double lo, double hi)
{
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}

int open_mouse_device(const char *path)
{
    int fd = open(path, O_RDONLY | O_NONBLOCK);

    if (fd < 0) {
        perror(path);
        return -1;
    }

    return fd;
}

void poll_mouse_and_update_paddle(int mouse_fd,
                                  GameObject *p,
                                  const GameObject *puck,
                                  double x_min,
                                  double x_max,
                                  double y_min,
                                  double y_max)
{
    struct input_event ev;
    int dx = 0;
    int dy = 0;

    // Collect all mouse movement since last frame
    while (read(mouse_fd, &ev, sizeof(ev)) == sizeof(ev)) {
        if (ev.type == EV_REL) {
            if (ev.code == REL_X) {
                dx += ev.value;
            } else if (ev.code == REL_Y) {
                dy += ev.value;
            }
        }
    }

    // convert mouse movement to paddle movement, with sensitivity and max speed limits
    double move_x = dx * MOUSE_SENSITIVITY;
    double move_y = dy * MOUSE_SENSITIVITY;

    move_x = clamp_double(move_x, -MAX_PADDLE_SPEED, MAX_PADDLE_SPEED);
    move_y = clamp_double(move_y, -MAX_PADDLE_SPEED, MAX_PADDLE_SPEED);

    // If there is no movement, do not update velocity to allow puck to push stationary paddle
    if (dx == 0 && dy == 0) {
        // No movement, do not update velocity
        p->vel.x = 0.0;
        p->vel.y = 0.0;
        return;
    }

    // Save current paddle position before moving
    double old_x = p->pos.x;
    double old_y = p->pos.y;

    // Compute where paddle would move to if we apply the full mouse movement, and clamp to arena bounds
    double proposed_x = clamp_double(old_x + move_x, x_min, x_max);
    double proposed_y = clamp_double(old_y + move_y, y_min, y_max);

    // Compute distance from current paddle position to puck position
    double old_dx = old_x - puck->pos.x;
    double old_dy = old_y - puck->pos.y;
    double old_dist2 = old_dx * old_dx + old_dy * old_dy;

    // Compute distance from proposed paddle position to puck
    double proposed_dx = proposed_x - puck->pos.x;
    double proposed_dy = proposed_y - puck->pos.y;
    double proposed_dist2 = proposed_dx * proposed_dx + proposed_dy * proposed_dy;

    // Minimum legal center to center distance between paddle and puck to avoid overlap
    double min_dist = p->radius + puck->radius;
    double min_dist2 = min_dist * min_dist;

    if (proposed_dist2 < min_dist2) {
        int moving_deeper_into_puck = proposed_dist2 <= old_dist2;

        if(moving_deeper_into_puck) {
            p->vel.x = 0.0;
            p->vel.y = 0.0;
            return;
        }
    }

    // Movement is legal, commit the proposed position and velocity
    p->pos.x = proposed_x;
    p->pos.y = proposed_y;

    p->vel.x = proposed_x - old_x;
    p->vel.y = proposed_y - old_y;
}