#include <stdio.h>
#include <math.h>
#include <float.h>

#include "game_sim.h"
#include "physics_engine.h"
#include "game_config.h"

/*
 * debug_physics is defined in main.c for now.
 *
 * Later, we can move it into game_debug.c, but for this step,
 * this lets game_sim.c use your current command-line debug flag
 * without changing too much at once.
 */
extern int debug_physics;

// Helper function for sub-frame loop
static double min_time(double a, double b)
{
    return (a < b) ? a : b;
}

// TODO: later move this to game_debug.c
static void print_time(const char *name, double t)
{
    if (t == DBL_MAX || isinf(t)) {
        printf("%s=INF ", name);
    } else {
        printf("%s=%.6f ", name, t);
    }
}

static void clamp_puck_to_arena(GameObject *puck)
{
    if (puck->pos.x < WALL_LEFT + puck->radius) {
        puck->pos.x = WALL_LEFT + puck->radius;
    } else if (puck->pos.x > WALL_RIGHT - puck->radius) {
        puck->pos.x = WALL_RIGHT - puck->radius;
    }

    if (puck->pos.y < WALL_TOP + puck->radius) {
        puck->pos.y = WALL_TOP + puck->radius;
    } else if (puck->pos.y > WALL_BOTTOM - puck->radius) {
        puck->pos.y = WALL_BOTTOM - puck->radius;
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
void simulate_frame(GameObject *puck,
                    GameObject *p1,
                    GameObject *p2,
                    GameObject *top_post,
                    GameObject *bottom_post)
{
    double t_remaining = 1.0;
    int bounce_count = 0;

    while (t_remaining > MIN_FRAME_TIME_REMAINING &&
           bounce_count < MAX_BOUNCES) {

        // Ensure puck is within bounds before calculations
        clamp_puck_to_arena(puck);

        /*
         * Print puck state at the start of this sub-step.
         * This tells us where the puck currently is and how it is moving
         * before we compute collision times.
         */
        if (debug_physics) {
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
        if (debug_physics) {
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

        // Minimum time threshold to avoid repeated zero-time collision loops
        if (t_c < MIN_COLLISION_TIME) {
            t_c = MIN_COLLISION_TIME;
        }

        /*
         * Print the chosen collision time for this sub-step.
         */
        if (debug_physics) {
            fprintf(stderr,
                    "[simulate_frame] chosen t_c=%.6f\n",
                    t_c);
        }

        if (t_c >= t_remaining) {
            // No collision within remaining time, just move puck to end of frame
            puck->pos.x += puck->vel.x * t_remaining;
            puck->pos.y += puck->vel.y * t_remaining;

            if (debug_physics) {
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

            /*
             * Identify which collision happened and apply response.
             *
             * This keeps your current structure exactly:
             * - paddle/post collision uses applyPaddleCollision()
             * - wall collision uses applyWallBounce()
             * - all use RESTITUTION from game_config.h
             */
            if (t_c == t_p1) {
                applyPaddleCollision(puck, p1, RESTITUTION);
                if (debug_physics) {
                    fprintf(stderr,
                            "[simulate_frame] COLLISION with P1 at t=%.6f\n",
                            t_c);
                }
            } else if (t_c == t_p2) {
                applyPaddleCollision(puck, p2, RESTITUTION);
                if (debug_physics) {
                    fprintf(stderr,
                            "[simulate_frame] COLLISION with P2 at t=%.6f\n",
                            t_c);
                }
            } else if (t_c == t_top) {
                applyPaddleCollision(puck, top_post, RESTITUTION);
                if (debug_physics) {
                    fprintf(stderr,
                            "[simulate_frame] COLLISION with TOP POST at t=%.6f\n",
                            t_c);
                }
            } else if (t_c == t_bot) {
                applyPaddleCollision(puck, bottom_post, RESTITUTION);
                if (debug_physics) {
                    fprintf(stderr,
                            "[simulate_frame] COLLISION with BOTTOM POST at t=%.6f\n",
                            t_c);
                }
            } else if (t_c == t_wall) {
                applyWallBounce(puck, RESTITUTION);
                if (debug_physics) {
                    fprintf(stderr,
                            "[simulate_frame] COLLISION with WALL at t=%.6f\n",
                            t_c);
                }
            }

            // Ensure puck does not exceed max speed after collision
            clamp_object_speed(puck, MAX_PUCK_SPEED);

            // Print puck state after applying collision response
            if (debug_physics) {
                fprintf(stderr,
                        "[simulate_frame] after collision response: "
                        "puck pos=(%.4f, %.4f) vel=(%.4f, %.4f)\n",
                        puck->pos.x, puck->pos.y,
                        puck->vel.x, puck->vel.y);
            }

            // Print puck speed after collision response
            if (debug_physics) {
                double speed = sqrt(puck->vel.x * puck->vel.x +
                                    puck->vel.y * puck->vel.y);
                fprintf(stderr,
                        "[simulate_frame] puck speed after collision=%.4f\n",
                        speed);
            }

            t_remaining -= t_c;
            bounce_count++;
        }
    }

    if (bounce_count >= MAX_BOUNCES) {
        // Prevent infinite loop in extreme edge cases
        if (debug_physics) {
            fprintf(stderr,
                    "simulateFrame: MAX_BOUNCES reached, "
                    "puck pos=(%.2f, %.2f) vel=(%.2f, %.2f)\n",
                    puck->pos.x, puck->pos.y,
                    puck->vel.x, puck->vel.y);
        }
    }
}