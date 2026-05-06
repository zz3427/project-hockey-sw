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
                                  double x_min,
                                  double x_max,
                                  double y_min,
                                  double y_max)
{
    struct input_event ev;
    int dx = 0;
    int dy = 0;

    while (read(mouse_fd, &ev, sizeof(ev)) == sizeof(ev)) {
        if (ev.type == EV_REL) {
            if (ev.code == REL_X) {
                dx += ev.value;
            } else if (ev.code == REL_Y) {
                dy += ev.value;
            }
        }
    }

    double move_x = dx * MOUSE_SENSITIVITY;
    double move_y = dy * MOUSE_SENSITIVITY;

    move_x = clamp_double(move_x, -MAX_PADDLE_SPEED, MAX_PADDLE_SPEED);
    move_y = clamp_double(move_y, -MAX_PADDLE_SPEED, MAX_PADDLE_SPEED);

    p->vel.x = move_x;
    p->vel.y = move_y;

    if (dx != 0 || dy != 0) {
        p->pos.x = clamp_double(p->pos.x + move_x, x_min, x_max);
        p->pos.y = clamp_double(p->pos.y + move_y, y_min, y_max);
    }
}