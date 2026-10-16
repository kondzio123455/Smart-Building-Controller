cat > ui.c << 'EOF'
#include "raylib.h"
#include "building.h"

#define MARGIN   20
#define BTN_W    150
#define BTN_H    40
#define BTN_GAP  10
#define BTN_Y    20

/* 2D room drawing area (left) and readout panel (right) */
#define ROOM_X   20
#define ROOM_Y   80
#define ROOM_W   520
#define ROOM_H   350
#define PANEL_X  560
#define PANEL_W  220

#define LAMP_R   22

static Rectangle button_rect(int i)
{
    return (Rectangle){
        (float)(MARGIN + i * (BTN_W + BTN_GAP)),
        (float)BTN_Y,
        (float)BTN_W,
        (float)BTN_H
    };
}

static Vector2 lamp_centre(void)
{
    return (Vector2){ ROOM_X + ROOM_W / 2.0f, ROOM_Y + ROOM_H / 2.0f };
}

static Rectangle door_rect(void)
{
    return (Rectangle){ ROOM_X + ROOM_W / 2.0f - 40, ROOM_Y + ROOM_H - 10, 80, 20 };
}

static Color alert_colour(AlertLevel a)
{
    switch (a) {
        case ALERT_WARNING:  return ORANGE;
        case ALERT_CRITICAL: return RED;
        default:             return GREEN;
    }
}

/* blue when cold, red when hot (15 C to 30 C) */
static Color temp_colour(float t)
{
    float f = (t - 15.0f) / 15.0f;
    if (f < 0.0f) f = 0.0f;
    if (f > 1.0f) f = 1.0f;
    return (Color){ (unsigned char)(60 + 195 * f), 80, (unsigned char)(255 - 195 * f), 255 };
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

        Room *room = &b->rooms[b->selectedRoom];

        /* click the lamp to toggle lights, click the door to lock/unlock */
        if (CheckCollisionPointCircle(mouse, lamp_centre(), LAMP_R)) {
            room_set_lights(room, !room->actuators.lightsOn);
        }
        if (CheckCollisionPointRec(mouse, door_rect())) {
            room_set_lock(room, !room->actuators.doorLocked);
        }
    }

    /* TEMPORARY test key: M fakes motion until the simulation does it */
    if (IsKeyPressed(KEY_M)) {
        Room *room = &b->rooms[b->selectedRoom];
        room->sensors.motionDetected = !room->sensors.motionDetected;
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

    const Room *room = &b->rooms[b->selectedRoom];

    /* ---- 2D room: floor and walls ---- */
    Color floor = room->actuators.lightsOn ? (Color){ 80, 80, 92, 255 }
                                           : (Color){ 28, 28, 34, 255 };
    DrawRectangle(ROOM_X, ROOM_Y, ROOM_W, ROOM_H, floor);

    /* ---- ceiling light: glow when on, click to toggle ---- */
    Vector2 lamp = lamp_centre();
    if (room->actuators.lightsOn) {
        DrawCircle((int)lamp.x, (int)lamp.y, 120, (Color){ 255, 230, 120, 20 });
        DrawCircle((int)lamp.x, (int)lamp.y, 85,  (Color){ 255, 230, 120, 35 });
        DrawCircle((int)lamp.x, (int)lamp.y, 55,  (Color){ 255, 230, 120, 55 });
        DrawCircle((int)lamp.x, (int)lamp.y, LAMP_R, YELLOW);
    } else {
        DrawCircle((int)lamp.x, (int)lamp.y, LAMP_R, DARKGRAY);
    }
    DrawCircleLines((int)lamp.x, (int)lamp.y, LAMP_R, RAYWHITE);
    DrawText("LIGHT", (int)lamp.x - 22, (int)lamp.y + LAMP_R + 6, 14, RAYWHITE);

    /* ---- HVAC vent on the top wall: colour shows the temperature ---- */
    Rectangle vent = { ROOM_X + 60, ROOM_Y + 10, 140, 16 };
    DrawRectangleRec(vent, room->actuators.hvacOn ? temp_colour(room->sensors.temperature) : GRAY);
    DrawText(room->actuators.hvacOn ? "HVAC ON" : "HVAC OFF",
             (int)vent.x, (int)vent.y + 22, 14, RAYWHITE);

    /* ---- smoke detector (top left): flashes red on smoke ---- */
    bool flash = ((int)(GetTime() * 4)) % 2 == 0;
    Color smokeCol = (room->sensors.smokeDetected && flash) ? RED : DARKGRAY;
    DrawCircle(ROOM_X + 30, ROOM_Y + 70, 10, smokeCol);
    DrawCircleLines(ROOM_X + 30, ROOM_Y + 70, 10, RAYWHITE);
    DrawText("SMOKE", ROOM_X + 12, ROOM_Y + 86, 12, RAYWHITE);

    /* ---- motion sensor (top right): pulses and shows a figure on motion ---- */
    int mx = ROOM_X + ROOM_W - 40;
    int my = ROOM_Y + 50;
    DrawCircle(mx, my, 10, room->sensors.motionDetected ? RED : DARKGRAY);
    DrawCircleLines(mx, my, 10, RAYWHITE);
    DrawText("MOTION", mx - 26, my + 16, 12, RAYWHITE);
    if (room->sensors.motionDetected) {
        int pulse = 12 + ((int)(GetTime() * 40)) % 30;
        DrawCircleLines(mx, my, (float)pulse, RED);

        int px = ROOM_X + 80 + ((int)(GetTime() * 60)) % (ROOM_W - 160);
        DrawCircle(px, ROOM_Y + ROOM_H / 2 + 80, 10, ORANGE);
    }

    /* ---- door on the bottom wall: red = locked, green = unlocked ---- */
    Rectangle door = door_rect();
    DrawRectangleRec(door, room->actuators.doorLocked ? RED : GREEN);
    DrawText(room->actuators.doorLocked ? "DOOR LOCKED" : "DOOR UNLOCKED",
             (int)door.x - 10, (int)door.y - 20, 14, RAYWHITE);

    /* ---- walls on top, coloured by the room's alert level ---- */
    DrawRectangleLinesEx((Rectangle){ ROOM_X, ROOM_Y, ROOM_W, ROOM_H }, 8,
                         alert_colour(room->alert));

    /* ---- readout panel ---- */
    int x = PANEL_X + 15;
    int y = ROOM_Y + 15;
    DrawRectangle(PANEL_X, ROOM_Y, PANEL_W, ROOM_H, BLACK);
    DrawText(room->name, x, y, 24, RAYWHITE);
    DrawText(TextFormat("Temp: %.1f C", room->sensors.temperature), x, y + 45, 18, RAYWHITE);
    DrawText(TextFormat("Target: %.1f C", room->sensors.targetTemp), x, y + 75, 18, RAYWHITE);
    DrawText(TextFormat("Motion: %s", room->sensors.motionDetected ? "YES" : "no"),
             x, y + 105, 18, RAYWHITE);
    DrawText(TextFormat("Door: %s", room->actuators.doorLocked ? "locked" : "unlocked"),
             x, y + 135, 18, RAYWHITE);
    DrawText(TextFormat("Lights: %s", room->actuators.lightsOn ? "on" : "off"),
             x, y + 165, 18, RAYWHITE);
    DrawText(room->alert == ALERT_CRITICAL ? "Status: CRITICAL"
             : room->alert == ALERT_WARNING ? "Status: WARNING" : "Status: OK",
             x, y + 195, 18, alert_colour(room->alert));

    DrawText("Click lamp / door to toggle", x, y + 265, 12, GRAY);
    DrawText("M = fake motion (test)",       x, y + 285, 12, GRAY);
}
EOF