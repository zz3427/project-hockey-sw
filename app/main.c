#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <math.h>
#include <float.h>
#include <string.h>

// For mouse input
#include <linux/input.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#include "physics_engine.h"
#include "game_io.h"

#define P1_X_MIN 24.0
#define P1_X_MAX 320.0
#define P2_X_MIN 320.0
#define P2_X_MAX 615.0
#define Y_MIN    24.0
#define Y_MAX    455.0

#define MOUSE_SENSITIVITY 1.0
#define MAX_PADDLE_SPEED 40.0
#define RESTITUTION 0.7
#define MAX_PUCK_SPEED 80.0

int debug_physics = 0; // Set to 1 to enable detailed physics debug prints


 /*
  * Send the current visible game object positions to hardware.
  *
  * This function acts as a bridge between:
  *   - the game/physics layer, which uses GameObject
  *   - the userspace I/O layer, which exposes simple game_io_set_* helpers
  *
  * This function should be called once per frame, after game logic updates and
  * after VSYNC synchronization has been handled.
  */
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

void update_board_leds(int state, int p1_score, int p2_score) { /* Update DE1-SoC LED behavior */ }
int get_button_press() { return 0; /* Read KEY0/KEY1 for game reset or serve constraints */ }

// Helper function for sub-frame loop
double min_time(double a, double b) { return (a < b) ? a : b; }


//TODO: to be deleted after debug
void print_time(const char *name, double t) {
    if (t == DBL_MAX || isinf(t)) {
        printf("%s=INF ", name);
    } else {
        printf("%s=%.6f ", name, t);
    }
}

static void clamp_puck_to_arena(GameObject *puck) {
    if (puck->pos.x < 10.0 + puck->radius) {
        puck->pos.x = 10.0 + puck->radius;
    } else if (puck->pos.x > 630.0 - puck->radius) {
        puck->pos.x = 630.0 - puck->radius;
    }
    if (puck->pos.y < 10.0 + puck->radius) {
        puck->pos.y = 10.0 + puck->radius;
    } else if (puck->pos.y > 470.0 - puck->radius) {
        puck->pos.y = 470.0 - puck->radius;
    }
}

static void clamp_object_speed(GameObject *obj, double max_speed)
{
    double speed = sqrt(obj->vel.x * obj->vel.x + obj->vel.y * obj->vel.y);

    if (speed > max_speed) {
        double scale = max_speed / speed;
        obj->vel.x *= scale;
        obj->vel.y *= scale;
    }
}

// The core 60 Hz Continuous Collision Detection (CCD) engine
void simulate_frame(GameObject *puck, GameObject *p1, GameObject *p2, 
                   GameObject *top_post, GameObject *bottom_post) {
    
    double t_remaining = 1.0; 
    int bounce_count = 0;
    const int MAX_BOUNCES = 3; 

    //

    while (t_remaining > 0.001 && bounce_count < MAX_BOUNCES) {

        clamp_puck_to_arena(puck); // Ensure puck is within bounds before calculations

        /*
         * Print puck state at the start of this sub-step.
         * This tells us where the puck currently is and how it is moving
         * before we compute collision times.
         */
        if(debug_physics) {
            fprintf(stderr,
                    "\n[simulate_frame] START step: "
                    "puck pos=(%.4f, %.4f) vel=(%.4f, %.4f) "
                    "t_remaining=%.6f bounce_count=%d\n",
                    puck->pos.x, puck->pos.y,
                    puck->vel.x, puck->vel.y,
                t_remaining, bounce_count);
            }
        double t_wall = getWallCollisionTime(puck);
        double t_p1   = getPaddleCollisionTime(puck, p1);
        double t_p2   = getPaddleCollisionTime(puck, p2);
        double t_top  = getPaddleCollisionTime(puck, top_post);
        double t_bot  = getPaddleCollisionTime(puck, bottom_post);

        /*
         * Print all candidate collision times.
         * This is the most important debug line.
         * If one of these is always 0.000000, that is probably your problem.
         */
        if(debug_physics){
            printf("[simulate_frame] times: ");
            print_time("t_wall", t_wall);
            print_time("t_p1", t_p1);
            print_time("t_p2", t_p2);
            print_time("t_top", t_top);
            print_time("t_bot", t_bot);
            printf("\n");
        }
        
        double t_c = min_time(t_wall, min_time(t_p1, t_p2));
        t_c = min_time(t_c, min_time(t_top, t_bot));

        const double MIN_COLLISION_TIME = 1e-6; // Minimum time threshold to consider a collision valid
        if (t_c < MIN_COLLISION_TIME) {
            t_c = MIN_COLLISION_TIME;
        }
        /*
         * Print the chosen collision time for this sub-step.
         */
        if(debug_physics) {
            fprintf(stderr,
                    "[simulate_frame] chosen t_c=%.6f\n",
                    t_c);
        }

        if (t_c >= t_remaining) {
            // No collision within remaining time, just move puck to end of frame
            puck->pos.x += puck->vel.x * t_remaining;
            puck->pos.y += puck->vel.y * t_remaining;

            if(debug_physics) {
                fprintf(stderr,
                        "[simulate_frame] no collision in remaining time, "
                        "advanced to pos=(%.4f, %.4f)\n",
                        puck->pos.x, puck->pos.y);
            }

            t_remaining = 0.0;
        } else {
            // Move puck to collision point first
            puck->pos.x += puck->vel.x * t_c;
            puck->pos.y += puck->vel.y * t_c;
            
            // Identify which collision happened and apply response
            if (t_c == t_p1){
                applyPaddleCollision(puck, p1, RESTITUTION);
                if(debug_physics) fprintf(stderr, "[simulate_frame] COLLISION with P1 at t=%.6f\n", t_c);
            } else if (t_c == t_p2) {
                applyPaddleCollision(puck, p2, RESTITUTION);
                if(debug_physics) fprintf(stderr, "[simulate_frame] COLLISION with P2 at t=%.6f\n", t_c);
            } else if (t_c == t_top) {
                applyPaddleCollision(puck, top_post, RESTITUTION);
                if(debug_physics) fprintf(stderr, "[simulate_frame] COLLISION with TOP POST at t=%.6f\n", t_c);
            } else if (t_c == t_bot) {
                applyPaddleCollision(puck, bottom_post, RESTITUTION);
                if(debug_physics) fprintf(stderr, "[simulate_frame] COLLISION with BOTTOM POST at t=%.6f\n", t_c);
            } else if (t_c == t_wall) {
                applyWallBounce(puck, RESTITUTION);
                if(debug_physics) fprintf(stderr, "[simulate_frame] COLLISION with WALL at t=%.6f\n", t_c);
            }
            clamp_object_speed(puck, MAX_PUCK_SPEED); // Ensure puck doesn't exceed max speed after collision
            // print puck state after applying collision response
            if(debug_physics) {
                fprintf(stderr,
                        "[simulate_frame] after collision response: "
                        "puck pos=(%.4f, %.4f) vel=(%.4f, %.4f)\n",
                        puck->pos.x, puck->pos.y,
                        puck->vel.x, puck->vel.y);
            }

            // print puck speed after collision response
            if(debug_physics) {
                double speed = sqrt(puck->vel.x * puck->vel.x + puck->vel.y * puck->vel.y);
                fprintf(stderr, "[simulate_frame] puck speed after collision=%.4f\n", speed);
            }

            t_remaining -= t_c;
            bounce_count++;
        }
    }
    
    if (bounce_count >= MAX_BOUNCES) {
        // puck->vel.x = 0;
        // puck->vel.y = 0;
        // Prevent infinite loop in extreme edge cases
        if(debug_physics) {
            fprintf(stderr,
            "simulateFrame: MAX_BOUNCES reached, puck pos=(%.2f, %.2f) vel=(%.2f, %.2f)\n",
            puck->pos.x, puck->pos.y, puck->vel.x, puck->vel.y);
        }
    }
}

static double clamp_double(double val, double lo, double hi)
{
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}

/*
 * Drain all pending events from one mouse and update one paddle once per frame.
 */
static void poll_mouse_and_update_paddle(int mouse_fd, GameObject *p,
                                         double x_min, double x_max,
                                         double y_min, double y_max)
{
    struct input_event ev;
    int dx = 0;
    int dy = 0;

    while (read(mouse_fd, &ev, sizeof(ev)) == sizeof(ev)) {
        if (ev.type == EV_REL) {
            if (ev.code == REL_X) dx += ev.value;
            else if (ev.code == REL_Y) dy += ev.value;
        }
    }

    double move_x = dx * MOUSE_SENSITIVITY;
    double move_y = dy * MOUSE_SENSITIVITY;

    p->vel.x = clamp_double(move_x, -MAX_PADDLE_SPEED, MAX_PADDLE_SPEED);
    p->vel.y = clamp_double(move_y, -MAX_PADDLE_SPEED, MAX_PADDLE_SPEED);

    if (dx != 0 || dy != 0) {
        p->pos.x = clamp_double(p->pos.x + move_x, x_min, x_max);
        p->pos.y = clamp_double(p->pos.y + move_y, y_min, y_max);
    }
}

int main(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug-physics") == 0) {
            debug_physics = 1;
        }
    }

    if (debug_physics) printf("[main] physics debug enabled\n");

    // 1. Initialize Game Objects
    GameObject p1 = {{100.0, 240.0}, {0.0, 0.0}, 20.0}; 
    GameObject p2 = {{540.0, 240.0}, {0.0, 0.0}, 20.0};
    GameObject left_top_post = {{0.0, 170.0}, {0.0, 0.0}, 10.0};
    GameObject left_bot_post = {{0.0, 310.0}, {0.0, 0.0}, 10.0};
    GameObject puck = {{320.0, 240.0}, {0.0, 0.0}, 10.0}; 

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

        // Wait for VGA to finish drawing the current frame (60 Hz sync)
        game_io_wait_for_vsync();

        // TODO: Read /dev/input/mice evdev accumulators here and apply to p1.pos and p2.pos
        // STEP 1: Read input nonblocking, and update paddle
        poll_mouse_and_update_paddle(mouse_fd1, &p1, P1_X_MIN, P1_X_MAX, Y_MIN, Y_MAX);
        poll_mouse_and_update_paddle(mouse_fd2, &p2, P2_X_MIN, P2_X_MAX, Y_MIN, Y_MAX);
        
        

        // STEP 2: Simulate puck
        // Run physics engine for this frame
        if (game_state == 0) {
            simulate_frame(&puck, &p1, &p2, &left_top_post, &left_bot_post);
        }

        // STEP 3: Detect goal / update game_state / score
        // If goal: update scores, reset puck to center, game_state = 1

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