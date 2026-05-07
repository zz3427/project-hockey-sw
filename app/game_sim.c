#include <stdio.h>
#include <math.h>
#include <float.h>

#include "game_sim.h"
#include "physics_engine.h"
#include "game_config.h"

extern int debug_physics;

typedef enum {
    COLLISION_NONE = 0,
    COLLISION_WALL,
    COLLISION_P1,
    COLLISION_P2
} CollisionType;

static double min_time(double a, double b)
{
    return (a < b) ? a : b;
}

static void print_time(const char *name, double t)
{
    if (t == DBL_MAX || isinf(t)) {
        printf("%s=INF ", name);
    } else {
        printf("%s=%.6f ", name, t);
    }
}

static int puck_y_is_inside_goal_opening(double y)
{
    return y >= GOAL_TOP && y <= GOAL_BOTTOM;
}

static void clamp_puck_to_arena(GameObject *puck)
{
    int in_goal_y = puck_y_is_inside_goal_opening(puck->pos.y);

    /*
     * If puck is inside the goal opening and moving outward,
     * allow it to pass beyond the left/right playable edge.
     * Score detection will reset it later.
     */
    int exiting_left_goal =
        in_goal_y &&
        puck->vel.x < 0.0 &&
        puck->pos.x <= PLAY_LEFT + puck->radius;

    int exiting_right_goal =
        in_goal_y &&
        puck->vel.x > 0.0 &&
        puck->pos.x >= PLAY_RIGHT - puck->radius;

    if (!exiting_left_goal && !exiting_right_goal) {
        if (puck->pos.x < PLAY_LEFT + puck->radius) {
            puck->pos.x = PLAY_LEFT + puck->radius;
        } else if (puck->pos.x > PLAY_RIGHT - puck->radius) {
            puck->pos.x = PLAY_RIGHT - puck->radius;
        }
    }

    if (puck->pos.y < PLAY_TOP + puck->radius) {
        puck->pos.y = PLAY_TOP + puck->radius;
    } else if (puck->pos.y > PLAY_BOTTOM - puck->radius) {
        puck->pos.y = PLAY_BOTTOM - puck->radius;
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

/*
 * Wall collision with goal opening.
 *
 * This replaces the fake post idea.
 *
 * Return DBL_MAX if:
 *   - no wall collision this frame, or
 *   - puck would cross left/right wall through the goal opening.
 *
 * Return a collision time if:
 *   - puck hits top/bottom wall, or
 *   - puck hits left/right wall outside the goal opening.
 */
static double get_segmented_wall_collision_time(const GameObject *puck)
{
    double t_min = DBL_MAX;
    double t;

    /*
     * Left/right walls.
     * These are solid only outside the goal opening.
     */
    if (puck->vel.x < 0.0) {
        t = (PLAY_LEFT + puck->radius - puck->pos.x) / puck->vel.x;

        if (t >= 0.0) {
            double hit_y = puck->pos.y + puck->vel.y * t;

            if (!puck_y_is_inside_goal_opening(hit_y)) {
                if (t < t_min) t_min = t;
            } else {
                if (debug_physics) {
                    fprintf(stderr,
                            "[wall] left goal opening: not bouncing, hit_y=%.4f\n",
                            hit_y);
                }
            }
        }
    } else if (puck->vel.x > 0.0) {
        t = (PLAY_RIGHT - puck->radius - puck->pos.x) / puck->vel.x;

        if (t >= 0.0) {
            double hit_y = puck->pos.y + puck->vel.y * t;

            if (!puck_y_is_inside_goal_opening(hit_y)) {
                if (t < t_min) t_min = t;
            } else {
                if (debug_physics) {
                    fprintf(stderr,
                            "[wall] right goal opening: not bouncing, hit_y=%.4f\n",
                            hit_y);
                }
            }
        }
    }

    /*
     * Top/bottom walls are always solid.
     */
    if (puck->vel.y < 0.0) {
        t = (PLAY_TOP + puck->radius - puck->pos.y) / puck->vel.y;

        if (t >= 0.0 && t < t_min) {
            t_min = t;
        }
    } else if (puck->vel.y > 0.0) {
        t = (PLAY_BOTTOM - puck->radius - puck->pos.y) / puck->vel.y;

        if (t >= 0.0 && t < t_min) {
            t_min = t;
        }
    }

    return t_min;
}

void simulate_frame(GameObject *puck,
                    GameObject *p1,
                    GameObject *p2)
{
    double t_remaining = 1.0;
    int bounce_count = 0;

    while (t_remaining > MIN_FRAME_TIME_REMAINING &&
           bounce_count < MAX_BOUNCES) {

        clamp_puck_to_arena(puck);

        if (debug_physics) {
            fprintf(stderr,
                    "\n[simulate_frame] START step: "
                    "puck pos=(%.4f, %.4f) vel=(%.4f, %.4f) "
                    "t_remaining=%.6f bounce_count=%d\n",
                    puck->pos.x, puck->pos.y,
                    puck->vel.x, puck->vel.y,
                    t_remaining, bounce_count);
        }

        double t_wall = get_segmented_wall_collision_time(puck);
        double t_p1   = getPaddleCollisionTime(puck, p1);
        double t_p2   = getPaddleCollisionTime(puck, p2);

        if (debug_physics) {
            printf("[simulate_frame] times: ");
            print_time("t_wall", t_wall);
            print_time("t_p1", t_p1);
            print_time("t_p2", t_p2);
            printf("\n");
        }

        double t_c = min_time(t_wall, min_time(t_p1, t_p2));

        CollisionType collision_type = COLLISION_NONE;

        if (t_c == t_wall) {
            collision_type = COLLISION_WALL;
        } else if (t_c == t_p1) {
            collision_type = COLLISION_P1;
        } else if (t_c == t_p2) {
            collision_type = COLLISION_P2;
        }

        if (t_c < MIN_COLLISION_TIME) {
            t_c = MIN_COLLISION_TIME;
        }

        if (debug_physics) {
            fprintf(stderr, "[simulate_frame] chosen t_c=%.6f\n", t_c);
        }

        if (t_c >= t_remaining || t_c == DBL_MAX || isinf(t_c)) {
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
            puck->pos.x += puck->vel.x * t_c;
            puck->pos.y += puck->vel.y * t_c;

            if (collision_type == COLLISION_P1) {
                applyPaddleCollision(puck, p1, PADDLE_RESTITUTION);

                if (debug_physics) {
                    fprintf(stderr,
                            "[simulate_frame] COLLISION with P1 at t=%.6f\n",
                            t_c);
                }
            } else if (collision_type == COLLISION_P2) {
                applyPaddleCollision(puck, p2, PADDLE_RESTITUTION);

                if (debug_physics) {
                    fprintf(stderr,
                            "[simulate_frame] COLLISION with P2 at t=%.6f\n",
                            t_c);
                }
            } else if (collision_type == COLLISION_WALL) {
                applyWallBounce(puck, WALL_RESTITUTION);

                if (debug_physics) {
                    fprintf(stderr,
                            "[simulate_frame] COLLISION with WALL at t=%.6f\n",
                            t_c);
                }
            }

            clamp_object_speed(puck, MAX_PUCK_SPEED);

            if (debug_physics) {
                double speed = sqrt(puck->vel.x * puck->vel.x +
                                    puck->vel.y * puck->vel.y);

                fprintf(stderr,
                        "[simulate_frame] after collision response: "
                        "puck pos=(%.4f, %.4f) vel=(%.4f, %.4f) speed=%.4f\n",
                        puck->pos.x, puck->pos.y,
                        puck->vel.x, puck->vel.y,
                        speed);
            }

            t_remaining -= t_c;
            bounce_count++;
        }
    }

    if (bounce_count >= MAX_BOUNCES && debug_physics) {
        fprintf(stderr,
                "simulate_frame: MAX_BOUNCES reached, "
                "puck pos=(%.2f, %.2f) vel=(%.2f, %.2f)\n",
                puck->pos.x, puck->pos.y,
                puck->vel.x, puck->vel.y);
    }
}