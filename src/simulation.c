#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "building.h"

/* Tuning knobs: power lost per second, and how long motion shows on a camera */
#define POWER_BASE_DRAIN   0.05f
#define POWER_DOOR_DRAIN   0.15f   /* for each locked door */
#define POWER_LEECH_DRAIN  0.08f
#define MOTION_SECONDS     4.0f

/*
 * Room layout (index order matters):
 *   0 Office  1 Hallway  2 Lobby  3 Kitchen  4 Server Room  5 Storage
 * NEXT_HOP[room] = the room an intruder steps into to get closer to the Office.
 */
static const int NEXT_HOP[MAX_ROOMS] = { -1, 0, 1, 1, 2, 3, -1, -1 };

static void add_room(Building *b, const char *name, float temp)
{
    if (b->roomCount >= MAX_ROOMS) return;

    Room *r = &b->rooms[b->roomCount];
    memset(r, 0, sizeof(Room));

    r->id = b->roomCount;
    strncpy(r->name, name, ROOM_NAME_LEN - 1);

    r->sensors.temperature        = temp;
    r->sensors.targetTemp         = 21.0f;
    r->sensors.lightLevel         = 0.5f;
    r->sensors.secondsSinceMotion = 999.0f;   /* no motion yet */

    r->actuators.lightsOn   = true;
    r->actuators.doorLocked = false;
    r->actuators.hvacOn     = true;

    r->alert = ALERT_NONE;
    b->roomCount++;
}

static void add_intruder(Building *b, IntruderType type, const char *name,
                         int room, int startHour, float interval, int aggression)
{
    if (b->intruderCount >= MAX_INTRUDERS) return;

    Intruder *in = &b->intruders[b->intruderCount];
    memset(in, 0, sizeof(Intruder));

    in->type       = type;
    strncpy(in->name, name, INTRUDER_NAME_LEN - 1);
    in->room       = room;
    in->home       = room;
    in->startHour  = startHour;
    in->interval   = interval;
    in->timer      = interval;
    in->aggression = aggression;
    b->intruderCount++;
}

void building_init(Building *b)
{
    memset(b, 0, sizeof(Building));
    srand((unsigned)time(NULL));

    add_room(b, "Office",      22.0f);
    add_room(b, "Hallway",     20.5f);
    add_room(b, "Lobby",       21.0f);
    add_room(b, "Kitchen",     23.0f);
    add_room(b, "Server Room", 24.0f);
    add_room(b, "Storage",     19.5f);

    b->selectedRoom = GOAL_ROOM;
    b->power  = 100.0f;
    b->killer = -1;

    /*                          type               name       room hour  secs aggr */
    add_intruder(b, INTRUDER_DRIFTER, "Drifter", 4, 0, 15.0f, 6);
    add_intruder(b, INTRUDER_SHADE,   "Shade",   5, 1, 12.0f, 6);
    add_intruder(b, INTRUDER_GLITCH,  "Glitch",  3, 2, 18.0f, 6);
    add_intruder(b, INTRUDER_CRAWLER, "Crawler", 4, 3, 20.0f, 8);
    add_intruder(b, INTRUDER_LEECH,   "Leech",   5, 4, 22.0f, 6);
}

void room_evaluate_alert(Room *r)
{
    if (r->sensors.smokeDetected) {
        r->alert = ALERT_CRITICAL;
    } else if (r->sensors.temperature > 28.0f || r->sensors.temperature < 15.0f) {
        r->alert = ALERT_WARNING;
    } else {
        r->alert = ALERT_NONE;
    }
}

static bool can_enter(const Building *b, int room)
{
    return room >= 0 && !b->rooms[room].actuators.doorLocked;
}

static void move_intruder(Building *b, int idx)
{
    Intruder *in = &b->intruders[idx];
    int target = NEXT_HOP[in->room];
    if (target < 0) return;

    /* the Crawler goes through the vents: it can skip a room (and its locked door),
       but only if that room's vent is open (HVAC on) */
    if (in->type == INTRUDER_CRAWLER && NEXT_HOP[target] >= 0
        && b->rooms[target].actuators.hvacOn && can_enter(b, NEXT_HOP[target])) {
        target = NEXT_HOP[target];
    }

    if (!can_enter(b, target)) {
        /* blocked at the Office door: it gives up and wanders back to where it started */
        if (target == GOAL_ROOM) {
            in->room = in->home;
            b->rooms[in->home].sensors.secondsSinceMotion = 0.0f;
        }
        return;
    }

    in->room = target;
    b->rooms[target].sensors.secondsSinceMotion = 0.0f;

    if (target == GOAL_ROOM) {
        b->killer = idx;
        b->state  = GAME_OVER;
    }
}

float building_power_usage(const Building *b)
{
    float use = POWER_BASE_DRAIN;

    for (int i = 0; i < b->roomCount; i++) {
        if (b->rooms[i].actuators.doorLocked) use += POWER_DOOR_DRAIN;
    }
    for (int i = 0; i < b->intruderCount; i++) {
        if (b->intruders[i].active && b->intruders[i].type == INTRUDER_LEECH) {
            use += POWER_LEECH_DRAIN;
        }
    }
    return use;
}

void building_update(Building *b, float dt)
{
    if (b->state != GAME_PLAYING) return;

    /* the night clock */
    b->nightTime += dt;
    if (b->nightTime >= NIGHT_HOURS * SECONDS_PER_HOUR) {
        b->nightTime = NIGHT_HOURS * SECONDS_PER_HOUR;
        b->state = GAME_WON;
        return;
    }
    int hour = building_hour(b);

    /* power: drains faster the more is switched on; at zero everything fails */
    if (!b->powerOut) {
        b->power -= building_power_usage(b) * dt;
        if (b->power <= 0.0f) {
            b->power = 0.0f;
            b->powerOut = true;
            for (int i = 0; i < b->roomCount; i++) {
                b->rooms[i].actuators.lightsOn   = false;
                b->rooms[i].actuators.doorLocked = false;
            }
        }
    }

    /* intruders */
    for (int i = 0; i < b->intruderCount; i++) {
        Intruder *in = &b->intruders[i];

        in->active = (hour >= in->startHour);
        if (!in->active) continue;

        /* the Shade only moves while no camera is on it */
        if (in->type == INTRUDER_SHADE && b->selectedRoom == in->room) continue;

        in->timer -= dt;
        if (in->timer <= 0.0f) {
            in->timer = in->interval;
            if (rand() % 20 < in->aggression) move_intruder(b, i);
            if (b->state != GAME_PLAYING) return;
        }

        /* the Glitch kills the lights wherever it stands */
        if (in->type == INTRUDER_GLITCH) b->rooms[in->room].actuators.lightsOn = false;
    }

    /* rooms: motion sensors and alerts */
    for (int i = 0; i < b->roomCount; i++) {
        Room *r = &b->rooms[i];
        r->sensors.secondsSinceMotion += dt;
        r->sensors.motionDetected = (r->sensors.secondsSinceMotion < MOTION_SECONDS);

        /* temperature: the HVAC pulls it to the target, otherwise the room slowly heats up */
        float goal = r->actuators.hvacOn ? r->sensors.targetTemp : 30.0f;
        float step = (r->actuators.hvacOn ? 0.08f : 0.03f) * dt;
        float diff = goal - r->sensors.temperature;
        if (diff > step)       r->sensors.temperature += step;
        else if (diff < -step) r->sensors.temperature -= step;
        else                   r->sensors.temperature = goal;

        room_evaluate_alert(r);
    }
}

void room_set_lights(Room *r, bool on)
{
    r->actuators.lightsOn = on;
}

void room_set_lock(Room *r, bool locked)
{
    r->actuators.doorLocked = locked;
}

void room_set_target_temp(Room *r, float temp)
{
    r->sensors.targetTemp = temp;
}

void room_set_hvac(Room *r, bool on)
{
    r->actuators.hvacOn = on;
}

int building_hour(const Building *b)
{
    int hour = (int)(b->nightTime / SECONDS_PER_HOUR);
    return hour > NIGHT_HOURS ? NIGHT_HOURS : hour;
}

void game_lose(Building *b)
{
    b->state = GAME_OVER;
}