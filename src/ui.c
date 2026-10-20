#include <math.h>
#include "raylib.h"
#include "building.h"
#include "graphs.h"

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

#define SCARE_LEN    1.6f
#define MAX_HOTSPOTS 12

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
    /* 1 hallway */ { { 85,  85,  90, 255 }, {  95, 110,  95, 255 }, {  4.6f, 1.8f, 3.7f }, { -1.5f, 1.2f, -2.0f } },
    /* 2 lobby   */ { { 120, 100, 80, 255 }, { 140, 125, 105, 255 }, {  4.6f, 3.2f, 3.7f }, { -0.5f, 1.0f, -1.5f } },
    /* 3 kitchen */ { { 190, 190, 195, 255 }, { 210, 210, 205, 255 }, { -4.6f, 3.0f, 3.7f }, {  1.0f, 1.0f, -1.5f } },
    /* 4 server  */ { { 55,  60,  75, 255 }, {  50,  65,  95, 255 }, {  3.5f, 2.0f, 3.7f }, { -1.0f, 1.2f, -2.0f } },
    /* 5 storage */ { { 100, 85,  65, 255 }, { 115, 100,  80, 255 }, {  0.0f, 3.4f, 3.6f }, {  0.0f, 0.6f, -1.2f } },
    /* 6 spare   */ { { 90,  90, 100, 255 }, { 120, 115, 110, 255 }, {  4.6f, 3.2f, 3.7f }, { -0.5f, 1.0f, -1.5f } },
    /* 7 spare   */ { { 90,  90, 100, 255 }, { 120, 115, 110, 255 }, {  4.6f, 3.2f, 3.7f }, { -0.5f, 1.0f, -1.5f } }
};

/* ------------------------------------------------------------------ */
/*  Sound: everything is generated in code, so there are no files      */
/* ------------------------------------------------------------------ */

enum { SFX_HUM, SFX_CLICK, SFX_THUMP, SFX_SQUEAK, SFX_DING,
       SFX_SCREECH, SFX_BEAT, SFX_STATIC, SFX_COUNT };

static Sound sfx[SFX_COUNT];
static bool  audioOk = false;

static void play(int id)
{
    if (audioOk) PlaySound(sfx[id]);
}

static Sound synth(int kind, float seconds)
{
    const int rate = 22050;
    int frames = (int)(seconds * rate);
    short *data = (short *)MemAlloc(frames * (int)sizeof(short));

    for (int i = 0; i < frames; i++) {
        float t = (float)i / rate;
        float noise = GetRandomValue(-1000, 1000) / 1000.0f;
        float s = 0.0f;

        switch (kind) {
            case SFX_HUM:
                s = 0.20f * sinf(2 * PI * 55 * t) + 0.10f * sinf(2 * PI * 110 * t) + 0.015f * noise;
                break;
            case SFX_CLICK:
                s = noise * expf(-t * 90.0f);
                break;
            case SFX_THUMP:
                s = sinf(2 * PI * (70.0f - 40.0f * t) * t) * expf(-t * 9.0f)
                  + 0.2f * noise * expf(-t * 40.0f);
                break;
            case SFX_SQUEAK:
                s = sinf(2 * PI * (700.0f + 3500.0f * t) * t) * (1.0f - t / seconds);
                break;
            case SFX_DING:
                s = (sinf(2 * PI * 1320 * t) + 0.5f * sinf(2 * PI * 1980 * t)) * expf(-t * 6.0f) * 0.7f;
                break;
            case SFX_SCREECH:
                s = 0.55f * noise
                  + 0.6f * sinf(2 * PI * (1100.0f + 450.0f * sinf(t * 55.0f)) * t)
                  + 0.5f * sinf(2 * PI * 170 * t);
                s *= expf(-t * 1.1f) * (t < 0.02f ? t / 0.02f : 1.0f);
                break;
            case SFX_BEAT:
                s = sinf(2 * PI * 55 * t) * (expf(-t * 22.0f)
                  + (t > 0.22f ? 0.7f * expf(-(t - 0.22f) * 22.0f) : 0.0f));
                break;
            default: /* static burst */
                s = noise * 0.5f * (1.0f - t / seconds);
                break;
        }
        if (s > 1.0f)  s = 1.0f;
        if (s < -1.0f) s = -1.0f;
        data[i] = (short)(s * 26000.0f);
    }

    Wave w = { (unsigned int)frames, (unsigned int)rate, 16, 1, data };
    Sound snd = LoadSoundFromWave(w);
    MemFree(data);
    return snd;
}

void ui_init(void)
{
    InitAudioDevice();
    audioOk = IsAudioDeviceReady();
    if (!audioOk) return;

    const float secs[SFX_COUNT] = { 2.0f, 0.06f, 0.4f, 0.15f, 0.9f, 1.5f, 0.7f, 0.25f };
    for (int i = 0; i < SFX_COUNT; i++) sfx[i] = synth(i, secs[i]);

    SetSoundVolume(sfx[SFX_HUM], 0.35f);
    SetSoundVolume(sfx[SFX_BEAT], 0.9f);
}

void ui_close(void)
{
    if (audioOk) {
        for (int i = 0; i < SFX_COUNT; i++) UnloadSound(sfx[i]);
    }
    CloseAudioDevice();
}

/* ------------------------------------------------------------------ */
/*  Things in the world you can click, and their animation state       */
/* ------------------------------------------------------------------ */

typedef enum {
    HS_DOOR, HS_LAMP, HS_VENT, HS_FAN, HS_DUCK, HS_LOCKER,
    HS_TV, HS_BELL, HS_FRIDGE, HS_RACK, HS_LANTERN
} HotspotKind;

typedef struct {
    HotspotKind kind;
    int         index;
    BoundingBox box;
} Hotspot;

/* cosmetic state that only the UI cares about */
static bool  fanOn = true, tvOn = true, fridgeOpen = false, rackFast = false, lanternOn = true;
static bool  lockerOpen[5];
static float lockerAnim[5], fridgeAnim = 0.0f, fanAngle = 0.0f, duckBounce = 0.0f;
static float doorClosed[MAX_ROOMS];
static float switchFlash = 0.0f, beatTimer = 0.0f;
static float scareTime = -1.0f;
static bool  scareDone = false;

static BoundingBox box_at(float x, float y, float z, float w, float h, float d)
{
    return (BoundingBox){ { x - w / 2, y - h / 2, z - d / 2 }, { x + w / 2, y + h / 2, z + d / 2 } };
}

static int add_hs(Hotspot *out, int n, HotspotKind kind, int index, BoundingBox box)
{
    if (n >= MAX_HOTSPOTS) return n;
    out[n].kind  = kind;
    out[n].index = index;
    out[n].box   = box;
    return n + 1;
}

static int build_hotspots(int roomId, Hotspot *out)
{
    int n = 0;

    /* every room has these three */
    n = add_hs(out, n, HS_DOOR, 0, box_at(2.5f, 1.1f, -3.85f, 1.6f, 2.2f, 0.3f));
    n = add_hs(out, n, HS_LAMP, 0, box_at(0.0f, 3.1f, 0.0f, 0.8f, 0.8f, 0.8f));
    n = add_hs(out, n, HS_VENT, 0, box_at(-2.5f, 2.7f, -3.85f, 2.0f, 0.5f, 0.3f));

    switch (roomId) {
        case 0:
            n = add_hs(out, n, HS_FAN,  0, box_at(1.2f, 1.15f, 0.9f, 0.7f, 0.7f, 0.7f));
            n = add_hs(out, n, HS_DUCK, 0, box_at(-0.9f, 0.95f, 1.0f, 0.4f, 0.35f, 0.4f));
            break;
        case 1:
            for (int i = 0; i < 5; i++) {
                n = add_hs(out, n, HS_LOCKER, i, box_at(-4.5f, 1.0f, -3.0f + 1.5f * i, 0.8f, 2.0f, 1.2f));
            }
            break;
        case 2:
            n = add_hs(out, n, HS_TV,   0, box_at(-1.0f, 1.9f, -3.82f, 2.2f, 1.2f, 0.2f));
            n = add_hs(out, n, HS_BELL, 0, box_at(-2.0f, 1.1f, -1.5f, 0.3f, 0.25f, 0.3f));
            break;
        case 3:
            n = add_hs(out, n, HS_FRIDGE, 0, box_at(-3.5f, 1.0f, -3.4f, 1.2f, 2.0f, 1.0f));
            break;
        case 4:
            for (int i = 0; i < 3; i++) {
                n = add_hs(out, n, HS_RACK, i, box_at(-3.6f + 1.6f * i, 1.2f, -3.2f, 1.2f, 2.4f, 1.0f));
            }
            break;
        case 5:
            n = add_hs(out, n, HS_LANTERN, 0, box_at(-2.2f, 1.25f, -3.0f, 0.4f, 0.5f, 0.4f));
            break;
        default:
            break;
    }
    return n;
}

static int pick_hotspot(const Hotspot *hs, int n, Ray ray)
{
    int best = -1;
    float bestDist = 1.0e9f;
    for (int i = 0; i < n; i++) {
        RayCollision c = GetRayCollisionBox(ray, hs[i].box);
        if (c.hit && c.distance < bestDist) {
            best = i;
            bestDist = c.distance;
        }
    }
    return best;
}

static Camera3D room_camera(int roomId, float t)
{
    const RoomLook *look = &LOOKS[roomId];

    Camera3D cam = { 0 };
    cam.position   = look->pos;
    cam.target     = look->target;
    cam.target.x  += sinf(t * 0.5f) * 1.5f;      /* slow security-camera pan */
    cam.up         = (Vector3){ 0.0f, 1.0f, 0.0f };
    cam.fovy       = 70.0f;
    cam.projection = CAMERA_PERSPECTIVE;
    return cam;
}

/* ------------------------------------------------------------------ */
/*  Buttons and small helpers                                          */
/* ------------------------------------------------------------------ */

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

static Rectangle graph_btn_rect(void)
{
    return (Rectangle){ MARGIN + 230, BTN_Y, 100, BTN_H };
}

static bool on_hud(const Building *b, Vector2 m)
{
    for (int i = 0; i < b->roomCount; i++) {
        if (CheckCollisionPointRec(m, button_rect(i))) return true;
    }
    return CheckCollisionPointRec(m, light_btn_rect())
        || CheckCollisionPointRec(m, door_btn_rect())
        || CheckCollisionPointRec(m, graph_btn_rect());
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

static void select_room(Building *b, int i)
{
    if (i == b->selectedRoom) return;
    b->selectedRoom = i;
    switchFlash = 0.3f;
    play(SFX_STATIC);
}

static void click_hotspot(Building *b, const Hotspot *h)
{
    Room *room = &b->rooms[b->selectedRoom];

    switch (h->kind) {
        case HS_DOOR:
            if (!b->powerOut) room_set_lock(room, !room->actuators.doorLocked);
            play(SFX_THUMP);
            break;
        case HS_LAMP:
            if (!b->powerOut) room_set_lights(room, !room->actuators.lightsOn);
            play(SFX_CLICK);
            break;
        case HS_VENT:
            if (!b->powerOut) room_set_hvac(room, !room->actuators.hvacOn);
            play(SFX_THUMP);
            break;
        case HS_FAN:     fanOn = !fanOn;                          play(SFX_CLICK);  break;
        case HS_DUCK:    duckBounce = 1.0f;                       play(SFX_SQUEAK); break;
        case HS_LOCKER:  lockerOpen[h->index] = !lockerOpen[h->index]; play(SFX_THUMP); break;
        case HS_TV:      tvOn = !tvOn;                            play(SFX_CLICK);  break;
        case HS_BELL:    play(SFX_DING);                          break;
        case HS_FRIDGE:  fridgeOpen = !fridgeOpen;                play(SFX_THUMP);  break;
        case HS_RACK:    rackFast = !rackFast;                    play(SFX_CLICK);  break;
        case HS_LANTERN: lanternOn = !lanternOn;                  play(SFX_CLICK);  break;
    }
}

void ui_handle_input(Building *b)
{
    if (b->state != GAME_PLAYING) {
        if (IsKeyPressed(KEY_R)) building_init(b);   /* restart the night */
        return;
    }

    /* keys 1-6 switch camera */
    for (int i = 0; i < b->roomCount; i++) {
        if (IsKeyPressed(KEY_ONE + i)) select_room(b, i);
    }

    /* while the dashboard is open, the game ignores the mouse */
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !graphs_visible()) {
        Vector2 mouse = GetMousePosition();

        if (on_hud(b, mouse)) {
            for (int i = 0; i < b->roomCount; i++) {
                if (CheckCollisionPointRec(mouse, button_rect(i))) select_room(b, i);
            }
            if (CheckCollisionPointRec(mouse, graph_btn_rect())) graphs_toggle();

            Room *room = &b->rooms[b->selectedRoom];
            if (!b->powerOut) {
                if (CheckCollisionPointRec(mouse, light_btn_rect())) {
                    room_set_lights(room, !room->actuators.lightsOn);
                    play(SFX_CLICK);
                }
                if (CheckCollisionPointRec(mouse, door_btn_rect())) {
                    room_set_lock(room, !room->actuators.doorLocked);
                    play(SFX_THUMP);
                }
            }
        } else {
            /* click on something in the room itself */
            Room *room = &b->rooms[b->selectedRoom];
            Hotspot hs[MAX_HOTSPOTS];
            int n = build_hotspots(room->id, hs);
            Ray ray = GetMouseRay(mouse, room_camera(room->id, (float)GetTime()));
            int hit = pick_hotspot(hs, n, ray);
            if (hit >= 0) click_hotspot(b, &hs[hit]);
        }
    }

    /* keyboard shortcuts for the selected room */
    if (!b->powerOut) {
        Room *room = &b->rooms[b->selectedRoom];
        if (IsKeyPressed(KEY_L)) { room_set_lights(room, !room->actuators.lightsOn); play(SFX_CLICK); }
        if (IsKeyPressed(KEY_D)) { room_set_lock(room, !room->actuators.doorLocked);  play(SFX_THUMP); }
    }

    /* TEMPORARY test key */
    if (IsKeyPressed(KEY_K)) game_lose(b);
}

/* eased animation values, updated once per frame */
static void update_anim(const Building *b, float dt)
{
    float ease = fminf(1.0f, dt * 6.0f);

    for (int i = 0; i < b->roomCount; i++) {
        float target = b->rooms[i].actuators.doorLocked ? 1.0f : 0.0f;
        doorClosed[i] += (target - doorClosed[i]) * ease;
    }
    for (int i = 0; i < 5; i++) {
        lockerAnim[i] += ((lockerOpen[i] ? 1.0f : 0.0f) - lockerAnim[i]) * ease;
    }
    fridgeAnim += ((fridgeOpen ? 1.0f : 0.0f) - fridgeAnim) * ease;

    if (fanOn) fanAngle += dt * 14.0f;
    if (duckBounce > 0.0f) duckBounce -= dt * 2.5f;
    if (switchFlash > 0.0f) switchFlash -= dt;
}

/* ------------------------------------------------------------------ */
/*  The room: walls, door, lamp, vent                                  */
/* ------------------------------------------------------------------ */

static void draw_floor(int roomId, const RoomLook *look, float k)
{
    if (roomId == 1 || roomId == 3) {
        /* tiled floor for the hallway and kitchen */
        for (int x = -5; x < 5; x++) {
            for (int z = -4; z < 4; z++) {
                float tone = (((x + z + 20) & 1) ? 1.0f : 0.78f) * k;
                DrawPlane((Vector3){ x + 0.5f, 0.0f, z + 0.5f }, (Vector2){ 1.0f, 1.0f },
                          shade(look->floor, tone));
            }
        }
    } else {
        DrawPlane((Vector3){ 0.0f, 0.0f, 0.0f }, (Vector2){ 10.0f, 8.0f }, shade(look->floor, k));
    }
}

static void draw_door(const Room *room, float k)
{
    float c = doorClosed[room->id];

    /* dark doorway */
    DrawCube((Vector3){ 2.5f, 1.1f, -3.88f }, 1.8f, 2.3f, 0.08f, (Color){ 4, 4, 6, 255 });

    /* metal shutter that drops down when the door is locked */
    float h = 2.2f * c;
    if (h > 0.03f) {
        DrawCube((Vector3){ 2.5f, 2.2f - h / 2.0f, -3.83f }, 1.6f, h, 0.1f,
                 shade((Color){ 170, 55, 45, 255 }, 0.5f + 0.5f * k));
        for (float y = 1.95f; y > 2.2f - h; y -= 0.3f) {
            DrawCube((Vector3){ 2.5f, y, -3.77f }, 1.6f, 0.04f, 0.03f,
                     shade((Color){ 60, 20, 15, 255 }, k));
        }
    }

    /* status light above the door */
    DrawSphere((Vector3){ 2.5f, 2.45f, -3.8f }, 0.07f, room->actuators.doorLocked ? RED : GREEN);
}

static void draw_lamp(const Room *room, float t)
{
    float sway = sinf(t * 0.9f) * 0.18f;      /* the lamp swings gently */
    DrawCylinderEx((Vector3){ 0.0f, 3.5f, 0.0f }, (Vector3){ sway, 3.25f, 0.0f }, 0.015f, 0.015f, 4, GRAY);
    DrawSphere((Vector3){ sway, 3.15f, 0.0f }, 0.22f, room->actuators.lightsOn ? YELLOW : DARKGRAY);
}

static void draw_vent(const Room *room, float k)
{
    Color c = room->actuators.hvacOn ? temp_colour(room->sensors.temperature) : GRAY;
    DrawCube((Vector3){ -2.5f, 2.7f, -3.85f }, 2.0f, 0.4f, 0.1f, c);
    for (int i = 0; i < 4; i++) {
        DrawCube((Vector3){ -2.5f, 2.58f + i * 0.08f, -3.78f }, 1.9f, 0.025f, 0.03f,
                 shade((Color){ 20, 20, 25, 255 }, k));
    }
}

/* ------------------------------------------------------------------ */
/*  Furniture and props, different in every room                       */
/* ------------------------------------------------------------------ */

static void draw_props(int id, float t, float k)
{
    Color wood  = shade((Color){ 110, 80, 50, 255 }, k);
    Color metal = shade((Color){ 70, 75, 85, 255 }, k);
    Color white = shade((Color){ 200, 200, 205, 255 }, k);
    Color green = shade((Color){ 50, 120, 60, 255 }, k);
    Color dark  = (Color){ 6, 6, 9, 255 };

    switch (id) {
        case 0: { /* office: desk with a fan and a rubber duck, cabinet, poster, moonlit window */
            DrawCube((Vector3){ 0.0f, 0.4f, 0.9f }, 4.0f, 0.8f, 1.2f, wood);
            DrawCube((Vector3){ -0.2f, 0.82f, 0.9f }, 0.5f, 0.02f, 0.35f, white);   /* papers */
            DrawCube((Vector3){ -3.8f, 0.7f, -3.2f }, 0.9f, 1.4f, 0.8f, metal);
            DrawCube((Vector3){ -3.0f, 1.8f, -3.85f }, 1.6f, 1.0f, 0.05f,
                     shade((Color){ 160, 60, 60, 255 }, k));
            DrawCube((Vector3){ -4.88f, 2.0f, -1.2f }, 0.05f, 1.4f, 1.8f, (Color){ 60, 90, 150, 255 });
            DrawCube((Vector3){ -4.85f, 2.0f, -1.2f }, 0.07f, 1.4f, 0.06f, metal);
            DrawCube((Vector3){ -4.85f, 2.0f, -1.2f }, 0.07f, 0.06f, 1.8f, metal);

            /* desk fan: the blades really turn */
            Vector3 hub = { 1.2f, 1.3f, 0.9f };
            DrawCylinder((Vector3){ 1.2f, 0.8f, 0.9f }, 0.12f, 0.15f, 0.45f, 10, metal);
            DrawSphere(hub, 0.07f, metal);
            for (int i = 0; i < 3; i++) {
                float a = fanAngle + i * 2.0f * PI / 3.0f;
                DrawCylinderEx(hub, (Vector3){ hub.x + cosf(a) * 0.3f, hub.y + sinf(a) * 0.3f, hub.z },
                               0.05f, 0.02f, 4, white);
            }

            /* rubber duck: squashes and bounces when clicked */
            float bounce = (duckBounce > 0.0f) ? sinf(duckBounce * 18.0f) * 0.12f * duckBounce : 0.0f;
            Color yellow = shade((Color){ 255, 210, 40, 255 }, 0.6f + 0.4f * k);
            DrawSphere((Vector3){ -0.9f, 0.95f + bounce, 1.0f }, 0.18f, yellow);
            DrawSphere((Vector3){ -0.9f, 1.15f + bounce, 1.05f }, 0.11f, yellow);
            DrawCube((Vector3){ -0.9f, 1.13f + bounce, 1.17f }, 0.1f, 0.03f, 0.08f, ORANGE);
            break;
        }
        case 1: { /* hallway: lockers with doors that slide open, runner, exit sign */
            DrawCube((Vector3){ 0.0f, 0.01f, 0.0f }, 1.4f, 0.02f, 8.0f, shade((Color){ 120, 40, 40, 255 }, k));
            for (int i = 0; i < 5; i++) {
                float z = -3.0f + 1.5f * i;
                float slide = lockerAnim[i] * 0.7f;
                DrawCube((Vector3){ -4.5f, 1.0f, z }, 0.8f, 2.0f, 1.2f, metal);
                DrawCube((Vector3){ -4.09f, 1.0f, z }, 0.02f, 1.85f, 1.05f, dark);
                DrawCube((Vector3){ -4.06f, 1.0f, z + slide }, 0.04f, 1.9f, 1.1f,
                         shade((Color){ 95, 100, 112, 255 }, k));
                for (int j = 0; j < 3; j++) {
                    DrawCube((Vector3){ -4.03f, 1.6f + j * 0.1f, z + slide }, 0.02f, 0.03f, 0.6f, dark);
                }
            }
            /* exit sign flickers now and then */
            bool lit = GetRandomValue(0, 40) != 0;
            DrawCube((Vector3){ 3.6f, 3.1f, -3.85f }, 0.9f, 0.3f, 0.08f,
                     lit ? (Color){ 40, 210, 90, 255 } : (Color){ 10, 50, 25, 255 });
            break;
        }
        case 2: { /* lobby: reception desk with a bell, bench, swaying plant, TV, ticking clock */
            DrawCube((Vector3){ -2.0f, 0.5f, -1.5f }, 3.2f, 1.0f, 1.0f, wood);
            DrawCylinder((Vector3){ -2.0f, 1.0f, -1.5f }, 0.18f, 0.18f, 0.05f, 10, metal);
            DrawSphere((Vector3){ -2.0f, 1.1f, -1.5f }, 0.12f, GOLD);
            DrawCube((Vector3){ -3.6f, 0.3f, 1.5f }, 1.0f, 0.6f, 3.0f, metal);

            float sway = sinf(t * 1.3f) * 0.08f;
            DrawCylinder((Vector3){ 3.8f, 0.0f, -2.5f }, 0.35f, 0.3f, 0.6f, 10, wood);
            DrawSphere((Vector3){ 3.8f + sway, 1.1f, -2.5f }, 0.6f, green);
            DrawSphere((Vector3){ 3.5f + sway * 1.5f, 1.5f, -2.4f }, 0.35f, green);
            DrawSphere((Vector3){ 4.1f + sway * 1.5f, 1.45f, -2.6f }, 0.3f, green);

            /* TV on the back wall: crackles with static when on */
            DrawCube((Vector3){ -1.0f, 1.9f, -3.85f }, 2.2f, 1.3f, 0.1f, (Color){ 12, 12, 14, 255 });
            Color screen = (Color){ 8, 8, 10, 255 };
            if (tvOn) {
                float f = 0.5f + 0.5f * sinf(t * 23.0f) * sinf(t * 7.0f);
                screen = (Color){ (unsigned char)(60 + 120 * f), (unsigned char)(70 + 120 * f),
                                  (unsigned char)(90 + 120 * f), 255 };
            }
            DrawCube((Vector3){ -1.0f, 1.9f, -3.78f }, 2.0f, 1.1f, 0.06f, screen);

            /* wall clock with a moving second hand */
            DrawCube((Vector3){ 3.2f, 2.6f, -3.85f }, 0.7f, 0.7f, 0.06f, white);
            float a = fmodf(t, 60.0f) / 60.0f * 2.0f * PI;
            DrawCylinderEx((Vector3){ 3.2f, 2.6f, -3.8f },
                           (Vector3){ 3.2f + sinf(a) * 0.28f, 2.6f + cosf(a) * 0.28f, -3.8f },
                           0.015f, 0.01f, 4, RED);
            break;
        }
        case 3: { /* kitchen: stove, dripping sink, island, hanging pots, fridge with a sliding door */
            DrawCube((Vector3){ -4.3f, 0.5f, 0.0f }, 1.0f, 1.0f, 6.0f, white);
            DrawCylinder((Vector3){ -4.3f, 1.0f, -1.0f }, 0.22f, 0.22f, 0.03f, 12, dark);
            DrawCylinder((Vector3){ -4.3f, 1.0f, -2.0f }, 0.22f, 0.22f, 0.03f, 12, dark);
            DrawCube((Vector3){ -4.3f, 1.01f, 1.5f }, 0.7f, 0.04f, 0.9f, shade((Color){ 40, 45, 55, 255 }, k));
            float drip = fmodf(t * 1.3f, 0.6f);
            DrawSphere((Vector3){ -4.3f, 1.7f - drip, 1.5f }, 0.03f, (Color){ 140, 190, 255, 255 });
            DrawCube((Vector3){ 0.0f, 0.5f, -0.5f }, 2.0f, 1.0f, 1.4f, wood);

            float sw = sinf(t * 1.1f) * 0.06f;
            for (int i = 0; i < 3; i++) {
                DrawCylinderEx((Vector3){ 0.5f + i * 0.5f, 3.4f, -0.5f },
                               (Vector3){ 0.5f + i * 0.5f + sw, 2.6f, -0.5f }, 0.01f, 0.01f, 4, GRAY);
                DrawCylinder((Vector3){ 0.5f + i * 0.5f + sw, 2.4f, -0.5f }, 0.18f, 0.14f, 0.22f, 10, metal);
            }

            /* fridge: the door slides open and a cold light spills out */
            float open = fridgeAnim;
            DrawCube((Vector3){ -3.5f, 1.0f, -3.4f }, 1.2f, 2.0f, 1.0f, white);
            DrawCube((Vector3){ -3.5f, 1.0f, -2.89f }, 1.0f, 1.8f, 0.02f, dark);
            if (open > 0.15f) {
                float fl = (GetRandomValue(0, 12) == 0) ? 0.6f : 1.0f;
                DrawCube((Vector3){ -3.5f, 1.0f, -2.95f }, 0.95f, 1.7f, 0.04f,
                         (Color){ (unsigned char)(200 * fl), (unsigned char)(230 * fl), (unsigned char)(255 * fl), 255 });
            }
            DrawCube((Vector3){ -3.5f - open * 1.1f, 1.0f, -2.86f }, 1.15f, 1.95f, 0.06f, white);
            DrawCube((Vector3){ -3.15f - open * 1.1f, 1.0f, -2.81f }, 0.04f, 0.6f, 0.04f, GRAY);
            break;
        }
        case 4: { /* server room: racks with blinking lights, spinning beacon, cables */
            float rate = rackFast ? 9.0f : 2.0f;
            for (int i = 0; i < 3; i++) {
                float x = -3.6f + i * 1.6f;
                DrawCube((Vector3){ x, 1.2f, -3.2f }, 1.2f, 2.4f, 1.0f, metal);
                for (int j = 0; j < 8; j++) {
                    bool on = sinf(t * rate + i * 1.7f + j * 0.9f) > 0.0f;
                    Color c = (j % 3 == 0) ? GREEN : (j % 3 == 1) ? (Color){ 60, 140, 255, 255 } : ORANGE;
                    DrawSphere((Vector3){ x - 0.4f + (j % 4) * 0.27f, 0.7f + (j / 4) * 0.5f, -2.66f },
                               0.05f, on ? c : shade(c, 0.15f));
                }
            }
            for (int i = 0; i < 3; i++) {
                DrawCylinderEx((Vector3){ -3.6f + i * 1.6f, 0.05f, -2.6f }, (Vector3){ 0.5f + i * 0.4f, 0.05f, 1.0f },
                               0.04f, 0.04f, 6, (Color){ 15, 15, 20, 255 });
            }
            float pulse = 0.5f + 0.5f * sinf(t * 6.0f);
            DrawSphere((Vector3){ 0.5f, 3.2f, -3.6f }, 0.14f,
                       (Color){ (unsigned char)(60 + 195 * pulse), 10, 10, 255 });
            break;
        }
        case 5: { /* storage: crates, shelf with boxes, an oil lantern */
            DrawCube((Vector3){ -3.5f, 0.5f, -2.5f }, 1.0f, 1.0f, 1.0f, wood);
            DrawCube((Vector3){ -3.5f, 1.5f, -2.5f }, 0.8f, 1.0f, 0.8f, wood);
            DrawCube((Vector3){ -2.2f, 0.5f, -3.0f }, 1.2f, 1.0f, 1.0f, wood);
            DrawCube((Vector3){ 0.5f, 0.5f, -3.2f }, 1.4f, 1.0f, 1.0f, wood);
            DrawCube((Vector3){ 3.8f, 1.0f, 0.0f }, 0.6f, 2.0f, 5.0f, metal);
            for (int i = 0; i < 4; i++) {
                DrawCube((Vector3){ 3.7f, 2.15f, -1.8f + i * 1.2f }, 0.4f, 0.3f, 0.7f, wood);
            }

            DrawCube((Vector3){ -2.2f, 1.1f, -3.0f }, 0.25f, 0.2f, 0.25f, metal);
            if (lanternOn) {
                float fl = 0.75f + 0.25f * sinf(t * 17.0f) * sinf(t * 5.0f);
                DrawCube((Vector3){ -2.2f, 1.3f, -3.0f }, 0.2f, 0.25f, 0.2f,
                         (Color){ (unsigned char)(255 * fl), (unsigned char)(170 * fl), (unsigned char)(60 * fl), 255 });
            } else {
                DrawCube((Vector3){ -2.2f, 1.3f, -3.0f }, 0.2f, 0.25f, 0.2f, (Color){ 40, 40, 40, 255 });
            }
            break;
        }
        default:
            break;
    }
}

/* ------------------------------------------------------------------ */
/*  Intruders: each kind has its own shape and idle animation          */
/* ------------------------------------------------------------------ */

static void draw_intruder(const Intruder *in, int slot, float t, float k)
{
    float x = -2.2f + slot * 1.2f;
    float z = 0.8f;
    float dim = 0.5f + 0.5f * k;   /* bodies fade in the dark, eyes keep glowing */
    Color body = shade((Color){ 30, 30, 30, 255 }, dim);
    float ph = t * 1.6f + slot;

    switch (in->type) {
        case INTRUDER_DRIFTER: { /* tall dark figure that sways, white eyes that flicker */
            float lean = sinf(ph) * 0.08f;
            DrawCylinder((Vector3){ x + lean, 0.0f, z }, 0.35f, 0.45f, 1.6f, 12, body);
            DrawSphere((Vector3){ x + lean * 1.8f, 1.9f, z }, 0.4f, shade((Color){ 60, 50, 40, 255 }, dim));
            if (GetRandomValue(0, 30) != 0) {
                DrawSphere((Vector3){ x + lean * 1.8f - 0.15f, 1.95f, z + 0.33f }, 0.07f, RAYWHITE);
                DrawSphere((Vector3){ x + lean * 1.8f + 0.15f, 1.95f, z + 0.33f }, 0.07f, RAYWHITE);
            }
            break;
        }
        case INTRUDER_SHADE: { /* very tall and thin, floats up and down, red eyes */
            float fy = sinf(t * 2.0f + slot) * 0.1f + 0.1f;
            DrawCylinder((Vector3){ x, fy, z }, 0.15f, 0.3f, 2.4f, 10, (Color){ 10, 10, 14, 255 });
            DrawSphere((Vector3){ x, 2.6f + fy, z }, 0.3f, (Color){ 10, 10, 14, 255 });
            DrawSphere((Vector3){ x - 0.1f, 2.65f + fy, z + 0.25f }, 0.06f, RED);
            DrawSphere((Vector3){ x + 0.1f, 2.65f + fy, z + 0.25f }, 0.06f, RED);
            break;
        }
        case INTRUDER_CRAWLER: { /* low and wide, legs skitter, orange eyes */
            DrawCube((Vector3){ x, 0.3f, z }, 1.2f, 0.3f, 0.8f, body);
            for (int i = 0; i < 4; i++) {
                float skit = sinf(t * 14.0f + i * 1.6f) * 0.08f;
                float lx = x + ((i % 2) ? 0.55f : -0.55f);
                float lz = z + ((i / 2) ? 0.35f : -0.35f) + skit;
                DrawCylinder((Vector3){ lx, 0.0f, lz }, 0.05f, 0.05f, 0.3f, 6, body);
            }
            DrawSphere((Vector3){ x - 0.2f, 0.35f, z + 0.42f }, 0.06f, ORANGE);
            DrawSphere((Vector3){ x + 0.2f, 0.35f, z + 0.42f }, 0.06f, ORANGE);
            break;
        }
        case INTRUDER_GLITCH: { /* jittering coloured cubes */
            for (int i = 0; i < 3; i++) {
                float j = t * 18.0f + i * 2.0f;
                DrawCube((Vector3){ x + sinf(j) * 0.15f, 0.8f + i * 0.5f, z + cosf(j * 1.3f) * 0.15f },
                         0.4f, 0.4f, 0.4f, (i % 2) ? MAGENTA : SKYBLUE);
            }
            break;
        }
        default: { /* leech: pulsing purple blob with green eyes */
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
}

/* ------------------------------------------------------------------ */
/*  Atmosphere: dust, eyes in the dark, haze                           */
/* ------------------------------------------------------------------ */

static void draw_dust(float t)
{
    for (int i = 0; i < 40; i++) {
        float fi = (float)i;
        float x = -4.5f + fmodf(fi * 2.37f + t * (0.15f + 0.05f * (i % 3)), 9.0f);
        float y = 0.2f + fmodf(fi * 1.13f + t * 0.06f, 3.0f) + sinf(t + fi) * 0.05f;
        float z = -3.5f + fmodf(fi * 3.71f, 7.0f);
        DrawSphere((Vector3){ x, y, z }, 0.018f, Fade(RAYWHITE, 0.35f));
    }
}

/* every so often a pair of eyes watches from the dark upper right corner */
static void draw_ambient_eyes(int roomId, float t)
{
    if (roomId == GOAL_ROOM) return;
    float c = fmodf(t + roomId * 3.1f, 13.0f);
    if (c > 9.0f && c < 9.4f) {
        DrawSphere((Vector3){ 3.9f, 2.1f, -3.6f }, 0.05f, RAYWHITE);
        DrawSphere((Vector3){ 3.65f, 2.1f, -3.6f }, 0.05f, RAYWHITE);
    }
}

/* thin see-through layers: the further away something is, the darker it looks */
static void draw_haze(void)
{
    static const float zs[5] = { -3.4f, -2.0f, -0.5f, 1.0f, 2.5f };   /* far to near */
    for (int i = 0; i < 5; i++) {
        DrawCube((Vector3){ 0.0f, 1.75f, zs[i] }, 10.4f, 3.6f, 0.02f, Fade(BLACK, 0.09f));
    }
}

/* ------------------------------------------------------------------ */
/*  The camera feed                                                    */
/* ------------------------------------------------------------------ */

static void draw_room_3d(const Building *b, float t, const Hotspot *hs, int hover)
{
    const Room *room = &b->rooms[b->selectedRoom];
    const RoomLook *look = &LOOKS[room->id];

    bool someoneHere = false;
    for (int i = 0; i < b->intruderCount; i++) {
        if (b->intruders[i].active && b->intruders[i].room == room->id) someoneHere = true;
    }

    /* lights flicker, and flicker harder when something is in the room */
    float k = room->actuators.lightsOn ? 1.0f : 0.22f;
    if (room->actuators.lightsOn && GetRandomValue(0, someoneHere ? 6 : 90) == 0) k *= 0.45f;

    BeginMode3D(room_camera(room->id, t));

    /* floor and walls (the camera side is left open) */
    draw_floor(room->id, look, k);
    Color wall = shade(look->wall, k);
    DrawCube((Vector3){ 0.0f, 1.75f, -4.0f }, 10.0f, 3.5f, 0.2f, wall);
    DrawCube((Vector3){ -5.0f, 1.75f, 0.0f }, 0.2f, 3.5f, 8.0f, wall);
    DrawCube((Vector3){ 5.0f, 1.75f, 0.0f }, 0.2f, 3.5f, 8.0f, wall);
    DrawCube((Vector3){ 0.0f, 0.15f, -3.9f }, 10.0f, 0.3f, 0.05f, shade((Color){ 25, 25, 30, 255 }, k));

    draw_door(room, k);
    draw_vent(room, k);
    draw_lamp(room, t);
    draw_props(room->id, t, k);

    /* every awake intruder standing in this room */
    for (int i = 0; i < b->intruderCount; i++) {
        const Intruder *in = &b->intruders[i];
        if (in->active && in->room == room->id) draw_intruder(in, i, t, k);
    }

    draw_ambient_eyes(room->id, t);
    draw_dust(t);

    /* outline whatever the mouse is over */
    if (hover >= 0) DrawBoundingBox(hs[hover].box, Fade(RAYWHITE, 0.9f));

    draw_haze();   /* must be last: see-through layers */

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

/* ------------------------------------------------------------------ */
/*  The jump scare                                                     */
/* ------------------------------------------------------------------ */

static void tri(Vector2 a, Vector2 b, Vector2 c, Color col)
{
    DrawTriangle(a, b, c, col);
    DrawTriangle(a, c, b, col);   /* both windings, so it always shows */
}

static void teeth(float cx, float y, float width, int count, float size, bool pointDown, Color col)
{
    float w = width / count;
    for (int i = 0; i < count; i++) {
        float x0 = cx - width / 2.0f + i * w;
        float tipY = pointDown ? y + size : y - size;
        tri((Vector2){ x0, y }, (Vector2){ x0 + w / 2.0f, tipY }, (Vector2){ x0 + w, y }, col);
    }
}

static void draw_scare_face(IntruderType type, float cx, float cy, float z)
{
    switch (type) {
        case INTRUDER_DRIFTER:
            DrawEllipse((int)cx, (int)cy, 190 * z, 230 * z, (Color){ 22, 20, 18, 255 });
            DrawEllipse((int)(cx - 75 * z), (int)(cy - 50 * z), 38 * z, 28 * z, RAYWHITE);
            DrawEllipse((int)(cx + 75 * z), (int)(cy - 50 * z), 38 * z, 28 * z, RAYWHITE);
            DrawCircle((int)(cx - 75 * z), (int)(cy - 50 * z), 8 * z, BLACK);
            DrawCircle((int)(cx + 75 * z), (int)(cy - 50 * z), 8 * z, BLACK);
            DrawEllipse((int)cx, (int)(cy + 110 * z), 120 * z, 60 * z, BLACK);
            teeth(cx, cy + 70 * z, 200 * z, 9, 45 * z, true, RAYWHITE);
            teeth(cx, cy + 150 * z, 200 * z, 9, 45 * z, false, RAYWHITE);
            break;
        case INTRUDER_SHADE:
            DrawEllipse((int)cx, (int)cy, 130 * z, 270 * z, (Color){ 8, 8, 12, 255 });
            DrawEllipse((int)(cx - 60 * z), (int)(cy - 70 * z), 36 * z, 13 * z, RED);
            DrawEllipse((int)(cx + 60 * z), (int)(cy - 70 * z), 36 * z, 13 * z, RED);
            DrawEllipse((int)(cx - 60 * z), (int)(cy - 70 * z), 18 * z, 6 * z, (Color){ 255, 160, 160, 255 });
            DrawEllipse((int)(cx + 60 * z), (int)(cy - 70 * z), 18 * z, 6 * z, (Color){ 255, 160, 160, 255 });
            for (int i = 0; i < 10; i++) {
                float x0 = cx - 90 * z + i * 20 * z;
                DrawLineEx((Vector2){ x0, cy + 120 * z + ((i % 2) ? 25 * z : -25 * z) },
                           (Vector2){ x0 + 20 * z, cy + 120 * z + ((i % 2) ? -25 * z : 25 * z) }, 5.0f, RED);
            }
            break;
        case INTRUDER_CRAWLER:
            DrawEllipse((int)cx, (int)(cy + 40 * z), 270 * z, 170 * z, (Color){ 25, 20, 15, 255 });
            for (int i = 0; i < 2; i++) {
                float s = (i == 0) ? -1.0f : 1.0f;
                DrawCircle((int)(cx + s * 60 * z), (int)(cy - 40 * z), 26 * z, ORANGE);
                DrawCircle((int)(cx + s * 140 * z), (int)(cy - 5 * z), 20 * z, ORANGE);
                DrawCircle((int)(cx + s * 60 * z), (int)(cy - 40 * z), 8 * z, BLACK);
                DrawCircle((int)(cx + s * 140 * z), (int)(cy - 5 * z), 6 * z, BLACK);
                tri((Vector2){ cx + s * 90 * z, cy + 90 * z }, (Vector2){ cx + s * 30 * z, cy + 200 * z },
                    (Vector2){ cx + s * 20 * z, cy + 90 * z }, RAYWHITE);
            }
            break;
        case INTRUDER_GLITCH:
            for (int i = 0; i < 26; i++) {
                Color col = (i % 3 == 0) ? MAGENTA : (i % 3 == 1) ? SKYBLUE : RAYWHITE;
                DrawRectangle((int)(cx + GetRandomValue(-220, 160) * z), (int)(cy + GetRandomValue(-220, 200) * z),
                              (int)(GetRandomValue(30, 140) * z), (int)(GetRandomValue(8, 40) * z), col);
            }
            DrawRectangle((int)(cx - 110 * z + GetRandomValue(-8, 8)), (int)(cy - 50 * z), (int)(70 * z), (int)(45 * z), RAYWHITE);
            DrawRectangle((int)(cx + 40 * z + GetRandomValue(-8, 8)), (int)(cy - 50 * z), (int)(70 * z), (int)(45 * z), RAYWHITE);
            DrawRectangle((int)(cx - 120 * z), (int)(cy + 80 * z), (int)(240 * z), (int)(30 * z), BLACK);
            break;
        default:
            DrawEllipse((int)cx, (int)cy, 240 * z, 200 * z, (Color){ 70, 30, 95, 255 });
            for (int i = 0; i < 7; i++) {
                DrawCircle((int)(cx + (i - 3) * 55 * z), (int)(cy - 70 * z + ((i % 2) ? 40 * z : 0)), 20 * z, LIME);
                DrawCircle((int)(cx + (i - 3) * 55 * z), (int)(cy - 70 * z + ((i % 2) ? 40 * z : 0)), 7 * z, BLACK);
            }
            DrawEllipse((int)cx, (int)(cy + 90 * z), 130 * z, 65 * z, (Color){ 20, 5, 25, 255 });
            for (int i = 0; i < 5; i++) {
                DrawLineEx((Vector2){ cx - 80 * z + i * 40 * z, cy + 140 * z },
                           (Vector2){ cx - 80 * z + i * 40 * z, cy + 220 * z }, 6.0f, LIME);
            }
            break;
    }
}

static void draw_jumpscare(IntruderType type, float t)
{
    bool strobe = ((int)(t * 28.0f)) % 2 == 0;
    ClearBackground(strobe ? (Color){ 40, 0, 0, 255 } : BLACK);

    float zoom = 0.35f + fminf(t / 0.22f, 1.0f) * 1.25f;          /* rushes at the camera */
    float cx = SCREEN_W / 2.0f + GetRandomValue(-22, 22);          /* screen shake */
    float cy = SCREEN_H / 2.0f + GetRandomValue(-22, 22);
    draw_scare_face(type, cx, cy, zoom);

    if (GetRandomValue(0, 5) == 0) DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(RAYWHITE, 0.35f));
    draw_camera_overlay();
}

/* ------------------------------------------------------------------ */
/*  Everything on screen                                               */
/* ------------------------------------------------------------------ */

void ui_draw(const Building *b)
{
    float dt = GetFrameTime();
    float t  = (float)GetTime();

    /* jump scare: plays once when an intruder gets in, before the game over text */
    if (b->state == GAME_PLAYING) {
        scareTime = -1.0f;
        scareDone = false;
    } else if (b->state == GAME_OVER && b->killer >= 0 && !scareDone) {
        if (scareTime < 0.0f) {
            scareTime = 0.0f;
            play(SFX_SCREECH);
        }
        scareTime += dt;
        if (scareTime >= SCARE_LEN) scareDone = true;
    }
    if (scareTime >= 0.0f && !scareDone) {
        if (audioOk) StopSound(sfx[SFX_HUM]);
        draw_jumpscare(b->intruders[b->killer].type, scareTime);
        return;
    }

    /* ambient hum while the night is running */
    if (audioOk) {
        if (b->state == GAME_PLAYING) {
            if (!IsSoundPlaying(sfx[SFX_HUM])) PlaySound(sfx[SFX_HUM]);
        } else {
            StopSound(sfx[SFX_HUM]);
        }
    }

    update_anim(b, dt);

    const Room *room = &b->rooms[b->selectedRoom];

    /* what is the mouse pointing at? (nothing while the dashboard is open) */
    Hotspot hs[MAX_HOTSPOTS];
    int n = build_hotspots(room->id, hs);
    int hover = -1;
    Vector2 mouse = GetMousePosition();
    if (b->state == GAME_PLAYING && !graphs_visible() && !on_hud(b, mouse)) {
        hover = pick_hotspot(hs, n, GetMouseRay(mouse, room_camera(room->id, t)));
    }
    SetMouseCursor(hover >= 0 ? MOUSE_CURSOR_POINTING_HAND : MOUSE_CURSOR_DEFAULT);

    ClearBackground(BLACK);
    draw_room_3d(b, t, hs, hover);
    draw_camera_overlay();

    /* danger: something is in the Hallway, right outside the Office */
    bool danger = false;
    for (int i = 0; i < b->intruderCount; i++) {
        if (b->intruders[i].active && b->intruders[i].room == 1) danger = true;
    }
    if (danger && b->state == GAME_PLAYING) {
        beatTimer -= dt;
        if (beatTimer <= 0.0f) {
            play(SFX_BEAT);
            beatTimer = 0.9f;
        }
        float pulse = fmaxf(0.0f, 1.0f - fmodf(t, 0.9f) * 2.2f);
        DrawRectangleGradientH(0, 0, 160, SCREEN_H, Fade(RED, 0.35f * pulse), Fade(RED, 0.0f));
        DrawRectangleGradientH(SCREEN_W - 160, 0, 160, SCREEN_H, Fade(RED, 0.0f), Fade(RED, 0.35f * pulse));
    }

    /* red emergency tint when the power is gone */
    if (b->powerOut) {
        DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(RED, 0.07f + 0.05f * sinf(t * 4.0f)));
    }

    /* burst of static when you change camera */
    if (switchFlash > 0.0f) {
        for (int i = 0; i < 500; i++) {
            DrawRectangle(GetRandomValue(0, SCREEN_W), GetRandomValue(0, SCREEN_H),
                          GetRandomValue(2, 14), 2, Fade(RAYWHITE, 0.35f));
        }
    }

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
    DrawText(TextFormat("TEMP %.1fC (TARGET %.1fC)   MOTION: %s   DOOR: %s   LIGHTS: %s   VENT: %s",
                        room->sensors.temperature, room->sensors.targetTemp,
                        room->sensors.motionDetected ? "YES" : "no",
                        room->actuators.doorLocked ? "locked" : "unlocked",
                        room->actuators.lightsOn ? "on" : "off",
                        room->actuators.hvacOn ? "open" : "sealed"),
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

    /* light, door and graphs buttons (bottom left) */
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

    Rectangle gb = graph_btn_rect();
    DrawRectangleRec(gb, Fade(BLACK, 0.6f));
    DrawRectangleLinesEx(gb, 2, SKYBLUE);
    DrawText("GRAPHS", (int)gb.x + 14, (int)gb.y + 10, 16, SKYBLUE);

    DrawText("Click things in the room. 1-6 cameras, L lights, D door, TAB graphs, K end night (test)",
             MARGIN, SCREEN_H - 20, 12, GRAY);

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
