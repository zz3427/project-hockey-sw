#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <math.h>
#include <float.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>


#include "physics_engine.h"
#include "game_io.h"
#include "game_config.h"
#include "game_render.h"
#include "game_sim.h"
#include "game_input.h"
#include "game_score.h"

int debug_physics = 0; // Set to 1 to enable detailed physics debug prints


int main(int argc, char *argv[]) {

    // 1. Initialize Game Objects
    GameObject p1 = {{100.0, 240.0}, {0.0, 0.0}, PADDLE_RADIUS}; 
    GameObject p2 = {{540.0, 240.0}, {0.0, 0.0}, PADDLE_RADIUS};
    GameObject puck = {{320.0, 240.0}, {0.0, 0.0}, PUCK_RADIUS}; 

    // if command line argument puck is provided, set initial puck velocity
    // command line argument format: puckx pucky puckvelx puckvely
    if (argc == 5) {
        debug_physics = 1;
        puck.pos.x = atof(argv[1]);
        puck.pos.y = atof(argv[2]);
        puck.vel.x = atof(argv[3]);
        puck.vel.y = atof(argv[4]);
        printf("[main] initial puck state from command line: pos=(%.2f, %.2f) vel=(%.2f, %.2f)\n",
               puck.pos.x, puck.pos.y, puck.vel.x, puck.vel.y);
    }


    int p1_score = 0;
    int p2_score = 0;
    int game_state = 0; // 0 = playing, 1 = goal scored/waiting for serve

    printf("Starting Embedded Air Hockey Engine...\n");
    if (game_io_open("/dev/air_hockey") != 0) {
        fprintf(stderr, "Failed to open /dev/air_hockey\n");
        return 1;
    }

    // open mouse input device for player 1 and player 2
    const char *dev_mouse1 = "/dev/input/event0";
    const char *dev_mouse2 = "/dev/input/event1";

    int mouse_fd1, mouse_fd2;

    mouse_fd1 = open(dev_mouse1, O_RDONLY | O_NONBLOCK);
    if (mouse_fd1 == -1) {
        perror("open(mouse device 1) failed");
        game_io_close();
        return 1;
    }

    mouse_fd2 = open(dev_mouse2, O_RDONLY | O_NONBLOCK);
    if (mouse_fd2 == -1) {
        perror("open(mouse device 2) failed");
        game_io_close();
        return 1;
    }
    unsigned char sound_event = AIR_HOCKEY_SOUND_NONE;

    // 2. Main Hardware Game Loop
    while (game_state != 2) {

        // Wait for VGA to finish drawing the current frame (60 Hz sync)
        game_io_wait_for_vsync();

        // TODO: Read /dev/input/mice evdev accumulators here and apply to p1.pos and p2.pos
        // STEP 1: Read input nonblocking, and update paddle
        poll_mouse_and_update_paddle(mouse_fd1, &p1, &puck, P1_X_MIN, P1_X_MAX, PADDLE_Y_MIN, PADDLE_Y_MAX);
        poll_mouse_and_update_paddle(mouse_fd2, &p2, &puck, P2_X_MIN, P2_X_MAX, PADDLE_Y_MIN, PADDLE_Y_MAX);
        
        

        // STEP 2: Simulate puck
        // Run physics engine for this frame
        if (game_state == 0) {
            sound_event = simulate_frame(&puck, &p1, &p2);
        }

        // STEP 3: Detect goal / update game_state / score
        // If goal: update scores, reset puck to center, game_state = 1
        int score_updater = handle_score_update(&puck, &p1, &p2, &p1_score, &p2_score);
        if (score_updater) {
            sound_event = AIR_HOCKEY_SOUND_GOAL;
            game_state = 1;
        }
        // STEP 4: Decide sound event for this frame

        // STEP 5: Push all state to hardware
        // Send updated coordinates to the Verilog VGA driver
        write_to_vga_registers(&puck, &p1, &p2, p1_score, p2_score, sound_event);

        // STEP 6: Reset after a goal has been scored
        if (score_updater == 1) {
            usleep(250000);
            reset_after_goal(&puck, &p1, &p2, PUCK_START_X_P1);
        } else if (score_updater == 2) {
            usleep(250000);
            reset_after_goal(&puck, &p1, &p2, PUCK_START_X_P2);
        }

        if (game_state == 1) {
            game_state = 0;
            if (p1_score >= MAX_SCORE || p2_score >= MAX_SCORE) {
                printf("Game over! Final score: P1=%d, P2=%d\n", p1_score, p2_score);
                for (int i = 0; i < 5; i++) {
                    write_to_vga_registers(&puck, &p1, &p2, i, i, AIR_HOCKEY_SOUND_GOAL);
                    usleep(500000);
                }
                game_state = 0; // end game loop
            }
        }
    }

    close(mouse_fd1);
    close(mouse_fd2);
    game_io_close(); //close the device when done
    return 0;
}
