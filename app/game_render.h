#ifndef GAME_RENDER_H
#define GAME_RENDER_H

#include "physics_engine.h"

void write_to_vga_registers(GameObject *puck,
                            GameObject *p1,
                            GameObject *p2,
                            int p1_score,
                            int p2_score,
                            unsigned char sound_event);

#endif