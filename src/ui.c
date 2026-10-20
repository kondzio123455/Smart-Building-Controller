#include <math.h>
#include "raylib.h"
#include "building.h"

/* Must match the window size in main.c */
#define SCREEN_W 800
#define SCREEN_H 450

#define MARGIN   20
#define BTN_H    36
#define BTN_Y    (SCREEN_H - 62)

/* camera map: a 3 x 2 grid of buttons in the bottom right */
#define MAP_COLS 3
#define MAP_W    84
#define MAP_H    30
#define MAP_GAP  6
#define MAP_X    (SCREEN_W - MARGIN - (MAP_COLS * MAP_W + (MAP_COLS - 1) * MAP_GAP))
#define MAP_Y    (SCREEN_H - 80)

/* Same order as the rooms created in building_init */
static const char *SHORT_NAME[MAX_ROOMS] = {
    "OFFICE", "HALL", "LOBBY", "KITCHEN", "SERVER", "STORAGE", "", ""
};

/* How each camera looks: floor and wall colours, camera position and aim */
typedef struct {
    Color   floor;
    Color   wall;
    Vector3 pos;
    Vector3 target;
} RoomLook;

static const RoomLook LOOKS[MAX_ROOMS] = {
    /* 0 office  */ { { 70,  72,  80, 255 }, {  70,  95, 105, 255 }, {  0.0f, 2.3f, 3.7f }, {  0.0f, 1.2f, -1.5f } },
    /* 1 hallway */ { { 85,  85,  90, 255 }, {  95, 110,  95, 255 }, { -4.5f, 1.8f, 3.7f }, {  1.0f, 1.2f, -2.0f } },
    /* 2 lobby   */ { { 120, 100, 80, 255 }, { 140, 125, 105, 255 }, {  4.6f, 3.2f, 3.7f }, { -0.5f, 1.0f, -1.5f } },
    /* 3 kitchen */ { { 190, 190, 195, 255 }, { 210, 210, 205, 255 }, { -4.6f, 3.0f, 3.7f }, {  1.0f, 1.0f, -1.5f } },
    /* 4 server  */ { { 55,  60,  75, 255 }, {  50,  65,  95, 255 }, {  3.5f, 2.0f, 3.7f }, { -1.0f, 1.2f, -2.0f } },
    /* 5 storage */ { { 100, 85,  65, 255 }, { 115, 100,  80, 255 }, {  0.0f, 3.4f, 3.6f }, {  0.0f, 0.6f, -1.2f } },
    /* 6 spare   */ { { 90,  90, 100, 255 }, { 120, 115, 110, 255 }, {  4.6f, 3.2f, 3.7f }, { -0.5f, 1.0f, -1.5f } },
    /* 7 spare   */ { { 90,  90, 100, 255 }, { 120, 115, 110, 255 }, {  4.6f, 3.2f, 3.7f }, { -0.5f, 1.0f, -1.5f } }
};

static Rectangle button_rect(int i)
{
    int col = i % MAP_COLS;
    int row = i / MAP_COLS;
    return (Rectangle){
        (float)(MAP_X + col * (MAP_W + MAP_GAP)),
        (float)(MAP_Y + row * (MAP_H + MAP_GAP)),
        (float)MAP_W,
        (float)MAP_H
    };
}

static Rectangle light_btn_rect(void)
{
    return (Rectangle){ MARGIN, BTN_Y, 100, BTN_H };
}

static Rectangle door_btn_rect(void)
{
    return (Rectangle){ MARGIN + 110, BTN_Y, 110, BTN_H };
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

    /* keys 1-6 switch camera */
    for (int i = 0; i < b->roomCount; i++) {
        if (IsKeyPressed(KEY_ONE + i)) b->selectedRoom = i;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 mouse = GetMousePosition();

        for (int i = 0; i < b->roomCount; i++) {
            if (CheckCollisionPointRec(mouse, button_rect(i))) {
                b->selectedRoom = i;
            }
        }

        Room *room = &b->rooms[b->selectedRoom];
        if (!b->powerOut) {
            if (CheckCollisionPointRec(mouse, light_btn_rect())) {
                room_set_lights(room, !room->actuators.lightsOn);
            }
            if (CheckCollisionPointRec(mouse, door_btn_rect())) {
                room_set_lock(room, !room->actuators.doorLocked);
            }
        }
    }

    /* keyboard shortcuts for the selected room */
    if (!b->powerOut) {
        Room *room = &b->rooms[b->selectedRoom];
        if (IsKeyPressed(KEY_L)) room_set_lights(room, !room->actuators.lightsOn);
        if (IsKeyPressed(KEY_D)) room_set_lock(room, !room->actuators.doorLocked);
        if (IsKeyPressed(KEY_V)) room_set_hvac(room, !room->actuators.hvacOn);
    }

    /* TEMPORARY test key */
    if (IsKeyPressed(KEY_K)) game_lose(b);
}

/* Each room has its own furniture so every camera looks different */
static void draw_furniture(int id, float k)
{
    Color wood  = shade((Color){ 110, 80, 50, 255 }, k);
    Color metal = shade((Color){ 70, 75, 85, 255 }, k);
    Color white = shade((Color){ 200, 200, 205, 255 }, k);
    Color green = shade((Color){ 50, 120, 60, 255 }, k);

    switch (id) {
        case 0: /* office: your desk in the foreground, filing cabinet, wall poster */
            DrawCube((Vector3){ 0.0f, 0.4f, 2.4f }, 4.0f, 0.8f, 1.2f, wood);
            DrawCube((Vector3){ -3.8f, 0.7f, -3.2f }, 0.9f, 1.4f, 0.8f, metal);
            DrawCube((Vector3){ -3.0f, 1.8f, -3.85f }, 1.6f, 1.0f, 0.05f,
                     shade((Color){ 160, 60, 60, 255 }, k));
            break;
        case 1: /* hallway: lockers down the left wall, carpet runner, wall lamp */
            for (int i = 0; i < 5; i++) {
                DrawCube((Vector3){ -4.5f, 1.0f, -3.0f + i * 1.5f }, 0.8f, 2.0f, 1.2f, metal);
            }
            DrawCube((Vector3){ 0.0f, 0.01f, 0.0f }, 1.4f, 0.02f, 8.0f,
                     shade((Color){ 120, 40, 40, 255 }, k));
            DrawCube((Vector3){ 0.0f, 2.8f, -3.85f }, 0.6f, 0.3f, 0.1f,
                     shade((Color){ 255, 230, 120, 255 }, k));
            break;
        case 2: /* lobby: reception desk, bench, potted plant */
            DrawCube((Vector3){ -2.0f, 0.5f, -1.5f }, 3.2f, 1.0f, 1.0f, wood);
            DrawCube((Vector3){ -3.6f, 0.3f, 1.5f }, 1.0f, 0.6f, 3.0f, metal);
            DrawCylinder((Vector3){ 3.8f, 0.0f, -2.5f }, 0.35f, 0.3f, 0.6f, 10, wood);
            DrawSphere((Vector3){ 3.8f, 1.1f, -2.5f }, 0.6f, green);
            break;
        case 3: /* kitchen: counter, island table, fridge */
            DrawCube((Vector3){ -4.3f, 0.5f, 0.0f }, 1.0f, 1.0f, 6.0f, white);
            DrawCube((Vector3){ 0.0f, 0.5f, -0.5f }, 2.0f, 1.0f, 1.4f, wood);
            DrawCube((Vector3){ -3.5f, 1.0f, -3.4f }, 1.2f, 2.0f, 1.0f, white);
            break;
        case 4: /* server room: racks with status lights */
            for (int i = 0; i < 3; i++) {
                float x = -3.6f + i * 1.6f;
                DrawCube((Vector3){ x, 1.2f, -3.2f }, 1.2f, 2.4f, 1.0f, metal);
                DrawSphere((Vector3){ x + 0.3f, 1.8f, -2.65f }, 0.06f, GREEN);
                DrawSphere((Vector3){ x - 0.3f, 1.5f, -2.65f }, 0.06f,
                           (Color){ 60, 140, 255, 255 });
            }
            break;
        case 5: /* storage: stacked crates and a shelf unit */
            DrawCube((Vector3){ -3.5f, 0.5f, -2.5f }, 1.0f, 1.0f, 1.0f, wood);
            DrawCube((Vector3){ -3.5f, 1.5f, -2.5f }, 0.8f, 1.0f, 0.8f, wood);
            DrawCube((Vector3){ -2.2f, 0.5f, -3.0f }, 1.2f, 1.0f, 1.0f, wood);
            DrawCube((Vector3){ 0.5f, 0.5f, -3.2f }, 1.4f, 1.0f, 1.0f, wood);
            DrawCube((Vector3){ 3.8f, 1.0f, 0.0f }, 0.6f, 2.0f, 5.0f, metal);
            break;
        default:
            break;
    }
}

/* Every kind of intruder has its own shape (all built from simple primitives) */
static void draw_intruder(const Intruder *in, int slot, float t, float k)
{
    float x = -2.2f + slot * 1.2f;
    float z = 0.8f;
    float dim = 0.5f + 0.5f * k;   /* bodies fade in the dark, eyes keep glowing */
    Color body = shade((Color){ 30, 30, 30, 255 }, dim);

    switch (in->type) {
        case INTRUDER_DRIFTER: /* tall dark figure with white eyes */
            DrawCylinder((Vector3){ x, 0.0f, z }, 0.35f, 0.45f, 1.6f, 12, body);
            DrawSphere((Vector3){ x, 1.9f, z }, 0.4f, shade((Color){ 60, 50, 40, 255 }, dim));
            DrawSphere((Vector3){ x - 0.15f, 1.95f, z + 0.33f }, 0.07f, RAYWHITE);
            DrawSphere((Vector3){ x + 0.15f, 1.95f, z + 0.33f }, 0.07f, RAYWHITE);
            break;
        case INTRUDER_SHADE: /* very tall and thin with red eyes */
            DrawCylinder((Vector3){ x, 0.0f, z }, 0.15f, 0.3f, 2.4f, 10, (Color){ 10, 10, 14, 255 });
            DrawSphere((Vector3){ x, 2.6f, z }, 0.3f, (Color){ 10, 10, 14, 255 });
            DrawSphere((Vector3){ x - 0.1f, 2.65f, z + 0.25f }, 0.06f, RED);
            DrawSphere((Vector3){ x + 0.1f, 2.65f, z + 0.25f }, 0.06f, RED);
            break;
        case INTRUDER_CRAWLER: /* low and wide, four legs, orange eyes */
            DrawCube((Vector3){ x, 0.3f, z }, 1.2f, 0.3f, 0.8f, body);
            for (int i = 0; i < 4; i++) {
                float lx = x + ((i % 2) ? 0.55f : -0.55f);
                float lz = z + ((i / 2) ? 0.35f : -0.35f);
                DrawCylinder((Vector3){ lx, 0.0f, lz }, 0.05f, 0.05f, 0.3f, 6, body);
            }
            DrawSphere((Vector3){ x - 0.2f, 0.35f, z + 0.42f }, 0.06f, ORANGE);
            DrawSphere((Vector3){ x + 0.2f, 0.35f, z + 0.42f }, 0.06f, ORANGE);
            break;
        case INTRUDER_GLITCH: /* jittering coloured cubes */
            for (int i = 0; i < 3; i++) {
                float j = t * 18.0f + i * 2.0f;
                DrawCube((Vector3){ x + sinf(j) * 0.15f, 0.8f + i * 0.5f, z + cosf(j * 1.3f) * 0.15f },
                         0.4f, 0.4f, 0.4f, (i % 2) ? MAGENTA : SKYBLUE);
            }
            break;
        default: /* leech: pulsing purple blob with green eyes */
            DrawSphere((Vector3){ x, 0.6f, z }, 0.6f + sinf(t * 3.0f) * 0.05f,
                       shade((Color){ 90, 40, 110, 255 }, dim));
            for (int i = 0; i < 3; i++) {
                DrawSphere((Vector3){ x + (i - 1) * 0.5f, 0.2f, z + 0.3f }, 0.2f,
                           shade((Color){ 70, 30, 90, 255 }, dim));
            }
            DrawSphere((Vector3){ x - 0.2f, 0.8f, z + 0.5f }, 0.08f, LIME);
            DrawSphere((Vector3){ x + 0.2f, 0.8f, z + 0.5f }, 0.08f, LIME);
            break;
    }
}

/* The "camera feed": a fixed camera for each room, slowly panning */
static void draw_room_3d(const Building *b, float t)
{
    const Room *room = &b->rooms[b->selectedRoom];
    const RoomLook *look = &LOOKS[room->id];

    float k = room->actuators.lightsOn ? 1.0f : 0.22f;
    if (room->actuators.lightsOn && GetRandomValue(0, 90) == 0) k = 0.6f; /* flicker */

    Camera3D cam = { 0 };
    cam.position   = look->pos;
    cam.target     = look->target;
    cam.target.x  += sinf(t * 0.5f) * 1.5f;
    cam.up         = (Vector3){ 0.0f, 1.0f, 0.0f };
    cam.fovy       = 70.0f;
    cam.projection = CAMERA_PERSPECTIVE;

    BeginMode3D(cam);

    /* floor and walls (the camera side is left open) */
    DrawPlane((Vector3){ 0.0f, 0.0f, 0.0f }, (Vector2){ 10.0f, 8.0f }, shade(look->floor, k));
    Color wall = shade(look->wall, k);
    DrawCube((Vector3){ 0.0f, 1.75f, -4.0f }, 10.0f, 3.5f, 0.2f, wall);
    DrawCube((Vector3){ -5.0f, 1.75f, 0.0f }, 0.2f, 3.5f, 8.0f, wall);
    DrawCube((Vector3){ 5.0f, 1.75f, 0.0f }, 0.2f, 3.5f, 8.0f, wall);

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

    /* every awake intruder standing in this room */
    for (int i = 0; i < b->intruderCount; i++) {
        const Intruder *in = &b->intruders[i];
        if (in->active && in->room == room->id) draw_intruder(in, i, t, k);
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
    draw_room_3d(b, t);
    draw_camera_overlay();

    /* camera label and blinking REC dot (top left) */
    DrawText(TextFormat("CAM %d  %s", room->id + 1, room->name), MARGIN, 18, 24, RAYWHITE);
    if (((int)(t * 2.0f)) % 2 == 0) DrawCircle(MARGIN + 8, 58, 7, RED);
    DrawText("REC", MARGIN + 24, 50, 18, RAYWHITE);

    /* night clock and power (top right) */
    int hour = building_hour(b);
    DrawText(TextFormat("%d AM", hour == 0 ? 12 : hour), SCREEN_W - 110, 18, 30, RAYWHITE);

    Color powerCol = (b->power > 40.0f) ? RAYWHITE : (b->power > 20.0f) ? ORANGE : RED;
    DrawText(TextFormat("POWER %.0f%%", b->power), SCREEN_W - 170, 56, 22, powerCol);

    int bars = (int)(building_power_usage(b) / 0.1f);
    if (bars < 1) bars = 1;
    if (bars > 8) bars = 8;
    DrawText("USAGE", SCREEN_W - 170, 86, 14, GRAY);
    for (int i = 0; i < bars; i++) {
        DrawRectangle(SCREEN_W - 110 + i * 11, 86, 8, 14, (i < 3) ? GREEN : (i < 5) ? ORANGE : RED);
    }

    if (b->powerOut && ((int)(t * 2.0f)) % 2 == 0) {
        DrawText("POWER OUT", SCREEN_W / 2 - 90, 60, 30, RED);
    }

    /* alert (top centre) */
    if (room->alert != ALERT_NONE) {
        DrawText(room->alert == ALERT_CRITICAL ? "! CRITICAL" : "! WARNING",
                 SCREEN_W / 2 - 60, 20, 24, alert_colour(room->alert));
    }

    /* readout line */
    DrawText(TextFormat("TEMP %.1fC (TARGET %.1fC)   MOTION: %s   DOOR: %s   LIGHTS: %s   HVAC: %s",
                        room->sensors.temperature, room->sensors.targetTemp,
                        room->sensors.motionDetected ? "YES" : "no",
                        room->actuators.doorLocked ? "locked" : "unlocked",
                        room->actuators.lightsOn ? "on" : "off",
                        room->actuators.hvacOn ? "on" : "off"),
             MARGIN, SCREEN_H - 110, 14, RAYWHITE);

    /* camera map (bottom right): red dot = motion in that room */
    for (int i = 0; i < b->roomCount; i++) {
        Rectangle r = button_rect(i);
        bool sel = (i == b->selectedRoom);
        DrawRectangleRec(r, sel ? Fade(RAYWHITE, 0.85f) : Fade(BLACK, 0.6f));
        DrawRectangleLinesEx(r, 2, alert_colour(b->rooms[i].alert));
        DrawText(TextFormat("%d %s", i + 1, SHORT_NAME[i]), (int)r.x + 6, (int)r.y + 9, 12,
                 sel ? BLACK : RAYWHITE);
        if (b->rooms[i].sensors.motionDetected) DrawCircle((int)r.x + MAP_W - 8, (int)r.y + 8, 4, RED);
    }

    /* light and door buttons (bottom left) */
    Rectangle lb = light_btn_rect();
    DrawRectangleRec(lb, Fade(BLACK, 0.6f));
    DrawRectangleLinesEx(lb, 2, room->actuators.lightsOn ? YELLOW : GRAY);
    DrawText("LIGHTS", (int)lb.x + 14, (int)lb.y + 10, 16,
             room->actuators.lightsOn ? YELLOW : GRAY);

    Rectangle db = door_btn_rect();
    bool locked = room->actuators.doorLocked;
    DrawRectangleRec(db, Fade(BLACK, 0.6f));
    DrawRectangleLinesEx(db, 2, locked ? RED : GREEN);
    DrawText(locked ? "LOCKED" : "UNLOCKED", (int)db.x + 14, (int)db.y + 10, 16,
             locked ? RED : GREEN);

    DrawText("1-6 cameras, L lights, D door, V vent, TAB graphs, K end night (test)", MARGIN, SCREEN_H - 20, 12, GRAY);

    /* end of night: shown over everything, no menu */
    if (b->state != GAME_PLAYING) {
        DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(BLACK, 0.85f));

        const char *msg = "GAME OVER";
        if (b->state == GAME_WON) msg = "6 AM - YOU SURVIVED";
        else if (b->killer >= 0)  msg = TextFormat("CAUGHT BY %s", b->intruders[b->killer].name);

        int w = MeasureText(msg, 50);
        DrawText(msg, (SCREEN_W - w) / 2, 170, 50, (b->state == GAME_WON) ? GREEN : RED);

        const char *hint = "Press R to restart";
        int hw = MeasureText(hint, 20);
        DrawText(hint, (SCREEN_W - hw) / 2, 240, 20, GRAY);
    }
}
