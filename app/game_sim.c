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
    COLLISION_P2,
    COLLISION_LEFT_TOP_POST,    
    COLLISION_LEFT_BOTTOM_POST, 
    COLLISION_RIGHT_TOP_POST,   
    COLLISION_RIGHT_BOTTOM_POST
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

/*
 * Center of puck is inside the visual goal opening.
 * This is used to decide whether the vertical goal wall is open.
 */
static int puck_center_inside_goal_opening(double y)
{
    return y >= GOAL_TOP && y <= GOAL_BOTTOM;
}

/*
 * Entire puck fits inside the goal opening.
 * This is useful for avoiding visual clipping, but we should NOT use this
 * to decide whether a vertical wall exists. The vertical wall opening is
 * based on the puck center crossing the opening; the horizontal goal lips
 * handle the top/bottom edge cases.
 */
static int puck_fully_inside_goal_opening(const GameObject *puck, double y)
{
    return (y - puck->radius >= GOAL_TOP) &&
           (y + puck->radius <= GOAL_BOTTOM);
}

static void clamp_puck_to_arena(GameObject *puck)
{
    int in_goal_y = puck_center_inside_goal_opening(puck->pos.y);

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


void simulate_frame(GameObject *puck,
                    GameObject *p1,
                    GameObject *p2)
{
    double t_remaining = 1.0;
    int bounce_count = 0;

    while (t_remaining > MIN_FRAME_TIME_REMAINING &&
           bounce_count < MAX_BOUNCES) {

        clamp_puck_to_arena(puck);
        
        double t_wall = get_wall_collision_time(puck);

        double t_p1 = getPaddleCollisionTime(puck, p1);
        double t_p2 = getPaddleCollisionTime(puck, p2);


        // get goal post time
        GameObject left_top_post = {{PLAY_LEFT, GOAL_TOP}, {0.0, 0.0}, 0.1};
        GameObject left_bottom_post = {{PLAY_LEFT, GOAL_BOTTOM}, {0.0, 0.0}, 0.1};
        GameObject right_top_post = {{PLAY_RIGHT, GOAL_TOP}, {0.0, 0.0}, 0.1};
        GameObject right_bottom_post = {{PLAY_RIGHT, GOAL_BOTTOM}, {0.0, 0.0}, 0.1};

        double t_post1, t_post2;

        if (puck->pos.x < (PLAY_LEFT + PLAY_RIGHT) / 2) {
            // near left goal, check collision with left goal posts
            t_post1 = getPaddleCollisionTime(puck, &left_top_post);
            t_post2 = getPaddleCollisionTime(puck, &left_bottom_post);
        } else {
            // near right goal, check collision with right goal posts 
            t_post1 = getPaddleCollisionTime(puck, &right_top_post);
            t_post2 = getPaddleCollisionTime(puck, &right_bottom_post);
        }

        double t_c = min_time(min_time(t_wall, min_time(t_p1, t_p2)), min_time(t_post1, t_post2));

        CollisionType collision_type = COLLISION_NONE;

        if (t_c == t_wall) {
            collision_type = COLLISION_WALL;
        } else if (t_c == t_p1) {
            collision_type = COLLISION_P1;
        } else if (t_c == t_p2) {
            collision_type = COLLISION_P2;
        } else if (t_c == t_post1) {
            if (puck->pos.x < (PLAY_LEFT + PLAY_RIGHT) / 2) {
                collision_type = COLLISION_LEFT_TOP_POST;
            } else {
                collision_type = COLLISION_RIGHT_TOP_POST;
            }
        } else if (t_c == t_post2) {
            if (puck->pos.x < (PLAY_LEFT + PLAY_RIGHT) / 2) {
                collision_type = COLLISION_LEFT_BOTTOM_POST;
            } else {
                collision_type = COLLISION_RIGHT_BOTTOM_POST;
            }
        } else {
            collision_type = COLLISION_NONE;
        }

        if (t_c < MIN_COLLISION_TIME) {
            t_c = MIN_COLLISION_TIME;
        }

        if (t_c >= t_remaining || t_c == DBL_MAX || isinf(t_c)) {
            puck->pos.x += puck->vel.x * t_remaining;
            puck->pos.y += puck->vel.y * t_remaining;

            t_remaining = 0.0;
        } else {
            puck->pos.x += puck->vel.x * t_c;
            puck->pos.y += puck->vel.y * t_c;

            if (collision_type == COLLISION_P1) {
                fprintf(stderr, "[collision] puck hit P1 paddle at t=%.6f\n", t_c);
                applyPaddleCollision(puck, p1, PADDLE_RESTITUTION);
            } else if (collision_type == COLLISION_P2) {
                fprintf(stderr, "[collision] puck hit P2 paddle at t=%.6f\n", t_c);
                applyPaddleCollision(puck, p2, PADDLE_RESTITUTION);
            } else if (collision_type == COLLISION_WALL) {
                fprintf(stderr, "[collision] puck hit wall at t=%.6f\n", t_c);
                applyWallBounce(puck, WALL_RESTITUTION);
            }  else if (collision_type == COLLISION_LEFT_BOTTOM_POST) {
                fprintf(stderr, "[collision] puck hit left bottom post at t=%.6f\n", t_c);
                applyPaddleCollision(puck, &left_bottom_post, POST_RESTITUTION);
            } else if (collision_type == COLLISION_LEFT_TOP_POST) {
                fprintf(stderr, "[collision] puck hit left top post at t=%.6f\n", t_c);
                applyPaddleCollision(puck, &left_top_post, POST_RESTITUTION);
            } else if (collision_type == COLLISION_RIGHT_BOTTOM_POST) {
                fprintf(stderr, "[collision] puck hit right bottom post at t=%.6f\n", t_c);
                applyPaddleCollision(puck, &right_bottom_post, POST_RESTITUTION);
            } else if (collision_type == COLLISION_RIGHT_TOP_POST) {
                fprintf(stderr, "[collision] puck hit right top post at t=%.6f\n", t_c);
                applyPaddleCollision(puck, &right_top_post, POST_RESTITUTION);
            }

            clamp_object_speed(puck, MAX_PUCK_SPEED);

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