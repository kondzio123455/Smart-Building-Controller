#include <string.h>
#include "raylib.h"
#include "graphs.h"

#define HIST            100     /* samples kept per graph */
#define SAMPLE_SECONDS  1.0f    /* real seconds between samples */

typedef struct {
    bool  visible;
    bool  started;
    float acc;
    float lastNight;
    int   n;

    float net[HIST];                  /* simulated */
    float roomTemp[MAX_ROOMS][HIST];
    float motion[HIST];
    float vent[HIST];
    float tOpen[HIST];                /* measured: avg temp, rooms with vent open   */
    float tClosed[HIST];              /* measured: avg temp, rooms with vent sealed */

    float netNow;
    float lastOpen, lastClosed;
    bool  prevMotion[MAX_ROOMS];
    int   motionBucket;
} GraphState;

static GraphState G;

void graphs_toggle(void)
{
    G.visible = !G.visible;
}

bool graphs_visible(void)
{
    return G.visible;
}

static void push(float *a, int n, float v)
{
    if (n < HIST) {
        a[n] = v;
    } else {
        memmove(a, a + 1, (HIST - 1) * sizeof(float));
        a[HIST - 1] = v;
    }
}

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static void reset(void)
{
    bool vis = G.visible;
    memset(&G, 0, sizeof(G));
    G.visible = vis;
    G.started = true;
    G.netNow  = 60.0f;
}

static void take_sample(const Building *b)
{
    /* network: the speed the simulation reports (it drops during an internet failure, a dead Server Room, a Glitch...) */
    float net = b->netSpeed;

    /* temperatures, plus open/sealed averages */
    float sumOn = 0.0f, sumOff = 0.0f, all = 0.0f;
    int   nOn = 0, nOff = 0;
    for (int i = 0; i < b->roomCount; i++) {
        float t = b->rooms[i].sensors.temperature;
        push(G.roomTemp[i], G.n, t);
        all += t;
        if (b->rooms[i].actuators.hvacOn) { sumOn += t;  nOn++;  }
        else                              { sumOff += t; nOff++; }
    }
    if (b->roomCount > 0) all /= b->roomCount;

    /* with no rooms in a group, hold the last value (starts at the overall average) */
    if (G.n == 0) G.lastOpen = G.lastClosed = all;
    if (nOn  > 0) G.lastOpen   = sumOn  / nOn;
    if (nOff > 0) G.lastClosed = sumOff / nOff;

    float vent = (b->roomCount > 0) ? 100.0f * nOn / b->roomCount : 0.0f;
    if (b->powerOut) vent = 0.0f;

    push(G.net,     G.n, net);
    push(G.motion,  G.n, (float)G.motionBucket);
    push(G.vent,    G.n, vent);
    push(G.tOpen,   G.n, G.lastOpen);
    push(G.tClosed, G.n, G.lastClosed);

    G.motionBucket = 0;
    if (G.n < HIST) G.n++;
}

void graphs_update(const Building *b, float dt)
{
    if (!G.started) reset();

    /* restarted night: old history is meaningless */
    if (b->nightTime < G.lastNight - 0.5f) reset();
    G.lastNight = b->nightTime;

    if (IsKeyPressed(KEY_TAB)) G.visible = !G.visible;

    /* CLOSE button, top right of the dashboard */
    if (G.visible && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Rectangle close = { (float)GetScreenWidth() - 90, 6, 78, 22 };
        if (CheckCollisionPointRec(GetMousePosition(), close)) G.visible = false;
    }

    /* motion trigger = a room going from quiet to motion */
    for (int i = 0; i < b->roomCount; i++) {
        bool m = b->rooms[i].sensors.motionDetected;
        if (m && !G.prevMotion[i]) G.motionBucket++;
        G.prevMotion[i] = m;
    }

    if (b->state != GAME_PLAYING) return;

    G.acc += dt;
    while (G.acc >= SAMPLE_SECONDS) {
        G.acc -= SAMPLE_SECONDS;
        take_sample(b);
    }
}

static void plot(Rectangle r, const char *title, const char *unit,
                 float lo, float hi, int nser, const float *const *ser,
                 const Color *cols, const char **names)
{
    DrawRectangleRec(r, Fade(DARKGRAY, 0.45f));
    DrawRectangleLinesEx(r, 1, GRAY);
    DrawText(title, (int)r.x + 6, (int)r.y + 4, 12, RAYWHITE);

    Rectangle p = { r.x + 30, r.y + 22, r.width - 38, r.height - 38 };

    for (int g = 0; g <= 4; g++) {
        float y = p.y + p.height * g / 4.0f;
        DrawLineEx((Vector2){ p.x, y }, (Vector2){ p.x + p.width, y }, 1, Fade(GRAY, 0.35f));
        DrawText(TextFormat("%.0f", hi - (hi - lo) * g / 4.0f), (int)r.x + 3, (int)y - 4, 8, GRAY);
    }

    for (int s = 0; s < nser; s++) {
        for (int i = 1; i < G.n; i++) {
            float x0 = p.x + p.width * (HIST - G.n + i - 1) / (float)(HIST - 1);
            float x1 = p.x + p.width * (HIST - G.n + i)     / (float)(HIST - 1);
            float v0 = clampf(ser[s][i - 1], lo, hi);
            float v1 = clampf(ser[s][i],     lo, hi);
            float y0 = p.y + p.height * (1.0f - (v0 - lo) / (hi - lo));
            float y1 = p.y + p.height * (1.0f - (v1 - lo) / (hi - lo));
            DrawLineEx((Vector2){ x0, y0 }, (Vector2){ x1, y1 }, 2, cols[s]);
        }
    }

    int lx = (int)r.x + 6;
    for (int s = 0; s < nser; s++) {
        DrawRectangle(lx, (int)(r.y + r.height - 12), 8, 8, cols[s]);
        DrawText(names[s], lx + 11, (int)(r.y + r.height - 13), 9, RAYWHITE);
        lx += 14 + MeasureText(names[s], 9) + 8;
    }
    if (G.n > 0) {
        const char *v = TextFormat("%.1f%s", ser[0][G.n - 1], unit);
        DrawText(v, (int)(r.x + r.width) - MeasureText(v, 12) - 6, (int)r.y + 4, 12, cols[0]);
    }
}

void graphs_draw(const Building *b)
{
    if (!G.visible) return;

    int W = GetScreenWidth();
    int H = GetScreenHeight();
    DrawRectangle(0, 0, W, H, Fade(BLACK, 0.9f));
    DrawText("MONITORING DASHBOARD   (TAB or CLOSE - the night keeps running)", 12, 8, 16, RAYWHITE);

    Rectangle closeBtn = { (float)W - 90, 6, 78, 22 };
    DrawRectangleRec(closeBtn, Fade(RED, 0.5f));
    DrawRectangleLinesEx(closeBtn, 2, RED);
    DrawText("CLOSE", (int)closeBtn.x + 14, (int)closeBtn.y + 4, 14, RAYWHITE);

    const int cols = 3, rows = 2, gap = 8, top = 34;
    float cw = (W - gap * (cols + 1)) / (float)cols;
    float ch = (H - top - gap * (rows + 1)) / (float)rows;
    #define CELL(i) ((Rectangle){ gap + ((i) % cols) * (cw + gap), top + gap + ((i) / cols) * (ch + gap), cw, ch })

    {   /* 1: network (simulated) */
        const float *s[1] = { G.net };
        Color c[1] = { SKYBLUE };
        const char *nm[1] = { "controller link (simulated)" };
        plot(CELL(0), "1  NETWORK SPEED (Mbps)", "", 0, 100, 1, s, c, nm);
    }
    {   /* 2: room temperatures */
        const float *s[MAX_ROOMS];
        Color c[MAX_ROOMS];
        const char *nm[MAX_ROOMS];
        const Color pal[MAX_ROOMS] = { RAYWHITE, ORANGE, YELLOW, LIME, SKYBLUE, PINK, MAGENTA, GOLD };
        static const char *short_nm[MAX_ROOMS] = { "Off", "Hall", "Lob", "Kit", "Srv", "Sto", "-", "-" };
        int k = b->roomCount;
        for (int i = 0; i < k; i++) { s[i] = G.roomTemp[i]; c[i] = pal[i]; nm[i] = short_nm[i]; }
        plot(CELL(1), "2  ROOM TEMPERATURE (C)", "", 15, 32, k, s, c, nm);
    }
    {   /* 3: motion */
        const float *s[1] = { G.motion };
        Color c[1] = { RED };
        const char *nm[1] = { "new triggers per second" };
        plot(CELL(2), "3  MOTION SENSOR TRIGGERS", "", 0, 6, 1, s, c, nm);
    }
    {   /* 4: ventilation */
        const float *s[1] = { G.vent };
        Color c[1] = { GREEN };
        const char *nm[1] = { "rooms with vent open" };
        plot(CELL(3), "4  VENTILATION (%)", "%", 0, 100, 1, s, c, nm);
    }
    {   /* 5: open vs sealed, measured */
        const float *s[2] = { G.tOpen, G.tClosed };
        Color c[2] = { SKYBLUE, ORANGE };
        const char *nm[2] = { "vent open (avg)", "vent sealed (avg)" };
        plot(CELL(4), "5  TEMP: VENT OPEN vs SEALED", "", 15, 32, 2, s, c, nm);
    }
    {   /* spare cell: summary */
        Rectangle r = CELL(5);
        DrawRectangleRec(r, Fade(DARKGRAY, 0.45f));
        DrawRectangleLinesEx(r, 1, GRAY);
        DrawText("CONTROLLER STATUS", (int)r.x + 6, (int)r.y + 4, 12, RAYWHITE);
        int hot = 0;
        for (int i = 0; i < b->roomCount; i++) if (b->rooms[i].sensors.motionDetected) hot++;
        DrawText(TextFormat("Power:  %.0f%%%s", b->power, b->powerOut ? "  OUT" : ""),
                 (int)r.x + 10, (int)r.y + 30, 14, b->powerOut ? RED : RAYWHITE);
        DrawText(TextFormat("Rooms:  %d", b->roomCount), (int)r.x + 10, (int)r.y + 52, 14, RAYWHITE);
        DrawText(TextFormat("Motion now:  %d room(s)", hot), (int)r.x + 10, (int)r.y + 74, 14, RAYWHITE);
        DrawText(TextFormat("Integrity:  %.0f%%", b->integrity), (int)r.x + 10, (int)r.y + 96, 14,
                 (b->integrity > 60.0f) ? RAYWHITE : (b->integrity > 30.0f) ? ORANGE : RED);
        DrawText(TextFormat("Fires tonight:  %d", b->fireCount), (int)r.x + 10, (int)r.y + 118, 14,
                 (b->fireCount > 0) ? ORANGE : RAYWHITE);
        DrawText(TextFormat("Samples:  %d / %d", G.n, HIST), (int)r.x + 10, (int)r.y + 140, 14, GRAY);
    }
    #undef CELL
}