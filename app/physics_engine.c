#include "physics_engine.h"
#include <float.h> // For DBL_MAX (infinity)

// ---------------------------------------------------------
// Section 3.2: Wall Collision Time
// ---------------------------------------------------------
double getWallCollisionTime(const GameObject *puck) {
    double t_min = DBL_MAX;
    double t;

    // Already outside left and still moving left
    if (puck->pos.x < 10.0 + puck->radius && puck->vel.x < 0) {
        return 0.0;
    }
    // Already outside right and still moving right
    if (puck->pos.x > 630.0 - puck->radius && puck->vel.x > 0) {
        return 0.0;
    }
    // Already outside top and still moving up
    if (puck->pos.y < 10.0 + puck->radius && puck->vel.y < 0) {
        return 0.0;
    }
    // Already outside bottom and still moving down
    if (puck->pos.y > 470.0 - puck->radius && puck->vel.y > 0) {
        return 0.0;
    }

    // Check Left Wall (X = 10)
    if (puck->vel.x < 0) {
        t = (10.0 + puck->radius - puck->pos.x) / puck->vel.x;
        if (t >= 0 && t < t_min) t_min = t;
    }
    // Check Right Wall (X = 630)
    else if (puck->vel.x > 0) {
        t = (630.0 - puck->radius - puck->pos.x) / puck->vel.x;
        if (t >= 0 && t < t_min) t_min = t;
    }

    // Check Top Wall (Y = 10)
    if (puck->vel.y < 0) {
        t = (10.0 + puck->radius - puck->pos.y) / puck->vel.y;
        if (t >= 0 && t < t_min) t_min = t;
    }
    // Check Bottom Wall (Y = 470)
    else if (puck->vel.y > 0) {
        t = (470.0 - puck->radius - puck->pos.y) / puck->vel.y;
        if (t >= 0 && t < t_min) t_min = t;
    }

    return t_min;
}

// ---------------------------------------------------------
// Section 3.3: Paddle Collision Time (The Quadratic Solver)
// ---------------------------------------------------------
double getPaddleCollisionTime(const GameObject *puck, const GameObject *paddle) {
    // Relative position (Delta P)
    double dp_x = puck->pos.x - paddle->pos.x;
    double dp_y = puck->pos.y - paddle->pos.y;
    
    // Relative velocity (Delta V)
    double dv_x = puck->vel.x - paddle->vel.x;
    double dv_y = puck->vel.y - paddle->vel.y;
    
    double r_sum = puck->radius + paddle->radius;

    // A, B, and C for the quadratic equation At^2 + Bt + C = 0
    double A = (dv_x * dv_x) + (dv_y * dv_y);
    double B = 2.0 * ((dp_x * dv_x) + (dp_y * dv_y));
    double C = (dp_x * dp_x) + (dp_y * dp_y) - (r_sum * r_sum);

    // If relative velocity is near 0 (A < EPS), they will never collide
    if (A < 1e-10) return DBL_MAX;

    // Check if they are currently overlapping and moving towards each other
    double closing = (dp_x * dv_x) + (dp_y * dv_y);

    // If objects are already overlapping (C <= 0), collision is happening right now
    if (C <= 0.0){
        if (closing < 0) {
            // They are moving towards each other while overlapping, treat as immediate collision
            return 0.0;
        } else {
            // They are moving apart while overlapping, treat as no future collision
            return DBL_MAX;
        }
    }

    // Calculate discriminant to check if paths intersect
    double discriminant = (B * B) - (4.0 * A * C);

    // If discriminant is negative, the paths never cross
    if (discriminant < 0.0) return DBL_MAX;

    // We only want the first intersection (- sqrt), not the exit (+ sqrt)
    double t_c = (-B - sqrt(discriminant)) / (2.0 * A);

    // If t_c is negative, the collision happened in the past
    if (t_c < 0.0) return DBL_MAX;

    return t_c;
}

// ---------------------------------------------------------
// Section 3.4: 2D Post-Collision Velocities
// ---------------------------------------------------------
void applyPaddleCollision(GameObject *puck, const GameObject *paddle, double restitution) {
    // 1. Define the Normal Vector (Line of Action)
    double Nx = puck->pos.x - paddle->pos.x;
    double Ny = puck->pos.y - paddle->pos.y;
    
    // Calculate magnitude to normalize the vector
    double distance = sqrt((Nx * Nx) + (Ny * Ny));
    
    // Prevent division by zero just in case of weird floating point overlap
    if (distance == 0.0) return;

    double nx = Nx / distance;
    double ny = Ny / distance;

    // 2. Define the Tangent Vector (Perpendicular to Normal)
    double tx = -ny;
    double ty = nx;

    // 3. Project Velocities onto Normal and Tangent Axes (Dot Products)
    double U_puck_n = (puck->vel.x * nx) + (puck->vel.y * ny);
    double U_puck_t = (puck->vel.x * tx) + (puck->vel.y * ty);
    
    double U_paddle_n = (paddle->vel.x * nx) + (paddle->vel.y * ny);

    // If the puck is already moving away from the paddle along the normal, 
    // do not recalculate (prevents objects getting "sticky" and trapping each other)
    if (U_puck_n >= U_paddle_n) return;

    // 4. Apply 1D Infinite Mass Collision Formula
    double V_puck_n_new = (1.0 + restitution) * U_paddle_n - (restitution * U_puck_n);
    
    // Tangent velocity is unchanged (frictionless)
    double V_puck_t_new = U_puck_t;

    // 5. Convert Back to Global X and Y Coordinates
    puck->vel.x = (V_puck_n_new * nx) + (V_puck_t_new * tx);
    puck->vel.y = (V_puck_n_new * ny) + (V_puck_t_new * ty);
}


//Gerald's version
void applyWallBounce(GameObject *puck, double restitution) {
    const double EPS = 0.001;

    double left_bound   = 10.0 + puck->radius;
    double right_bound  = 630.0 - puck->radius;
    double top_bound    = 10.0 + puck->radius;
    double bottom_bound = 470.0 - puck->radius;

    // Left wall
    if (puck->pos.x <= left_bound + EPS && puck->vel.x < 0) {
        puck->pos.x = left_bound + EPS;
        puck->vel.x = -puck->vel.x * restitution;
    }

    // Right wall
    else if (puck->pos.x >= right_bound - EPS && puck->vel.x > 0) {
        puck->pos.x = right_bound - EPS;
        puck->vel.x = -puck->vel.x * restitution;
    }

    // Top wall
    if (puck->pos.y <= top_bound + EPS && puck->vel.y < 0) {
        puck->pos.y = top_bound + EPS;
        puck->vel.y = -puck->vel.y * restitution;
    }

    // Bottom wall
    else if (puck->pos.y >= bottom_bound - EPS && puck->vel.y > 0) {
        puck->pos.y = bottom_bound - EPS;
        puck->vel.y = -puck->vel.y * restitution;
    }
}

// // ---------------------------------------------------------
// // Section 3.2.1: Flat Wall Bounce Resolution
// // ---------------------------------------------------------
// void applyWallBounce(GameObject *puck) {
//     // Floating point math can be slightly imprecise. 
//     // We use a tiny epsilon (0.001) to safely check boundaries.
    
//     // Check Vertical Walls (Flip X)
//     if (puck->pos.x <= 10.0 + puck->radius + 0.001 || 
//         puck->pos.x >= 630.0 - puck->radius - 0.001) {
        
//         // Only flip if it's actually heading into the wall (prevents sticking)
//         if ((puck->pos.x < 320.0 && puck->vel.x < 0) || 
//             (puck->pos.x > 320.0 && puck->vel.x > 0)) {
//             puck->vel.x = -puck->vel.x;
//         }
//     }

//     // Check Horizontal Walls (Flip Y)
//     if (puck->pos.y <= 10.0 + puck->radius + 0.001 || 
//         puck->pos.y >= 470.0 - puck->radius - 0.001) {
        
//         if ((puck->pos.y < 240.0 && puck->vel.y < 0) || 
//             (puck->pos.y > 240.0 && puck->vel.y > 0)) {
//             puck->vel.y = -puck->vel.y;
//         }
//     }
// }