#include <stdio.h>
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
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug-physics") == 0) {
            debug_physics = 1;
        }
    }

    if (debug_physics) printf("[main] physics debug enabled\n");

    // 1. Initialize Game Objects
    GameObject p1 = {{100.0, 240.0}, {0.0, 0.0}, PADDLE_RADIUS}; 
    GameObject p2 = {{540.0, 240.0}, {0.0, 0.0}, PADDLE_RADIUS};
    GameObject puck = {{320.0, 240.0}, {0.0, 0.0}, PUCK_RADIUS}; 

    // if command line argument puck is provided, set initial puck velocity for testing
    // command line argument format: 100 100 50 50
    for(int i = 1; i < argc; i++) {
        puck.pos.x = atof(argv[i + 1]);
        puck.pos.y = atof(argv[i + 2]);
        puck.vel.x = atof(argv[i + 3]);
        puck.vel.y = atof(argv[i + 4]);
        printf("[main] initial puck velocity set to (%.2f, %.2f)\n", puck.vel.x, puck.vel.y);
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

    // 2. Main Hardware Game Loop
    while (1) {
        /*
        static int frame_count = 0;
        static struct timespec last_time = {0, 0};

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        if(last_time.tv_sec == 0){
            last_time = now;
        }
        frame_count++;

        double elapsed =
            (now.tv_sec - last_time.tv_sec) +
            (now.tv_nsec - last_time.tv_nsec) / 1000000000.0;

        if (elapsed >= 1.0) {
            fprintf(stderr, "[main] FPS = %d\n", frame_count);
            frame_count = 0;
            last_time = now;
        }
        */

        // Wait for VGA to finish drawing the current frame (60 Hz sync)
        game_io_wait_for_vsync();

        // TODO: Read /dev/input/mice evdev accumulators here and apply to p1.pos and p2.pos
        // STEP 1: Read input nonblocking, and update paddle
        poll_mouse_and_update_paddle(mouse_fd1, &p1, P1_X_MIN, P1_X_MAX, PADDLE_Y_MIN, PADDLE_Y_MAX);
        poll_mouse_and_update_paddle(mouse_fd2, &p2, P2_X_MIN, P2_X_MAX, PADDLE_Y_MIN, PADDLE_Y_MAX);
        
        

        // STEP 2: Simulate puck
        // Run physics engine for this frame
        if (game_state == 0) {
            simulate_frame(&puck, &p1, &p2);
        }

        // STEP 3: Detect goal / update game_state / score
        // If goal: update scores, reset puck to center, game_state = 1
        handle_score_update(&puck, &p1, &p2, &p1_score, &p2_score);
        // STEP 4: Decide sound event for this frame

        // STEP 5: Push all state to hardware
        // Send updated coordinates to the Verilog VGA driver
        write_to_vga_registers(&puck, &p1, &p2, p1_score, p2_score, AIR_HOCKEY_SOUND_NONE);
        
    }

    close(mouse_fd1);
    close(mouse_fd2);
    game_io_close(); //close the device when done
    return 0;
}