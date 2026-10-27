#include <stdio.h>
#include <string.h>
#include "raylib.h"
#include "building.h"
#include "graphs.h"
#include "help.h"
#include "panel.h"
#include "dashboard.h"

/* Must match the window size in main.c */
#define SCREEN_W 800
#define SCREEN_H 450

#define RIGHT_X  504
#define RIGHT_W  284

static bool open = false;

void dashboard_toggle(void)       { open = !open; }
void dashboard_show(bool show)    { open = show; }
bool dashboard_visible(void)      { return open; }

/* ------------------------------------------------------------------ */
/*  The floor plan                                                     */
/* ------------------------------------------------------------------ */

/*
 *   [ Server Room ] [ Storage ]
 *   [ Lobby       ] [ Kitchen ]
 *   [        Hallway          ]
 *   [         Office          ]
 */
static Rectangle tile_rect(int room)
{
    switch (room) {
        case 4:  return (Rectangle){  12,  40, 234, 78 };   /* Server Room */
        case 5:  return (Rectangle){ 258,  40, 234, 78 };   /* Storage     */
        case 2:  return (Rectangle){  12, 126, 234, 78 };   /* Lobby       */
        case 3:  return (Rectangle){ 258, 126, 234, 78 };   /* Kitchen     */
        case 1:  return (Rectangle){  12, 212, 480, 66 };   /* Hallway     */
        default: return (Rectangle){  12, 286, 480, 78 };   /* Office      */
    }
}

/* where the doors between rooms are drawn (same links the simulation uses) */
static const int   LINK_ROOMS[5][2] = { { 4, 2 }, { 5, 3 }, { 2, 1 }, { 3, 1 }, { 1, 0 } };
static const float LINK_X[5]        = { 121, 367, 121, 367, 238 };
static const float LINK_Y[5]        = { 118, 118, 204, 204, 278 };

static Color blend(Color a, Color b, float f)
{
    return (Color){ (unsigned char)(a.r + (b.r - a.r) * f), (unsigned char)(a.g + (b.g - a.g) * f),
                    (unsigned char)(a.b + (b.b - a.b) * f), 255 };
}

/* cold is blue, comfortable is green, warm is yellow, hot is red */
static Color heat_colour(float t)
{
    Color cold = { 90, 150, 255, 255 }, ok = { 110, 220, 140, 255 };
    Color warm = { 255, 220, 90, 255 }, hot = { 255, 90, 50, 255 };
    if (t <= 16.0f) return cold;
    if (t <= 22.0f) return blend(cold, ok, (t - 16.0f) / 6.0f);
    if (t <= 28.0f) return blend(ok, warm, (t - 22.0f) / 6.0f);
    if (t <= 40.0f) return blend(warm, hot, (t - 28.0f) / 12.0f);
    return hot;
}

static bool hit(Rectangle r)
{
    return CheckCollisionPointRec(GetMousePosition(), r);
}

static const char *room_status(const Room *r, Color *col)
{
    if (r->burnt)                       { *col = RED;     return "ROOM LOST"; }
    if (r->sensors.smokeDetected)       { *col = RED;     return "SMOKE ALARM"; }
    if (r->sensors.temperature >= 45.0f){ *col = RED;     return "HEAT ALARM"; }
    if (r->leak)                        { *col = SKYBLUE; return "WATER LEAK"; }
    if (r->breakerTripped)              { *col = ORANGE;  return "NO POWER"; }
    if (r->lockFault > 0.0f)            { *col = ORANGE;  return "DOOR CONTROLLER OFFLINE"; }
    if (r->actuators.sprinklers)        { *col = SKYBLUE; return "SPRINKLERS ON"; }
    if (r->sensors.motionDetected)      { *col = ORANGE;  return "MOTION"; }
    *col = GREEN;
    return "OK";
}

static void draw_chip(Rectangle r, const char *label, Color col, bool lit)
{
    DrawRectangleRec(r, Fade(col, lit ? 0.35f : 0.10f));
    DrawRectangleLinesEx(r, 1, Fade(col, lit ? 1.0f : 0.5f));
    DrawText(label, (int)r.x + 4, (int)r.y + 4, 9, lit ? RAYWHITE : GRAY);
}

static Rectangle chip_rect(int room, int k)
{
    Rectangle t = tile_rect(room);
    if (t.width > 300.0f) {
        return (Rectangle){ t.x + t.width - 6 * 38.0f - 4 + k * 38.0f, t.y + 8, 34, 16 };
    }
    return (Rectangle){ t.x + t.width - 3 * 38.0f - 4 + (k % 3) * 38.0f, t.y + 8 + (k / 3) * 20.0f, 34, 16 };
}

static void draw_tile(const Building *b, int i, float t)
{
    const Room *r = &b->rooms[i];
    Rectangle tile = tile_rect(i);
    bool wide = tile.width > 300.0f;
    bool sel = (i == b->selectedRoom);

    DrawRectangleRec(tile, Fade(heat_colour(r->sensors.temperature), 0.22f));

    Color statusCol;
    const char *status = room_status(r, &statusCol);
    bool alarm = (statusCol.r == RED.r && statusCol.g == RED.g && statusCol.b == RED.b);
    Color edge = (strcmp(status, "OK") == 0) ? Fade(GRAY, 0.7f) : statusCol;
    if (alarm && ((int)(t * 4.0f)) % 2 == 0) edge = RAYWHITE;
    DrawRectangleLinesEx(tile, sel ? 3 : 1, sel ? RAYWHITE : edge);

    int x = (int)tile.x + 8;
    int y = (int)tile.y;
    DrawText(r->name, x, y + 6, 14, RAYWHITE);
    DrawText(TextFormat("%.1f C", r->sensors.temperature), x, y + 24, 22, heat_colour(r->sensors.temperature));
    DrawText(TextFormat("set %.0f C", r->sensors.targetTemp), x, y + 50, 11, GRAY);
    if (wide) DrawText(status, x + 130, y + 30, 12, statusCol);
    else      DrawText(status, x, y + 63, 11, statusCol);

    const Actuators *a = &r->actuators;
    draw_chip(chip_rect(i, 0), r->breakerTripped ? "OFF" : "LGT", YELLOW, a->lightsOn);
    draw_chip(chip_rect(i, 1), a->doorLocked ? "LCK" : "OPN", a->doorLocked ? RED : GREEN, true);
    draw_chip(chip_rect(i, 2), a->hvacOn ? "VNT" : "SEAL", SKYBLUE, a->hvacOn);
    draw_chip(chip_rect(i, 3), "SPR", BLUE, a->sprinklers);
    draw_chip(chip_rect(i, 4), r->breakerTripped ? "TRIP" : "BKR", r->breakerTripped ? RED : GREEN, true);
    draw_chip(chip_rect(i, 5), "PIR", ORANGE, r->sensors.motionDetected);
}

/* ------------------------------------------------------------------ */
/*  The right-hand side: system status and alarms                      */
/* ------------------------------------------------------------------ */

static void draw_meter(int y, const char *label, float fraction, Color col, const char *value)
{
    if (fraction < 0.0f) fraction = 0.0f;
    if (fraction > 1.0f) fraction = 1.0f;
    DrawText(label, RIGHT_X + 8, y, 11, RAYWHITE);
    DrawRectangle(RIGHT_X + 76, y + 1, 130, 8, Fade(GRAY, 0.35f));
    DrawRectangle(RIGHT_X + 76, y + 1, (int)(130 * fraction), 8, col);
    DrawText(value, RIGHT_X + 214, y, 11, RAYWHITE);
}

typedef struct {
    char  text[72];
    Color col;
} AlarmLine;

#define MAX_ALARMS 16

static int collect_alarms(const Building *b, AlarmLine *out)
{
    int n = 0;
#define ADD(color, ...) do { if (n < MAX_ALARMS) { snprintf(out[n].text, sizeof out[n].text, __VA_ARGS__); out[n].col = (color); n++; } } while (0)

    if (b->powerOut)           ADD(RED, "POWER OUT");
    else if (b->power < 20.0f) ADD(ORANGE, "POWER LOW (%.0f%%)", b->power);
    if (b->water < 20.0f)      ADD(ORANGE, "WATER TANK LOW (%.0f%%)", b->water);
    if (b->netOutage > 0.0f)   ADD(ORANGE, "INTERNET DOWN: cloud rules paused");
    if (b->heatwave > 0.0f)    ADD(ORANGE, "HVAC FAULT: plant struggling");

    for (int i = 0; i < b->roomCount; i++) {
        const Room *r = &b->rooms[i];
        if (r->burnt)                         ADD(RED, "%s: ROOM LOST", r->name);
        else if (r->sensors.smokeDetected)    ADD(RED, "%s: SMOKE ALARM", r->name);
        else if (r->sensors.temperature >= 45.0f) ADD(RED, "%s: HEAT ALARM", r->name);
        if (r->leak)                          ADD(SKYBLUE, "%s: WATER LEAK", r->name);
        if (r->breakerTripped)                ADD(ORANGE, "%s: BREAKER OFF", r->name);
        if (r->lockFault > 0.0f)              ADD(ORANGE, "%s: DOOR CONTROLLER OFFLINE", r->name);
        if (r->actuators.sprinklers)          ADD(SKYBLUE, "%s: SPRINKLERS RUNNING", r->name);
    }
#undef ADD
    return n;
}

static void draw_right_pane(const Building *b)
{
    /* system status */
    Rectangle sys = { RIGHT_X, 40, RIGHT_W, 170 };
    DrawRectangleRec(sys, Fade(BLACK, 0.5f));
    DrawRectangleLinesEx(sys, 1, GRAY);
    DrawText("SYSTEM", RIGHT_X + 8, 46, 11, GRAY);

    draw_meter(62, "POWER", b->power / 100.0f,
               (b->power > 40.0f) ? GREEN : (b->power > 20.0f) ? ORANGE : RED, TextFormat("%.0f%%", b->power));
    draw_meter(78, "WATER", b->water / 100.0f, (b->water > 30.0f) ? SKYBLUE : ORANGE, TextFormat("%.0f%%", b->water));
    draw_meter(94, "NETWORK", b->netSpeed / 100.0f, (b->netSpeed > 30.0f) ? GREEN : RED,
               b->netOutage > 0.0f ? "DOWN" : TextFormat("%.0f Mb", b->netSpeed));

    float inside = 0.0f;
    int vents = 0;
    for (int i = 0; i < b->roomCount; i++) {
        inside += b->rooms[i].sensors.temperature;
        if (b->rooms[i].actuators.hvacOn) vents++;
    }
    if (b->roomCount > 0) inside /= (float)b->roomCount;

    DrawText(TextFormat("OUTSIDE %.0f C    INSIDE AVERAGE %.1f C", building_outside_temp(b), inside),
             RIGHT_X + 8, 114, 11, RAYWHITE);
    DrawText(TextFormat("HVAC: %d of %d vents open", vents, b->roomCount), RIGHT_X + 8, 129, 11, RAYWHITE);
    DrawText(TextFormat("TARIFF: %s  %.2f EUR/kWh", building_tariff_name(b), building_tariff(b)),
             RIGHT_X + 8, 144, 11, RAYWHITE);
    DrawText(TextFormat("ENERGY %.1f kWh   COST EUR %.2f   CO2 %.1f kg", b->energy, b->cost, b->energy * 0.30f),
             RIGHT_X + 8, 159, 11, RAYWHITE);
    const char *sched = (b->scheduleMode == 1) ? "NIGHT SETBACK 18 C" : (b->scheduleMode == 2) ? "COMFORT 21 C"
                      : b->rules[RULE_SCHEDULE].enabled ? "starting..." : "OFF";
    DrawText(TextFormat("SCHEDULE: %s", sched), RIGHT_X + 8, 174, 11, GREEN);
    DrawText(b->ecoMode ? "MODE: ECO" : (b->lockdownTimer > 0.0f ? "MODE: LOCKDOWN" : "MODE: NORMAL"),
             RIGHT_X + 8, 190, 11, b->lockdownTimer > 0.0f ? RED : GRAY);

    /* alarms */
    AlarmLine alarms[MAX_ALARMS];
    int n = collect_alarms(b, alarms);

    Rectangle box = { RIGHT_X, 216, RIGHT_W, 72 };
    DrawRectangleRec(box, Fade(BLACK, 0.5f));
    DrawRectangleLinesEx(box, 1, (n > 0) ? RED : GRAY);
    DrawText(TextFormat("ACTIVE ALARMS (%d)", n), RIGHT_X + 8, 221, 11, (n > 0) ? RED : GRAY);

    if (n == 0) {
        DrawText("No active alarms", RIGHT_X + 8, 238, 12, GREEN);
    }
    for (int i = 0; i < n && i < 3; i++) {
        DrawText(alarms[i].text, RIGHT_X + 8, 236 + i * 14, 11, alarms[i].col);
    }
    if (n > 3) DrawText(TextFormat("+ %d more", n - 3), RIGHT_X + 200, 221, 10, GRAY);

    /* automation: the rules the building runs by itself */
    Rectangle auto_box = { RIGHT_X, 292, RIGHT_W, 74 };
    int active = 0;
    for (int i = 0; i < MAX_RULES; i++) if (b->rules[i].enabled) active++;
    DrawRectangleRec(auto_box, Fade(BLACK, 0.5f));
    DrawRectangleLinesEx(auto_box, 1, GRAY);
    DrawText(TextFormat("AUTOMATION (%d of %d rules on)", active, MAX_RULES), RIGHT_X + 8, 297, 11, GREEN);
    if (b->netOutage > 0.0f) DrawText("CLOUD OFFLINE", RIGHT_X + 190, 297, 10, ORANGE);
    for (int i = 0; i < MAX_RULES; i++) {
        int col = i % 2, row = i / 2;
        int x = RIGHT_X + 8 + col * 140;
        int y = 313 + row * 13;
        bool on = b->rules[i].enabled;
        bool offline = rule_is_cloud((RuleId)i) && b->netOutage > 0.0f;
        DrawRectangle(x, y + 1, 6, 7, !on ? RED : offline ? ORANGE : GREEN);
        DrawText(TextFormat("%s %s", rule_name((RuleId)i), b->rules[i].fired > 0 ? TextFormat("x%d", b->rules[i].fired) : ""),
                 x + 10, y - 1, 10, on ? RAYWHITE : GRAY);
    }
}

/* ------------------------------------------------------------------ */
/*  The thermostat schedule and the buttons along the bottom           */
/* ------------------------------------------------------------------ */

static Rectangle cameras_btn(void) { return (Rectangle){ RIGHT_X,       376, 90, 28 }; }
static Rectangle control_btn(void) { return (Rectangle){ RIGHT_X + 96,  376, 90, 28 }; }
static Rectangle graphs_btn(void)  { return (Rectangle){ RIGHT_X + 192, 376, 90, 28 }; }
static Rectangle help_btn(void)    { return (Rectangle){ 524, 4, 66, 22 }; }

static void draw_button(Rectangle r, const char *label, Color col)
{
    DrawRectangleRec(r, Fade(col, hit(r) ? 0.30f : 0.10f));
    DrawRectangleLinesEx(r, 1, col);
    DrawText(label, (int)r.x + 8, (int)r.y + 8, 12, RAYWHITE);
}

static void draw_schedule(const Building *b)
{
    DrawText("THERMOSTAT SCHEDULE", 12, 372, 11, GRAY);
    Rectangle bar = { 12, 388, 480, 14 };
    DrawRectangle((int)bar.x, (int)bar.y, 400, 14, (Color){ 50, 90, 160, 255 });          /* 12 AM to 5 AM */
    DrawRectangle((int)bar.x + 400, (int)bar.y, 80, 14, (Color){ 200, 120, 40, 255 });    /* 5 AM to 6 AM */
    DrawText("NIGHT SETBACK 18 C", 20, 391, 10, RAYWHITE);
    DrawText("COMFORT 21 C", 412, 391, 10, RAYWHITE);
    DrawRectangleLinesEx(bar, 1, GRAY);

    float x = bar.x + bar.width * (b->nightTime / (NIGHT_HOURS * SECONDS_PER_HOUR));
    DrawRectangle((int)x - 1, (int)bar.y - 4, 3, 22, RED);

    for (int h = 0; h <= NIGHT_HOURS; h++) {
        int hx = 12 + h * 80;
        DrawText(h == 0 ? "12AM" : TextFormat("%dAM", h), hx - (h == 6 ? 22 : 0), 406, 10, GRAY);
    }
}

/* ------------------------------------------------------------------ */
/*  Input and drawing                                                  */
/* ------------------------------------------------------------------ */

void dashboard_update(Building *b)
{
    if (IsKeyPressed(KEY_B)) open = !open;
    if (!open || b->state != GAME_PLAYING) return;
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return;
    if (panel_contains((int)GetMousePosition().x, (int)GetMousePosition().y)) return;

    static double lastClick = -10.0;
    static int lastRoom = -1;

    for (int i = 0; i < b->roomCount; i++) {
        if (hit(tile_rect(i))) {
            b->selectedRoom = i;
            ui_click_sound();
            double now = GetTime();
            if (lastRoom == i && now - lastClick < 0.4) open = false;   /* double click: watch its camera */
            lastClick = now;
            lastRoom = i;
        }
    }
    if (hit(cameras_btn())) { open = false; ui_click_sound(); }
    if (hit(control_btn())) { panel_toggle(); ui_click_sound(); }
    if (hit(graphs_btn()))  { graphs_toggle(); ui_click_sound(); }
    if (hit(help_btn()))    { help_show(true); ui_click_sound(); }
}

/* what to say when the mouse rests on something */
static const char *tip_for(const Building *b, char *buf, size_t n)
{
    for (int i = 0; i < b->roomCount; i++) {
        const Room *r = &b->rooms[i];
        const char *chips[6] = {
            r->actuators.lightsOn ? "Lights: ON" : "Lights: OFF",
            r->actuators.doorLocked ? "Door: LOCKED" : "Door: open",
            r->actuators.hvacOn ? "Vent: OPEN" : "Vent: SEALED",
            r->actuators.sprinklers ? "Sprinkler valve: OPEN" : "Sprinkler valve: shut",
            r->breakerTripped ? "Circuit breaker: TRIPPED (no power)" : "Circuit breaker: on",
            r->sensors.motionDetected ? "Motion sensor: motion detected" : "Motion sensor: quiet"
        };
        for (int k = 0; k < 6; k++) {
            if (hit(chip_rect(i, k))) {
                snprintf(buf, n, "%s: %s", r->name, chips[k]);
                return buf;
            }
        }
    }
    for (int l = 0; l < 5; l++) {
        Rectangle d = { LINK_X[l] - 4, LINK_Y[l] - 4, 24, 16 };
        if (hit(d)) {
            const Room *ra = &b->rooms[LINK_ROOMS[l][0]];
            const Room *rb = &b->rooms[LINK_ROOMS[l][1]];
            bool locked = ra->actuators.doorLocked || rb->actuators.doorLocked;
            snprintf(buf, n, "Door between %s and %s: %s", ra->name, rb->name, locked ? "LOCKED" : "open");
            return buf;
        }
    }
    for (int i = 0; i < b->roomCount; i++) {
        if (hit(tile_rect(i))) {
            Color c;
            const Room *r = &b->rooms[i];
            snprintf(buf, n, "%s: %.1f C, set to %.0f C\nStatus: %s\nClick to select, double-click for its camera",
                     r->name, r->sensors.temperature, r->sensors.targetTemp, room_status(r, &c));
            return buf;
        }
    }
    if (hit(cameras_btn())) return "CAMERAS\nWatch the selected room (or double-click a room)";
    if (hit(control_btn())) return "CONTROL PANEL (SPACE)\nThermostats, doors, scenes, automation, power, water";
    if (hit(graphs_btn()))  return "GRAPHS (TAB)\nNetwork, temperatures, motion, ventilation";
    if (hit(help_btn()))    return "HELP (H)\nHow to play";
    return NULL;
}

void dashboard_draw(const Building *b)
{
    float t = (float)GetTime();

    /* header */
    DrawRectangle(0, 0, SCREEN_W, 30, (Color){ 12, 16, 22, 255 });
    DrawText("SMART BUILDING CONTROLLER", 12, 7, 16, RAYWHITE);
    DrawText("LIVE MONITORING", 290, 10, 12, GOLD);
    DrawRectangleRec(help_btn(), Fade(GOLD, hit(help_btn()) ? 0.30f : 0.10f));
    DrawRectangleLinesEx(help_btn(), 1, GOLD);
    DrawText("? HELP", 534, 9, 12, RAYWHITE);
    char clock[16];
    clock_text(b->nightTime, clock, sizeof clock);
    const char *right = TextFormat("%s     OUTSIDE %.0f C", clock, building_outside_temp(b));
    DrawText(right, SCREEN_W - 12 - MeasureText(right, 14), 8, 14, GRAY);

    /* the rooms, and the doors between them */
    for (int i = 0; i < b->roomCount; i++) draw_tile(b, i, t);
    for (int l = 0; l < 5; l++) {
        const Room *ra = &b->rooms[LINK_ROOMS[l][0]];
        const Room *rb = &b->rooms[LINK_ROOMS[l][1]];
        bool locked = ra->actuators.doorLocked || rb->actuators.doorLocked;
        DrawRectangle((int)LINK_X[l], (int)LINK_Y[l], 16, 8, locked ? RED : GREEN);
    }

    draw_right_pane(b);
    draw_schedule(b);

    draw_button(cameras_btn(), "CAMERAS", GREEN);
    draw_button(control_btn(), "CONTROL", SKYBLUE);
    draw_button(graphs_btn(),  "GRAPHS",  ORANGE);
    DrawText("Click a room to select it; double-click or", RIGHT_X, 412, 10, GRAY);
    DrawText("CAMERAS to watch it.  B = back to cameras.", RIGHT_X, 424, 10, GRAY);

    DrawText("Chips: LGT lights  LCK/OPN door  VNT/SEAL vent  SPR sprinklers  BKR breaker  PIR motion",
             12, 434, 10, GRAY);

    char buf[160];
    const char *tip = tip_for(b, buf, sizeof buf);
    if (tip && !panel_contains((int)GetMousePosition().x, (int)GetMousePosition().y)) help_tooltip(tip);
}