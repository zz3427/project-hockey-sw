#ifndef GAME_SCORE_H
#define GAME_SCORE_H

#include "physics_engine.h"

typedef enum {
    GOAL_NONE = 0,
    GOAL_P1_SCORED,
    GOAL_P2_SCORED
} GoalResult;

GoalResult check_goal(const GameObject *puck);

void reset_after_goal(GameObject *puck,
                      GameObject *p1,
                      GameObject *p2);

int handle_score_update(GameObject *puck,
                         GameObject *p1,
                         GameObject *p2,
                         int *p1_score,
                         int *p2_score);

#endif