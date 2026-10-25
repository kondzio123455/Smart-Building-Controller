#include <stdio.h>
#include <string.h>
#include "raylib.h"
#include "building.h"
#include "panel.h"

/* The panel sits above the camera map, on the right of the 800 x 450 window */
#define PX 396
#define PY 40
#define PW 388
#define PH 322
#define CX (PX + 10)
#define CY (PY + 36)

enum { TAB_ROOM, TAB_SCENES, TAB_AUTO, TAB_UTIL, TAB_LOG, TAB_COUNT };
static const char *TAB_NAMES[TAB_COUNT] = { "ROOM", "SCENES", "AUTO", "UTILITY", "LOG" };

static bool open = false;
static int  tab  = TAB_ROOM;

void panel_toggle(void) { open = !open; }
void panel_show_room(void) { open = true; tab = TAB_ROOM; }
bool panel_visible(void) { return open; }

bool panel_contains(int x, int y)
{
    return open && x >= PX && x < PX + PW && y >= PY && y < PY + PH;
}

/* ---- small helpers ---- */

static bool hit(Rectangle r)
{
    return CheckCollisionPointRec(GetMousePosition(), r);
}

static bool pressed(Rectangle r)
{
    return IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hit(r);
}

static Rectangle tab_rect(int i)
{
    float w = (float)PW / TAB_COUNT;
    return (Rectangle){ PX + i * w, PY, w, 26 };
}

static void draw_button(Rectangle r, const char *label, Color col, bool lit)
{
    bool hov = hit(r);
    DrawRectangleRec(r, Fade(col, lit ? 0.35f : (hov ? 0.20f : 0.08f)));
    DrawRectangleLinesEx(r, 1, hov ? RAYWHITE : col);
    DrawText(label, (int)r.x + 8, (int)(r.y + (r.height - 12.0f) / 2.0f), 12, RAYWHITE);
}

static void draw_bar(Rectangle r, float fraction, Color col)
{
    if (fraction < 0.0f) fraction = 0.0f;
    if (fraction > 1.0f) fraction = 1.0f;
    DrawRectangleRec(r, Fade(GRAY, 0.3f));
    DrawRectangle((int)r.x, (int)r.y, (int)(r.width * fraction), (int)r.height, col);
    DrawRectangleLinesEx(r, 1, GRAY);
}

/* the room controls refuse to work when the power is gone, the room has no breaker power, or it burnt down */
static bool responds(Building *b, const Room *room, bool isDoor)
{
    if (b->powerOut)        { ui_click_sound(); return false; }
    if (room->burnt)        { ui_click_sound(); return false; }
    if (room->breakerTripped) {
        log_event(b, "%s has no power", room->name);
        ui_click_sound();
        return false;
    }
    if (isDoor && room->lockFault > 0.0f) {
        log_event(b, "%s door controller is not responding", room->name);
        ui_click_sound();
        return false;
    }
    return true;
}

/* ---- the layout of the ROOM tab (shared by update and draw) ---- */

static Rectangle room_btn(int i)    /* 0 lights, 1 door, 2 vent, 3 sprinkler valve, 4 breaker */
{
    return (Rectangle){ CX + (i % 2) * 186.0f, CY + 26.0f + (i / 2) * 36.0f, 178, 30 };
}
static Rectangle temp_minus(void) { return (Rectangle){ CX + 190, CY + 138, 30, 26 }; }
static Rectangle temp_plus(void)  { return (Rectangle){ CX + 280, CY + 138, 30, 26 }; }

static Rectangle scene_row(int i) { return (Rectangle){ CX, CY + i * 62.0f, 368, 56 }; }
static Rectangle rule_row(int i)  { return (Rectangle){ CX, CY + i * 38.0f, 368, 34 }; }
static Rectangle valve_btn(void)  { return (Rectangle){ CX + 238, CY + 36, 130, 26 }; }
static Rectangle breaker_btn(int i) { return (Rectangle){ CX + i * 62.0f, CY + 132, 56, 30 }; }

static const char *SHORT[MAX_ROOMS] = { "OFFICE", "HALL", "LOBBY", "KITCHEN", "SERVER", "STORAGE", "", "" };

static const char *SCENE_INFO[MAX_SCENES] = {
    "All doors locked, vents sealed for 20 s. Costs power.",
    "Lights off except the Office. Standby power halved.",
    "Seals and sprays burning rooms. Cuts electrical power.",
    "Everything back to normal. Main water valve open."
};
static const char SCENE_KEY[MAX_SCENES] = { 'Z', 'X', 'C', 'V' };

/* ------------------------------------------------------------------ */
/*  Input                                                              */
/* ------------------------------------------------------------------ */

void panel_update(Building *b)
{
    if (IsKeyPressed(KEY_SPACE)) open = !open;
    if (!open) return;

    for (int i = 0; i < TAB_COUNT; i++) {
        if (pressed(tab_rect(i))) { tab = i; ui_click_sound(); }
    }

    Room *room = &b->rooms[b->selectedRoom];

    switch (tab) {
        case TAB_ROOM:
            if (pressed(room_btn(0)) && responds(b, room, false)) {
                room_set_lights(room, !room->actuators.lightsOn);
                log_event(b, "%s lights %s", room->name, room->actuators.lightsOn ? "on" : "off");
                ui_click_sound();
            }
            if (pressed(room_btn(1)) && responds(b, room, true)) {
                room_set_lock(room, !room->actuators.doorLocked);
                log_event(b, "%s door %s", room->name, room->actuators.doorLocked ? "locked" : "unlocked");
                ui_click_sound();
            }
            if (pressed(room_btn(2)) && responds(b, room, false)) {
                room_set_hvac(room, !room->actuators.hvacOn);
                log_event(b, "%s vent %s", room->name, room->actuators.hvacOn ? "opened" : "sealed");
                ui_click_sound();
            }
            if (pressed(room_btn(3)) && responds(b, room, false)) {
                room_set_sprinklers(room, !room->actuators.sprinklers);
                log_event(b, "%s sprinkler valve %s", room->name, room->actuators.sprinklers ? "opened" : "closed");
                ui_click_sound();
            }
            if (pressed(room_btn(4)) && !b->powerOut) {
                /* the breaker: cut the room's power by hand, or switch it back on */
                building_set_breaker(b, b->selectedRoom, room->breakerTripped);
                ui_click_sound();
            }
            if (pressed(temp_minus()) && responds(b, room, false)) {
                room_set_target_temp(room, room->sensors.targetTemp - 1.0f);
                ui_click_sound();
            }
            if (pressed(temp_plus()) && responds(b, room, false)) {
                room_set_target_temp(room, room->sensors.targetTemp + 1.0f);
                ui_click_sound();
            }
            break;

        case TAB_SCENES:
            for (int i = 0; i < MAX_SCENES; i++) {
                if (pressed(scene_row(i))) {
                    scene_activate(b, (SceneId)i);
                    ui_click_sound();
                }
            }
            break;

        case TAB_AUTO:
            for (int i = 0; i < MAX_RULES; i++) {
                if (pressed(rule_row(i))) {
                    b->rules[i].enabled = !b->rules[i].enabled;
                    log_event(b, "%s switched %s", rule_name((RuleId)i), b->rules[i].enabled ? "on" : "off");
                    ui_click_sound();
                }
            }
            break;

        case TAB_UTIL:
            if (pressed(valve_btn()) && !b->powerOut) {
                building_set_main_valve(b, !b->mainOpen);
                ui_click_sound();
            }
            for (int i = 0; i < b->roomCount; i++) {
                if (pressed(breaker_btn(i)) && !b->powerOut) {
                    building_set_breaker(b, i, b->rooms[i].breakerTripped);
                    ui_click_sound();
                }
            }
            break;

        default:
            break;
    }
}

/* ------------------------------------------------------------------ */
/*  Drawing                                                            */
/* ------------------------------------------------------------------ */

static void draw_room_tab(const Building *b)
{
    const Room *r = &b->rooms[b->selectedRoom];

    DrawText(TextFormat("%s", r->name), CX, CY, 16, RAYWHITE);
    const char *state = r->burnt ? "BURNT OUT" : r->breakerTripped ? "NO POWER" : r->leak ? "LEAK"
                      : r->sensors.smokeDetected ? "SMOKE ALARM" : "OK";
    Color sc = (strcmp(state, "OK") == 0) ? GREEN : RED;
    DrawText(state, CX + 368 - MeasureText(state, 12), CY + 3, 12, sc);

    const Actuators *a = &r->actuators;
    draw_button(room_btn(0), TextFormat("LIGHTS: %s", a->lightsOn ? "ON" : "OFF"), YELLOW, a->lightsOn);
    draw_button(room_btn(1), TextFormat("DOOR: %s%s", a->doorLocked ? "LOCKED" : "OPEN", r->lockFault > 0.0f ? " (FAULT)" : ""),
                a->doorLocked ? RED : GREEN, a->doorLocked);
    draw_button(room_btn(2), TextFormat("VENT: %s", a->hvacOn ? "OPEN" : "SEALED"), SKYBLUE, !a->hvacOn);
    draw_button(room_btn(3), TextFormat("SPRINKLER VALVE: %s", a->sprinklers ? "OPEN" : "SHUT"), BLUE, a->sprinklers);
    draw_button(room_btn(4), r->breakerTripped ? "BREAKER OFF: RESET" : "BREAKER ON: CUT POWER",
                r->breakerTripped ? RED : GREEN, r->breakerTripped);

    DrawText("THERMOSTAT", CX, CY + 144, 12, RAYWHITE);
    draw_button(temp_minus(), "-", ORANGE, false);
    DrawText(TextFormat("%.0f C", r->sensors.targetTemp), CX + 232, CY + 144, 14, RAYWHITE);
    draw_button(temp_plus(), "+", ORANGE, false);

    DrawText(TextFormat("Temperature %.1f C     Smoke %.0f%%     Wet %.0f%%",
                        r->sensors.temperature, r->sensors.smoke * 100.0f, r->wet * 100.0f), CX, CY + 182, 12, GRAY);
    DrawText(TextFormat("Motion: %s     Load: %.3f/s",
                        r->sensors.motionDetected ? "YES" : "no", room_load(b, b->selectedRoom)), CX, CY + 200, 12, GRAY);
    if (r->lockFault > 0.0f)
        DrawText(TextFormat("Door controller offline for %.0f s", r->lockFault), CX, CY + 220, 12, ORANGE);
    if (r->headFused)
        DrawText("Head fused: shut the valve to stop the water", CX, CY + 238, 12, SKYBLUE);
    if (r->leak)
        DrawText("Burst pipe: close the main valve (UTILITY tab)", CX, CY + 256, 12, SKYBLUE);
}

static void draw_scenes_tab(const Building *b)
{
    for (int i = 0; i < MAX_SCENES; i++) {
        Rectangle r = scene_row(i);
        float cd = b->sceneCooldown[i];
        bool active = (i == SCENE_LOCKDOWN && b->lockdownTimer > 0.0f) || (i == SCENE_ECO && b->ecoMode);
        Color edge = (cd > 0.0f) ? GRAY : (i == SCENE_FIRE) ? ORANGE : SKYBLUE;
        DrawRectangleRec(r, Fade(edge, active ? 0.30f : 0.08f));
        DrawRectangleLinesEx(r, 1, hit(r) ? RAYWHITE : edge);
        DrawText(TextFormat("[%c]  %s", SCENE_KEY[i], scene_name((SceneId)i)), (int)r.x + 8, (int)r.y + 6, 14,
                 (cd > 0.0f) ? GRAY : RAYWHITE);
        DrawText(SCENE_INFO[i], (int)r.x + 8, (int)r.y + 28, 10, GRAY);
        if (cd > 0.0f) {
            DrawText(TextFormat("wait %ds", (int)cd + 1), (int)(r.x + r.width) - 60, (int)r.y + 6, 12, GRAY);
        } else if (active) {
            DrawText("ACTIVE", (int)(r.x + r.width) - 60, (int)r.y + 6, 12, GREEN);
        }
    }
}

static void draw_auto_tab(const Building *b)
{
    for (int i = 0; i < MAX_RULES; i++) {
        Rectangle r = rule_row(i);
        bool on = b->rules[i].enabled;
        bool cloud = rule_is_cloud((RuleId)i);
        bool offline = cloud && b->netOutage > 0.0f;
        DrawRectangleRec(r, Fade(on ? GREEN : GRAY, 0.14f));
        DrawRectangleLinesEx(r, 1, hit(r) ? RAYWHITE : (on ? GREEN : GRAY));
        DrawText(rule_name((RuleId)i), (int)r.x + 8, (int)r.y + 3, 12, RAYWHITE);
        DrawText(rule_info((RuleId)i), (int)r.x + 8, (int)r.y + 20, 9, GRAY);
        DrawText(on ? "ON" : "OFF", (int)(r.x + r.width) - 34, (int)r.y + 3, 13, on ? GREEN : RED);
        DrawText(offline ? "OFFLINE" : (cloud ? "CLOUD" : "LOCAL"), (int)(r.x + r.width) - 96, (int)r.y + 5, 10,
                 offline ? ORANGE : (cloud ? SKYBLUE : GREEN));
        DrawText(TextFormat("x%d", b->rules[i].fired), (int)(r.x + r.width) - 34, (int)r.y + 20, 10, GRAY);
    }
}

static void draw_util_tab(const Building *b)
{
    /* power */
    DrawText("POWER", CX, CY, 12, RAYWHITE);
    draw_bar((Rectangle){ CX + 70, CY + 2, 190, 10 }, b->power / 100.0f,
             (b->power > 40.0f) ? GREEN : (b->power > 20.0f) ? ORANGE : RED);
    DrawText(TextFormat("%.0f%%", b->power), CX + 270, CY, 12, RAYWHITE);
    int left = (int)(building_power_left(b) < 5999.0f ? building_power_left(b) : 5999.0f);
    DrawText(b->powerOut ? "NO POWER" : TextFormat("using %.2f/s   empty in %d:%02d   %.1f kWh  EUR %.2f",
             building_power_usage(b), left / 60, left % 60, b->energy, b->energy * 0.30f),
             CX, CY + 16, 10, b->powerOut ? RED : GRAY);

    /* water and the main valve */
    DrawText("WATER", CX, CY + 40, 12, RAYWHITE);
    draw_bar((Rectangle){ CX + 70, CY + 42, 150, 10 }, b->water / 100.0f, (b->water > 30.0f) ? SKYBLUE : ORANGE);
    DrawText(TextFormat("%.0f%%", b->water), CX + 2, CY + 56, 10, GRAY);
    DrawText(TextFormat("%d heads, %.0f%% flow", building_heads_running(b), building_head_flow(b) * 100.0f),
             CX + 40, CY + 56, 10, GRAY);
    draw_button(valve_btn(), b->mainOpen ? "MAIN VALVE: OPEN" : "MAIN VALVE: SHUT", b->mainOpen ? SKYBLUE : ORANGE, !b->mainOpen);

    /* network */
    DrawText("NETWORK", CX, CY + 82, 12, RAYWHITE);
    DrawText(TextFormat("outside %.0f C   %s %.2f EUR/kWh", building_outside_temp(b), building_tariff_name(b), building_tariff(b)),
             CX, CY + 98, 10, GRAY);
    draw_bar((Rectangle){ CX + 70, CY + 84, 150, 10 }, b->netSpeed / 100.0f, (b->netSpeed > 30.0f) ? GREEN : RED);
    DrawText(b->netOutage > 0.0f ? "INTERNET DOWN" : TextFormat("%.0f Mbps", b->netSpeed), CX + 230, CY + 82, 12,
             b->netOutage > 0.0f ? ORANGE : RAYWHITE);

    /* breakers */
    DrawText("CIRCUIT BREAKERS (click to cut or reset)", CX, CY + 114, 10, GRAY);
    for (int i = 0; i < b->roomCount; i++) {
        Rectangle r = breaker_btn(i);
        bool tripped = b->rooms[i].breakerTripped;
        DrawRectangleRec(r, Fade(tripped ? RED : GREEN, hit(r) ? 0.35f : 0.18f));
        DrawRectangleLinesEx(r, 1, tripped ? RED : GREEN);
        DrawText(SHORT[i], (int)r.x + 4, (int)r.y + 4, 9, RAYWHITE);
        DrawText(tripped ? "OFF" : "ON", (int)r.x + 4, (int)r.y + 16, 10, tripped ? RED : GREEN);
    }

    /* what each room uses */
    DrawText("POWER USED BY EACH ROOM", CX, CY + 172, 10, GRAY);
    for (int i = 0; i < b->roomCount; i++) {
        float load = room_load(b, i);
        float h = load / 0.25f * 60.0f;
        if (h > 60.0f) h = 60.0f;
        Rectangle col = { CX + i * 62.0f + 14, CY + 190 + (60.0f - h), 26, h };
        DrawRectangleRec(col, (load > 0.12f) ? ORANGE : SKYBLUE);
        DrawRectangleLines((int)CX + i * 62 + 14, CY + 190, 26, 60, GRAY);
        DrawText(SHORT[i], (int)CX + i * 62 + 6, CY + 254, 9, GRAY);
    }
}

static void draw_log_tab(const Building *b)
{
    for (int i = 0; i < 15; i++) {
        const LogEntry *e = log_recent(b, i);
        if (!e) break;
        char clock[16];
        clock_text(e->time, clock, sizeof clock);
        DrawText(TextFormat("%s  %s", clock, e->text), CX, CY + i * 17, 11, Fade(RAYWHITE, 1.0f - 0.04f * i));
    }
}

void panel_draw(const Building *b)
{
    if (!open) return;

    Rectangle p = { PX, PY, PW, PH };
    DrawRectangleRec(p, (Color){ 10, 14, 20, 238 });
    DrawRectangleLinesEx(p, 2, GRAY);

    for (int i = 0; i < TAB_COUNT; i++) {
        Rectangle r = tab_rect(i);
        DrawRectangleRec(r, Fade(SKYBLUE, (i == tab) ? 0.35f : (hit(r) ? 0.15f : 0.04f)));
        DrawRectangleLinesEx(r, 1, GRAY);
        DrawText(TAB_NAMES[i], (int)r.x + 8, (int)r.y + 7, 12, (i == tab) ? RAYWHITE : GRAY);
    }

    switch (tab) {
        case TAB_ROOM:   draw_room_tab(b);   break;
        case TAB_SCENES: draw_scenes_tab(b); break;
        case TAB_AUTO:   draw_auto_tab(b);   break;
        case TAB_UTIL:   draw_util_tab(b);   break;
        default:         draw_log_tab(b);    break;
    }

    DrawText("SPACE closes this panel", PX + 10, PY + PH - 16, 10, GRAY);
}