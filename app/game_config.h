#ifndef GAME_CONFIG_H
#define GAME_CONFIG_H

// Screen/rink geometry from hardware
#define SCREEN_WIDTH   640.0
#define SCREEN_HEIGHT  480.0
#define WALL_LEFT      10.0
#define WALL_RIGHT     629.0
#define WALL_TOP       10.0
#define WALL_BOTTOM    469.0
#define WALL_WIDTH     4.0

// Goal opening from hardware Verilog
#define GOAL_TOP       190.0
#define GOAL_BOTTOM    290.0

// Object radii from hardware Verilog
#define PADDLE_RADIUS 30.0
#define PUCK_RADIUS   20.0
#define POST_RADIUS   10.0

// Starting positions
#define P1_START_X     100.0
#define P1_START_Y     240.0
#define P2_START_X     540.0
#define P2_START_Y     240.0
#define PUCK_START_X   320.0
#define PUCK_START_Y   240.0

// Paddle movement bounds
#define P1_X_MIN       (WALL_LEFT + PADDLE_RADIUS)
#define P1_X_MAX       (320.0 - PADDLE_RADIUS)

#define P2_X_MIN       (320.0 + PADDLE_RADIUS)
#define P2_X_MAX       (WALL_RIGHT - PADDLE_RADIUS)

#define PADDLE_Y_MIN   (WALL_TOP + PADDLE_RADIUS)
#define PADDLE_Y_MAX   (WALL_BOTTOM - PADDLE_RADIUS)

// Puck movement bounds
#define PUCK_X_MIN     (WALL_LEFT + PUCK_RADIUS)
#define PUCK_X_MAX     (WALL_RIGHT - PUCK_RADIUS)
#define PUCK_Y_MIN     (WALL_TOP + PUCK_RADIUS)
#define PUCK_Y_MAX     (WALL_BOTTOM - PUCK_RADIUS)


// Gameplay tuning
#define MOUSE_SENSITIVITY 1.0
#define MAX_PADDLE_SPEED  40.0
#define MAX_PUCK_SPEED    80.0
#define RESTITUTION       0.7

// Goal detection threshold.
// Score only after puck has visibly entered the goal/off-screen area.
#define LEFT_GOAL_SCORE_X   (WALL_LEFT - PUCK_RADIUS)
#define RIGHT_GOAL_SCORE_X  (WALL_RIGHT + PUCK_RADIUS)

// Simulation tuning
#define MAX_BOUNCES 3
#define MIN_FRAME_TIME_REMAINING 0.001
#define MIN_COLLISION_TIME 1e-6

// Score limits
#define MAX_SCORE 7

#endif