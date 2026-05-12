#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
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

#define INPUT_BY_PATH_DIR "/dev/input/by-path"
#define MAX_MOUSE_DEVICES 8

static int compare_mouse_paths(const void *a, const void *b)
{
    const char *pa = *(const char * const *)a;
    const char *pb = *(const char * const *)b;
    return strcmp(pa, pb);
}

int open_two_mouse_devices(int *mouse_fd1, int *mouse_fd2)
{
    DIR *dir;
    struct dirent *entry;
    char *mouse_paths[MAX_MOUSE_DEVICES];
    int mouse_count = 0;

    *mouse_fd1 = -1;
    *mouse_fd2 = -1;

    dir = opendir(INPUT_BY_PATH_DIR);
    if (dir == NULL) {
        perror("opendir /dev/input/by-path");
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, "event-mouse") != NULL) {
            char full_path[512];

            if (mouse_count >= MAX_MOUSE_DEVICES) {
                break;
            }

            snprintf(full_path, sizeof(full_path), "%s/%s",
                     INPUT_BY_PATH_DIR, entry->d_name);

            mouse_paths[mouse_count] = strdup(full_path);
            if (mouse_paths[mouse_count] == NULL) {
                perror("strdup");
                closedir(dir);

                for (int i = 0; i < mouse_count; i++) {
                    free(mouse_paths[i]);
                }

                return -1;
            }

            mouse_count++;
        }
    }

    closedir(dir);

    if (mouse_count < 2) {
        fprintf(stderr, "Found only %d mouse device(s). Need 2.\n", mouse_count);

        for (int i = 0; i < mouse_count; i++) {
            fprintf(stderr, "Found mouse: %s\n", mouse_paths[i]);
            free(mouse_paths[i]);
        }

        return -1;
    }

    qsort(mouse_paths, mouse_count, sizeof(char *), compare_mouse_paths);

    fprintf(stderr, "Using mouse 1: %s\n", mouse_paths[0]);
    fprintf(stderr, "Using mouse 2: %s\n", mouse_paths[1]);

    *mouse_fd1 = open_mouse_device(mouse_paths[0]);
    *mouse_fd2 = open_mouse_device(mouse_paths[1]);

    for (int i = 0; i < mouse_count; i++) {
        free(mouse_paths[i]);
    }

    if (*mouse_fd1 < 0 || *mouse_fd2 < 0) {
        if (*mouse_fd1 >= 0) close(*mouse_fd1);
        if (*mouse_fd2 >= 0) close(*mouse_fd2);

        *mouse_fd1 = -1;
        *mouse_fd2 = -1;

        return -1;
    }

    return 0;
}

void poll_mouse_and_update_paddle(int mouse_fd,
                                  GameObject *p,
                                  const GameObject *puck,
                                  double x_min,
                                  double x_max,
                                  double y_min,
                                  double y_max,
                                  int x_sign,
                                  int y_sign)
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

    int mapped_dx = dx;
    int mapped_dy = dy;

    #if MOUSE_SWAP_XY
    mapped_dx = dy;
    mapped_dy = dx;
    #endif

    mapped_dx *= x_sign;
    mapped_dy *= y_sign;

    double move_x = mapped_dx * MOUSE_SENSITIVITY;
    double move_y = mapped_dy * MOUSE_SENSITIVITY;

    // convert mouse movement to paddle movement, with sensitivity and max speed limits
    double move_x = mapped_dx * MOUSE_SENSITIVITY;
    double move_y = mapped_dy * MOUSE_SENSITIVITY;

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