#include <math.h>
#include "raylib.h"
#include "building.h"

/* Must match the window size in main.c */
#define SCREEN_W 800
#define SCREEN_H 450

#define MARGIN   20
#define BTN_W    120
#define BTN_H    36
#define BTN_GAP  10
#define BTN_Y    (SCREEN_H - 62)

static Rectangle button_rect(int i)
{
    return (Rectangle){
        (float)(MARGIN + i * (BTN_W + BTN_GAP)),
        (float)BTN_Y,
        (float)BTN_W,
        (float)BTN_H
    };
}

static Rectangle light_btn_rect(void)
{
    return (Rectangle){ SCREEN_W - 230, BTN_Y, 100, BTN_H };
}

static Rectangle door_btn_rect(void)
{
    return (Rectangle){ SCREEN_W - 120, BTN_Y, 100, BTN_H };
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

/* scale a colour's brightness (k = 1.0 normal, lower = darker) */
static Color shade(Color c, float k)
{
    return (Color){ (unsigned char)(c.r * k), (unsigned char)(c.g * k),
                    (unsigned char)(c.b * k), 255 };
}

void ui_handle_input(Building *b)
{
    if (b->state != GAME_PLAYING) {
        if (IsKeyPressed(KEY_R)) building_init(b);   /* restart the night */
        return;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 mouse = GetMousePosition();

        for (int i = 0; i < b->roomCount; i++) {
            if (CheckCollisionPointRec(mouse, button_rect(i))) {
                b->selectedRoom = i;
            }
        }

        Room *room = &b->rooms[b->selectedRoom];
        if (CheckCollisionPointRec(mouse, light_btn_rect())) {
            room_set_lights(room, !room->actuators.lightsOn);
        }
        if (CheckCollisionPointRec(mouse, door_btn_rect())) {
            room_set_lock(room, !room->actuators.doorLocked);
        }
    }

    /* TEMPORARY test keys until the simulation does these itself */
    if (IsKeyPressed(KEY_M)) {
        Room *room = &b->rooms[b->selectedRoom];
        room->sensors.motionDetected = !room->sensors.motionDetected;
    }
    if (IsKeyPressed(KEY_K)) game_lose(b);
}

/* Different furniture per room so each camera looks distinct */
static void draw_furniture(int id, float k)
{
    Color wood  = shade((Color){ 110, 80, 50, 255 }, k);
    Color metal = shade((Color){ 70, 75, 85, 255 }, k);

    switch (id % 4) {
        case 0: /* lobby: reception desk and bench */
            DrawCube((Vector3){ -2.0f, 0.5f, -1.5f }, 3.2f, 1.0f, 1.0f, wood);
            DrawCube((Vector3){ -3.6f, 0.3f, 1.5f }, 1.0f, 0.6f, 3.0f, metal);
            break;
        case 1: /* office: desk with a glowing monitor */
            DrawCube((Vector3){ -1.5f, 0.45f, -1.0f }, 3.0f, 0.9f, 1.4f, wood);
            DrawCube((Vector3){ -1.5f, 1.3f, -1.4f }, 1.0f, 0.7f, 0.1f,
                     (Color){ 40, 90, 120, 255 });
            break;
        case 2: /* server room: racks with status lights */
            for (int i = 0; i < 3; i++) {
                float x = -3.6f + i * 1.6f;
                DrawCube((Vector3){ x, 1.2f, -3.2f }, 1.2f, 2.4f, 1.0f, metal);
                DrawSphere((Vector3){ x + 0.3f, 1.8f, -2.65f }, 0.06f, GREEN);
                DrawSphere((Vector3){ x - 0.3f, 1.5f, -2.65f }, 0.06f,
                           (Color){ 60, 140, 255, 255 });
            }
            break;
        default: /* kitchen: counter along the left wall and a table */
            DrawCube((Vector3){ -4.3f, 0.5f, 0.0f }, 1.0f, 1.0f, 6.0f, metal);
            DrawCube((Vector3){ 0.0f, 0.4f, -0.5f }, 2.0f, 0.8f, 1.4f, wood);
            break;
    }
}

/* The "camera feed": a fixed camera in the corner of the room, slowly panning */
static void draw_room_3d(const Room *room, float t)
{
    float k = room->actuators.lightsOn ? 1.0f : 0.22f;
    if (room->actuators.lightsOn && GetRandomValue(0, 90) == 0) k = 0.6f; /* flicker */

    Camera3D cam = { 0 };
    cam.position   = (Vector3){ 4.6f, 3.2f, 3.7f };
    cam.target     = (Vector3){ -0.5f + sinf(t * 0.5f) * 1.5f, 1.0f, -1.5f };
    cam.up         = (Vector3){ 0.0f, 1.0f, 0.0f };
    cam.fovy       = 70.0f;
    cam.projection = CAMERA_PERSPECTIVE;

    BeginMode3D(cam);

    /* floor and walls (the camera side is left open) */
    DrawPlane((Vector3){ 0.0f, 0.0f, 0.0f }, (Vector2){ 10.0f, 8.0f },
              shade((Color){ 90, 90, 100, 255 }, k));
    Color wall = shade((Color){ 120, 115, 110, 255 }, k);
    DrawCube((Vector3){ 0.0f, 1.75f, -4.0f }, 10.0f, 3.5f, 0.2f, wall);
    DrawCube((Vector3){ -5.0f, 1.75f, 0.0f }, 0.2f, 3.5f, 8.0f, wall);

    /* door on the back wall: red = locked, green = unlocked */
    Color doorCol = room->actuators.doorLocked ? (Color){ 200, 40, 40, 255 }
                                               : (Color){ 40, 180, 70, 255 };
    DrawCube((Vector3){ 2.5f, 1.1f, -3.85f }, 1.6f, 2.2f, 0.15f,
             shade(doorCol, 0.5f + 0.5f * k));

    /* HVAC vent: colour shows the temperature */
    DrawCube((Vector3){ -2.5f, 3.0f, -3.85f }, 2.0f, 0.3f, 0.15f,
             room->actuators.hvacOn ? temp_colour(room->sensors.temperature) : GRAY);

    /* ceiling light */
    DrawSphere((Vector3){ 0.0f, 3.2f, 0.0f }, 0.25f,
               room->actuators.lightsOn ? YELLOW : DARKGRAY);

    draw_furniture(room->id, k);

    /* something moving: a dark figure with glowing eyes (fake until intruders exist) */
    if (room->sensors.motionDetected) {
        float fx = -3.0f + fmodf(t * 1.0f, 6.0f);
        float fz = 1.0f;
        DrawCylinder((Vector3){ fx, 0.0f, fz }, 0.35f, 0.45f, 1.6f, 12,
                     (Color){ 30, 30, 30, 255 });
        DrawSphere((Vector3){ fx, 1.9f, fz }, 0.4f, (Color){ 60, 50, 40, 255 });
        DrawSphere((Vector3){ fx - 0.15f, 1.95f, fz + 0.33f }, 0.07f, RAYWHITE);
        DrawSphere((Vector3){ fx + 0.15f, 1.95f, fz + 0.33f }, 0.07f, RAYWHITE);
    }

    EndMode3D();
}

/* CRT look: scanlines, static and dark edges */
static void draw_camera_overlay(void)
{
    for (int y = 0; y < SCREEN_H; y += 3) {
        DrawRectangle(0, y, SCREEN_W, 1, Fade(BLACK, 0.25f));
    }
    for (int i = 0; i < 150; i++) {
        DrawRectangle(GetRandomValue(0, SCREEN_W), GetRandomValue(0, SCREEN_H),
                      GetRandomValue(1, 3), 1, Fade(RAYWHITE, 0.15f));
    }
    DrawRectangleGradientV(0, 0, SCREEN_W, 100, Fade(BLACK, 0.8f), Fade(BLACK, 0.0f));
    DrawRectangleGradientV(0, SCREEN_H - 100, SCREEN_W, 100, Fade(BLACK, 0.0f), Fade(BLACK, 0.8f));
    DrawRectangleGradientH(0, 0, 120, SCREEN_H, Fade(BLACK, 0.8f), Fade(BLACK, 0.0f));
    DrawRectangleGradientH(SCREEN_W - 120, 0, 120, SCREEN_H, Fade(BLACK, 0.0f), Fade(BLACK, 0.8f));
}

void ui_draw(const Building *b)
{
    const Room *room = &b->rooms[b->selectedRoom];
    float t = (float)GetTime();

    ClearBackground(BLACK);
    draw_room_3d(room, t);
    draw_camera_overlay();

    /* camera label and blinking REC dot (top left) */
    DrawText(TextFormat("CAM %d  %s", room->id + 1, room->name), MARGIN, 18, 24, RAYWHITE);
    if (((int)(t * 2.0f)) % 2 == 0) DrawCircle(MARGIN + 8, 58, 7, RED);
    DrawText("REC", MARGIN + 24, 50, 18, RAYWHITE);

    /* night clock (top right) */
    int hour = building_hour(b);
    DrawText(TextFormat("%d AM", hour == 0 ? 12 : hour), SCREEN_W - 110, 18, 30, RAYWHITE);

    /* alert (top centre) */
    if (room->alert != ALERT_NONE) {
        DrawText(room->alert == ALERT_CRITICAL ? "! CRITICAL" : "! WARNING",
                 SCREEN_W / 2 - 60, 20, 24, alert_colour(room->alert));
    }

    /* readout line */
    DrawText(TextFormat("TEMP %.1fC (TARGET %.1fC)   MOTION: %s   DOOR: %s   LIGHTS: %s",
                        room->sensors.temperature, room->sensors.targetTemp,
                        room->sensors.motionDetected ? "YES" : "no",
                        room->actuators.doorLocked ? "locked" : "unlocked",
                        room->actuators.lightsOn ? "on" : "off"),
             MARGIN, SCREEN_H - 90, 16, RAYWHITE);

    /* camera buttons (bottom left) */
    for (int i = 0; i < b->roomCount; i++) {
        Rectangle r = button_rect(i);
        bool sel = (i == b->selectedRoom);
        DrawRectangleRec(r, sel ? Fade(RAYWHITE, 0.85f) : Fade(BLACK, 0.6f));
        DrawRectangleLinesEx(r, 2, alert_colour(b->rooms[i].alert));
        DrawText(b->rooms[i].name, (int)r.x + 8, (int)r.y + 10, 16,
                 sel ? BLACK : RAYWHITE);
    }

    /* light and door buttons (bottom right) */
    Rectangle lb = light_btn_rect();
    DrawRectangleRec(lb, Fade(BLACK, 0.6f));
    DrawRectangleLinesEx(lb, 2, room->actuators.lightsOn ? YELLOW : GRAY);
    DrawText("LIGHTS", (int)lb.x + 14, (int)lb.y + 10, 16,
             room->actuators.lightsOn ? YELLOW : GRAY);

    Rectangle db = door_btn_rect();
    bool locked = room->actuators.doorLocked;
    DrawRectangleRec(db, Fade(BLACK, 0.6f));
    DrawRectangleLinesEx(db, 2, locked ? RED : GREEN);
    DrawText(locked ? "LOCKED" : "UNLOCKED", (int)db.x + 12, (int)db.y + 10, 16,
             locked ? RED : GREEN);

    DrawText("TEST KEYS: M = fake motion, K = end night", MARGIN, SCREEN_H - 20, 12, GRAY);

    /* end of night: shown over everything, no menu */
    if (b->state != GAME_PLAYING) {
        DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(BLACK, 0.85f));
        const char *msg = (b->state == GAME_WON) ? "6 AM - YOU SURVIVED" : "GAME OVER";
        int w = MeasureText(msg, 50);
        DrawText(msg, (SCREEN_W - w) / 2, 170, 50, (b->state == GAME_WON) ? GREEN : RED);

        const char *hint = "Press R to restart";
        int hw = MeasureText(hint, 20);
        DrawText(hint, (SCREEN_W - hw) / 2, 240, 20, GRAY);
    }
}