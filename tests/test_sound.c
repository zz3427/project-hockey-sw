#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include "../app/game_io.h"

static void play_sound_event(unsigned char event_id, const char *name)
{
    fprintf(stderr, "[test_sound] sending %s, event_id=%u\n",
            name, event_id);

    /*
     * Wait for a clean frame boundary before sending the event.
     * This matches how the main game writes frame state.
     */
    game_io_wait_for_vsync();

    game_io_set_sound(event_id);

    /*
     * Give hardware time to play the sound before sending the next one.
     * Adjust if your sound effect is longer/shorter.
     */
    sleep(1);
}

int main(void)
{
    fprintf(stderr, "Starting sound test through game_io + kernel driver...\n");

    if (game_io_open("/dev/air_hockey") != 0) {
        fprintf(stderr, "Failed to open /dev/air_hockey, errno=%d\n", errno);
        return 1;
    }

    fprintf(stderr, "Opened /dev/air_hockey successfully.\n");

    /*
     * Send NONE first to clear/idle the sound command.
     */
    play_sound_event(AIR_HOCKEY_SOUND_NONE, "NONE");

    /*
     * Test each real sound effect individually.
     */
    play_sound_event(AIR_HOCKEY_SOUND_HIT_WALL, "HIT_WALL");
    play_sound_event(AIR_HOCKEY_SOUND_HIT_PADDLE, "HIT_PADDLE");
    play_sound_event(AIR_HOCKEY_SOUND_GOAL, "GOAL");

    /*
     * Repeat a few times so it is easy to hear.
     */
    for (int i = 0; i < 3; i++) {
        fprintf(stderr, "[test_sound] repeat round %d\n", i + 1);

        play_sound_event(AIR_HOCKEY_SOUND_HIT_WALL, "HIT_WALL");
        play_sound_event(AIR_HOCKEY_SOUND_HIT_PADDLE, "HIT_PADDLE");
        play_sound_event(AIR_HOCKEY_SOUND_GOAL, "GOAL");
    }

    /*
     * Return to no sound.
     */
    play_sound_event(AIR_HOCKEY_SOUND_NONE, "NONE");

    game_io_close();

    fprintf(stderr, "Sound test complete.\n");
    return 0;
}