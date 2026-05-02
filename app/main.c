#include <stdio.h>
#include <stdbool.h>
#include "physics_engine.h"
#include "game_io.h"

// Hardware abstraction placeholders (To be implemented with actual memory-mapped I/O)
bool is_vsync_ready() { return true; /* Replace with hardware register poll */ }
// void clear_vsync() { /* Acknowledge VSYNC */ } // we don't clear_vsync
void wait_for_vsync() { game_io_wait_for_vsync(); /* Use game_io's busy-wait for VSYNC */ }

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

    // 2. Main Hardware Game Loop
    while (1) {
        // Wait for VGA to finish drawing the current frame (60 Hz sync)
        wait_for_vsync();
            
        // Process button-press logic for state control (e.g., serving the puck after a goal)
        int btn = get_button_press();
        if (btn != 0) {
            // Handle constraints for button logic here
        }

        // TODO: Read /dev/input/mice evdev accumulators here and apply to p1.pos and p2.pos
        
        // Run physics engine for this frame
        if (game_state == 0) {
            simulateFrame(&puck, &p1, &p2, &left_top_post, &left_bot_post);
        }

        // TODO: Add goal detection bounds check here (Section 3.2.1)
        // If goal: update scores, reset puck to center, game_state = 1
        
        // Send updated coordinates to the Verilog VGA driver
        write_to_vga_registers(&puck, &p1, &p2);

        // Output current state to hardware LEDs
        update_board_leds(game_state, p1_score, p2_score);

        // Acknowledge VSYNC to prevent running the frame twice
        clear_vsync();
        
        // For testing on PC: break after 1 frame so it doesn't infinite loop in the terminal
        break; 
    }

    return 0;
}