#ifndef PHYSICS_ENGINE_H
#define PHYSICS_ENGINE_H

#include <math.h>

typedef struct {
    double x;
    double y;
} Vec2D;

typedef struct {
    Vec2D pos;
    Vec2D vel;
    double radius;
} GameObject;

double getWallCollisionTime(const GameObject *puck);
double getPaddleCollisionTime(const GameObject *puck, const GameObject *paddle);
void applyPaddleCollision(GameObject *puck, const GameObject *paddle, double restitution);
void applyWallBounce(GameObject *puck);

#endif