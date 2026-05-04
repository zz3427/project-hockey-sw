// tests/test_game_io_gameobject.c

#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <math.h>
#include <float.h>
#include <string.h>
#include <unistd.h>

#include "../app/physics_engine.h"
#include "../app/game_io.h"

void write_to_vga_registers(GameObject *puck,
                            GameObject *p1,
                            GameObject *p2,
                            int p1_score,
                            int p2_score,
                            unsigned char sound_event)
{
    printf("[write_to_vga_registers]\n");
    printf("  P1   GameObject=(%.2f, %.2f) -> ushort=(%hu, %hu)\n",
           p1->pos.x, p1->pos.y,
           (unsigned short)p1->pos.x, (unsigned short)p1->pos.y);

    printf("  P2   GameObject=(%.2f, %.2f) -> ushort=(%hu, %hu)\n",
           p2->pos.x, p2->pos.y,
           (unsigned short)p2->pos.x, (unsigned short)p2->pos.y);

    printf("  Puck GameObject=(%.2f, %.2f) -> ushort=(%hu, %hu)\n",
           puck->pos.x, puck->pos.y,
           (unsigned short)puck->pos.x, (unsigned short)puck->pos.y);

    printf("  Score=(%d, %d), sound=%u\n\n",
           p1_score, p2_score, sound_event);

    game_io_set_p1((unsigned short)p1->pos.x, (unsigned short)p1->pos.y);
    game_io_set_p2((unsigned short)p2->pos.x, (unsigned short)p2->pos.y);
    game_io_set_puck((unsigned short)puck->pos.x, (unsigned short)puck->pos.y);
    game_io_set_score((unsigned char)p1_score, (unsigned char)p2_score);
    game_io_set_sound(sound_event);
}

int main(void)
{
    printf("Starting GameObject -> game_io hardware path test...\n");

    GameObject p1 = {{100.0, 240.0}, {0.0, 0.0}, 20.0};
    GameObject p2 = {{540.0, 240.0}, {0.0, 0.0}, 20.0};
    GameObject puck = {{320.0, 240.0}, {-20.0, -15.0}, 10.0};

    int p1_score = 0;
    int p2_score = 0;

    if (game_io_open("/dev/air_hockey") != 0) {
        fprintf(stderr, "Failed to open /dev/air_hockey, errno=%d\n", errno);
        return 1;
    }

    printf("Opened /dev/air_hockey successfully.\n");

    printf("Test 1: same initial positions as main\n");
    game_io_wait_for_vsync();
    write_to_vga_registers(&puck, &p1, &p2, p1_score, p2_score, AIR_HOCKEY_SOUND_NONE);
    sleep(3);

    printf("Test 2: puck at legal top-left\n");
    puck.pos.x = 20.0;
    puck.pos.y = 20.0;
    p1_score = 1;
    game_io_wait_for_vsync();
    write_to_vga_registers(&puck, &p1, &p2, p1_score, p2_score, AIR_HOCKEY_SOUND_NONE);
    sleep(3);

    printf("Test 3: puck at legal top-right\n");
    puck.pos.x = 620.0;
    puck.pos.y = 20.0;
    p2_score = 1;
    game_io_wait_for_vsync();
    write_to_vga_registers(&puck, &p1, &p2, p1_score, p2_score, AIR_HOCKEY_SOUND_NONE);
    sleep(3);

    printf("Test 4: puck at legal bottom-right\n");
    puck.pos.x = 620.0;
    puck.pos.y = 460.0;
    p1_score = 2;
    game_io_wait_for_vsync();
    write_to_vga_registers(&puck, &p1, &p2, p1_score, p2_score, AIR_HOCKEY_SOUND_NONE);
    sleep(3);

    printf("Test 5: puck at legal bottom-left\n");
    puck.pos.x = 20.0;
    puck.pos.y = 460.0;
    p2_score = 2;
    game_io_wait_for_vsync();
    write_to_vga_registers(&puck, &p1, &p2, p1_score, p2_score, AIR_HOCKEY_SOUND_NONE);
    sleep(3);

    printf("Test 6: move paddles too\n");
    puck.pos.x = 320.0;
    puck.pos.y = 240.0;
    p1.pos.x = 80.0;
    p1.pos.y = 120.0;
    p2.pos.x = 560.0;
    p2.pos.y = 360.0;
    game_io_wait_for_vsync();
    write_to_vga_registers(&puck, &p1, &p2, 3, 3, AIR_HOCKEY_SOUND_NONE);
    sleep(3);

    printf("Test 7: sweep puck left to right, same write path as main\n");
    p1.pos.x = 100.0;
    p1.pos.y = 240.0;
    p2.pos.x = 540.0;
    p2.pos.y = 240.0;
    puck.pos.y = 240.0;

    for (double x = 20.0; x <= 620.0; x += 20.0) {
        puck.pos.x = x;
        game_io_wait_for_vsync();
        write_to_vga_registers(&puck, &p1, &p2, 4, 4, AIR_HOCKEY_SOUND_NONE);
        usleep(100000);
    }

    printf("Test 8: intentional bad negative coordinate conversion\n");
    puck.pos.x = -20.0;
    puck.pos.y = 240.0;
    game_io_wait_for_vsync();
    write_to_vga_registers(&puck, &p1, &p2, 9, 9, AIR_HOCKEY_SOUND_NONE);
    sleep(3);

    game_io_close();
    printf("Test complete.\n");
    return 0;
}