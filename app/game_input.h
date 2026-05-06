#ifndef GAME_INPUT_H
#define GAME_INPUT_H

#include "physics_engine.h"

int open_mouse_device(const char *path);

void poll_mouse_and_update_paddle(int mouse_fd,
                                  GameObject *p,
                                  double x_min,
                                  double x_max,
                                  double y_min,
                                  double y_max);

#endif