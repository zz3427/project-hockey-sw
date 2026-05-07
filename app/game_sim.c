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

typedef enum {
    WALL_HIT_NONE = 0,
    WALL_HIT_VERTICAL,
    WALL_HIT_HORIZONTAL
} WallHitType;

typedef struct {
    double t;
    WallHitType type;

    /*
     * For vertical wall hit:   center_bound is the x coordinate
     * For horizontal wall hit: center_bound is the y coordinate
     */
    double center_bound;
} WallHit;

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

static void consider_wall_hit(WallHit *best,
                              double t,
                              WallHitType type,
                              double center_bound)
{
    const double EPS = 1e-9;

    if (t < -EPS) {
        return;
    }

    if (t < 0.0) {
        t = 0.0;
    }

    if (t < best->t) {
        best->t = t;
        best->type = type;
        best->center_bound = center_bound;
    }
}

/*
 * Continuous collision detection against the rink walls.
 *
 * This is still CCD:
 * every collision time is calculated as distance / velocity.
 *
 * Difference from the old version:
 * - top/bottom rink walls are horizontal surfaces
 * - left/right rink walls are vertical surfaces, but open at the goal mouth
 * - the top/bottom edges of the goal mouth are horizontal "goal lips"
 *
 * That fixes the bug where the puck bounced off an invisible vertical wall
 * near the goal opening.
 */
static WallHit get_rink_wall_collision(const GameObject *puck)
{
    WallHit best;
    best.t = DBL_MAX;
    best.type = WALL_HIT_NONE;
    best.center_bound = 0.0;

    double t;

    double left_center_bound   = PLAY_LEFT + puck->radius;
    double right_center_bound  = PLAY_RIGHT - puck->radius;
    double top_center_bound    = PLAY_TOP + puck->radius;
    double bottom_center_bound = PLAY_BOTTOM - puck->radius;

    /*
     * Left/right vertical wall segments.
     *
     * Important:
     * If the puck center reaches the left/right wall inside the visual goal
     * opening, do NOT treat it as a vertical wall. The opening is empty.
     *
     * If the puck is near the top/bottom edge of the goal, the horizontal
     * goal lip checks below can catch that and flip vy instead.
     */
    if (puck->vel.x < 0.0) {
        t = (left_center_bound - puck->pos.x) / puck->vel.x;

        if (t >= -1e-9) {
            double hit_y = puck->pos.y + puck->vel.y * t;

            if (!puck_center_inside_goal_opening(hit_y)) {
                consider_wall_hit(&best, t, WALL_HIT_VERTICAL,
                                  left_center_bound);
            } else if (debug_physics) {
                fprintf(stderr,
                        "[wall] left goal opening: no vertical bounce, hit_y=%.4f\n",
                        hit_y);
            }
        }
    } else if (puck->vel.x > 0.0) {
        t = (right_center_bound - puck->pos.x) / puck->vel.x;

        if (t >= -1e-9) {
            double hit_y = puck->pos.y + puck->vel.y * t;

            if (!puck_center_inside_goal_opening(hit_y)) {
                consider_wall_hit(&best, t, WALL_HIT_VERTICAL,
                                  right_center_bound);
            } else if (debug_physics) {
                fprintf(stderr,
                        "[wall] right goal opening: no vertical bounce, hit_y=%.4f\n",
                        hit_y);
            }
        }
    }

    /*
     * Normal top/bottom rink walls.
     */
    if (puck->vel.y < 0.0) {
        t = (top_center_bound - puck->pos.y) / puck->vel.y;
        consider_wall_hit(&best, t, WALL_HIT_HORIZONTAL, top_center_bound);
    } else if (puck->vel.y > 0.0) {
        t = (bottom_center_bound - puck->pos.y) / puck->vel.y;
        consider_wall_hit(&best, t, WALL_HIT_HORIZONTAL, bottom_center_bound);
    }

    /*
     * Goal mouth horizontal lips.
     *
     * These are the missing pieces from the previous implementation.
     *
     * If the puck enters the goal at an angle and its edge hits the top or
     * bottom edge of the goal opening, it should bounce vertically, not bounce
     * off an invisible vertical wall.
     */

    // Top goal lip: puck is inside/near goal mouth and moving upward.
    if (puck->vel.y < 0.0) {
        double top_goal_lip_center_bound = GOAL_TOP + puck->radius;
        t = (top_goal_lip_center_bound - puck->pos.y) / puck->vel.y;

        if (t >= -1e-9) {
            double hit_x = puck->pos.x + puck->vel.x * t;

            int near_left_goal =
                hit_x <= left_center_bound &&
                hit_x >= LEFT_GOAL_SCORE_X;

            int near_right_goal =
                hit_x >= right_center_bound &&
                hit_x <= RIGHT_GOAL_SCORE_X;

            if (near_left_goal || near_right_goal) {
                consider_wall_hit(&best, t, WALL_HIT_HORIZONTAL,
                                  top_goal_lip_center_bound);

                if (debug_physics) {
                    fprintf(stderr,
                            "[wall] top goal lip candidate: hit_x=%.4f\n",
                            hit_x);
                }
            }
        }
    }

    // Bottom goal lip: puck is inside/near goal mouth and moving downward.
    if (puck->vel.y > 0.0) {
        double bottom_goal_lip_center_bound = GOAL_BOTTOM - puck->radius;
        t = (bottom_goal_lip_center_bound - puck->pos.y) / puck->vel.y;

        if (t >= -1e-9) {
            double hit_x = puck->pos.x + puck->vel.x * t;

            int near_left_goal =
                hit_x <= left_center_bound &&
                hit_x >= LEFT_GOAL_SCORE_X;

            int near_right_goal =
                hit_x >= right_center_bound &&
                hit_x <= RIGHT_GOAL_SCORE_X;

            if (near_left_goal || near_right_goal) {
                consider_wall_hit(&best, t, WALL_HIT_HORIZONTAL,
                                  bottom_goal_lip_center_bound);

                if (debug_physics) {
                    fprintf(stderr,
                            "[wall] bottom goal lip candidate: hit_x=%.4f\n",
                            hit_x);
                }
            }
        }
    }

    return best;
}

static void apply_wall_hit(GameObject *puck, WallHit wall_hit)
{
    const double EPS = 0.001;

    if (wall_hit.type == WALL_HIT_VERTICAL) {
        /*
         * Vertical surface: flip x velocity only.
         */
        puck->pos.x = wall_hit.center_bound;

        if (puck->vel.x < 0.0) {
            puck->pos.x += EPS;
        } else if (puck->vel.x > 0.0) {
            puck->pos.x -= EPS;
        }

        puck->vel.x = -WALL_RESTITUTION * puck->vel.x;
    } else if (wall_hit.type == WALL_HIT_HORIZONTAL) {
        /*
         * Horizontal surface: flip y velocity only.
         */
        puck->pos.y = wall_hit.center_bound;

        if (puck->vel.y < 0.0) {
            puck->pos.y += EPS;
        } else if (puck->vel.y > 0.0) {
            puck->pos.y -= EPS;
        }

        puck->vel.y = -WALL_RESTITUTION * puck->vel.y;
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

        if (debug_physics) {
            fprintf(stderr,
                    "\n[simulate_frame] START step: "
                    "puck pos=(%.4f, %.4f) vel=(%.4f, %.4f) "
                    "t_remaining=%.6f bounce_count=%d\n",
                    puck->pos.x, puck->pos.y,
                    puck->vel.x, puck->vel.y,
                    t_remaining, bounce_count);
        }

        WallHit wall_hit = get_rink_wall_collision(puck);
        double t_wall = wall_hit.t;

        double t_p1 = getPaddleCollisionTime(puck, p1);
        double t_p2 = getPaddleCollisionTime(puck, p2);


        // get goal post time
        GameObject left_top_post = {{LEFT_GOAL_SCORE_X, GOAL_TOP}, {0.0, 0.0}, 0.1};
        GameObject left_bottom_post = {{LEFT_GOAL_SCORE_X, GOAL_BOTTOM}, {0.0, 0.0}, 0.1};
        GameObject right_top_post = {{RIGHT_GOAL_SCORE_X, GOAL_TOP}, {0.0, 0.0}, 0.1};
        GameObject right_bottom_post = {{RIGHT_GOAL_SCORE_X, GOAL_BOTTOM}, {0.0, 0.0}, 0.1};

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

        if (debug_physics) {
            printf("[simulate_frame] times: ");
            print_time("t_wall", t_wall);
            print_time("t_p1", t_p1);
            print_time("t_p2", t_p2);
            printf("\n");
        }

        double t_c = min_time((t_wall, min_time(t_p1, t_p2)), min_time(t_post1, t_post2));

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
                fprintf(stderr, "[collision] puck hit P1 paddle at t=%.6f\n", t_c);
                applyPaddleCollision(puck, p1, PADDLE_RESTITUTION);
            } else if (collision_type == COLLISION_P2) {
                fprintf(stderr, "[collision] puck hit P2 paddle at t=%.6f\n", t_c);
                applyPaddleCollision(puck, p2, PADDLE_RESTITUTION);
            } else if (collision_type == COLLISION_WALL) {
                fprintf(stderr, "[collision] puck hit wall at t=%.6f\n", t_c);
                apply_wall_hit(puck, wall_hit);
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