#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include "../app/game_io.h"

static void show_score(unsigned char p1, unsigned char p2, int seconds)
{
    fprintf(stderr, "[test_score_display] setting score P1=%u P2=%u\n", p1, p2);

    /*
     * Match the main app behavior: wait for VSYNC before updating visible state.
     */
    game_io_wait_for_vsync();

    game_io_set_score(p1, p2);

    sleep(seconds);
}

int main(void)
{
    fprintf(stderr, "Starting score display test...\n");

    if (game_io_open("/dev/air_hockey") != 0) {
        fprintf(stderr, "Failed to open /dev/air_hockey, errno=%d\n", errno);
        return 1;
    }

    /*
     * First clear both scores.
     */
    show_score(0, 0, 2);

    /*
     * Test obvious individual values.
     */
    show_score(1, 0, 2);
    show_score(0, 1, 2);
    show_score(3, 5, 2);
    show_score(7, 7, 2);

    /*
     * Count P1 from 0 to 7 while P2 stays 0.
     */
    for (unsigned char i = 0; i <= 7; i++) {
        show_score(i, 0, 1);
    }

    /*
     * Count P2 from 0 to 7 while P1 stays 0.
     */
    for (unsigned char i = 0; i <= 7; i++) {
        show_score(0, i, 1);
    }

    /*
     * Count both together.
     */
    for (unsigned char i = 0; i <= 7; i++) {
        show_score(i, i, 1);
    }

    /*
     * Leave display at 0-0 when done.
     */
    show_score(0, 0, 2);

    game_io_close();

    fprintf(stderr, "Score display test complete.\n");
    return 0;
}