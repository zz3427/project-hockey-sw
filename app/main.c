#include <stdio.h>
#include <stdbool.h>
#include <errno.h>

// For mouse input
#include <linux/input.h>
#include <unistd.h>
#include <fcntl.h>

#include "physics_engine.h"
#include "game_io.h"

#define P1_X_MIN 24.0
#define P1_X_MAX 320.0
#define P2_X_MIN 320.0
#define P2_X_MAX 615.0
#define Y_MIN    24.0
#define Y_MAX    455.0

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

// The core 60 Hz Continuous Collision Detection (CCD) engine
void simulateFrame(GameObject *puck, GameObject *p1, GameObject *p2, 
                   GameObject *top_post, GameObject *bottom_post) {
    
    double t_remaining = 1.0; 
    int bounce_count = 0;
    const int MAX_BOUNCES = 3; 

    while (t_remaining > 0.001 && bounce_count < MAX_BOUNCES) {
        
        double t_wall = getWallCollisionTime(puck);
        double t_p1   = getPaddleCollisionTime(puck, p1);
        double t_p2   = getPaddleCollisionTime(puck, p2);
        double t_top  = getPaddleCollisionTime(puck, top_post);
        double t_bot  = getPaddleCollisionTime(puck, bottom_post);
        
        double t_c = min_time(t_wall, min_time(t_p1, t_p2));
        t_c = min_time(t_c, min_time(t_top, t_bot));

        if (t_c >= t_remaining) {
            puck->pos.x += puck->vel.x * t_remaining;
            puck->pos.y += puck->vel.y * t_remaining;
            t_remaining = 0.0;
        } else {
            puck->pos.x += puck->vel.x * t_c;
            puck->pos.y += puck->vel.y * t_c;
            
            if (t_c == t_p1) applyPaddleCollision(puck, p1, 1.0);
            else if (t_c == t_p2) applyPaddleCollision(puck, p2, 1.0);
            else if (t_c == t_top) applyPaddleCollision(puck, top_post, 1.0);
            else if (t_c == t_bot) applyPaddleCollision(puck, bottom_post, 1.0);
            else if (t_c == t_wall) applyWallBounce(puck);

            t_remaining -= t_c;
            bounce_count++;
        }
    }
    
    if (bounce_count >= MAX_BOUNCES) {
        puck->vel.x = 0;
        puck->vel.y = 0;
    }
}

static double clamp_double(double val, double lo, double hi)
{
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}

void update_p1_from_mouse(GameObject *p1, int dx, int dy)
{
    p1->pos.x = clamp_double(p1->pos.x + dx, 24.0, 615.0);
    p1->pos.y = clamp_double(p1->pos.y + dy, 24.0, 455.0);
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

    if (dx != 0 || dy != 0) {
        p->pos.x = clamp_double(p->pos.x + dx, x_min, x_max);
        p->pos.y = clamp_double(p->pos.y + dy, y_min, y_max);
    }
}

int main() {
    // 1. Initialize Game Objects
    GameObject p1 = {{100.0, 240.0}, {0.0, 0.0}, 20.0}; 
    GameObject p2 = {{540.0, 240.0}, {0.0, 0.0}, 20.0};
    GameObject left_top_post = {{0.0, 170.0}, {0.0, 0.0}, 10.0};
    GameObject left_bot_post = {{0.0, 310.0}, {0.0, 0.0}, 10.0};
    GameObject puck = {{320.0, 240.0}, {-20.0, -15.0}, 10.0}; 

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

    int mouse_fd1 = open(dev_mouse1, O_RDONLY | O_NONBLOCK);
    if (mouse_fd1 == -1) {
        perror("open(mouse device 1) failed");
        game_io_close();
        return 1;
    }

    int mouse_fd2 = open(dev_mouse2, O_RDONLY | O_NONBLOCK);
    if (mouse_fd2 == -1) {
        perror("open(mouse device 2) failed");
        game_io_close();
        return 1;
    }

    // 2. Main Hardware Game Loop
    while (1) {
        // Wait for VGA to finish drawing the current frame (60 Hz sync)
        game_io_wait_for_vsync();

        // TODO: Read /dev/input/mice evdev accumulators here and apply to p1.pos and p2.pos
        // STEP 1: Read input nonblocking, and update paddle
        poll_mouse_and_update_paddle(mouse_fd1, &p1, P1_X_MIN, P1_X_MAX, Y_MIN, Y_MAX);
        poll_mouse_and_update_paddle(mouse_fd2, &p2, P2_X_MIN, P2_X_MAX, Y_MIN, Y_MAX);
        
        

        // STEP 2: Simulate puck
        // Run physics engine for this frame
        if (game_state == 0) {
            simulateFrame(&puck, &p1, &p2, &left_top_post, &left_bot_post);
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