#include "raylib.h"
#include "building.h"
#include "graphs.h"

/* The QUIT button on the end-of-night screen (the panel's SITE tab has one during play) */
static Rectangle end_quit_btn(void)
{
    return (Rectangle){ 340, 300, 120, 34 };
}

int main(void)
{
    Building building;
    building_init(&building);

    InitWindow(800, 450, "Smart Building Controller");
    SetTargetFPS(60);
    ui_init();                                       /* starts the generated sound */

    while (!WindowShouldClose() && !building.quitRequested) {
        ui_handle_input(&building);

        /* the world stands still while the game is paused or the help is open */
        float dt = ui_paused() ? 0.0f : GetFrameTime();
        building_update(&building, dt);
        graphs_update(&building, dt);

        /* after the night ends there is no panel, so main() offers the QUIT button itself */
        if (building.state != GAME_PLAYING && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)
            && CheckCollisionPointRec(GetMousePosition(), end_quit_btn())) {
            building_request_quit(&building);
        }

        BeginDrawing();
        ui_draw(&building);
        if (building.state != GAME_PLAYING) {
            Rectangle q = end_quit_btn();
            bool hover = CheckCollisionPointRec(GetMousePosition(), q);
            DrawRectangleRec(q, Fade(RED, hover ? 0.45f : 0.20f));
            DrawRectangleLinesEx(q, 2, RED);
            DrawText("QUIT", (int)q.x + (int)(q.width - MeasureText("QUIT", 18)) / 2, (int)q.y + 8, 18, RAYWHITE);
        }
        graphs_draw(&building);
        EndDrawing();
    }

    ui_close();
    CloseWindow();
    return 0;
}
