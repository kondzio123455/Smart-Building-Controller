#include "raylib.h"
#include "building.h"

#define MARGIN  20
#define BTN_W   150
#define BTN_H   40
#define BTN_GAP 10
#define BTN_Y   20
#define PANEL_Y 80

static Rectangle button_rect(int i)
{
    return (Rectangle){
        (float)(MARGIN + i * (BTN_W + BTN_GAP)),
        (float)BTN_Y,
        (float)BTN_W,
        (float)BTN_H
    };
}

static Color alert_colour(AlertLevel a)
{
    switch (a) {
        case ALERT_WARNING:  return ORANGE;
        case ALERT_CRITICAL: return RED;
        default:             return GREEN;
    }
}

void ui_handle_input(Building *b)
{
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 mouse = GetMousePosition();
        for (int i = 0; i < b->roomCount; i++) {
            if (CheckCollisionPointRec(mouse, button_rect(i))) {
                b->selectedRoom = i;
            }
        }
    }
}

void ui_draw(const Building *b)
{
    ClearBackground(DARKGRAY);

    /* Room selector row */
    for (int i = 0; i < b->roomCount; i++) {
        Rectangle r = button_rect(i);
        bool selected = (i == b->selectedRoom);

        DrawRectangleRec(r, selected ? RAYWHITE : GRAY);
        DrawRectangleLinesEx(r, 3, alert_colour(b->rooms[i].alert));
        DrawText(b->rooms[i].name, (int)r.x + 10, (int)r.y + 11, 18,
                 selected ? BLACK : RAYWHITE);
    }

    /* Selected room panel */
    const Room *room = &b->rooms[b->selectedRoom];
    int x = MARGIN + 20;
    int y = PANEL_Y + 20;

    DrawRectangle(MARGIN, PANEL_Y, 760, 340, BLACK);
    DrawText(room->name, x, y, 30, RAYWHITE);

    DrawText(TextFormat("Temperature: %.1f C (target %.1f C)",
                        room->sensors.temperature, room->sensors.targetTemp),
             x, y + 50, 20, RAYWHITE);
    DrawText(TextFormat("Motion: %s",
                        room->sensors.motionDetected ? "DETECTED" : "none"),
             x, y + 85, 20, RAYWHITE);
    DrawText(TextFormat("Door: %s",
                        room->actuators.doorLocked ? "locked" : "unlocked"),
             x, y + 120, 20, RAYWHITE);
    DrawText(TextFormat("Lights: %s",
                        room->actuators.lightsOn ? "on" : "off"),
             x, y + 155, 20, RAYWHITE);
}
