#include "game_render.h"
#include "game_io.h"

void write_to_vga_registers(GameObject *puck,
                            GameObject *p1,
                            GameObject *p2,
                            int p1_score,
                            int p2_score,
                            unsigned char sound_event)
{
    game_io_set_p1((unsigned short)p1->pos.x, (unsigned short)p1->pos.y);
    game_io_set_p2((unsigned short)p2->pos.x, (unsigned short)p2->pos.y);
    game_io_set_puck((unsigned short)puck->pos.x, (unsigned short)puck->pos.y);
    game_io_set_score((unsigned char)p1_score, (unsigned char)p2_score);
    game_io_set_sound(sound_event);
}