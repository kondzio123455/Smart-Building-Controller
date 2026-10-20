#include "raylib.h"
#include "building.h"
#include "graphs.h"

int main(void)
{
    Building building;
    building_init(&building);

    InitWindow(800, 450, "Smart Building Controller");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        ui_handle_input(&building);
        building_update(&building, GetFrameTime());
        graphs_update(&building, GetFrameTime());

        BeginDrawing();
        ui_draw(&building);
        graphs_draw(&building);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
