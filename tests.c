#include <stdio.h>
#include <math.h>
#include <float.h>
#include "physics_engine.h"

// Helper function for clean test assertions
void assert_float(const char* test_name, double expected, double actual) {
    // Epsilon for floating point comparisons
    if (expected == DBL_MAX && actual == DBL_MAX) {
        printf("[PASS] %-40s | Expected: INF  | Actual: INF\n", test_name);
    } else if (fabs(expected - actual) < 0.001) {
        printf("[PASS] %-40s | Expected: %5.2f | Actual: %5.2f\n", test_name, expected, actual);
    } else {
        printf("[FAIL] %-40s | Expected: %5.2f | Actual: %5.2f\n", test_name, expected, actual);
    }
}

// Helper for vector assertions
void assert_vec(const char* test_name, double exp_x, double exp_y, double act_x, double act_y) {
    if (fabs(exp_x - act_x) < 0.001 && fabs(exp_y - act_y) < 0.001) {
        printf("[PASS] %-40s | Expected: (%.2f, %.2f) | Actual: (%.2f, %.2f)\n", 
               test_name, exp_x, exp_y, act_x, act_y);
    } else {
        printf("[FAIL] %-40s | Expected: (%.2f, %.2f) | Actual: (%.2f, %.2f)\n", 
               test_name, exp_x, exp_y, act_x, act_y);
    }
}

// ---------------------------------------------------------
// TEST SUITE 1: getWallCollisionTime
// ---------------------------------------------------------
void testWallCollisionTime() {
    printf("\n--- TEST SUITE 1: Wall Collision Time ---\n");
    
    // 1. Standard Left Wall
    GameObject p1 = {{50.0, 240.0}, {-20.0, 0.0}, 10.0};
    assert_float("Direct hit on left wall (X=10)", 1.50, getWallCollisionTime(&p1));

    // 2. Standard Right Wall
    GameObject p2 = {{580.0, 240.0}, {20.0, 0.0}, 10.0};
    assert_float("Direct hit on right wall (X=630)", 2.00, getWallCollisionTime(&p2));

    // 3. Standard Top Wall
    GameObject p3 = {{320.0, 40.0}, {0.0, -10.0}, 10.0};
    assert_float("Direct hit on top wall (Y=10)", 2.00, getWallCollisionTime(&p3));

    // 4. Moving AWAY from the wall (Should return infinity/DBL_MAX)
    GameObject p4 = {{50.0, 240.0}, {20.0, 0.0}, 10.0};
    assert_float("Moving away from nearest wall", 28.50, getWallCollisionTime(&p4));
    
    // 5. Diagonal approach to corner
    GameObject p5 = {{30.0, 30.0}, {-10.0, -10.0}, 10.0};
    // Reaches X=20 and Y=20 at exactly t=1.0
    assert_float("Diagonal corner approach", 1.00, getWallCollisionTime(&p5));
}

// ---------------------------------------------------------
// TEST SUITE 2: applyWallBounce
// ---------------------------------------------------------
void testWallBounce() {
    printf("\n--- TEST SUITE 2: Flat Wall Bounce ---\n");
    
    // 1. Hitting the Right Wall
    GameObject p1 = {{620.0, 240.0}, {15.0, 5.0}, 10.0};
    applyWallBounce(&p1);
    assert_vec("Right wall bounce (X flips)", -15.0, 5.0, p1.vel.x, p1.vel.y);

    // 2. Hitting the Bottom Wall
    GameObject p2 = {{320.0, 460.0}, {10.0, 25.0}, 10.0};
    applyWallBounce(&p2);
    assert_vec("Bottom wall bounce (Y flips)", 10.0, -25.0, p2.vel.x, p2.vel.y);

    // 3. The "Sticky Wall" Bug Test
    // Puck is technically touching the wall boundary, but is already moving away from it.
    // The engine should NOT flip the velocity, otherwise it traps the puck inside the wall.
    GameObject p3 = {{20.0, 240.0}, {10.0, 0.0}, 10.0}; 
    applyWallBounce(&p3);
    assert_vec("Anti-stick: Moving away from wall", 10.0, 0.0, p3.vel.x, p3.vel.y);
}

// ---------------------------------------------------------
// TEST SUITE 3: getPaddleCollisionTime
// ---------------------------------------------------------
void testPaddleCollisionTime() {
    printf("\n--- TEST SUITE 3: Paddle Collision Time ---\n");
    
    GameObject paddle = {{100.0, 240.0}, {0.0, 0.0}, 20.0};
    
    // 1. Direct Head-On Collision
    GameObject p1 = {{150.0, 240.0}, {-10.0, 0.0}, 10.0};
    assert_float("Direct head-on collision", 2.00, getPaddleCollisionTime(&p1, &paddle));

    // 2. Total Miss (Parallel Trajectory)
    // Puck is at Y=280. Combined radius is 30. Puck passes safely 10 pixels below.
    GameObject p2 = {{150.0, 280.0}, {-10.0, 0.0}, 10.0};
    assert_float("Parallel miss trajectory", DBL_MAX, getPaddleCollisionTime(&p2, &paddle));

    // 3. Moving Away
    GameObject p3 = {{150.0, 240.0}, {10.0, 0.0}, 10.0};
    assert_float("Moving away from paddle", DBL_MAX, getPaddleCollisionTime(&p3, &paddle));

    // 4. Glancing Blow (Hitting the very edge of the radius)
    // Puck approaches at Y=265. Combined radius is 30. It will clip the edge.
    GameObject p4 = {{150.0, 265.0}, {-10.0, 0.0}, 10.0};
    double t4 = getPaddleCollisionTime(&p4, &paddle);
    // Calculated by hand: dp_y=25. r_sum=30. Needs dp_x = sqrt(900 - 625) = 16.583
    // Travel dist = 50 - 16.583 = 33.416. t = 3.34
    assert_float("Glancing blow trajectory", 3.34, t4);
}

// ---------------------------------------------------------
// TEST SUITE 4: applyPaddleCollision
// ---------------------------------------------------------
void testPaddleBounce() {
    printf("\n--- TEST SUITE 4: 2D Paddle Vector Projections ---\n");
    
    // 1. Dead center 1D bounce
    GameObject pad1 = {{100.0, 240.0}, {0.0, 0.0}, 20.0};
    GameObject p1 = {{130.0, 240.0}, {-20.0, 0.0}, 10.0};
    applyPaddleCollision(&p1, &pad1, 1.0);
    assert_vec("Dead center 1D bounce", 20.0, 0.0, p1.vel.x, p1.vel.y);

    // 2. 45-Degree Angle Strike
    GameObject pad2 = {{100.0, 100.0}, {0.0, 0.0}, 20.0};
    GameObject p2 = {{121.21, 121.21}, {-20.0, -20.0}, 10.0};
    applyPaddleCollision(&p2, &pad2, 1.0);
    assert_vec("45-degree angle strike", 20.0, 20.0, p2.vel.x, p2.vel.y);

    // 3. Glancing off the top-dead-center of the paddle
    // Puck hits the exact top of the paddle while moving left.
    // Normal vector points straight up. It should preserve all X velocity, and flip Y to 0.
    GameObject pad3 = {{100.0, 240.0}, {0.0, 0.0}, 20.0};
    GameObject p3 = {{100.0, 210.0}, {-15.0, 10.0}, 10.0};
    applyPaddleCollision(&p3, &pad3, 1.0);
    assert_vec("Top-edge glancing blow", -15.0, -10.0, p3.vel.x, p3.vel.y);

    // 4. Moving Paddle hitting a stationary puck
    GameObject pad4 = {{80.0, 240.0}, {10.0, 0.0}, 20.0};
    GameObject p4 = {{110.0, 240.0}, {0.0, 0.0}, 10.0};
    applyPaddleCollision(&p4, &pad4, 1.0);
    // V_puck_new = 2 * U_paddle_n - U_puck_n = 2*(10) - 0 = 20
    assert_vec("Moving paddle hits stationary puck", 20.0, 0.0, p4.vel.x, p4.vel.y);
    
    // 5. Anti-stick test: Puck inside paddle radius, but already moving away
    GameObject pad5 = {{100.0, 240.0}, {0.0, 0.0}, 20.0};
    GameObject p5 = {{120.0, 240.0}, {10.0, 0.0}, 10.0}; // Deeply intersecting, but moving right
    applyPaddleCollision(&p5, &pad5, 1.0);
    assert_vec("Anti-stick: overlapping but escaping", 10.0, 0.0, p5.vel.x, p5.vel.y);
}

int main() {
    printf("====================================================\n");
    printf("         EMBEDDED AIR HOCKEY PHYSICS TESTS          \n");
    printf("====================================================\n");
    
    testWallCollisionTime();
    testWallBounce();
    testPaddleCollisionTime();
    testPaddleBounce();
    
    printf("\n====================================================\n");
    printf("All test suites completed.\n");
    
    return 0;
}