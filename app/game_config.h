#ifndef GAME_CONFIG_H
#define GAME_CONFIG_H

// Screen geometry
#define SCREEN_WIDTH    640.0
#define SCREEN_HEIGHT   480.0

// Outer rink wall geometry from hardware Verilog
#define WALL_LEFT       10.0
#define WALL_RIGHT      629.0
#define WALL_TOP        10.0
#define WALL_BOTTOM     469.0
#define WALL_WIDTH      4.0

// Inner playable ice area from hardware:
// Verilog uses:
// px >= WL + WW && px <= WR - WW
// py >= WT + WW && py <= WB - WW
#define PLAY_LEFT       (WALL_LEFT + WALL_WIDTH)      
#define PLAY_RIGHT      (WALL_RIGHT - WALL_WIDTH)     
#define PLAY_TOP        (WALL_TOP + WALL_WIDTH)       
#define PLAY_BOTTOM     (WALL_BOTTOM - WALL_WIDTH)    

// Goal opening from hardware Verilog
#define GOAL_TOP        190.0
#define GOAL_BOTTOM     290.0

// Object radii from hardware Verilog
// PADDLE_R2 = 900  -> radius 30
// PUCK_R2   = 400  -> radius 20
#define PADDLE_RADIUS   30.0
#define PUCK_RADIUS     20.0
#define POST_RADIUS     10.0

// Starting positions
#define P1_START_X      100.0
#define P1_START_Y      240.0

#define P2_START_X      540.0
#define P2_START_Y      240.0

#define PUCK_START_X    320.0
#define PUCK_START_Y    240.0

#define PUCK_START_X_P1 220
#define PUCK_START_X_P2 420

// Paddle movement bounds
// These keep the whole paddle inside the visible ice, not inside the border.
#define P1_X_MIN        (PLAY_LEFT + PADDLE_RADIUS)      
#define P1_X_MAX        (320.0 - PADDLE_RADIUS)          

#define P2_X_MIN        (320.0 + PADDLE_RADIUS)          
#define P2_X_MAX        (PLAY_RIGHT - PADDLE_RADIUS)     

#define PADDLE_Y_MIN    (PLAY_TOP + PADDLE_RADIUS)       
#define PADDLE_Y_MAX    (PLAY_BOTTOM - PADDLE_RADIUS)    

// Puck movement bounds
// These keep the whole puck inside the visible ice.
#define PUCK_X_MIN      (PLAY_LEFT + PUCK_RADIUS)        
#define PUCK_X_MAX      (PLAY_RIGHT - PUCK_RADIUS)       
#define PUCK_Y_MIN      (PLAY_TOP + PUCK_RADIUS)         
#define PUCK_Y_MAX      (PLAY_BOTTOM - PUCK_RADIUS)      

// Gameplay tuning
#define MOUSE_SENSITIVITY 0.5
#define MAX_PADDLE_SPEED  20.0
#define MAX_PUCK_SPEED    40.0

#define RESTITUTION       0.6
#define WALL_RESTITUTION  0.85
#define PADDLE_RESTITUTION 0.7
#define POST_RESTITUTION  0.7

// Goal detection threshold.
// Score only after puck has visibly entered the goal/off-screen area.
// Keep this based on outer wall, not PLAY_LEFT/PLAY_RIGHT.
#define LEFT_GOAL_SCORE_X   (WALL_LEFT)    // 
#define RIGHT_GOAL_SCORE_X  (WALL_RIGHT)   // 

// Simulation tuning
#define MAX_BOUNCES 3
#define MIN_FRAME_TIME_REMAINING 0.001
#define MIN_COLLISION_TIME 1e-6

// Score limits
#define MAX_SCORE 7

#endif