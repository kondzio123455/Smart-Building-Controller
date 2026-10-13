#include "raylib.h"

int main(void) {
    InitWindow(800, 450, "Smart Building Controller");
    SetTargetFPS(60);
    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(DARKGRAY);
        DrawText("raylib works", 300, 200, 30, RAYWHITE);
        EndDrawing();
    }
    CloseWindow();
    return 0;
}