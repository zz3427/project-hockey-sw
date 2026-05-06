#include "game_score.h"
#include "game_config.h"

GoalResult check_goal(const GameObject *puck)
{
    int in_goal_y =
        puck->pos.y >= GOAL_TOP &&
        puck->pos.y <= GOAL_BOTTOM;

    if (!in_goal_y) {
        return GOAL_NONE;
    }

    if (puck->pos.x <= PUCK_X_MIN) {
        return GOAL_P2_SCORED;
    }

    if (puck->pos.x >= PUCK_X_MAX) {
        return GOAL_P1_SCORED;
    }

    return GOAL_NONE;
}

void reset_after_goal(GameObject *puck,
                      GameObject *p1,
                      GameObject *p2)
{
    puck->pos.x = PUCK_START_X;
    puck->pos.y = PUCK_START_Y;
    puck->vel.x = 0.0;
    puck->vel.y = 0.0;
    puck->radius = PUCK_RADIUS;

    p1->pos.x = P1_START_X;
    p1->pos.y = P1_START_Y;
    p1->vel.x = 0.0;
    p1->vel.y = 0.0;
    p1->radius = PADDLE_RADIUS;

    p2->pos.x = P2_START_X;
    p2->pos.y = P2_START_Y;
    p2->vel.x = 0.0;
    p2->vel.y = 0.0;
    p2->radius = PADDLE_RADIUS;
}

void handle_score_update(GameObject *puck,
                         GameObject *p1,
                         GameObject *p2,
                         int *p1_score,
                         int *p2_score)
{
    GoalResult goal = check_goal(puck);

    if (goal == GOAL_P1_SCORED) {
        if (*p1_score < MAX_SCORE) {
            (*p1_score)++;
        }

        reset_after_goal(puck, p1, p2);
    } else if (goal == GOAL_P2_SCORED) {
        if (*p2_score < MAX_SCORE) {
            (*p2_score)++;
        }

        reset_after_goal(puck, p1, p2);
    }
}