#ifndef GAME_INPUT_H
#define GAME_INPUT_H

#include "physics_engine.h"

int open_mouse_device(const char *path);

int open_two_mouse_devices(int *mouse_fd1, int *mouse_fd2);

void poll_mouse_and_update_paddle(int mouse_fd,
                                  GameObject *p,
                                  const GameObject *puck,
                                  double x_min,
                                  double x_max,
                                  double y_min,
                                  double y_max,
                                  int x_sign,
                                  int y_sign);

#endif