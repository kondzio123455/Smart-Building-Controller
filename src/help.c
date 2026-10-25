#include <string.h>
#include "raylib.h"
#include "help.h"

#define SCREEN_W 800
#define SCREEN_H 450
#define PAGES    3

static bool open = false;
static int  page = 0;

void help_show(bool show)
{
    open = show;
    if (show) page = 0;
}

bool help_visible(void) { return open; }

void help_page(int p)
{
    if (p >= 0 && p < PAGES) page = p;
}

/* ------------------------------------------------------------------ */
/*  The pages                                                          */
/* ------------------------------------------------------------------ */

typedef struct {
    const char *text;
    int         kind;       /* 0 body, 1 heading, 2 gap */
} Line;

static const Line PAGE1[] = {
    { "You are the night manager of a six-room smart building. Sensors watch every room and", 0 },
    { "controllers run the lights, doors, vents, sprinklers and power. Get everyone safely to 6 AM.", 0 },
    { "", 2 },
    { "WHAT CAN GO WRONG", 1 },
    { "  INTRUDERS creep through the building toward the OFFICE. If one gets in, the night is over.", 0 },
    { "  FIRES start at random. They spread, fill rooms with smoke and can burn a room down for good.", 0 },
    { "  FAULTS: power surges, burst pipes, internet outages, false alarms, broken door controllers.", 0 },
    { "  RESOURCES run out: the battery and the sprinkler water. Locked doors use the most power.", 0 },
    { "", 2 },
    { "THE SCREENS", 1 },
    { "  BUILDING  (B)        the floor plan, with live readings and alarms for every room", 0 },
    { "  CAMERAS  (1-6)       look into a room and click things in it: doors, lights, vents, sprinklers", 0 },
    { "  CONTROL  (SPACE)     thermostats, doors, vents, scenes, automations, power and water", 0 },
    { "  GRAPHS  (TAB)        network, temperatures, motion, ventilation", 0 },
    { "", 2 },
    { "Point at anything to see what it is. Press H whenever you want this help again.", 0 },
};

static const Line PAGE2[] = {
    { "KEYBOARD", 1 },
    { "  1-6 cameras     B building     SPACE control panel     TAB graphs     P pause     H help", 0 },
    { "  L lights     D door lock     S sprinklers     G fancy graphics on or off     R restart", 0 },
    { "  Z LOCKDOWN     X ECO     C FIRE RESPONSE     V RESET   (the four scenes)", 0 },
    { "", 2 },
    { "MOUSE", 1 },
    { "  Click objects in a room: the door, the ceiling lights, the vent, the sprinkler, the thermostat.", 0 },
    { "  On the building overview, click a room to select it, double-click to watch its camera.", 0 },
    { "", 2 },
    { "WHEN IT HAPPENS...", 1 },
    { "Something in the HALLWAY|lock the Office door (camera 1, press D), or switch on AUTO-DOOR", 3 },
    { "A fire or a smoke alarm|press C. Never open the door of a sealed, burning room: backdraft!", 3 },
    { "Grease fire (the Kitchen)|no water: seal the room and let the fire starve", 3 },
    { "Electrical fire (Server Room)|cut the room's power in the control panel", 3 },
    { "Power running low|press X for ECO mode", 3 },
    { "Burst pipe|CONTROL > UTILITY > close the main water valve", 3 },
    { "Breaker tripped|CONTROL > UTILITY > click the breaker to switch it back on", 3 },
};

static const Line PAGE3[] = {
    { "AUTOMATION: LET THE BUILDING HELP YOU   (CONTROL > AUTO, click a rule to switch it)", 1 },
    { "", 2 },
    { "AUTO-FIRE|LOCAL|seals a room when its smoke alarm rings, sprays when it is hot, cuts power in electrical fires", 4 },
    { "AUTO-VENT|CLOUD|opens a vent when a room passes 26 C", 4 },
    { "AUTO-ECO|CLOUD|switches to ECO mode when the battery drops below 25%", 4 },
    { "AUTO-LIGHTS|CLOUD|turns the lights on when motion is seen in a dark room", 4 },
    { "AUTO-DOOR|CLOUD|locks the Office door while a villain is close   (starts switched OFF)", 4 },
    { "AUTO-LEAK|CLOUD|shuts the main water valve when a leak sensor trips", 4 },
    { "AUTO-SCHEDULE|CLOUD|night setback to 18 C, back to 21 C at 5 AM to save energy", 4 },
    { "", 2 },
    { "CLOUD rules stop while the internet is down. LOCAL rules always work.", 0 },
    { "", 2 },
    { "READING THE SCREEN", 1 },
    { "  GREEN = fine     ORANGE = warning     RED = alarm     BLUE = water     YELLOW = lights", 0 },
    { "  On the building overview a flashing room is on fire or full of smoke.", 0 },
    { "  The little chips on each room: LGT lights, LCK/OPN door, VNT/SEAL vent, SPR sprinklers,", 0 },
    { "  BKR breaker, PIR motion sensor.", 0 },
};

#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

static const char *TITLES[PAGES] = {
    "WELCOME TO THE SMART BUILDING CONTROLLER", "HOW TO PLAY", "AUTOMATION AND COLOURS"
};

/* ------------------------------------------------------------------ */
/*  Input and drawing                                                  */
/* ------------------------------------------------------------------ */

static Rectangle back_btn(void)  { return (Rectangle){ 40,  392, 100, 28 }; }
static Rectangle next_btn(void)  { return (Rectangle){ 150, 392, 100, 28 }; }
static Rectangle close_btn(void) { return (Rectangle){ 600, 392, 160, 28 }; }

void help_update(void)
{
    if (IsKeyPressed(KEY_H) || IsKeyPressed(KEY_F1)) {
        open = !open;
        if (open) page = 0;
        return;
    }
    if (!open) return;

    if (IsKeyPressed(KEY_RIGHT) && page < PAGES - 1) page++;
    if (IsKeyPressed(KEY_LEFT)  && page > 0)         page--;
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        if (page < PAGES - 1) page++;
        else                  open = false;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 m = GetMousePosition();
        if (CheckCollisionPointRec(m, back_btn()) && page > 0)         page--;
        if (CheckCollisionPointRec(m, next_btn()) && page < PAGES - 1) page++;
        if (CheckCollisionPointRec(m, close_btn()))                    open = false;
    }
}

/* draws "one|two|three" in columns that start at fixed places */
static void draw_columns(const char *text, int y, const int *xs, int cols)
{
    char part[200];
    const char *p = text;
    for (int c = 0; c < cols && p; c++) {
        const char *bar = strchr(p, '|');
        int len = bar ? (int)(bar - p) : (int)strlen(p);
        if (len > 199) len = 199;
        memcpy(part, p, (size_t)len);
        part[len] = '\0';
        DrawText(part, xs[c], y, 12, (c == 0) ? SKYBLUE : (cols == 3 && c == 1) ? GRAY : RAYWHITE);
        p = bar ? bar + 1 : NULL;
    }
}

static void draw_page(const Line *lines, int count)
{
    static const int twoCols[2]   = { 52, 290 };
    static const int threeCols[3] = { 52, 160, 230 };

    int y = 62;
    for (int i = 0; i < count; i++) {
        switch (lines[i].kind) {
            case 2:  y += 8; break;
            case 1:  DrawText(lines[i].text, 40, y, 14, GOLD);      y += 20; break;
            case 3:  draw_columns(lines[i].text, y, twoCols, 2);    y += 18; break;
            case 4:  draw_columns(lines[i].text, y, threeCols, 3);  y += 18; break;
            default: DrawText(lines[i].text, 40, y, 12, RAYWHITE);  y += 17; break;
        }
    }
}

static void draw_button(Rectangle r, const char *label, Color col, bool enabled)
{
    bool hov = enabled && CheckCollisionPointRec(GetMousePosition(), r);
    DrawRectangleRec(r, Fade(col, hov ? 0.35f : 0.12f));
    DrawRectangleLinesEx(r, 1, enabled ? col : GRAY);
    DrawText(label, (int)r.x + 10, (int)r.y + 8, 12, enabled ? RAYWHITE : GRAY);
}

void help_draw(void)
{
    if (!open) return;

    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(BLACK, 0.80f));
    Rectangle box = { 24, 14, 752, 422 };
    DrawRectangleRec(box, (Color){ 10, 14, 20, 245 });
    DrawRectangleLinesEx(box, 2, SKYBLUE);

    DrawText(TITLES[page], 40, 26, 20, RAYWHITE);
    DrawText(TextFormat("%d / %d", page + 1, PAGES), 700, 30, 14, GRAY);

    switch (page) {
        case 0:  draw_page(PAGE1, COUNT(PAGE1)); break;
        case 1:  draw_page(PAGE2, COUNT(PAGE2)); break;
        default: draw_page(PAGE3, COUNT(PAGE3)); break;
    }

    draw_button(back_btn(), "< BACK", SKYBLUE, page > 0);
    draw_button(next_btn(), "NEXT >", SKYBLUE, page < PAGES - 1);
    draw_button(close_btn(), page == PAGES - 1 ? "START THE NIGHT" : "CLOSE (H)", GREEN, true);
    DrawText("Arrow keys or the buttons change page. The game is paused while this is open.", 40, 372, 11, GRAY);
}

/* ------------------------------------------------------------------ */
/*  Hint boxes                                                         */
/* ------------------------------------------------------------------ */

void help_tooltip(const char *text)
{
    if (!text || !text[0]) return;

    /* split into lines and find the widest */
    char lines[5][96];
    int count = 0, widest = 0;
    const char *p = text;
    while (*p && count < 5) {
        const char *end = strchr(p, '\n');
        int len = end ? (int)(end - p) : (int)strlen(p);
        if (len > 95) len = 95;
        memcpy(lines[count], p, (size_t)len);
        lines[count][len] = '\0';
        int w = MeasureText(lines[count], 12);
        if (w > widest) widest = w;
        count++;
        if (!end) break;
        p = end + 1;
    }

    Vector2 m = GetMousePosition();
    int bw = widest + 16;
    int bh = count * 16 + 10;
    int x = (int)m.x + 16;
    int y = (int)m.y + 18;
    if (x + bw > SCREEN_W - 4) x = (int)m.x - bw - 8;
    if (y + bh > SCREEN_H - 4) y = (int)m.y - bh - 8;
    if (x < 4) x = 4;
    if (y < 4) y = 4;

    DrawRectangle(x, y, bw, bh, (Color){ 8, 10, 16, 235 });
    DrawRectangleLines(x, y, bw, bh, GOLD);
    for (int i = 0; i < count; i++) {
        DrawText(lines[i], x + 8, y + 5 + i * 16, 12, (i == 0) ? RAYWHITE : LIGHTGRAY);
    }
}
