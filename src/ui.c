#include <math.h>
#include <stdio.h>
#include <string.h>
#include "raylib.h"
#include "building.h"
#include "graphs.h"
#include "panel.h"
#include "dashboard.h"
#include "render.h"
#include "help.h"

/* Must match the window size in main.c */
#define SCREEN_W 800
#define SCREEN_H 450

#define MARGIN   20
#define BTN_H    28
#define BTN_Y    (SCREEN_H - 50)

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
    /* 3 kitchen */ { { 150, 150, 156, 255 }, { 168, 168, 164, 255 }, { -4.6f, 3.0f, 3.7f }, {  1.0f, 1.0f, -1.5f } },
    /* 4 server  */ { { 55,  60,  75, 255 }, {  50,  65,  95, 255 }, {  3.5f, 2.0f, 3.7f }, { -1.0f, 1.2f, -2.0f } },
    /* 5 storage */ { { 100, 85,  65, 255 }, { 115, 100,  80, 255 }, {  0.0f, 3.4f, 3.6f }, {  0.0f, 0.6f, -1.2f } },
    /* 6 spare   */ { { 90,  90, 100, 255 }, { 120, 115, 110, 255 }, {  4.6f, 3.2f, 3.7f }, { -0.5f, 1.0f, -1.5f } },
    /* 7 spare   */ { { 90,  90, 100, 255 }, { 120, 115, 110, 255 }, {  4.6f, 3.2f, 3.7f }, { -0.5f, 1.0f, -1.5f } }
};

/* ------------------------------------------------------------------ */
/*  Sound: everything is generated in code, so there are no files      */
/* ------------------------------------------------------------------ */

enum { SFX_HUM, SFX_CLICK, SFX_THUMP, SFX_SQUEAK, SFX_DING,
       SFX_SCREECH, SFX_BEAT, SFX_STATIC, SFX_ALARM, SFX_HISS,
       SFX_STEP, SFX_BANG, SFX_BOOM, SFX_ZAP, SFX_WHISPER, SFX_COUNT };

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
            case SFX_ALARM:   /* two harsh beeps a second */
                s = (fmodf(t, 0.5f) < 0.25f) ? ((sinf(2 * PI * 880 * t) > 0.0f) ? 0.5f : -0.5f) : 0.0f;
                break;
            case SFX_HISS:    /* water spraying */
                s = noise * 0.18f;
                break;
            case SFX_STEP:    /* a heavy footstep */
                s = sinf(2 * PI * (90.0f - 50.0f * t) * t) * expf(-t * 18.0f) + 0.15f * noise * expf(-t * 60.0f);
                break;
            case SFX_BANG:    /* a fist on a steel door */
                s = sinf(2 * PI * (110.0f - 60.0f * t) * t) * expf(-t * 7.0f) + 0.6f * noise * expf(-t * 25.0f);
                break;
            case SFX_BOOM:    /* flashover or backdraft */
                s = (0.9f * sinf(2 * PI * (60.0f - 30.0f * t) * t) + 0.7f * noise) * expf(-t * 3.5f);
                break;
            case SFX_ZAP:     /* a breaker tripping */
                s = (0.7f * noise + 0.5f * sinf(2 * PI * (2000.0f - 1500.0f * t) * t)) * expf(-t * 9.0f);
                break;
            case SFX_WHISPER: /* breathy noise, swelling and fading */
                s = noise * 0.25f * (0.5f + 0.5f * sinf(2 * PI * 3.0f * t)) * sinf(PI * t / seconds);
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

/* 1 = the game starts on the building overview (the controller); 0 = it starts on a camera */
#define START_IN_BUILDING_VIEW 1

void ui_init(void)
{
    dashboard_show(START_IN_BUILDING_VIEW);
    render_init(SCREEN_W, SCREEN_H);
    InitAudioDevice();
    audioOk = IsAudioDeviceReady();
    if (!audioOk) return;

    const float secs[SFX_COUNT] = { 2.0f, 0.06f, 0.4f, 0.15f, 0.9f, 1.5f, 0.7f, 0.25f, 1.0f, 1.0f, 0.3f, 0.5f, 1.2f, 0.4f, 1.5f };
    for (int i = 0; i < SFX_COUNT; i++) sfx[i] = synth(i, secs[i]);

    SetSoundVolume(sfx[SFX_HUM], 0.35f);
    SetSoundVolume(sfx[SFX_BEAT], 0.9f);
    SetSoundVolume(sfx[SFX_ALARM], 0.5f);
    SetSoundVolume(sfx[SFX_HISS], 0.4f);
    SetSoundVolume(sfx[SFX_BANG], 0.9f);
    SetSoundVolume(sfx[SFX_BOOM], 1.0f);
    SetSoundVolume(sfx[SFX_ZAP], 0.7f);
    SetSoundVolume(sfx[SFX_WHISPER], 0.5f);
}

void ui_close(void)
{
    render_close();
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
    HS_TV, HS_BELL, HS_FRIDGE, HS_RACK, HS_LANTERN, HS_SPRINKLER, HS_THERMOSTAT
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
static float shake = 0.0f, boomFlash = 0.0f, stare = 0.0f, whisperTimer = 0.0f;
static bool  paused = false;
static float dwell[MAX_INTRUDERS];   /* seconds each villain has been in the room you watch */

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

    /* every room has these five */
    n = add_hs(out, n, HS_DOOR, 0, box_at(2.5f, 1.1f, -3.85f, 1.6f, 2.2f, 0.3f));
    n = add_hs(out, n, HS_LAMP, 0, box_at(-1.4f, 3.35f, 1.0f, 2.3f, 0.4f, 0.8f));
    n = add_hs(out, n, HS_VENT, 0, box_at(-2.5f, 2.7f, -3.85f, 2.0f, 0.5f, 0.3f));
    n = add_hs(out, n, HS_SPRINKLER, 0, box_at(1.0f, 2.8f, -3.55f, 0.6f, 0.6f, 0.9f));
    n = add_hs(out, n, HS_THERMOSTAT, 0, box_at(4.25f, 1.5f, -3.8f, 0.4f, 0.5f, 0.3f));

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

/* the only two buttons on the main screen: everything else is in the control panel */
static Rectangle control_btn_rect(void)
{
    return (Rectangle){ MARGIN, BTN_Y, 120, BTN_H };
}

static Rectangle graph_btn_rect(void)
{
    return (Rectangle){ MARGIN + 128, BTN_Y, 100, BTN_H };
}

static Rectangle building_btn_rect(void)
{
    return (Rectangle){ MARGIN + 236, BTN_Y, 110, BTN_H };
}

static Rectangle help_btn_rect(void)
{
    return (Rectangle){ MARGIN + 354, BTN_Y, 34, BTN_H };
}

static const int SCENE_KEY[MAX_SCENES] = { KEY_Z, KEY_X, KEY_C, KEY_V };

static bool on_hud(const Building *b, Vector2 m)
{
    for (int i = 0; i < b->roomCount; i++) {
        if (CheckCollisionPointRec(m, button_rect(i))) return true;
    }
    return CheckCollisionPointRec(m, control_btn_rect())
        || CheckCollisionPointRec(m, graph_btn_rect())
        || CheckCollisionPointRec(m, building_btn_rect())
        || CheckCollisionPointRec(m, help_btn_rect())
        || panel_contains((int)m.x, (int)m.y);
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

/* a surface that gives off light: the lighting shader does not darken it (it recognises alpha 252) */
static Color glow(Color c)
{
    c.a = 252;
    return c;
}

static void select_room(Building *b, int i)
{
    if (i == b->selectedRoom) return;
    b->selectedRoom = i;
    switchFlash = 0.3f;
    play(SFX_STATIC);
}

void ui_click_sound(void)
{
    play(SFX_CLICK);
}

bool ui_paused(void)
{
    return paused || help_visible();
}

/* manual controls. Each one changes the room through the control API and writes to the event log */
static bool power_ok(const Building *b)
{
    if (b->powerOut) play(SFX_CLICK);   /* the controls are dead */
    return !b->powerOut;
}

/* a room's controls need power, a working breaker and a room that still exists */
static bool room_responds(Building *b, const Room *room, bool isDoor)
{
    if (!power_ok(b)) return false;
    if (room->burnt) { play(SFX_CLICK); return false; }
    if (room->breakerTripped) {
        log_event(b, "%s has no power", room->name);
        play(SFX_CLICK);
        return false;
    }
    if (isDoor && room->lockFault > 0.0f) {
        log_event(b, "%s door controller is not responding", room->name);
        play(SFX_CLICK);
        return false;
    }
    return true;
}

static void toggle_lights(Building *b, Room *room)
{
    if (!room_responds(b, room, false)) return;
    room_set_lights(room, !room->actuators.lightsOn);
    log_event(b, "%s lights %s", room->name, room->actuators.lightsOn ? "on" : "off");
    play(SFX_CLICK);
}

static void toggle_door(Building *b, Room *room)
{
    if (!room_responds(b, room, true)) return;
    room_set_lock(room, !room->actuators.doorLocked);
    log_event(b, "%s door %s", room->name, room->actuators.doorLocked ? "locked" : "unlocked");
    play(SFX_THUMP);
}

static void toggle_vent(Building *b, Room *room)
{
    if (!room_responds(b, room, false)) return;
    room_set_hvac(room, !room->actuators.hvacOn);
    log_event(b, "%s vent %s", room->name, room->actuators.hvacOn ? "opened" : "sealed");
    play(SFX_THUMP);
}

static void toggle_sprinklers(Building *b, Room *room)
{
    if (!room_responds(b, room, false)) return;
    room_set_sprinklers(room, !room->actuators.sprinklers);
    log_event(b, "%s sprinkler valve %s", room->name, room->actuators.sprinklers ? "opened" : "closed");
    if (room->actuators.sprinklers && room->fire > 0.0f && room->fireKind == FIRE_GREASE) {
        log_alert(b, "WARNING: water on a grease fire makes it worse!");
    }
    if (room->actuators.sprinklers && room->fireKind == FIRE_ELECTRICAL) {
        log_alert(b, "WARNING: water near live circuits shorts them out");
    }
    play(SFX_CLICK);
}

static void run_scene(Building *b, SceneId id)
{
    play(scene_activate(b, id) ? SFX_THUMP : SFX_CLICK);
}

static void click_hotspot(Building *b, const Hotspot *h)
{
    Room *room = &b->rooms[b->selectedRoom];

    switch (h->kind) {
        case HS_DOOR:      toggle_door(b, room);       break;
        case HS_LAMP:      toggle_lights(b, room);     break;
        case HS_VENT:      toggle_vent(b, room);       break;
        case HS_SPRINKLER: toggle_sprinklers(b, room); break;
        case HS_THERMOSTAT: panel_show_room(); play(SFX_CLICK); break;
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
    help_update();                                       /* H, the arrow keys and the help buttons */
    if (IsKeyPressed(KEY_P) && !help_visible()) paused = !paused;

    if (b->state != GAME_PLAYING) {
        paused = false;
        if (IsKeyPressed(KEY_R) && !help_visible()) building_init(b);   /* restart the night */
        return;
    }
    if (ui_paused()) return;                             /* nothing else reacts while paused */

    /* the control panel (SPACE) and the building overview (B) handle their own keys and clicks */
    bool dashWasOpen = dashboard_visible();
    if (!graphs_visible()) {
        panel_update(b);
        dashboard_update(b);
    }

    Vector2 mouse = GetMousePosition();

    /* keys 1-6 switch camera (and leave the building overview) */
    for (int i = 0; i < b->roomCount; i++) {
        if (IsKeyPressed(KEY_ONE + i)) { select_room(b, i); dashboard_show(false); }
    }

    /* scenes: Z lockdown, X eco, C fire response, V reset */
    for (int i = 0; i < MAX_SCENES; i++) {
        if (IsKeyPressed(SCENE_KEY[i])) run_scene(b, (SceneId)i);
    }

    /* shortcuts for the room you are viewing */
    {
        Room *room = &b->rooms[b->selectedRoom];
        if (IsKeyPressed(KEY_L)) toggle_lights(b, room);
        if (IsKeyPressed(KEY_D)) toggle_door(b, room);
        if (IsKeyPressed(KEY_S)) toggle_sprinklers(b, room);
    }

    /* while the dashboard is open, or the mouse is on the panel, the room ignores the mouse */
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !graphs_visible() && !dashWasOpen
        && !panel_contains((int)mouse.x, (int)mouse.y)) {

        if (on_hud(b, mouse)) {
            for (int i = 0; i < b->roomCount; i++) {
                if (CheckCollisionPointRec(mouse, button_rect(i))) select_room(b, i);
            }
            if (CheckCollisionPointRec(mouse, control_btn_rect())) { panel_toggle(); play(SFX_CLICK); }
            if (CheckCollisionPointRec(mouse, graph_btn_rect()))   graphs_toggle();
            if (CheckCollisionPointRec(mouse, building_btn_rect())) { dashboard_show(true); play(SFX_CLICK); }
            if (CheckCollisionPointRec(mouse, help_btn_rect()))     { help_show(true); play(SFX_CLICK); }
        } else if (!b->rooms[b->selectedRoom].burnt) {
            /* click on something in the room itself */
            Room *room = &b->rooms[b->selectedRoom];
            Hotspot hs[MAX_HOTSPOTS];
            int n = build_hotspots(room->id, hs);
            Ray ray = GetMouseRay(mouse, room_camera(room->id, (float)GetTime()));
            int hit = pick_hotspot(hs, n, ray);
            if (hit >= 0) click_hotspot(b, &hs[hit]);
        }
    }

    /* G switches the fancy graphics (lighting, glow, film look) on or off */
    if (IsKeyPressed(KEY_G)) render_toggle_quality();

    /* TEMPORARY test keys: F starts a fire here, E a random event, K ends the night */
    if (IsKeyPressed(KEY_F)) building_start_fire(b, b->selectedRoom);
    if (IsKeyPressed(KEY_E)) building_start_event(b, (EventId)GetRandomValue(0, EVENT_COUNT - 1));
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
    if (shake > 0.0f)       shake -= dt * 1.5f;
    if (boomFlash > 0.0f)   boomFlash -= dt;
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
    DrawSphere((Vector3){ 2.5f, 2.45f, -3.8f }, 0.07f, glow(room->actuators.doorLocked ? RED : GREEN));
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
/*  The smart devices on the wall: thermostat, motion sensor, door reader */
/* ------------------------------------------------------------------ */

static void draw_devices(const Room *room, float t, float k)
{
    bool powered = !room->breakerTripped;

    /* the wall thermostat: a screen that glows with the room's temperature, and a status light */
    DrawCube((Vector3){ 4.25f, 1.5f, -3.84f }, 0.30f, 0.40f, 0.06f, shade((Color){ 225, 225, 230, 255 }, 0.4f + 0.6f * k));
    DrawCube((Vector3){ 4.25f, 1.55f, -3.80f }, 0.20f, 0.16f, 0.02f,
             powered ? glow(temp_colour(room->sensors.temperature)) : (Color){ 15, 15, 18, 255 });
    DrawSphere((Vector3){ 4.25f, 1.37f, -3.80f }, 0.018f,
               !powered ? DARKGRAY : glow(room->actuators.hvacOn ? GREEN : ORANGE));

    /* the PIR motion sensor in the corner: its light turns red when it sees something */
    DrawSphere((Vector3){ 4.4f, 3.15f, -3.7f }, 0.10f, shade((Color){ 235, 235, 235, 255 }, 0.4f + 0.6f * k));
    DrawSphere((Vector3){ 4.34f, 3.12f, -3.62f }, 0.03f,
               (powered && room->sensors.motionDetected && ((int)(t * 6.0f)) % 2 == 0) ? glow(RED) : (Color){ 25, 30, 25, 255 });

    /* the card reader beside the door: red when locked, green when open */
    DrawCube((Vector3){ 1.45f, 1.25f, -3.84f }, 0.20f, 0.32f, 0.05f, (Color){ 25, 25, 30, 255 });
    DrawSphere((Vector3){ 1.45f, 1.36f, -3.80f }, 0.02f,
               !powered ? DARKGRAY : glow(room->actuators.doorLocked ? RED : GREEN));
}

/* ------------------------------------------------------------------ */
/*  Furniture and props, different in every room                       */
/* ------------------------------------------------------------------ */

static void draw_props(int id, float t, float k)
{
    Color wood  = shade((Color){ 96, 74, 55, 255 }, k);
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
            DrawCube((Vector3){ -1.0f, 1.9f, -3.78f }, 2.0f, 1.1f, 0.06f, tvOn ? glow(screen) : screen);

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

static Vector3 v3(float x, float y, float z)
{
    return (Vector3){ x, y, z };
}

/* a tapered tube between two points: arms, legs, tentacles */
static void limb(Vector3 a, Vector3 b, float r0, float r1, Color c)
{
    DrawCylinderEx(a, b, r0, r1, 6, c);
}

/* a very cheap sphere, for droplets and sparks */
static void speck(Vector3 p, float r, Color c)
{
    DrawSphereEx(p, r, 4, 4, c);
}

/* which way a creature turns to look straight at the camera */
typedef struct {
    Vector3 face;   /* from the creature toward the camera */
    Vector3 side;   /* to its left, as the camera sees it */
} Facing;

static Facing facing_to(Vector3 cam, float x, float z)
{
    float dx = cam.x - x;
    float dz = cam.z - z;
    float len = sqrtf(dx * dx + dz * dz);
    if (len < 0.001f) len = 1.0f;

    Facing f;
    f.face = v3(dx / len, 0.0f, dz / len);
    f.side = v3(-dz / len, 0.0f, dx / len);
    return f;
}

static Vector3 eye_at(Vector3 head, Facing f, float forward, float sideways, float up)
{
    return v3(head.x + f.face.x * forward + f.side.x * sideways,
              head.y + up,
              head.z + f.face.z * forward + f.side.z * sideways);
}

static void draw_shadow(float x, float z, float r)
{
    DrawCylinder(v3(x, 0.014f, z), r, r, 0.004f, 14, Fade(BLACK, 0.55f));
}

/* THE DRIFTER: a hunched figure whose arms hang below its knees, with a jaw that never quite closes */
static void draw_walker(float x, float z, float t, int slot, float dim, Facing f)
{
    float ph      = t * 1.4f + slot;
    float sway    = sinf(ph) * 0.06f;
    float stride  = sinf(ph * 1.7f) * 0.14f;
    float breathe = sinf(t * 2.2f + slot) * 0.02f;
    Color cloth = shade((Color){ 36, 34, 38, 255 }, dim);
    Color skin  = shade((Color){ 150, 148, 140, 255 }, dim);
    Color dark  = shade((Color){ 18, 18, 20, 255 }, dim);

    limb(v3(x - 0.16f + sway, 0.95f, z), v3(x - 0.18f, 0.0f, z + stride), 0.11f, 0.07f, dark);
    limb(v3(x + 0.16f + sway, 0.95f, z), v3(x + 0.18f, 0.0f, z - stride), 0.11f, 0.07f, dark);

    Vector3 chest = v3(x + sway * 1.5f, 1.72f + breathe, z - 0.12f);
    limb(v3(x + sway, 0.95f, z), chest, 0.26f, 0.20f, cloth);

    /* arms far too long, with claws */
    float swing = sinf(ph * 1.7f) * 0.1f;
    for (int s = -1; s <= 1; s += 2) {
        Vector3 sh = v3(chest.x + s * 0.30f, 1.62f, chest.z);
        Vector3 el = v3(sh.x + s * 0.08f, 1.05f, z + 0.05f + swing * s);
        Vector3 ha = v3(sh.x + s * 0.06f, 0.32f, z + 0.12f + swing * 1.6f * s);
        limb(sh, el, 0.07f, 0.05f, skin);
        limb(el, ha, 0.05f, 0.035f, skin);
        for (int c = 0; c < 3; c++) {
            limb(ha, v3(ha.x + (c - 1) * 0.06f, ha.y - 0.24f, ha.z + 0.05f), 0.02f, 0.004f, skin);
        }
    }

    /* neck, tilted head, a long jaw */
    Vector3 neck = v3(chest.x, chest.y + 0.12f, chest.z);
    float tilt = sinf(t * 0.7f + slot) * 0.12f;
    Vector3 head = v3(neck.x + tilt, neck.y + 0.30f, neck.z + 0.14f);
    limb(neck, head, 0.07f, 0.06f, skin);
    DrawSphere(head, 0.20f, skin);
    DrawSphere(v3(head.x, head.y - 0.13f, head.z + 0.02f), 0.14f, skin);

    /* eyes that flicker and follow the camera */
    if (GetRandomValue(0, 40) != 0) {
        Vector3 e1 = eye_at(head, f, 0.17f, -0.08f, 0.03f);
        Vector3 e2 = eye_at(head, f, 0.17f,  0.08f, 0.03f);
        DrawSphere(e1, 0.045f, glow(RAYWHITE));
        DrawSphere(e2, 0.045f, glow(RAYWHITE));
        glow_queue(e1, 0.22f, (Color){ 255, 255, 255, 110 });
        glow_queue(e2, 0.22f, (Color){ 255, 255, 255, 110 });
    }
    /* the mouth opens and closes, full of little teeth */
    float jaw = 0.5f + 0.5f * sinf(t * 3.0f + slot);
    Vector3 m = eye_at(head, f, 0.16f, 0.0f, -0.12f - jaw * 0.04f);
    DrawSphere(m, 0.05f + jaw * 0.03f, BLACK);
    for (int k = -2; k <= 2; k++) {
        speck(eye_at(head, f, 0.18f, k * 0.03f, -0.10f), 0.014f, RAYWHITE);
    }
    draw_shadow(x, z, 0.5f);
}

/* THE SHADE: tall, hooded, hovering, with tendrils along the floor and red eyes that never blink */
static void draw_shade(float x, float z, float t, int slot, Facing f)
{
    Color black = (Color){ 8, 8, 12, 255 };
    float fy = 0.15f + sinf(t * 1.6f + slot) * 0.08f;

    limb(v3(x, fy, z), v3(x, fy + 2.1f, z), 0.38f, 0.14f, black);          /* the robe */
    Vector3 head = v3(x + sinf(t * 0.8f) * 0.05f, fy + 2.35f, z + 0.05f);
    DrawSphere(head, 0.20f, black);
    DrawCylinder(v3(head.x, head.y - 0.22f, head.z - 0.02f), 0.0f, 0.30f, 0.55f, 8, black);   /* the hood */

    for (int s = -1; s <= 1; s += 2) {                                      /* arms to the floor */
        Vector3 sh   = v3(x + s * 0.20f, fy + 1.9f, z);
        Vector3 hand = v3(x + s * (0.42f + sinf(t * 1.3f + s) * 0.04f), 0.05f, z + 0.15f + sinf(t * 0.9f) * 0.05f);
        limb(sh, hand, 0.06f, 0.03f, black);
        for (int c = 0; c < 4; c++) {
            limb(hand, v3(hand.x + (c - 1.5f) * 0.04f, 0.0f, hand.z + 0.25f), 0.012f, 0.003f, black);
        }
    }
    for (int i = 0; i < 6; i++) {                                           /* tendrils of shadow */
        float a = i * 1.047f + t * 0.5f;
        float reach = 0.6f + 0.2f * sinf(t * 2.0f + i);
        limb(v3(x, fy + 0.2f, z), v3(x + cosf(a) * reach, 0.05f, z + sinf(a) * reach), 0.04f, 0.005f, black);
    }

    Vector3 e1 = eye_at(head, f, 0.17f, -0.08f, 0.02f);
    Vector3 e2 = eye_at(head, f, 0.17f,  0.08f, 0.02f);
    DrawSphere(e1, 0.035f, glow(RED));
    DrawSphere(e2, 0.035f, glow(RED));
    glow_queue(e1, 0.45f, (Color){ 255, 30, 20, 190 });      /* a red glow that never blinks */
    glow_queue(e2, 0.45f, (Color){ 255, 30, 20, 190 });
    draw_shadow(x, z, 0.7f);
}

/* THE CRAWLER: six jointed legs that skitter, a cluster of orange eyes, mandibles */
static void draw_crawler(float x, float z, float t, int slot, float dim, Facing f)
{
    Color body = shade((Color){ 28, 24, 20, 255 }, dim);
    Color bone = shade((Color){ 190, 180, 160, 255 }, dim);
    float bob = sinf(t * 9.0f + slot) * 0.02f;
    Vector3 c = v3(x, 0.42f + bob, z);

    DrawSphere(c, 0.32f, body);
    Vector3 head = v3(c.x + f.face.x * 0.32f, c.y + 0.04f, c.z + f.face.z * 0.32f);
    DrawSphere(head, 0.20f, body);

    for (int i = 0; i < 6; i++) {
        float side  = (i % 2) ? 1.0f : -1.0f;
        float row   = (float)(i / 2 - 1) * 0.28f;
        float phase = t * 11.0f + i * 1.7f;
        Vector3 hip  = v3(c.x + side * 0.22f, c.y, c.z + row);
        Vector3 knee = v3(hip.x + side * 0.55f, c.y + 0.35f + sinf(phase) * 0.06f, hip.z + row * 0.3f);
        Vector3 foot = v3(hip.x + side * 0.95f + sinf(phase) * 0.05f, 0.0f, hip.z + row * 0.8f + cosf(phase) * 0.12f);
        limb(hip, knee, 0.035f, 0.03f, body);
        limb(knee, foot, 0.03f, 0.008f, body);
    }
    for (int e = 0; e < 4; e++) {
        Vector3 ep = eye_at(head, f, 0.17f, (e - 1.5f) * 0.06f, 0.04f + (e % 2) * 0.05f);
        DrawSphere(ep, 0.025f, glow(ORANGE));
        glow_queue(ep, 0.22f, (Color){ 255, 140, 20, 170 });
    }
    limb(eye_at(head, f, 0.14f, -0.06f, -0.04f), eye_at(head, f, 0.30f, -0.02f, -0.14f), 0.025f, 0.004f, bone);
    limb(eye_at(head, f, 0.14f,  0.06f, -0.04f), eye_at(head, f, 0.30f,  0.02f, -0.14f), 0.025f, 0.004f, bone);
    draw_shadow(x, z, 0.8f);
}

/* THE GLITCH: a person made of jittering blocks that tear, vanish and change colour */
static void draw_glitch(float x, float z, float t, int slot)
{
    static const float parts[9][5] = {
        /* x      y     sx    sy    sz */
        {  0.00f, 1.85f, 0.30f, 0.30f, 0.30f },   /* head */
        {  0.00f, 1.45f, 0.50f, 0.40f, 0.25f },   /* chest */
        {  0.00f, 1.00f, 0.35f, 0.40f, 0.20f },   /* belly */
        { -0.38f, 1.30f, 0.14f, 0.70f, 0.14f },   /* arms */
        {  0.38f, 1.30f, 0.14f, 0.70f, 0.14f },
        { -0.14f, 0.45f, 0.16f, 0.90f, 0.16f },   /* legs */
        {  0.14f, 0.45f, 0.16f, 0.90f, 0.16f },
        { -0.55f, 1.90f, 0.12f, 0.12f, 0.12f },   /* floating shards */
        {  0.60f, 0.90f, 0.10f, 0.10f, 0.10f }
    };
    for (int i = 0; i < 9; i++) {
        if (GetRandomValue(0, 9) == 0) continue;                           /* a part drops out */
        float j = t * 20.0f + i * 3.0f + slot;
        float tear = (GetRandomValue(0, 12) == 0) ? GetRandomValue(-30, 30) / 100.0f : 0.0f;
        Vector3 p = v3(x + parts[i][0] + sinf(j) * 0.04f + tear, parts[i][1] + cosf(j * 1.3f) * 0.03f,
                       z + sinf(j * 0.7f) * 0.04f);
        int pick = ((int)(t * 8.0f) + i) % 3;
        Color c = (pick == 0) ? MAGENTA : (pick == 1) ? SKYBLUE : RAYWHITE;
        DrawCube(p, parts[i][2], parts[i][3], parts[i][4], glow(c));
        glow_queue(p, 0.5f, Fade(c, 0.35f));
    }
    DrawCube(v3(x - 0.08f, 1.88f, z + 0.16f), 0.07f, 0.04f, 0.02f, glow(RAYWHITE));   /* eyes */
    DrawCube(v3(x + 0.08f, 1.88f, z + 0.16f), 0.07f, 0.04f, 0.02f, glow(RAYWHITE));
    draw_shadow(x, z, 0.5f);
}

/* THE LEECH: a pulsing mass with tentacles on the floor and a lot of green eyes */
static void draw_leech(float x, float z, float t, int slot, float dim, Facing f)
{
    Color flesh = shade((Color){ 88, 38, 108, 255 }, dim);
    float pulse = 1.0f + sinf(t * 3.0f + slot) * 0.06f;
    Vector3 c = v3(x, 0.6f, z);

    DrawSphere(c, 0.62f * pulse, flesh);
    DrawSphere(v3(x + 0.35f, 0.35f, z + 0.10f), 0.35f * pulse, flesh);
    DrawSphere(v3(x - 0.38f, 0.30f, z - 0.05f), 0.30f * pulse, flesh);

    for (int i = 0; i < 8; i++) {
        float a    = i * 0.785f;
        float wave = sinf(t * 2.0f + i) * 0.25f;
        Vector3 a0 = v3(x + cosf(a) * 0.45f,                  0.18f, z + sinf(a) * 0.45f);
        Vector3 a1 = v3(x + cosf(a + wave) * 0.95f,           0.10f, z + sinf(a + wave) * 0.95f);
        Vector3 a2 = v3(x + cosf(a + wave * 1.6f) * 1.40f,    0.03f, z + sinf(a + wave * 1.6f) * 1.40f);
        limb(a0, a1, 0.08f, 0.05f, flesh);
        limb(a1, a2, 0.05f, 0.01f, flesh);
    }
    for (int e = 0; e < 6; e++) {
        Vector3 p = eye_at(c, f, 0.55f, (e - 2.5f) * 0.14f, 0.10f + (e % 3) * 0.10f);
        DrawSphere(p, 0.04f, glow(LIME));
        glow_queue(p, 0.25f, (Color){ 120, 255, 60, 150 });
        DrawSphere(eye_at(c, f, 0.59f, (e - 2.5f) * 0.14f, 0.10f + (e % 3) * 0.10f), 0.016f, BLACK);
    }
    /* slime dripping off it */
    speck(v3(x + f.face.x * 0.5f, 0.45f - fmodf(t * 0.7f + slot, 0.4f), z + f.face.z * 0.5f), 0.02f, LIME);
    draw_shadow(x, z, 1.0f);
}

/* creep = how far it has crept toward the camera while you kept watching */
static void draw_intruder(const Intruder *in, int slot, float t, float k, Vector3 cam, float creep)
{
    float x = -2.4f + slot * 1.2f + sinf(t * 0.3f + slot) * 0.15f;
    float z = 0.2f + creep;
    float dim = 0.5f + 0.5f * k;   /* bodies fade in the dark, eyes keep glowing */
    Facing f = facing_to(cam, x, z);

    switch (in->type) {
        case INTRUDER_DRIFTER: draw_walker(x, z, t, slot, dim, f);  break;
        case INTRUDER_SHADE:   draw_shade(x, z, t, slot, f);        break;
        case INTRUDER_CRAWLER: draw_crawler(x, z, t, slot, dim, f); break;
        case INTRUDER_GLITCH:  draw_glitch(x, z, t, slot);          break;
        default:               draw_leech(x, z, t, slot, dim, f);   break;
    }
}

/* ------------------------------------------------------------------ */
/*  Light, fog and the room itself                                     */
/* ------------------------------------------------------------------ */

static Vector3 vscale(Vector3 v, float s)
{
    return v3(v.x * s, v.y * s, v.z * s);
}

static Vector3 vmix(Vector3 a, Vector3 b, float f)
{
    return v3(a.x + (b.x - a.x) * f, a.y + (b.y - a.y) * f, a.z + (b.z - a.z) * f);
}

/* where flames and smoke show up in a burning room */
static const Vector3 FIRE_SPOTS[6] = {
    { -1.5f, 0.0f, 0.2f }, { 1.2f, 0.0f, -0.8f }, { -2.5f, 0.0f, -1.0f },
    {  2.0f, 0.0f, 0.8f }, { 0.0f, 0.0f, -2.0f }, { -3.2f, 0.0f, 1.0f }
};

/* the sprinkler head on the back wall, with its red glass bulb */
#define SPR_X  1.0f
#define SPR_Y  2.75f
#define SPR_Z -3.3f

/* the lights in the room: two ceiling panels, the glow of a fire, and an alarm light */
static void build_light(const Building *b, const Room *room, float t, bool someoneHere, SceneLight *L)
{
    memset(L, 0, sizeof *L);
    bool lampsOn = room->actuators.lightsOn && !room->breakerTripped;
    float flick = 1.0f;
    if (lampsOn && GetRandomValue(0, someoneHere ? 6 : 90) == 0) flick = 0.35f;

    if (lampsOn)                   L->ambient = v3(0.17f, 0.18f, 0.21f);
    else if (room->breakerTripped) L->ambient = v3(0.06f, 0.065f, 0.08f);
    else                           L->ambient = v3(0.09f, 0.10f, 0.13f);

    if (lampsOn) {
        Vector3 warm = v3(1.0f, 0.88f, 0.70f);
        L->pos[0] = v3(-1.4f, 3.3f, 1.0f);
        L->col[0] = vscale(warm, 2.0f * flick);
        L->pos[1] = v3(1.6f, 3.3f, -1.6f);
        L->col[1] = vscale(warm, 1.7f * flick);
    }
    if (!lampsOn && !b->powerOut) {           /* the emergency light has its own battery */
        L->pos[3] = v3(0.0f, 3.3f, 0.0f);
        L->col[3] = v3(0.45f, 0.55f, 0.45f);
    }
    if (room->fire > 0.0f) {
        float fl = 0.75f + 0.25f * sinf(t * 13.0f) * sinf(t * 7.0f) + 0.10f * sinf(t * 31.0f);
        L->pos[2] = v3(-0.8f, 1.1f, -0.2f);
        L->col[2] = vscale(v3(1.0f, 0.42f, 0.10f), (0.9f + 3.2f * room->fire) * fl);
    }
    if (b->powerOut) {
        float pulse = 0.6f + 0.4f * sinf(t * 2.0f);
        L->pos[3] = v3(0.0f, 3.0f, 0.0f);
        L->col[3] = vscale(v3(1.0f, 0.08f, 0.05f), 1.6f * pulse);
    } else if (room->sensors.smokeDetected) {
        L->pos[3] = v3(0.0f, 3.0f, 0.0f);
        L->col[3] = vscale(v3(1.0f, 0.10f, 0.05f), (((int)(t * 3.0f)) % 2 == 0) ? 1.4f : 0.2f);
    } else if (someoneHere && !lampsOn) {
        L->pos[3] = v3(0.0f, 2.2f, 2.0f);                  /* the camera's own faint night light */
        L->col[3] = v3(0.25f, 0.40f, 0.60f);
    }

    float smoke = room->sensors.smoke;
    L->fog = 0.035f + 0.35f * smoke;
    L->fogColour = vmix(v3(0.02f, 0.025f, 0.035f), v3(0.15f, 0.14f, 0.14f), smoke);
    if (room->fire > 0.0f) L->fogColour = vmix(L->fogColour, v3(0.18f, 0.07f, 0.02f), room->fire * 0.4f);
    L->wet = room->wet;
}

/* the walls, the ceiling with its tile grid, the frame round the door */
static void draw_shell(const Room *room, const RoomLook *look, float k)
{
    draw_floor(room->id, look, k);

    Color wall = shade(look->wall, k);
    Color trim = shade((Color){ 28, 28, 34, 255 }, k);
    DrawCube(v3(0.0f, 1.75f, -4.0f), 10.0f, 3.5f, 0.2f, wall);
    DrawCube(v3(-5.0f, 1.75f, 0.0f), 0.2f, 3.5f, 8.0f, wall);
    DrawCube(v3(5.0f, 1.75f, 0.0f), 0.2f, 3.5f, 8.0f, wall);
    DrawCube(v3(0.0f, 1.75f, 4.0f), 10.0f, 3.5f, 0.2f, wall);          /* the wall behind the camera */

    /* skirting boards round the room */
    DrawCube(v3(0.0f, 0.12f, -3.88f), 9.8f, 0.24f, 0.05f, trim);
    DrawCube(v3(-4.88f, 0.12f, 0.0f), 0.05f, 0.24f, 7.8f, trim);
    DrawCube(v3(4.88f, 0.12f, 0.0f), 0.05f, 0.24f, 7.8f, trim);

    /* the ceiling, with its grid of tiles */
    DrawCube(v3(0.0f, 3.55f, 0.0f), 10.0f, 0.1f, 8.0f, shade((Color){ 200, 200, 205, 255 }, k));
    for (int i = -3; i <= 3; i++) {
        DrawCube(v3(i * 1.25f, 3.49f, 0.0f), 0.03f, 0.02f, 8.0f, trim);
    }
    for (int i = -3; i <= 3; i++) {
        DrawCube(v3(0.0f, 3.49f, i * 1.1f), 10.0f, 0.02f, 0.03f, trim);
    }

    /* the frame round the door, and its handle */
    Color frame = shade((Color){ 60, 62, 70, 255 }, k);
    DrawCube(v3(1.62f, 1.15f, -3.84f), 0.12f, 2.3f, 0.14f, frame);
    DrawCube(v3(3.38f, 1.15f, -3.84f), 0.12f, 2.3f, 0.14f, frame);
    DrawCube(v3(2.5f, 2.3f, -3.84f), 1.88f, 0.12f, 0.14f, frame);
    DrawCube(v3(3.1f, 1.05f, -3.75f), 0.05f, 0.14f, 0.05f, shade((Color){ 190, 190, 195, 255 }, k));
}

/* ceiling light panels and the little sprinkler heads in the ceiling */
static void draw_fixtures(const Room *room)
{
    bool on = room->actuators.lightsOn && !room->breakerTripped;
    Color panel = on ? glow((Color){ 255, 242, 215, 255 }) : (Color){ 70, 72, 78, 255 };
    Color rim = (Color){ 90, 92, 100, 255 };

    DrawCube(v3(-1.4f, 3.46f, 1.0f), 2.3f, 0.03f, 0.78f, rim);
    DrawCube(v3(-1.4f, 3.45f, 1.0f), 2.2f, 0.03f, 0.68f, panel);
    DrawCube(v3(1.6f, 3.46f, -1.6f), 2.3f, 0.03f, 0.78f, rim);
    DrawCube(v3(1.6f, 3.45f, -1.6f), 2.2f, 0.03f, 0.68f, panel);

    for (int i = 0; i < 4; i++) {
        float x = (i % 2) ? 3.2f : -3.2f;
        float z = (i / 2) ? 2.4f : -2.2f;
        DrawCylinder(v3(x, 3.46f, z), 0.04f, 0.04f, 0.04f, 6, GRAY);
        if (!room->headFused) DrawSphere(v3(x, 3.43f, z), 0.025f, glow((Color){ 220, 30, 30, 255 }));
    }
}

/* little things that make a room look lived in */
static void draw_details(const Room *room, float t, float k)
{
    int id = room->id;
    Color white = shade((Color){ 215, 215, 220, 255 }, k);
    Color dark  = shade((Color){ 25, 25, 30, 255 }, k);

    /* a socket and a light switch near the door */
    DrawCube(v3(0.95f, 0.35f, -3.85f), 0.12f, 0.12f, 0.03f, white);
    DrawCube(v3(0.95f, 1.25f, -3.85f), 0.08f, 0.12f, 0.03f, white);

    /* a fire extinguisher on the wall (the real ones are in the corridors and kitchens) */
    if (id == 1 || id == 2 || id == 3 || id == 5) {
        DrawCube(v3(4.62f, 0.55f, -3.85f), 0.34f, 0.74f, 0.03f, white);
        DrawCylinder(v3(4.62f, 0.22f, -3.76f), 0.09f, 0.09f, 0.5f, 10, (Color){ 190, 25, 25, 255 });
        DrawCylinder(v3(4.62f, 0.72f, -3.76f), 0.035f, 0.035f, 0.08f, 8, dark);
        DrawCube(v3(4.62f, 0.82f, -3.76f), 0.12f, 0.03f, 0.04f, dark);
    }

    /* the security camera in the corner: it has a little red light */
    DrawSphere(v3(-4.5f, 3.3f, -3.6f), 0.14f, dark);
    DrawSphere(v3(-4.43f, 3.27f, -3.5f), 0.035f, ((int)(t * 1.5f)) % 2 == 0 ? glow(RED) : dark);

    /* rooms with something to look out of */
    if (id == 2) {
        DrawCube(v3(-4.88f, 1.9f, 0.5f), 0.05f, 1.5f, 3.2f, (Color){ 12, 18, 34, 255 });
        for (int i = 0; i < 9; i++) {      /* the lights of the city */
            DrawCube(v3(-4.84f, 1.4f + (i % 3) * 0.35f, -0.6f + i * 0.28f), 0.02f, 0.04f, 0.05f,
                     glow((Color){ 255, (unsigned char)(200 - i * 8), 120, 255 }));
        }
        DrawCube(v3(-4.84f, 1.9f, 0.5f), 0.05f, 1.54f, 0.06f, dark);
        DrawCube(v3(-4.84f, 1.9f, 0.5f), 0.05f, 0.06f, 3.2f, dark);
    }
    if (id == 1) {                                  /* a notice board in the corridor */
        DrawCube(v3(-1.6f, 1.7f, -3.85f), 1.4f, 0.9f, 0.03f, shade((Color){ 150, 110, 70, 255 }, k));
        for (int i = 0; i < 4; i++) {
            DrawCube(v3(-2.0f + i * 0.35f, 1.7f + ((i % 2) ? 0.12f : -0.1f), -3.82f), 0.22f, 0.3f, 0.01f, white);
        }
    }
    if (id == 4) {                                  /* the air conditioner that keeps the servers cool */
        DrawCube(v3(3.9f, 2.6f, -3.8f), 1.6f, 0.7f, 0.3f, shade((Color){ 200, 205, 210, 255 }, k));
        for (int i = 0; i < 5; i++) DrawCube(v3(3.9f, 2.4f + i * 0.08f, -3.64f), 1.4f, 0.02f, 0.02f, dark);
        DrawCube(v3(0.0f, 3.2f, -1.0f), 8.0f, 0.12f, 0.5f, shade((Color){ 90, 95, 105, 255 }, k));   /* the cable tray */
    }
}

/* the sprinkler's pipe, plate and glass bulb (the water itself is drawn as sprites) */
static void draw_sprinkler_head(const Room *room)
{
    Vector3 head = v3(SPR_X, SPR_Y, SPR_Z);
    limb(v3(SPR_X, 2.8f, -3.85f), head, 0.04f, 0.04f, GRAY);
    DrawCylinder(v3(SPR_X, SPR_Y - 0.10f, SPR_Z), 0.09f, 0.09f, 0.02f, 8, GRAY);
    limb(v3(SPR_X - 0.05f, SPR_Y, SPR_Z), v3(SPR_X - 0.05f, SPR_Y - 0.10f, SPR_Z), 0.008f, 0.008f, GRAY);
    limb(v3(SPR_X + 0.05f, SPR_Y, SPR_Z), v3(SPR_X + 0.05f, SPR_Y - 0.10f, SPR_Z), 0.008f, 0.008f, GRAY);
    if (!room->headFused) DrawSphere(v3(SPR_X, SPR_Y - 0.04f, SPR_Z), 0.03f, glow((Color){ 220, 30, 30, 255 }));
}

/* a burst pipe pours water from the ceiling and spreads a puddle */
static void draw_leak(const Room *room)
{
    if (!room->leak) return;
    limb(v3(-1.6f, 3.5f, -0.5f), v3(-1.6f, 0.05f, -0.5f), 0.05f, 0.04f, Fade((Color){ 140, 190, 255, 255 }, 0.75f));
    float r = 0.4f + 1.8f * room->wet;
    DrawCylinder(v3(-1.6f, 0.016f, -0.5f), r, r, 0.004f, 16, Fade((Color){ 80, 130, 190, 255 }, 0.7f));
}

/* the smoke detector on the ceiling: its light blinks red when it is ringing */
static void draw_detector(const Room *room, float t)
{
    DrawCylinder(v3(-1.2f, 3.43f, 1.3f), 0.12f, 0.12f, 0.05f, 10, (Color){ 220, 220, 225, 255 });
    bool ringing = room->sensors.smokeDetected && ((int)(t * 6.0f)) % 2 == 0;
    DrawSphere(v3(-1.2f, 3.40f, 1.3f), 0.035f, ringing ? glow(RED) : (Color){ 20, 90, 30, 255 });
}

/* ------------------------------------------------------------------ */
/*  Glows, flames and smoke: soft sprites that add light                */
/* ------------------------------------------------------------------ */

static void draw_halos(Camera3D cam, const Room *room, float t, bool lampsOn)
{
    if (lampsOn) {
        sprite_glow(cam, v3(-1.4f, 3.3f, 1.0f), 3.4f, (Color){ 255, 235, 190, 60 });
        sprite_glow(cam, v3(1.6f, 3.3f, -1.6f), 3.2f, (Color){ 255, 235, 190, 50 });
    }
    bool powered = !room->breakerTripped;
    if (powered) {
        sprite_glow(cam, v3(2.5f, 2.45f, -3.78f), 0.35f, room->actuators.doorLocked ? (Color){ 255, 40, 30, 150 } : (Color){ 40, 255, 80, 150 });
        sprite_glow(cam, v3(1.45f, 1.36f, -3.78f), 0.25f, room->actuators.doorLocked ? (Color){ 255, 40, 30, 130 } : (Color){ 40, 255, 80, 130 });
        sprite_glow(cam, v3(4.25f, 1.55f, -3.78f), 0.5f, Fade(temp_colour(room->sensors.temperature), 0.35f));
    }
    if (room->sensors.smokeDetected && ((int)(t * 6.0f)) % 2 == 0) {
        sprite_glow(cam, v3(-1.2f, 3.40f, 1.3f), 0.8f, (Color){ 255, 30, 20, 200 });
    }
}

static void draw_dust(Camera3D cam, float t)
{
    for (int i = 0; i < 30; i++) {
        float fi = (float)i;
        float x = -4.5f + fmodf(fi * 2.37f + t * (0.15f + 0.05f * (i % 3)), 9.0f);
        float y = 0.3f + fmodf(fi * 1.13f + t * 0.06f, 3.0f) + sinf(t + fi) * 0.05f;
        float z = -3.5f + fmodf(fi * 3.71f, 7.0f);
        sprite_glow(cam, v3(x, y, z), 0.07f, (Color){ 255, 255, 255, 70 });
    }
}

/* every so often a pair of eyes watches from the dark upper right corner */
static void draw_ambient_eyes(int roomId, float t)
{
    if (roomId == GOAL_ROOM) return;
    float c = fmodf(t + roomId * 3.1f, 13.0f);
    if (c > 9.0f && c < 9.4f) {
        glow_queue(v3(3.9f, 2.1f, -3.6f), 0.35f, (Color){ 255, 255, 255, 160 });
        glow_queue(v3(3.65f, 2.1f, -3.6f), 0.35f, (Color){ 255, 255, 255, 160 });
    }
}

/* flames are columns of soft glowing sprites, hottest and whitest at the bottom */
static void draw_flames(Camera3D cam, const Room *room, float t)
{
    float f = room->fire;
    if (f <= 0.0f || room->burnt) return;

    int n = 2 + (int)(f * 6.0f);
    if (n > 8) n = 8;
    for (int i = 0; i < n; i++) {
        Vector3 p = FIRE_SPOTS[i % 6];
        if (i >= 6) p.x += 0.9f;
        float flick = 0.75f + 0.25f * sinf(t * 13.0f + i * 2.1f) * sinf(t * 8.0f + i);
        float h = (0.8f + 2.2f * f) * flick;
        float w = 0.45f + 0.55f * f;

        for (int j = 0; j < 6; j++) {
            float u = j / 5.0f;
            float sway = sinf(t * 6.0f + i * 1.7f + j * 0.9f) * 0.12f * u * (0.5f + f);
            Vector3 pos = v3(p.x + sway, 0.15f + u * h, p.z + cosf(t * 5.0f + i + j) * 0.05f);
            Color c = { 255, (unsigned char)(220.0f - 160.0f * u), (unsigned char)(130.0f - 115.0f * u),
                        (unsigned char)(190.0f * (1.0f - 0.40f * u)) };
            sprite_glow(cam, pos, w * (1.0f - 0.55f * u), c);
        }
        sprite_glow(cam, v3(p.x, 0.08f, p.z), 1.4f + 1.6f * f, (Color){ 255, 110, 30, 38 });
    }
    for (int i = 0; i < 16; i++) {                                       /* embers */
        float rise = fmodf(t * (0.8f + 0.1f * (i % 4)) + i * 0.37f, 3.0f);
        Vector3 p = FIRE_SPOTS[i % 6];
        sprite_glow(cam, v3(p.x + sinf(t * 3.0f + i) * 0.3f, 0.5f + rise, p.z + cosf(t * 2.0f + i) * 0.3f),
                    0.09f, (Color){ 255, 170, 60, 220 });
    }
    if (f > 0.7f) {                                                      /* fire rolling along the ceiling */
        for (int i = 0; i < 6; i++) {
            sprite_glow(cam, v3(-3.5f + i * 1.4f + sinf(t * 4.0f + i) * 0.2f, 3.2f, -1.0f + (i % 2) * 1.5f),
                        1.8f, (Color){ 255, 120, 30, 110 });
        }
    }
}

/* water from the sprinkler: a fan of droplets, and ripples where it lands */
static void draw_spray(Camera3D cam, const Room *room, float flow, float t)
{
    if (flow <= 0.0f || !room->actuators.sprinklers) return;
    Vector3 head = v3(SPR_X, SPR_Y, SPR_Z);

    for (int i = 0; i < 70; i++) {
        float fall  = fmodf(t * (2.2f + 0.5f * flow) + i * 0.0733f, 1.0f);
        float a     = i * 2.399f;
        float reach = (0.2f + 1.9f * (float)(i % 7) / 6.0f) * flow * fall;
        sprite_glow(cam, v3(head.x + cosf(a) * reach, head.y - fall * head.y * 0.98f, head.z + 0.3f + sinf(a) * reach),
                    0.07f, (Color){ 190, 225, 255, (unsigned char)(110 + 100 * flow) });
    }
    for (int i = 0; i < 8; i++) {
        float age = fmodf(t * 1.3f + i * 0.37f, 1.0f);
        float a   = i * 2.1f;
        Vector3 c = v3(SPR_X + cosf(a) * (0.4f + 0.25f * i), 0.03f, SPR_Z + 0.6f + sinf(a) * (0.4f + 0.2f * i));
        DrawCircle3D(c, 0.05f + age * 0.35f, v3(1.0f, 0.0f, 0.0f), 90.0f, Fade(RAYWHITE, 0.5f * (1.0f - age)));
    }
}

static void draw_mist(Camera3D cam, const Room *room, float flow, float t)
{
    if (flow > 0.0f && room->actuators.sprinklers) {
        for (int i = 0; i < 6; i++) {
            float a = i * 1.05f + t * 0.4f;
            sprite_smoke(cam, v3(SPR_X + cosf(a) * 1.2f, 0.4f, SPR_Z + 0.5f + sinf(a) * 1.0f), 1.3f,
                         (Color){ 220, 230, 240, (unsigned char)(35 * flow) });
        }
    }
}

/* smoke: big soft dark clouds that rise, spread and sink as the room fills */
static void draw_smoke(Camera3D cam, const Room *room, float t)
{
    float s = room->sensors.smoke;
    if (s < 0.03f) return;

    int n = 8 + (int)(s * 24.0f);
    for (int i = 0; i < n; i++) {
        float rise = fmodf(t * 0.35f + i * 0.31f, 1.0f);
        float x = -3.5f + fmodf(i * 2.17f, 7.0f) + sinf(t * 0.5f + i) * 0.5f;
        float z = -2.5f + fmodf(i * 1.73f, 5.0f) + cosf(t * 0.4f + i * 1.3f) * 0.5f;
        sprite_smoke(cam, v3(x, 0.8f + rise * 2.4f, z), 1.2f + 1.6f * rise + s,
                     (Color){ 35, 35, 40, (unsigned char)(255.0f * (0.05f + 0.22f * s) * (1.0f - 0.4f * rise)) });
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
    bool lampsOn = room->actuators.lightsOn && !room->breakerTripped;

    /* with the lighting shader the lamps do the work; without it, fake the darkness */
    float k = 1.0f;
    if (!render_lit()) {
        k = room->actuators.lightsOn ? 1.0f : 0.22f;
        if (room->breakerTripped) k = 0.10f;
        if (room->actuators.lightsOn && GetRandomValue(0, someoneHere ? 6 : 90) == 0) k *= 0.45f;
        if (room->fire > 0.0f) k = fmaxf(k, 0.35f + 0.6f * room->fire);
    }

    SceneLight light;
    build_light(b, room, t, someoneHere, &light);

    /* the picture shakes when something bangs or explodes */
    Camera3D cam = room_camera(room->id, t);
    if (shake > 0.0f) {
        cam.position.x += GetRandomValue(-100, 100) / 100.0f * shake * 0.25f;
        cam.position.y += GetRandomValue(-100, 100) / 100.0f * shake * 0.18f;
    }

    render_scene_begin(cam, &light);

    /* everything solid, lit by the lamps */
    draw_shell(room, look, k);
    draw_door(room, k);
    draw_vent(room, k);
    draw_fixtures(room);
    draw_props(room->id, t, k);
    draw_details(room, t, k);
    draw_devices(room, t, k);
    draw_sprinkler_head(room);
    draw_leak(room);
    draw_detector(room, t);

    /* every awake intruder standing in this room; they creep closer the longer you watch */
    for (int i = 0; i < b->intruderCount; i++) {
        const Intruder *in = &b->intruders[i];
        if (in->active && in->room == room->id) {
            draw_intruder(in, i, t, k, cam.position, fminf(1.6f, dwell[i] * 0.07f));
        }
    }
    draw_ambient_eyes(room->id, t);

    render_lit_end();

    /* soft glowing things: lights, eyes, fire, sparks, spray, dust */
    bool spraying = room->actuators.sprinklers && b->water > 0.0f && !room->burnt;
    float flow = spraying ? building_head_flow(b) : 0.0f;

    sprites_glow_begin();
    glow_flush(cam);
    draw_halos(cam, room, t, lampsOn);
    draw_flames(cam, room, t);
    draw_spray(cam, room, flow, t);
    draw_dust(cam, t);
    sprites_glow_end();

    sprites_smoke_begin();
    draw_smoke(cam, room, t);
    draw_mist(cam, room, flow, t);
    sprites_smoke_end();

    /* outline whatever the mouse is over */
    if (hover >= 0) DrawBoundingBox(hs[hover].box, Fade(RAYWHITE, 0.9f));

    render_scene_end(t, 0.9f + 0.6f * fminf(1.0f, stare * 0.2f));
}

/* a camera inside a room that burnt down shows nothing but static */
static void draw_dead_camera(void)
{
    ClearBackground((Color){ 10, 10, 12, 255 });
    for (int i = 0; i < 1800; i++) {
        DrawRectangle(GetRandomValue(0, SCREEN_W), GetRandomValue(0, SCREEN_H), GetRandomValue(1, 8), 1,
                      Fade(RAYWHITE, 0.25f));
    }
    const char *msg = "NO SIGNAL";
    DrawText(msg, (SCREEN_W - MeasureText(msg, 40)) / 2, 175, 40, Fade(RAYWHITE, 0.8f));
    const char *sub = "this room has burnt down";
    DrawText(sub, (SCREEN_W - MeasureText(sub, 14)) / 2, 225, 14, GRAY);
}

/* the picture breaks up when something is watching you back */
static void draw_interference(float strength)
{
    int bars = 2 + (int)(strength * 10.0f);
    for (int i = 0; i < bars; i++) {
        int y = GetRandomValue(0, SCREEN_H);
        int h = GetRandomValue(1, 3 + (int)(strength * 4.0f));
        Color c = (GetRandomValue(0, 3) == 0) ? (Color){ 200, 30, 30, 255 } : RAYWHITE;
        DrawRectangle(0, y, SCREEN_W, h, Fade(c, 0.04f + 0.10f * strength));
    }
    if (strength > 0.6f && GetRandomValue(0, 20) == 0) {
        DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(BLACK, 0.5f));      /* a blackout flicker */
    }
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

    /* the picture tears, and the edges go red */
    for (int i = 0; i < 8; i++) {
        DrawRectangle(0, GetRandomValue(0, SCREEN_H), SCREEN_W, GetRandomValue(1, 6), Fade(RAYWHITE, 0.10f));
    }
    DrawRectangleGradientH(0, 0, 200, SCREEN_H, Fade(RED, 0.45f), Fade(RED, 0.0f));
    DrawRectangleGradientH(SCREEN_W - 200, 0, 200, SCREEN_H, Fade(RED, 0.0f), Fade(RED, 0.45f));
    draw_camera_overlay();
}

/* ------------------------------------------------------------------ */
/*  Reactions: sounds and shakes for what the simulation just did      */
/* ------------------------------------------------------------------ */

static void react_to_events(const Building *b)
{
    static bool started = false;
    static int moveSeq, bangSeq, boomSeq, zapSeq;

    /* first frame, or a restarted night: just catch up */
    if (!started || b->moveSeq < moveSeq || b->bangSeq < bangSeq || b->boomSeq < boomSeq || b->zapSeq < zapSeq) {
        moveSeq = b->moveSeq;
        bangSeq = b->bangSeq;
        boomSeq = b->boomSeq;
        zapSeq  = b->zapSeq;
        started = true;
        return;
    }

    if (b->moveSeq != moveSeq) {                 /* footsteps: loud if close, faint if far */
        moveSeq = b->moveSeq;
        bool close = (b->lastMoveRoom == b->selectedRoom) || (b->lastMoveRoom == 1);
        if (audioOk) {
            SetSoundVolume(sfx[SFX_STEP], close ? 0.7f : 0.2f);
            PlaySound(sfx[SFX_STEP]);
        }
    }
    if (b->bangSeq != bangSeq) { bangSeq = b->bangSeq; play(SFX_BANG); shake = 0.6f; }
    if (b->boomSeq != boomSeq) { boomSeq = b->boomSeq; play(SFX_BOOM); shake = 1.0f; boomFlash = 0.7f; }
    if (b->zapSeq  != zapSeq)  { zapSeq  = b->zapSeq;  play(SFX_ZAP);  switchFlash = 0.25f; }
}

/* the biggest text size (down to 9) at which the text still fits in maxw pixels */
static int fit_size(const char *s, int size, int maxw)
{
    while (size > 9 && MeasureText(s, size) > maxw) size--;
    return size;
}

/* what to say when the mouse rests on an object in the room */
static const char *hotspot_tip(const Hotspot *h, const Room *room, char *buf, size_t n)
{
    const Actuators *a = &room->actuators;
    switch (h->kind) {
        case HS_DOOR:
            snprintf(buf, n, "Door: %s\nClick to %s it.\nA locked door keeps villains out and fire in, but costs power.",
                     a->doorLocked ? "LOCKED" : "open", a->doorLocked ? "unlock" : "lock");
            break;
        case HS_LAMP:
            snprintf(buf, n, "Ceiling lights: %s\nClick to switch them %s.", a->lightsOn ? "ON" : "OFF", a->lightsOn ? "off" : "on");
            break;
        case HS_VENT:
            snprintf(buf, n, "Air vent: %s\nClick to %s it.\nA sealed vent starves a fire, but the room heats up.",
                     a->hvacOn ? "OPEN" : "SEALED", a->hvacOn ? "seal" : "open");
            break;
        case HS_SPRINKLER:
            snprintf(buf, n, "Sprinkler valve: %s\nClick to %s it.\nWater puts out ordinary fires, not grease or electrical ones.",
                     a->sprinklers ? "OPEN" : "shut", a->sprinklers ? "shut" : "open");
            break;
        case HS_THERMOSTAT:
            snprintf(buf, n, "Thermostat: set to %.0f C, room is %.1f C\nClick to open this room's controls.",
                     room->sensors.targetTemp, room->sensors.temperature);
            break;
        case HS_FAN:      snprintf(buf, n, "Desk fan\nClick to switch it on or off."); break;
        case HS_DUCK:     snprintf(buf, n, "A rubber duck.\nClick it. You know you want to."); break;
        case HS_LOCKER:   snprintf(buf, n, "Locker\nClick to open or close it."); break;
        case HS_TV:       snprintf(buf, n, "Television\nClick to switch it on or off."); break;
        case HS_BELL:     snprintf(buf, n, "Reception bell\nClick to ring it."); break;
        case HS_FRIDGE:   snprintf(buf, n, "Fridge\nClick to open or close the door."); break;
        case HS_RACK:     snprintf(buf, n, "Server rack\nClick to make its lights blink faster."); break;
        case HS_LANTERN:  snprintf(buf, n, "Oil lantern\nClick to light or snuff it."); break;
        default:          buf[0] = '\0'; break;
    }
    return buf;
}

static const char *hud_tip(const Building *b, Vector2 m, char *buf, size_t n)
{
    for (int i = 0; i < b->roomCount; i++) {
        if (CheckCollisionPointRec(m, button_rect(i))) {
            const Room *r = &b->rooms[i];
            const char *state = r->burnt ? "burnt down" : r->sensors.smokeDetected ? "SMOKE ALARM"
                              : r->sensors.motionDetected ? "motion detected" : "quiet";
            snprintf(buf, n, "Camera %d: %s (%s)\nClick, or press %d, to watch this room.", i + 1, r->name, state, i + 1);
            return buf;
        }
    }
    if (CheckCollisionPointRec(m, control_btn_rect())) return "CONTROL PANEL (SPACE)\nThermostats, doors, scenes, automation, power and water.";
    if (CheckCollisionPointRec(m, graph_btn_rect()))   return "GRAPHS (TAB)\nNetwork, temperatures, motion and ventilation over time.";
    if (CheckCollisionPointRec(m, building_btn_rect())) return "BUILDING OVERVIEW (B)\nThe floor plan with live readings and alarms.";
    if (CheckCollisionPointRec(m, help_btn_rect()))    return "HELP (H)\nHow to play.";
    return NULL;
}

/* hints that appear once, the first time something happens */
typedef enum { C_HALLWAY, C_SMOKE, C_FIRE, C_POWER, C_BREAKER, C_LEAK, C_NET, C_COUNT } CoachId;
static bool  coached[C_COUNT];
static const char *coachText = NULL;
static float coachTime = 0.0f;

static void coach(CoachId id, const char *text)
{
    if (coached[id] || coachTime > 0.0f) return;
    coached[id] = true;
    coachText = text;
    coachTime = 10.0f;
}

static void coach_check(const Building *b, bool danger, int fires, int tripped, int leaks, bool alarm, float dt)
{
    if (coachTime > 0.0f) coachTime -= dt;
    if (b->state != GAME_PLAYING || dt <= 0.0f) return;

    if (fires > 0)         coach(C_FIRE, "FIRE! Press C for the fire response. Grease fire (Kitchen): never use water.\nElectrical fire (Server Room): cut its power. Never open the door of a burning, sealed room.");
    else if (alarm)        coach(C_SMOKE, "A smoke alarm is ringing. AUTO-FIRE seals the room for you.\nCheck its camera, and keep the door shut until the fire is out. (It may be a false alarm.)");
    if (danger)            coach(C_HALLWAY, "Something is in the Hallway, right outside the Office!\nPress 1, then D, to lock the Office door. Or switch on AUTO-DOOR in CONTROL > AUTO.");
    if (b->power < 40.0f)  coach(C_POWER, "The battery is draining. Locked doors and sprinklers use the most.\nPress X for ECO mode, and only lock the Office door when you must.");
    if (tripped > 0)       coach(C_BREAKER, "A circuit breaker has tripped, so that room is dead.\nOpen CONTROL > UTILITY and click its breaker to switch it back on.");
    if (leaks > 0)         coach(C_LEAK, "A pipe has burst! Open CONTROL > UTILITY and close the main water valve.\nAUTO-LEAK does it for you if it is switched on.");
    if (b->netOutage > 0.0f) coach(C_NET, "The internet is down. Cloud automations (everything except AUTO-FIRE) are paused.\nIt comes back by itself.");
}

static void draw_coach(void)
{
    if (coachTime <= 0.0f || !coachText) return;

    float a = fminf(1.0f, coachTime);                 /* fades out in the last second */
    Rectangle r = { MARGIN, 338, 470, 52 };
    DrawRectangleRec(r, Fade((Color){ 8, 12, 20, 255 }, 0.88f * a));
    DrawRectangleLinesEx(r, 1, Fade(GOLD, a));
    DrawText("TIP", (int)r.x + 8, (int)r.y + 6, 12, Fade(GOLD, a));

    char line[2][160] = { "", "" };
    const char *nl = strchr(coachText, '\n');
    int n1 = nl ? (int)(nl - coachText) : (int)strlen(coachText);
    if (n1 > 159) n1 = 159;
    memcpy(line[0], coachText, (size_t)n1);
    line[0][n1] = '\0';
    if (nl) snprintf(line[1], sizeof line[1], "%s", nl + 1);

    DrawText(line[0], (int)r.x + 40, (int)r.y + 7, 10, Fade(RAYWHITE, a));
    DrawText(line[1], (int)r.x + 40, (int)r.y + 28, 10, Fade(RAYWHITE, a));
}

/* one line of status text in the top right, only shown when something needs attention */
static void chip(int *y, const char *text, Color col)
{
    DrawText(text, SCREEN_W - 12 - MeasureText(text, 11), *y, 11, col);
    *y += 14;
}

/* ------------------------------------------------------------------ */
/*  Everything on screen                                               */
/* ------------------------------------------------------------------ */

void ui_draw(const Building *b)
{
    float dt = ui_paused() ? 0.0f : GetFrameTime();
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
        if (audioOk) {
            StopSound(sfx[SFX_HUM]);
            StopSound(sfx[SFX_ALARM]);
            StopSound(sfx[SFX_HISS]);
        }
        draw_jumpscare(b->intruders[b->killer].type, scareTime);
        return;
    }

    react_to_events(b);

    /* ambient hum while the night is running */
    if (audioOk) {
        if (b->state == GAME_PLAYING && !ui_paused()) {
            if (!IsSoundPlaying(sfx[SFX_HUM])) PlaySound(sfx[SFX_HUM]);
        } else {
            StopSound(sfx[SFX_HUM]);
        }
    }

    /* fire alarm while any smoke alarm is ringing, hiss while any sprinkler is spraying */
    int fires = 0, tripped = 0, leaks = 0;
    bool alarm = false, spray = false;
    for (int i = 0; i < b->roomCount; i++) {
        const Room *r = &b->rooms[i];
        if (r->fire > 0.0f) fires++;
        if (r->breakerTripped) tripped++;
        if (r->leak) leaks++;
        if (r->sensors.smokeDetected) alarm = true;
        if (r->actuators.sprinklers && b->water > 0.0f && !r->burnt) spray = true;
    }
    if (audioOk) {
        if (b->state != GAME_PLAYING || ui_paused()) { alarm = false; spray = false; }
        if (alarm && !IsSoundPlaying(sfx[SFX_ALARM])) PlaySound(sfx[SFX_ALARM]);
        if (!alarm) StopSound(sfx[SFX_ALARM]);
        if (spray && !IsSoundPlaying(sfx[SFX_HISS])) PlaySound(sfx[SFX_HISS]);
        if (!spray) StopSound(sfx[SFX_HISS]);
    }

    update_anim(b, dt);

    const Room *room = &b->rooms[b->selectedRoom];

    /* who is in the room you are watching, and for how long? the longer, the closer and the worse it gets */
    bool watched = false;
    for (int i = 0; i < b->intruderCount; i++) {
        const Intruder *in = &b->intruders[i];
        if (in->active && in->room == room->id) { dwell[i] += dt; watched = true; }
        else                                     dwell[i] = 0.0f;
    }
    stare = watched ? stare + dt : fmaxf(0.0f, stare - 2.0f * dt);
    whisperTimer -= dt;
    if (stare > 3.0f && whisperTimer <= 0.0f) { play(SFX_WHISPER); whisperTimer = 4.0f; }

    /* danger: something is in the Hallway, right outside the Office (the heartbeat plays on every screen) */
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
    }

    coach_check(b, danger, fires, tripped, leaks, alarm, dt);

    /* the building overview replaces the camera picture */
    if (dashboard_visible() && b->state == GAME_PLAYING) {
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        ClearBackground((Color){ 8, 12, 16, 255 });
        dashboard_draw(b);
        if (danger) {
            float pulse = fmaxf(0.0f, 1.0f - fmodf(t, 0.9f) * 2.2f);
            DrawRectangleGradientH(0, 0, 100, SCREEN_H, Fade(RED, 0.35f * pulse), Fade(RED, 0.0f));
            DrawRectangleGradientH(SCREEN_W - 100, 0, 100, SCREEN_H, Fade(RED, 0.0f), Fade(RED, 0.35f * pulse));
        }
        panel_draw(b);
        draw_coach();
        if (paused && !help_visible()) {
            DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(BLACK, 0.55f));
            DrawText("PAUSED", (SCREEN_W - MeasureText("PAUSED", 50)) / 2, 180, 50, RAYWHITE);
            DrawText("press P to carry on", (SCREEN_W - MeasureText("press P to carry on", 16)) / 2, 240, 16, GRAY);
        }
        help_draw();
        return;
    }

    /* what is the mouse pointing at? (nothing while the dashboard or the panel is in the way) */
    Hotspot hs[MAX_HOTSPOTS];
    int n = build_hotspots(room->id, hs);
    int hover = -1;
    Vector2 mouse = GetMousePosition();
    if (b->state == GAME_PLAYING && !room->burnt && !graphs_visible() && !on_hud(b, mouse)) {
        hover = pick_hotspot(hs, n, GetMouseRay(mouse, room_camera(room->id, t)));
    }
    SetMouseCursor(hover >= 0 ? MOUSE_CURSOR_POINTING_HAND : MOUSE_CURSOR_DEFAULT);

    ClearBackground(BLACK);
    if (room->burnt) draw_dead_camera();
    else             draw_room_3d(b, t, hs, hover);
    if (!render_post() || room->burnt) draw_camera_overlay();
    if (stare > 0.5f && !room->burnt) draw_interference(fminf(1.0f, 0.2f + stare * 0.12f));

    /* orange glow when the room you are looking at is on fire, and a flash when it explodes */
    if (room->fire > 0.0f && !room->burnt) {
        float flick = 0.7f + 0.3f * sinf(t * 17.0f) * sinf(t * 5.0f);
        DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(ORANGE, 0.02f + 0.06f * room->fire * flick));
    }
    if (boomFlash > 0.0f) {
        DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade((Color){ 255, 200, 120, 255 }, fminf(0.8f, boomFlash)));
    }

    if (danger && b->state == GAME_PLAYING) {
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

    /* ---- the uncluttered screen ---- */

    /* top left: which camera, and the temperature */
    DrawText(TextFormat("CAM %d  %s", room->id + 1, room->name), MARGIN, 14, 22, RAYWHITE);
    if (((int)(t * 2.0f)) % 2 == 0) DrawCircle(MARGIN + 7, 46, 6, RED);
    DrawText("REC", MARGIN + 20, 40, 14, RAYWHITE);
    DrawText(TextFormat("%.0f C", room->sensors.temperature), MARGIN + 60, 40, 14,
             (room->sensors.temperature > 40.0f) ? ORANGE : GRAY);

    /* top right: the clock, power and water */
    int hour = building_hour(b);
    DrawText(TextFormat("%d AM", hour == 0 ? 12 : hour), SCREEN_W - 100, 10, 28, RAYWHITE);

    Color powerCol = (b->power > 40.0f) ? RAYWHITE : (b->power > 20.0f) ? ORANGE : RED;
    DrawText(TextFormat("POWER %.0f%%", b->power), SCREEN_W - 160, 44, 11, powerCol);
    DrawRectangle(SCREEN_W - 160, 58, 140, 5, Fade(GRAY, 0.5f));
    DrawRectangle(SCREEN_W - 160, 58, (int)(1.4f * b->power), 5, powerCol);

    DrawText(TextFormat("WATER %.0f%%", b->water), SCREEN_W - 160, 68, 11, (b->water > 30.0f) ? SKYBLUE : ORANGE);
    DrawRectangle(SCREEN_W - 160, 82, 140, 5, Fade(GRAY, 0.5f));
    DrawRectangle(SCREEN_W - 160, 82, (int)(1.4f * b->water), 5, (b->water > 30.0f) ? SKYBLUE : ORANGE);

    /* only what needs attention shows up here */
    {
        int cy = 94;
        bool blink = ((int)(t * 3.0f)) % 2 == 0;
        if (b->powerOut)                   { if (blink) chip(&cy, "NO POWER", RED); else cy += 14; }
        else if (b->ecoMode)                chip(&cy, "ECO MODE", GREEN);
        if (fires > 0)                     { if (blink) chip(&cy, TextFormat("FIRE x%d", fires), ORANGE); else cy += 14; }
        if (b->netOutage > 0.0f)            chip(&cy, "INTERNET DOWN", ORANGE);
        if (b->heatwave > 0.0f)             chip(&cy, "HVAC FAULT", ORANGE);
        if (leaks > 0)                      chip(&cy, "LEAK", SKYBLUE);
        if (tripped > 0)                    chip(&cy, TextFormat("BREAKER OFF x%d", tripped), ORANGE);
        if (b->lockdownTimer > 0.0f)        chip(&cy, TextFormat("LOCKDOWN %.0fs", b->lockdownTimer), RED);
    }

    /* top centre: the latest important event, and the advisor's tip when something is wrong */
    if (b->noticeTimer > 0.0f) {
        const char *msg = b->notice;
        Color nc = GOLD;
        if (strstr(msg, "FIRE") || strstr(msg, "FLASHOVER") || strstr(msg, "BACKDRAFT") || strstr(msg, "BURNT")) nc = ORANGE;
        if (strstr(msg, "POWER OUT")) nc = RED;
        int size = fit_size(msg, 18, 370);
        int x = 260 + (370 - MeasureText(msg, size)) / 2;
        DrawText(msg, x, 12, size, Fade(nc, fminf(1.0f, b->noticeTimer)));
    }
    {
        const char *tip = building_advice(b);
        if (strcmp(tip, "All systems normal") != 0) {
            int size = fit_size(tip, 12, 370);
            int x = 260 + (370 - MeasureText(tip, size)) / 2;
            DrawText(tip, x, 38, size, GOLD);
        }
    }

    /* bottom right: the camera map (red dot = motion, orange = fire, grey = smoke) */
    for (int i = 0; i < b->roomCount; i++) {
        const Room *r2 = &b->rooms[i];
        Rectangle r = button_rect(i);
        bool sel = (i == b->selectedRoom);
        DrawRectangleRec(r, sel ? Fade(RAYWHITE, 0.85f) : Fade(BLACK, 0.6f));
        DrawRectangleLinesEx(r, 2, alert_colour(r2->alert));
        DrawText(TextFormat("%d %s", i + 1, SHORT_NAME[i]), (int)r.x + 6, (int)r.y + 9, 12,
                 sel ? BLACK : RAYWHITE);
        if (r2->burnt)                          DrawText("X", (int)r.x + MAP_W - 14, (int)r.y + 3, 12, RED);
        else {
            if (r2->sensors.motionDetected)     DrawCircle((int)r.x + MAP_W - 8, (int)r.y + 8, 4, RED);
            if (r2->fire > 0.0f)                DrawCircle((int)r.x + MAP_W - 20, (int)r.y + 8, 4, ORANGE);
            else if (r2->sensors.smokeDetected) DrawCircle((int)r.x + MAP_W - 20, (int)r.y + 8, 4, GRAY);
            if (r2->breakerTripped)             DrawRectangle((int)r.x + MAP_W - 32, (int)r.y + 4, 8, 8, YELLOW);
        }
    }

    /* bottom left: the two buttons */
    Rectangle cb = control_btn_rect();
    DrawRectangleRec(cb, Fade(BLACK, 0.6f));
    DrawRectangleLinesEx(cb, 2, panel_visible() ? RAYWHITE : GREEN);
    DrawText("CONTROL", (int)cb.x + 10, (int)cb.y + 7, 14, GREEN);

    Rectangle gb = graph_btn_rect();
    DrawRectangleRec(gb, Fade(BLACK, 0.6f));
    DrawRectangleLinesEx(gb, 2, SKYBLUE);
    DrawText("GRAPHS", (int)gb.x + 10, (int)gb.y + 7, 14, SKYBLUE);

    Rectangle bb = building_btn_rect();
    DrawRectangleRec(bb, Fade(BLACK, 0.6f));
    DrawRectangleLinesEx(bb, 2, ORANGE);
    DrawText("BUILDING", (int)bb.x + 10, (int)bb.y + 7, 14, ORANGE);

    Rectangle hb = help_btn_rect();
    DrawRectangleRec(hb, Fade(BLACK, 0.6f));
    DrawRectangleLinesEx(hb, 2, GOLD);
    DrawText("?", (int)hb.x + 12, (int)hb.y + 6, 16, GOLD);

    DrawText("H help   B building   SPACE control   TAB graphs   1-6 cameras   P pause   G graphics",
             MARGIN, SCREEN_H - 16, 10, GRAY);

    panel_draw(b);
    draw_coach();

    /* point at something to learn what it is */
    if (!ui_paused() && !graphs_visible() && b->state == GAME_PLAYING) {
        char tip[320];
        const char *text = NULL;
        if (panel_contains((int)mouse.x, (int)mouse.y)) text = NULL;
        else if (on_hud(b, mouse))                        text = hud_tip(b, mouse, tip, sizeof tip);
        else if (hover >= 0)                              text = hotspot_tip(&hs[hover], room, tip, sizeof tip);
        if (text && text[0]) help_tooltip(text);
    }

    if (paused && !help_visible() && b->state == GAME_PLAYING) {
        DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(BLACK, 0.55f));
        DrawText("PAUSED", (SCREEN_W - MeasureText("PAUSED", 50)) / 2, 180, 50, RAYWHITE);
        DrawText("press P to carry on", (SCREEN_W - MeasureText("press P to carry on", 16)) / 2, 240, 16, GRAY);
    }
    help_draw();

    /* end of night: shown over everything, no menu */
    if (b->state != GAME_PLAYING) {
        DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(BLACK, 0.85f));

        const char *msg = "GAME OVER";
        if (b->state == GAME_WON)               msg = "6 AM - YOU SURVIVED";
        else if (b->cause == CAUSE_FIRE)        msg = "THE BUILDING BURNED";
        else if (b->cause == CAUSE_SMOKE)       msg = "OVERCOME BY SMOKE";
        else if (b->killer >= 0)                msg = TextFormat("CAUGHT BY %s", b->intruders[b->killer].name);

        int w = MeasureText(msg, 50);
        DrawText(msg, (SCREEN_W - w) / 2, 170, 50, (b->state == GAME_WON) ? GREEN : RED);

        const char *hint = "Press R to restart";
        int hw = MeasureText(hint, 20);
        DrawText(hint, (SCREEN_W - hw) / 2, 240, 20, GRAY);
    }
}