#ifndef GAME_CONFIG_H
#define GAME_CONFIG_H

// Hardware-matching object sizes
#define PADDLE_RADIUS 30.0
#define PUCK_RADIUS   20.0
#define POST_RADIUS   10.0

// Hardware rink geometry
#define WALL_LEFT    10.0
#define WALL_RIGHT   630.0
#define WALL_TOP     10.0
#define WALL_BOTTOM  470.0

// Paddle movement bounds
#define P1_X_MIN 24.0
#define P1_X_MAX 320.0
#define P2_X_MIN 320.0
#define P2_X_MAX 615.0
#define Y_MIN    24.0
#define Y_MAX    455.0

// Gameplay tuning
#define MOUSE_SENSITIVITY 1.0
#define MAX_PADDLE_SPEED  40.0
#define MAX_PUCK_SPEED    80.0
#define RESTITUTION       0.7

// Simulation tuning
#define MAX_BOUNCES 3
#define MIN_FRAME_TIME_REMAINING 0.001
#define MIN_COLLISION_TIME 1e-6

#endif