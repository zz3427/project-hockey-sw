#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <math.h>
#include <float.h>
#include <string.h>

#include "../app/physics_engine.h"
#include "../app/game_io.h"

void write_to_vga_registers(GameObject *puck,
                            GameObject *p1,
                            GameObject *p2,
                            int p1_score,
                            int p2_score,
                            unsigned char sound_event)
{
    unsigned short p1_x   = (unsigned short)p1->pos.x;
    unsigned short p1_y   = (unsigned short)p1->pos.y;
    unsigned short p2_x   = (unsigned short)p2->pos.x;
    unsigned short p2_y   = (unsigned short)p2->pos.y;
    unsigned short puck_x = (unsigned short)puck->pos.x;
    unsigned short puck_y = (unsigned short)puck->pos.y;

    printf("Writing to VGA:\n");
    printf("  P1   GameObject=(%.2f, %.2f) -> ushort=(%hu, %hu)\n",
           p1->pos.x, p1->pos.y, p1_x, p1_y);
    printf("  P2   GameObject=(%.2f, %.2f) -> ushort=(%hu, %hu)\n",
           p2->pos.x, p2->pos.y, p2_x, p2_y);
    printf("  Puck GameObject=(%.2f, %.2f) -> ushort=(%hu, %hu)\n",
           puck->pos.x, puck->pos.y, puck_x, puck_y);
    printf("  Score=(%d, %d), sound=%u\n",
           p1_score, p2_score, sound_event);

    game_io_set_p1(p1_x, p1_y);
    game_io_set_p2(p2_x, p2_y);
    game_io_set_puck(puck_x, puck_y);
    game_io_set_score((unsigned char)p1_score, (unsigned char)p2_score);
    game_io_set_sound(sound_event);
}

static void set_object(GameObject *obj,
                       double x,
                       double y,
                       double vx,
                       double vy,
                       double radius)
{
    obj->pos.x = x;
    obj->pos.y = y;
    obj->vel.x = vx;
    obj->vel.y = vy;
    obj->radius = radius;
}

int main(void)
{
    printf("Starting GameObject -> game_io VGA test\n");

    if (game_io_init() != 0) {
        printf("ERROR: game_io_init() failed\n");
        return 1;
    }

    GameObject puck;
    GameObject p1;
    GameObject p2;

    set_object(&p1, 120.0, 240.0, 0.0, 0.0, 25.0);
    set_object(&p2, 520.0, 240.0, 0.0, 0.0, 25.0);
    set_object(&puck, 320.0, 240.0, 0.0, 0.0, 10.0);

    printf("\nTest 1: center puck\n");
    write_to_vga_registers(&puck, &p1, &p2, 0, 0, 0);
    sleep(3);

    printf("\nTest 2: puck near top-left valid bound\n");
    puck.pos.x = 20.0;
    puck.pos.y = 20.0;
    write_to_vga_registers(&puck, &p1, &p2, 1, 0, 0);
    sleep(3);

    printf("\nTest 3: puck near top-right valid bound\n");
    puck.pos.x = 620.0;
    puck.pos.y = 20.0;
    write_to_vga_registers(&puck, &p1, &p2, 1, 1, 0);
    sleep(3);

    printf("\nTest 4: puck near bottom-right valid bound\n");
    puck.pos.x = 620.0;
    puck.pos.y = 460.0;
    write_to_vga_registers(&puck, &p1, &p2, 2, 1, 0);
    sleep(3);

    printf("\nTest 5: puck near bottom-left valid bound\n");
    puck.pos.x = 20.0;
    puck.pos.y = 460.0;
    write_to_vga_registers(&puck, &p1, &p2, 2, 2, 0);
    sleep(3);

    printf("\nTest 6: horizontal sweep using GameObject double positions\n");

    puck.pos.y = 240.0;

    for (double x = 20.0; x <= 620.0; x += 20.0) {
        puck.pos.x = x;

        write_to_vga_registers(&puck, &p1, &p2, 3, 2, 0);

        usleep(100000);
    }

    printf("\nTest 7: deliberately bad negative coordinate conversion test\n");
    puck.pos.x = -20.0;
    puck.pos.y = 240.0;
    write_to_vga_registers(&puck, &p1, &p2, 9, 9, 0);
    sleep(3);

    printf("\nTest complete\n");

    game_io_close();

    return 0;
}