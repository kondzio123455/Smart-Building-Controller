#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "building.h"

/* ------------------------------------------------------------------ */
/*  Tuning knobs                                                       */
/* ------------------------------------------------------------------ */

/* power lost per second */
#define POWER_BASE_DRAIN   0.02f
#define POWER_LIGHT_DRAIN  0.005f   /* each light that is on */
#define POWER_VENT_DRAIN   0.0015f  /* each open vent; more the further the setting is from the weather outside */
#define POWER_DOOR_DRAIN   0.15f    /* each locked door */
#define POWER_PUMP_DRAIN   0.10f    /* the sprinkler pump, while any head is spraying */
#define POWER_LEECH_DRAIN  0.08f
#define ECO_FACTOR         0.5f     /* ECO halves the standby loads (base, lights, vents) */
#define KWH_PER_POWER      0.25f    /* 1% of the battery is a quarter of a kWh */

#define MOTION_SECONDS     4.0f

/* temperature: an open vent pulls a room to its target, a sealed one lets it heat up */
#define TEMP_RATE_OPEN     0.08f    /* degrees C per second */
#define TEMP_RATE_SEALED   0.03f
#define HEATWAVE_SECONDS   70.0f    /* an HVAC fault */
#define HEATWAVE_BONUS     8.0f     /* sealed rooms get 8 C hotter, open vents work at half speed */

/* fire */
#define FIRE_GROW          0.020f   /* base growth per second; it speeds up as the fire gets bigger */
#define FIRE_SEALED_AIR    0.55f    /* a sealed vent with an unlocked door still lets some air in */
#define FIRE_SMOTHER       0.012f   /* sealed vent AND locked door: the fire starves at this rate */
#define FIRE_FLASHOVER     0.85f    /* past this the whole room ignites at once */
#define FIRE_BURNOUT_SECONDS 10.0f  /* fully ablaze this long and the room is lost for good */
#define FIRE_SPREAD_AT     0.60f    /* a fire this big can jump to a neighbouring room... */
#define FIRE_SPREAD_CHANCE 0.05f    /* ...with this chance per second if both doors are open */
#define FIRE_HEAT          2.0f     /* degrees C per second at full fire */
#define FIRE_FUEL_USE      0.004f   /* fuel burnt per second at full fire */
#define FIRE_FIRST_MIN     70       /* seconds until the first random fire: 70-129 */
#define FIRE_GAP_MIN       110      /* seconds between random fires: 110-199 */
#define SMOKE_PER_FIRE     0.22f
#define SMOKE_ALARM        0.10f
#define HEAT_ALARM         45.0f    /* the heat detector */
#define INTEGRITY_LOSS     0.35f    /* percent per second for each fully burning room */
#define SMOKE_DEATH_AT     0.85f    /* smoke this thick in the Office... */
#define SMOKE_DEATH_SECONDS 10.0f   /* ...for this long ends the night */

/* sprinklers: heat-activated heads on a shared tank */
#define HEAD_FUSE_TEMP     57.0f    /* a head's heat link melts at this temperature */
#define PUMP_CAPACITY      3.0f     /* heads that can spray at full strength, pump running */
#define GRAVITY_CAPACITY   1.5f     /* ...and with no power, on the tank's own pressure */
#define HEAD_WATER_USE     2.0f     /* tank units per second for a head at full strength */
#define FIRE_SPRINKLER     0.035f   /* fire level put out per second at full strength */
#define GREASE_FLARE       0.030f   /* fire level ADDED per second when water hits a grease fire */
#define WET_RATE           0.06f
#define WET_DRY            0.012f
#define TANK_REFILL        0.5f
#define LEAK_DRAIN         1.2f
#define SHORT_CHANCE       0.10f    /* per second, once a soaked room has live electrics */

#define LOCKDOWN_SECONDS   20.0f

/* random events */
#define EVENT_FIRST_MIN    50       /* seconds until the first event: 50-79 */
#define EVENT_GAP_MIN      45       /* seconds between events: 45-79 */
#define NETFAIL_SECONDS    40.0f
#define FALSEALARM_SECONDS 25.0f
#define DOORFAULT_SECONDS  20.0f

/*
 * Room layout (index order matters):
 *   0 Office  1 Hallway  2 Lobby  3 Kitchen  4 Server Room  5 Storage
 * NEXT_HOP[room] = the room an intruder steps into to get closer to the Office.
 */
static const int NEXT_HOP[MAX_ROOMS] = { -1, 0, 1, 1, 2, 3, -1, -1 };

/* which rooms touch each other (fire and smoke travel along these) */
#define LINK_COUNT 5
static const int LINKS[LINK_COUNT][2] = { { 0, 1 }, { 1, 2 }, { 1, 3 }, { 2, 4 }, { 3, 5 } };

#define SERVER_ROOM 4

/* what a sealed room settles at: the Server Room is full of hot equipment, the Storage room has none */
static const float SEALED_GOAL[MAX_ROOMS] = { 27.0f, 25.0f, 26.0f, 31.0f, 36.0f, 24.0f, 26.0f, 26.0f };

static const char *SCENE_NAMES[MAX_SCENES] = { "LOCKDOWN", "ECO", "FIRE RESP", "RESET" };

static const char *RULE_NAMES[MAX_RULES] = {
    "AUTO-VENT", "AUTO-FIRE", "AUTO-ECO", "AUTO-LIGHTS", "AUTO-DOOR", "AUTO-LEAK", "AUTO-SCHEDULE"
};
static const char *RULE_INFO[MAX_RULES] = {
    "Opens a room's vent when it passes 26 C",
    "Smoke: seal. Heat: sprinklers. Electrics: cut power",
    "Switches to ECO when power drops below 25%",
    "Lights on when motion is seen in a dark room",
    "Locks the Office door while a villain is close",
    "Shuts the main water valve when a leak is sensed",
    "Night setback to 18 C until 5 AM, then comfort 21 C"
};
/* the cloud rules need the internet; the fire rule runs locally and always works */
static const bool RULE_CLOUD[MAX_RULES] = { true, false, true, true, true, true, true };

static const char *EVENT_NAMES[EVENT_COUNT] = {
    "POWER SURGE", "BURST PIPE", "HVAC FAULT", "INTERNET DOWN", "FALSE ALARM", "DOOR FAULT"
};

const char *scene_name(SceneId id)
{
    return ((int)id >= 0 && (int)id < MAX_SCENES) ? SCENE_NAMES[id] : "?";
}

const char *rule_name(RuleId id)
{
    return ((int)id >= 0 && (int)id < MAX_RULES) ? RULE_NAMES[id] : "?";
}

const char *rule_info(RuleId id)
{
    return ((int)id >= 0 && (int)id < MAX_RULES) ? RULE_INFO[id] : "";
}

bool rule_is_cloud(RuleId id)
{
    return ((int)id >= 0 && (int)id < MAX_RULES) ? RULE_CLOUD[id] : false;
}

const char *event_name(EventId id)
{
    return ((int)id >= 0 && (int)id < EVENT_COUNT) ? EVENT_NAMES[id] : "?";
}

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* true with probability (perSecond * dt) */
static bool chance(float perSecond, float dt)
{
    return ((float)rand() / (float)RAND_MAX) < perSecond * dt;
}

/* ------------------------------------------------------------------ */
/*  Event log and banners                                              */
/* ------------------------------------------------------------------ */

static void write_log(Building *b, bool banner, const char *fmt, va_list ap)
{
    char text[LOG_TEXT_LEN];
    vsnprintf(text, sizeof text, fmt, ap);

    EventLog *l = &b->log;
    LogEntry *e = &l->entries[l->count % MAX_LOG];
    e->time = b->nightTime;
    memcpy(e->text, text, sizeof text);
    l->count++;

    if (banner) {
        snprintf(b->notice, NOTICE_LEN, "%s", text);
        b->noticeTimer = 5.0f;
    }
}

void log_event(Building *b, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    write_log(b, false, fmt, ap);
    va_end(ap);
}

void log_alert(Building *b, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    write_log(b, true, fmt, ap);
    va_end(ap);
}

const LogEntry *log_recent(const Building *b, int back)
{
    int stored = (b->log.count < MAX_LOG) ? b->log.count : MAX_LOG;
    if (back < 0 || back >= stored) return NULL;
    return &b->log.entries[(b->log.count - 1 - back) % MAX_LOG];
}

void clock_text(float nightTime, char *out, size_t size)
{
    int hour   = (int)(nightTime / SECONDS_PER_HOUR);
    int minute = (int)((nightTime - hour * SECONDS_PER_HOUR) / SECONDS_PER_HOUR * 60.0f);
    snprintf(out, size, "%d:%02d AM", hour == 0 ? 12 : hour, minute);
}

/* ------------------------------------------------------------------ */
/*  Setting the building up                                            */
/* ------------------------------------------------------------------ */

static void add_room(Building *b, const char *name, float temp, FireKind kind, float fuel)
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

    r->fireKind = kind;
    r->fuel     = fuel;
    r->alert    = ALERT_NONE;
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

    /*                 name           temp  what burns here   fuel */
    add_room(b, "Office",      22.0f, FIRE_ORDINARY,   0.8f);
    add_room(b, "Hallway",     20.5f, FIRE_ORDINARY,   0.6f);
    add_room(b, "Lobby",       21.0f, FIRE_ORDINARY,   0.8f);
    add_room(b, "Kitchen",     23.0f, FIRE_GREASE,     0.9f);
    add_room(b, "Server Room", 24.0f, FIRE_ELECTRICAL, 0.7f);
    add_room(b, "Storage",     19.5f, FIRE_ORDINARY,   1.0f);

    for (int i = 0; i < b->roomCount; i++) b->rooms[i].wasLocked = false;

    b->selectedRoom = GOAL_ROOM;
    b->power      = 100.0f;
    b->killer     = -1;
    b->integrity  = 100.0f;
    b->water      = 100.0f;
    b->mainOpen   = true;
    b->netSpeed   = 70.0f;
    b->fireTimer  = (float)(FIRE_FIRST_MIN + rand() % 60);
    b->eventTimer = (float)(EVENT_FIRST_MIN + rand() % 30);

    /* the safety rules start on; the ones that play the game for you start off */
    b->rules[RULE_VENT].enabled = true;
    b->rules[RULE_FIRE].enabled = true;
    b->rules[RULE_ECO].enabled  = true;
    b->rules[RULE_LEAK].enabled = true;
    b->rules[RULE_SCHEDULE].enabled = true;

    /*                          type               name       room hour  secs aggr */
    add_intruder(b, INTRUDER_DRIFTER, "Drifter", 4, 0, 15.0f, 6);
    add_intruder(b, INTRUDER_SHADE,   "Shade",   5, 1, 12.0f, 6);
    add_intruder(b, INTRUDER_GLITCH,  "Glitch",  3, 2, 18.0f, 6);
    add_intruder(b, INTRUDER_CRAWLER, "Crawler", 4, 3, 20.0f, 8);
    add_intruder(b, INTRUDER_LEECH,   "Leech",   5, 4, 22.0f, 6);

    log_event(b, "Night shift started");
}

void room_evaluate_alert(Room *r)
{
    if (r->sensors.smokeDetected || r->fire > 0.0f || r->burnt) {
        r->alert = ALERT_CRITICAL;
    } else if (r->breakerTripped || r->leak || r->sensors.temperature > 28.0f
               || r->sensors.temperature < 15.0f) {
        r->alert = ALERT_WARNING;
    } else {
        r->alert = ALERT_NONE;
    }
}

/* a room's controls work only while its breaker is on and the room still exists */
static bool controllable(const Room *r)
{
    return !r->breakerTripped && !r->burnt;
}

/* ------------------------------------------------------------------ */
/*  Weather and energy prices                                          */
/* ------------------------------------------------------------------ */

/* an Irish autumn night: about 9 C at midnight, falling to about 4 C by 6 AM */
float building_outside_temp(const Building *b)
{
    return 9.0f - 0.9f * (b->nightTime / SECONDS_PER_HOUR);
}

/* cheap night rate until 5 AM, then the morning peak */
float building_tariff(const Building *b)
{
    return (building_hour(b) < 5) ? 0.15f : 0.30f;
}

const char *building_tariff_name(const Building *b)
{
    return (building_hour(b) < 5) ? "NIGHT RATE" : "MORNING PEAK";
}

/* ------------------------------------------------------------------ */
/*  Temperature                                                        */
/* ------------------------------------------------------------------ */

static float temp_step(float temp, float target, float sealedGoal, bool ventOpen, bool heatwave, float dt)
{
    float goal = ventOpen ? target : sealedGoal + (heatwave ? HEATWAVE_BONUS : 0.0f);
    float rate = ventOpen ? TEMP_RATE_OPEN : TEMP_RATE_SEALED;
    if (heatwave) rate *= ventOpen ? 0.5f : 2.0f;   /* the HVAC struggles, sealed rooms bake */
    float step = rate * dt;
    float diff = goal - temp;

    if (diff > step)  return temp + step;
    if (diff < -step) return temp - step;
    return goal;
}

/* ------------------------------------------------------------------ */
/*  Intruders                                                          */
/* ------------------------------------------------------------------ */

static bool can_enter(const Building *b, int room)
{
    return room >= 0 && !b->rooms[room].actuators.doorLocked;
}

static void trigger_motion(Building *b, int room)
{
    b->rooms[room].sensors.secondsSinceMotion = 0.0f;
    if (!b->rooms[room].breakerTripped) {
        log_event(b, "Motion detected in %s", b->rooms[room].name);
    }
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
        /* blocked at the Office door: it bangs on it, gives up and wanders back */
        if (target == GOAL_ROOM) {
            b->bangSeq++;
            in->room = in->home;
            trigger_motion(b, in->home);
        }
        return;
    }

    in->room = target;
    b->moveSeq++;
    b->lastMoveRoom = target;
    trigger_motion(b, target);

    if (target == GOAL_ROOM) {
        b->killer = idx;
        b->cause  = CAUSE_INTRUDER;
        b->state  = GAME_OVER;
        log_event(b, "%s got into the Office", in->name);
    }
}

/* ------------------------------------------------------------------ */
/*  Sprinkler supply                                                   */
/* ------------------------------------------------------------------ */

int building_heads_running(const Building *b)
{
    int n = 0;
    if (b->water <= 0.0f) return 0;
    for (int i = 0; i < b->roomCount; i++) {
        if (b->rooms[i].actuators.sprinklers && !b->rooms[i].burnt) n++;
    }
    return n;
}

/* every head shares the pump: the more that spray, the weaker each one is */
float building_head_flow(const Building *b)
{
    int heads = building_heads_running(b);
    if (heads == 0) return 0.0f;
    float capacity = b->powerOut ? GRAVITY_CAPACITY : PUMP_CAPACITY;
    float flow = capacity / (float)heads;
    return flow > 1.0f ? 1.0f : flow;
}

/* ------------------------------------------------------------------ */
/*  Power                                                              */
/* ------------------------------------------------------------------ */

float room_load(const Building *b, int room)
{
    if (room < 0 || room >= b->roomCount) return 0.0f;
    const Room *r = &b->rooms[room];
    if (r->burnt || r->breakerTripped) return 0.0f;

    float standby = 0.0f;
    if (r->actuators.lightsOn) standby += POWER_LIGHT_DRAIN;
    if (r->actuators.hvacOn) {
        /* the HVAC works harder the further the setting is from the weather outside */
        float lift = r->sensors.targetTemp - building_outside_temp(b);
        if (lift < 0.0f) lift = -lift;
        standby += POWER_VENT_DRAIN * (1.0f + lift / 10.0f);
    }
    float load = standby * (b->ecoMode ? ECO_FACTOR : 1.0f);
    if (r->actuators.doorLocked && !r->contained) load += POWER_DOOR_DRAIN;   /* fire doors close on their own */
    return load;
}

float building_power_usage(const Building *b)
{
    float use = POWER_BASE_DRAIN * (b->ecoMode ? ECO_FACTOR : 1.0f);

    for (int i = 0; i < b->roomCount; i++) use += room_load(b, i);
    if (!b->powerOut && building_heads_running(b) > 0) use += POWER_PUMP_DRAIN;

    for (int i = 0; i < b->intruderCount; i++) {
        if (b->intruders[i].active && b->intruders[i].type == INTRUDER_LEECH) {
            use += POWER_LEECH_DRAIN;
        }
    }
    return use;
}

float building_power_left(const Building *b)
{
    float use = building_power_usage(b);
    if (b->powerOut) return 0.0f;
    return (use > 0.0f) ? b->power / use : 99999.0f;
}

/* ------------------------------------------------------------------ */
/*  Electrics                                                          */
/* ------------------------------------------------------------------ */

static float short_limit(const Room *r)
{
    if (r->fireKind == FIRE_ELECTRICAL) return 0.30f;   /* the Server Room shorts out first */
    if (r->id == GOAL_ROOM)             return 0.50f;
    return 2.0f;                                        /* nothing sensitive: never */
}

static void trip_breaker(Building *b, int room, const char *why)
{
    Room *r = &b->rooms[room];
    if (r->breakerTripped) return;
    r->breakerTripped = true;
    r->actuators.lightsOn = false;
    b->zapSeq++;
    log_alert(b, "%s: %s breaker tripped", why, r->name);
}

bool building_set_breaker(Building *b, int room, bool on)
{
    if (room < 0 || room >= b->roomCount) return false;
    Room *r = &b->rooms[room];

    if (!on) {                                   /* cut the power to the room by hand */
        if (r->breakerTripped || r->burnt) return false;
        trip_breaker(b, room, "POWER CUT");
        return true;
    }
    if (!r->breakerTripped || r->burnt || b->powerOut) return false;
    if (r->wet > short_limit(r) * 0.8f) {
        log_event(b, "%s is too wet to switch the power back on", r->name);
        return false;
    }
    r->breakerTripped = false;
    r->actuators.lightsOn = true;
    log_event(b, "%s breaker reset", r->name);
    return true;
}

void building_set_main_valve(Building *b, bool open)
{
    if (b->mainOpen == open) return;
    b->mainOpen = open;
    log_event(b, "Main water valve %s", open ? "opened" : "closed");

    if (!open) {                                 /* closing it isolates every burst pipe */
        for (int i = 0; i < b->roomCount; i++) {
            Room *r = &b->rooms[i];
            if (r->leak) {
                r->leak = false;
                r->sensors.leakDetected = false;
                log_event(b, "Burst pipe isolated in %s", r->name);
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Fire, smoke and sprinklers                                         */
/* ------------------------------------------------------------------ */

bool building_start_fire(Building *b, int room)
{
    if (room < 0 || room >= b->roomCount) return false;

    Room *r = &b->rooms[room];
    if (r->fire > 0.0f || r->burnt) return false;

    r->fire = 0.08f;
    b->fireCount++;
    log_alert(b, "FIRE in %s", r->name);
    if (r->fireKind == FIRE_GREASE)     log_event(b, "Grease fire: water makes it worse, smother it");
    if (r->fireKind == FIRE_ELECTRICAL) log_event(b, "Electrical fire: cut the power to the room");
    return true;
}

static void flashover(Building *b, int i)
{
    Room *r = &b->rooms[i];
    r->fire = 1.0f;
    r->sensors.temperature = clampf(r->sensors.temperature + 15.0f, -20.0f, 95.0f);
    b->boomSeq++;
    log_alert(b, "FLASHOVER in %s!", r->name);

    /* the fireball rolls through any open doorway */
    for (int l = 0; l < LINK_COUNT; l++) {
        int other = (LINKS[l][0] == i) ? LINKS[l][1] : (LINKS[l][1] == i) ? LINKS[l][0] : -1;
        if (other < 0 || other >= b->roomCount) continue;
        Room *o = &b->rooms[other];
        if (!r->actuators.doorLocked && !o->actuators.doorLocked && !o->burnt && o->fire < 0.15f) {
            o->fire = 0.15f;
            b->fireCount++;
            log_alert(b, "Fire rolled into %s", o->name);
        }
    }
}

static void backdraft(Building *b, int i)
{
    Room *r = &b->rooms[i];
    r->sensors.smoke = 1.0f;
    r->fire = clampf(r->fire + 0.4f, 0.0f, 1.0f);
    b->boomSeq++;
    log_alert(b, "BACKDRAFT in %s! Never open a door on a sealed fire", r->name);

    for (int l = 0; l < LINK_COUNT; l++) {
        int other = (LINKS[l][0] == i) ? LINKS[l][1] : (LINKS[l][1] == i) ? LINKS[l][0] : -1;
        if (other < 0 || other >= b->roomCount) continue;
        Room *o = &b->rooms[other];
        if (!o->actuators.doorLocked && !o->burnt && o->fire < 0.15f) {
            o->fire = 0.15f;
            b->fireCount++;
        }
    }
    if (r->fire >= FIRE_FLASHOVER) flashover(b, i);
}

static void burn_out(Building *b, int i)
{
    Room *r = &b->rooms[i];
    r->burnt = true;
    r->fuel  = 0.0f;
    r->contained = false;
    r->actuators.lightsOn   = false;
    r->actuators.hvacOn     = false;
    r->actuators.doorLocked = false;     /* the door burns away: anything can walk through */
    r->actuators.sprinklers = false;
    b->integrity = clampf(b->integrity - 15.0f, 0.0f, 100.0f);
    b->boomSeq++;
    log_alert(b, "%s HAS BURNT OUT", r->name);
}

static bool any_leak(const Building *b)
{
    for (int i = 0; i < b->roomCount; i++) {
        if (b->rooms[i].leak) return true;
    }
    return false;
}

static void update_hazards(Building *b, float dt)
{
    /* a random fire now and then (never more than two at once, never in the Office) */
    b->fireTimer -= dt;
    if (b->fireTimer <= 0.0f) {
        b->fireTimer = (float)(FIRE_GAP_MIN + rand() % 90);
        int burning = 0;
        for (int i = 0; i < b->roomCount; i++) {
            if (b->rooms[i].fire > 0.0f) burning++;
        }
        if (burning < 2 && b->roomCount > 1) {
            building_start_fire(b, 1 + rand() % (b->roomCount - 1));
        }
    }

    /* the sprinkler tank: heads use water, the mains refill it, a burst pipe drains it */
    float flow = building_head_flow(b);
    int   heads = building_heads_running(b);
    b->water -= (float)heads * flow * HEAD_WATER_USE * dt;
    if (b->mainOpen) b->water += (any_leak(b) ? -LEAK_DRAIN : TANK_REFILL) * dt;
    b->water = clampf(b->water, 0.0f, 100.0f);

    /* backdraft: opening the door of a sealed, smoke-filled fire feeds it air */
    for (int i = 0; i < b->roomCount; i++) {
        Room *r = &b->rooms[i];
        if (r->wasLocked && !r->actuators.doorLocked && !r->burnt
            && r->fire >= 0.3f && r->sensors.smoke >= 0.5f && !r->actuators.hvacOn) {
            backdraft(b, i);
        }
        r->wasLocked = r->actuators.doorLocked;
    }

    float totalFire = 0.0f;
    for (int i = 0; i < b->roomCount; i++) {
        Room *r = &b->rooms[i];
        SensorData *s = &r->sensors;

        bool  spraying = r->actuators.sprinklers && !r->burnt && b->water > 0.0f;
        float eff = spraying ? flow : 0.0f;

        /* the heat link in the sprinkler head melts and opens it, whatever the controller says */
        if (!r->headFused && !r->burnt && s->temperature >= HEAD_FUSE_TEMP) {
            r->headFused = true;
            r->actuators.sprinklers = true;
            log_alert(b, "SPRINKLER HEAD fused in %s", r->name);
        }

        /* rooms get wet; soaked electrics short out */
        if (eff > 0.0f) r->wet += WET_RATE * eff * dt;
        else            r->wet -= WET_DRY * dt;
        if (r->leak)    r->wet += 0.04f * dt;
        r->wet = clampf(r->wet, 0.0f, 1.0f);
        if (!r->breakerTripped && !r->burnt && r->wet > short_limit(r) && chance(SHORT_CHANCE, dt)) {
            trip_breaker(b, i, "WATER SHORT");
        }

        if (r->fire > 0.0f) {
            float before = r->fire;

            if (r->burnt) {
                r->fire -= 0.05f * dt;                    /* nothing left to burn */
            } else {
                bool sealed    = !r->actuators.hvacOn;
                bool smothered = sealed && r->actuators.doorLocked;
                float air      = !sealed ? 1.0f : FIRE_SEALED_AIR;
                float rate;

                if (smothered) rate = -FIRE_SMOTHER;
                else           rate = FIRE_GROW * (1.0f + 3.0f * r->fire) * air * (0.6f + 0.7f * r->fuel);
                if (r->fuel <= 0.0f) rate = -0.03f;

                if (eff > 0.0f) {
                    if (r->fireKind == FIRE_GREASE) rate += GREASE_FLARE * eff;     /* water on hot oil */
                    else                            rate -= FIRE_SPRINKLER * eff;
                }
                if (r->fireKind == FIRE_ELECTRICAL && r->breakerTripped) rate -= 0.02f;   /* no power, no spark */

                r->fire += rate * dt;
                r->fuel  = clampf(r->fuel - r->fire * FIRE_FUEL_USE * dt, 0.0f, 1.0f);
            }
            r->fire = clampf(r->fire, 0.0f, 1.0f);

            if (!r->burnt && before < FIRE_FLASHOVER && r->fire >= FIRE_FLASHOVER) flashover(b, i);

            if (!r->burnt) {
                if (r->fire >= 0.99f) {
                    r->burnTimer += dt;
                    if (r->burnTimer >= FIRE_BURNOUT_SECONDS) burn_out(b, i);
                } else if (r->burnTimer > 0.0f) {
                    r->burnTimer -= dt;
                }
            }

            if (r->fire <= 0.0f) {
                r->fire = 0.0f;
                log_event(b, "Fire out in %s", r->name);
            } else {
                float smothering = (r->actuators.hvacOn || !r->actuators.doorLocked) ? 1.0f : 1.5f;   /* sealed rooms fill up */
                s->smoke       += r->fire * SMOKE_PER_FIRE * smothering * dt;
                s->temperature  = clampf(s->temperature + r->fire * FIRE_HEAT * dt, -20.0f, 95.0f);
                totalFire += r->fire;
            }
        }
    }

    /* smoke clears through open vents and under sprinklers... */
    for (int i = 0; i < b->roomCount; i++) {
        Room *r = &b->rooms[i];
        if (r->actuators.hvacOn) r->sensors.smoke -= 0.03f * dt;
        if (r->actuators.sprinklers && b->water > 0.0f) r->sensors.smoke -= 0.02f * flow * dt;
    }

    /* ...and drifts between linked rooms through open doors and open vents */
    for (int l = 0; l < LINK_COUNT; l++) {
        if (LINKS[l][0] >= b->roomCount || LINKS[l][1] >= b->roomCount) continue;
        Room *ra = &b->rooms[LINKS[l][0]];
        Room *rb = &b->rooms[LINKS[l][1]];

        bool doors = !ra->actuators.doorLocked && !rb->actuators.doorLocked;
        bool vents = ra->actuators.hvacOn && rb->actuators.hvacOn;
        float rate = (doors ? 0.08f : 0.0f) + (vents ? 0.04f : 0.0f);
        float flowS = (ra->sensors.smoke - rb->sensors.smoke) * rate * dt;
        ra->sensors.smoke -= flowS;
        rb->sensors.smoke += flowS;

        /* a big fire can jump to the next room if the doors between them are open */
        if (doors) {
            for (int dir = 0; dir < 2; dir++) {
                Room *from = dir ? rb : ra;
                Room *to   = dir ? ra : rb;
                if (from->fire >= FIRE_SPREAD_AT && to->fire <= 0.0f && !to->burnt && to->fuel > 0.0f
                    && chance(FIRE_SPREAD_CHANCE, dt)) {
                    to->fire = 0.05f;
                    b->fireCount++;
                    log_alert(b, "Fire spread from %s to %s", from->name, to->name);
                }
            }
        }
    }

    /* smoke alarms (a false alarm rings with no smoke at all) */
    for (int i = 0; i < b->roomCount; i++) {
        Room *r = &b->rooms[i];
        r->sensors.smoke = clampf(r->sensors.smoke, 0.0f, 1.0f);

        bool alarm = (r->sensors.smoke >= SMOKE_ALARM || r->falseAlarm > 0.0f) && !r->burnt;
        if (alarm && !r->sensors.smokeDetected) log_event(b, "SMOKE ALARM in %s", r->name);
        r->sensors.smokeDetected = alarm;
    }

    /* the building takes damage while anything burns; a fire in the Office ends the night */
    b->integrity -= totalFire * INTEGRITY_LOSS * dt;
    if (b->integrity < 0.0f) b->integrity = 0.0f;

    /* smoke in the Office is deadly */
    if (b->rooms[GOAL_ROOM].sensors.smoke >= SMOKE_DEATH_AT) b->smokeDeath += dt;
    else if (b->smokeDeath > 0.0f)                           b->smokeDeath -= dt;

    if (b->rooms[GOAL_ROOM].fire >= 0.5f) {
        b->cause = CAUSE_FIRE;
        b->state = GAME_OVER;
        log_event(b, "The fire reached the Office");
    } else if (b->smokeDeath >= SMOKE_DEATH_SECONDS) {
        b->cause = CAUSE_SMOKE;
        b->state = GAME_OVER;
        log_event(b, "The Office filled with smoke");
    } else if (b->integrity <= 0.0f) {
        b->cause = CAUSE_FIRE;
        b->state = GAME_OVER;
        log_event(b, "The building was lost to fire");
    }
}

/* ------------------------------------------------------------------ */
/*  Random events                                                      */
/* ------------------------------------------------------------------ */

/* a random room that is still standing and passes the test; -1 if none */
static int pick_room(Building *b, int mode)
{
    for (int tries = 0; tries < 30; tries++) {
        int i = rand() % b->roomCount;
        const Room *r = &b->rooms[i];
        if (r->burnt) continue;
        if (mode == EVENT_SURGE     && r->breakerTripped)                 continue;
        if (mode == EVENT_LEAK      && r->leak)                           continue;
        if (mode == EVENT_FALSEALARM && (r->falseAlarm > 0.0f || r->fire > 0.0f)) continue;
        if (mode == EVENT_DOORFAULT && r->lockFault > 0.0f)               continue;
        return i;
    }
    return -1;
}

bool building_start_event(Building *b, EventId id)
{
    int room;

    switch (id) {
        case EVENT_SURGE:
            room = pick_room(b, EVENT_SURGE);
            if (room < 0) return false;
            trip_breaker(b, room, "POWER SURGE");
            return true;

        case EVENT_LEAK:
            if (!b->mainOpen) return false;          /* the pipes are empty */
            room = pick_room(b, EVENT_LEAK);
            if (room < 0) return false;
            b->rooms[room].leak = true;
            b->rooms[room].sensors.leakDetected = true;
            log_alert(b, "BURST PIPE in %s: close the main valve", b->rooms[room].name);
            return true;

        case EVENT_HEATWAVE:
            if (b->heatwave > 0.0f) return false;
            b->heatwave = HEATWAVE_SECONDS;
            log_alert(b, "HVAC FAULT: the plant is struggling to keep up");
            return true;

        case EVENT_NETFAIL:
            if (b->netOutage > 0.0f) return false;
            b->netOutage = NETFAIL_SECONDS;
            log_alert(b, "INTERNET DOWN: cloud automations are offline");
            return true;

        case EVENT_FALSEALARM:
            room = pick_room(b, EVENT_FALSEALARM);
            if (room < 0) return false;
            b->rooms[room].falseAlarm = FALSEALARM_SECONDS;
            log_alert(b, "Smoke alarm in %s: real or false? check the camera", b->rooms[room].name);
            return true;

        case EVENT_DOORFAULT:
            room = pick_room(b, EVENT_DOORFAULT);
            if (room < 0) return false;
            b->rooms[room].lockFault = DOORFAULT_SECONDS;
            log_alert(b, "DOOR CONTROLLER OFFLINE in %s", b->rooms[room].name);
            return true;

        default:
            return false;
    }
}

static void update_events(Building *b, float dt)
{
    if (b->heatwave > 0.0f) {
        b->heatwave -= dt;
        if (b->heatwave <= 0.0f) { b->heatwave = 0.0f; log_event(b, "The HVAC fault is cleared"); }
    }
    if (b->netOutage > 0.0f) {
        b->netOutage -= dt;
        if (b->netOutage <= 0.0f) { b->netOutage = 0.0f; log_event(b, "The internet is back"); }
    }
    for (int i = 0; i < b->roomCount; i++) {
        Room *r = &b->rooms[i];
        if (r->lockFault > 0.0f) {
            r->lockFault -= dt;
            if (r->lockFault <= 0.0f) { r->lockFault = 0.0f; log_event(b, "%s door controller is back online", r->name); }
        }
        if (r->falseAlarm > 0.0f) {
            r->falseAlarm -= dt;
            if (r->falseAlarm <= 0.0f) { r->falseAlarm = 0.0f; log_event(b, "%s smoke alarm stopped: it was a false alarm", r->name); }
        }
        /* a tripped room is dead: the lights stay off */
        if (r->breakerTripped) r->actuators.lightsOn = false;
    }

    /* one event every minute or so; try a few kinds until one can start */
    b->eventTimer -= dt;
    if (b->eventTimer <= 0.0f) {
        b->eventTimer = (float)(EVENT_GAP_MIN + rand() % 35);
        for (int tries = 0; tries < 8; tries++) {
            if (building_start_event(b, (EventId)(rand() % EVENT_COUNT))) {
                b->eventCount++;
                break;
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/*  The network                                                        */
/* ------------------------------------------------------------------ */

static bool server_down(const Building *b)
{
    if (b->roomCount <= SERVER_ROOM) return false;
    const Room *s = &b->rooms[SERVER_ROOM];
    return s->breakerTripped || s->burnt || s->sensors.temperature > 70.0f;
}

static void update_network(Building *b, float dt)
{
    float target = 70.0f;
    for (int i = 0; i < b->intruderCount; i++) {
        if (!b->intruders[i].active) continue;
        if (b->intruders[i].type == INTRUDER_GLITCH) target *= 0.5f;
        if (b->intruders[i].type == INTRUDER_LEECH)  target -= 12.0f;
    }
    if (b->netOutage > 0.0f || b->powerOut || server_down(b)) target = 0.0f;

    float noise = ((float)(rand() % 2001) - 1000.0f) / 1000.0f;
    b->netSpeed += (target - b->netSpeed) * 2.0f * dt + noise * 6.0f * dt;
    b->netSpeed = clampf(b->netSpeed, 0.0f, 100.0f);
    if (target == 0.0f && b->netSpeed < 1.0f) b->netSpeed = 0.0f;
}

/* ------------------------------------------------------------------ */
/*  Scenes                                                             */
/* ------------------------------------------------------------------ */

static bool room_hazard(const Room *r)
{
    return r->fire > 0.0f || r->sensors.smoke >= SMOKE_ALARM || r->sensors.temperature >= HEAT_ALARM;
}

/* a door controller that is offline, or a room with no power, cannot be commanded */
static void set_door(Room *r, bool locked)
{
    if (controllable(r) && r->lockFault <= 0.0f) r->actuators.doorLocked = locked;
}

static void contain_room(Building *b, int i, bool spray)
{
    Room *r = &b->rooms[i];
    if (!controllable(r)) return;

    r->contained = true;
    r->actuators.hvacOn = false;
    if (i != GOAL_ROOM) set_door(r, true);          /* never lock yourself out of the Office */

    /* never spray a grease fire; never put water on live circuits: cut the power instead */
    r->actuators.sprinklers = spray && r->fireKind == FIRE_ORDINARY;
    if (r->fireKind == FIRE_ELECTRICAL) trip_breaker(b, i, "FIRE RESPONSE");
}

static void release_room(Building *b, int i)
{
    Room *r = &b->rooms[i];
    r->contained = false;
    r->actuators.sprinklers = false;
    r->headFused = false;
    if (!controllable(r)) return;
    r->actuators.hvacOn = true;
    if (i != GOAL_ROOM) set_door(r, false);
}

static void apply_eco(Building *b)
{
    b->ecoMode = true;
    for (int i = 0; i < b->roomCount; i++) {
        Room *r = &b->rooms[i];
        if (!controllable(r)) continue;
        r->actuators.lightsOn = (i == GOAL_ROOM);
        r->sensors.targetTemp = 19.0f;
    }
    b->scheduleMode = 0;
}

bool scene_activate(Building *b, SceneId id)
{
    if ((int)id < 0 || (int)id >= MAX_SCENES) return false;
    if (b->state != GAME_PLAYING) return false;
    if (b->powerOut) {
        log_event(b, "Scene %s refused: no power", scene_name(id));
        return false;
    }
    if (b->sceneCooldown[id] > 0.0f) return false;

    switch (id) {
        case SCENE_LOCKDOWN:
            for (int i = 0; i < b->roomCount; i++) {
                Room *r = &b->rooms[i];
                if (!controllable(r)) continue;
                set_door(r, true);
                r->actuators.hvacOn = false;
            }
            b->doorByRule     = false;
            b->lockdownTimer  = LOCKDOWN_SECONDS;
            b->sceneCooldown[id] = 60.0f;
            log_event(b, "Scene LOCKDOWN: all doors locked, all vents sealed");
            break;

        case SCENE_ECO:
            apply_eco(b);
            b->sceneCooldown[id] = 10.0f;
            log_event(b, "Scene ECO: lights off except the Office");
            break;

        case SCENE_FIRE: {
            int contained = 0;
            for (int i = 0; i < b->roomCount; i++) {
                if (room_hazard(&b->rooms[i]) && !b->rooms[i].burnt) {
                    contain_room(b, i, true);
                    contained++;
                } else if (controllable(&b->rooms[i])) {
                    b->rooms[i].actuators.lightsOn = true;   /* lights up the escape routes */
                }
            }
            b->sceneCooldown[id] = 8.0f;
            if (contained > 0) log_event(b, "Scene FIRE: %d room(s) contained", contained);
            else               log_event(b, "Scene FIRE: nothing is burning");
            break;
        }

        case SCENE_RESET:
            b->ecoMode       = false;
            b->lockdownTimer = 0.0f;
            b->doorByRule    = false;
            for (int i = 0; i < b->roomCount; i++) {
                Room *r = &b->rooms[i];
                if (!controllable(r)) continue;
                r->actuators.lightsOn = true;
                r->sensors.targetTemp = 21.0f;
                b->scheduleMode = 0;
                if (!r->contained) {
                    set_door(r, false);
                    r->actuators.hvacOn = true;
                    if (!room_hazard(r)) { r->actuators.sprinklers = false; r->headFused = false; }
                }
            }
            building_set_main_valve(b, true);
            b->sceneCooldown[id] = 8.0f;
            log_event(b, "Scene RESET: back to normal");
            break;
    }
    return true;
}

/* ------------------------------------------------------------------ */
/*  Automation rules: the building acting on its own                   */
/* ------------------------------------------------------------------ */

static bool villain_close(const Building *b)
{
    for (int i = 0; i < b->intruderCount; i++) {
        const Intruder *in = &b->intruders[i];
        if (!in->active) continue;
        if (in->room == 1) return true;
        if (in->type == INTRUDER_CRAWLER && (in->room == 2 || in->room == 3)) return true;
    }
    return false;
}

static void rule_fire(Building *b)
{
    for (int i = 0; i < b->roomCount; i++) {
        Room *r = &b->rooms[i];
        if (r->burnt) continue;

        bool smoke    = r->sensors.smokeDetected;
        bool heat     = r->sensors.temperature >= HEAT_ALARM;
        bool detected = smoke || heat;
        bool spray    = r->sensors.temperature >= 40.0f;     /* sprinklers only once it is hot, not for smoke alone */

        if (detected && !r->contained) {
            contain_room(b, i, spray);
            b->rules[RULE_FIRE].fired++;
            if (r->fireKind == FIRE_ELECTRICAL)      log_event(b, "AUTO-FIRE: %s sealed, power cut", r->name);
            else if (r->actuators.sprinklers)        log_event(b, "AUTO-FIRE: %s sealed, sprinklers on", r->name);
            else                                     log_event(b, "AUTO-FIRE: %s sealed", r->name);
        } else if (detected && r->contained && spray && !r->actuators.sprinklers && controllable(r)
                   && r->fireKind == FIRE_ORDINARY) {
            r->actuators.sprinklers = true;
            log_event(b, "AUTO-FIRE: %s is hot, sprinklers on", r->name);
        } else if (r->contained && r->fire <= 0.0f) {
            /* the fire is out: close the valve, bring the power back, vent the smoke, then release */
            if (r->breakerTripped && r->fireKind == FIRE_ELECTRICAL && r->wet <= short_limit(r) * 0.8f) {
                if (building_set_breaker(b, i, true)) log_event(b, "AUTO-FIRE: power back on in %s", r->name);
            }
            if (controllable(r)) {
                if (r->actuators.sprinklers && r->sensors.temperature < 40.0f) {
                    r->actuators.sprinklers = false;
                    r->headFused = false;
                    log_event(b, "AUTO-FIRE: sprinkler valve closed in %s", r->name);
                }
                if (!r->actuators.hvacOn) {
                    r->actuators.hvacOn = true;
                    log_event(b, "AUTO-FIRE: venting the smoke from %s", r->name);
                }
                if (!detected) {
                    release_room(b, i);
                    log_event(b, "AUTO-FIRE: %s back to normal", r->name);
                }
            }
        }
    }
}

static void update_rules(Building *b)
{
    if (b->powerOut) return;

    /* the fire rule is local safety equipment: it works even with the internet down */
    if (b->rules[RULE_FIRE].enabled) rule_fire(b);

    /* everything else is cloud-based and goes quiet while the internet is out */
    if (b->netOutage > 0.0f) return;

    /* AUTO-VENT: the thermostat */
    if (b->rules[RULE_VENT].enabled) {
        for (int i = 0; i < b->roomCount; i++) {
            Room *r = &b->rooms[i];
            if (r->sensors.temperature > 26.0f && !r->actuators.hvacOn && !r->contained
                && controllable(r) && !room_hazard(r) && b->lockdownTimer <= 0.0f) {
                r->actuators.hvacOn = true;
                b->rules[RULE_VENT].fired++;
                log_event(b, "AUTO-VENT: %s vent opened (%.0f C)", r->name, r->sensors.temperature);
            }
        }
    }

    /* AUTO-LIGHTS: light up a dark room the moment motion is seen in it */
    if (b->rules[RULE_LIGHTS].enabled && !b->ecoMode) {
        for (int i = 0; i < b->roomCount; i++) {
            Room *r = &b->rooms[i];
            if (controllable(r) && r->sensors.motionDetected && r->sensors.secondsSinceMotion < 0.6f
                && !r->actuators.lightsOn) {
                r->actuators.lightsOn = true;
                b->rules[RULE_LIGHTS].fired++;
                log_event(b, "AUTO-LIGHTS: %s lights on", r->name);
            }
        }
    }

    /* AUTO-ECO: save power when it is running low */
    if (b->rules[RULE_ECO].enabled && b->power < 25.0f && !b->ecoMode) {
        apply_eco(b);
        b->rules[RULE_ECO].fired++;
        log_event(b, "AUTO-ECO: power is low, ECO on");
    }

    /* AUTO-DOOR: lock the Office door only while a villain is close, to save power */
    if (b->rules[RULE_DOOR].enabled && b->lockdownTimer <= 0.0f) {
        Room *office = &b->rooms[GOAL_ROOM];
        bool close = villain_close(b);
        if (close && !office->actuators.doorLocked && controllable(office) && office->lockFault <= 0.0f) {
            office->actuators.doorLocked = true;
            b->doorByRule = true;
            b->rules[RULE_DOOR].fired++;
            log_event(b, "AUTO-DOOR: Office door locked");
        } else if (!close && b->doorByRule && controllable(office) && office->lockFault <= 0.0f) {
            office->actuators.doorLocked = false;
            b->doorByRule = false;
            log_event(b, "AUTO-DOOR: Office door unlocked");
        }
    }

    /* AUTO-SCHEDULE: cheaper, quieter thermostats overnight, comfortable again before the morning peak */
    if (b->rules[RULE_SCHEDULE].enabled && !b->ecoMode) {
        int mode = (building_hour(b) < 5) ? 1 : 2;
        if (mode != b->scheduleMode) {
            b->scheduleMode = mode;
            float target = (mode == 1) ? 18.0f : 21.0f;
            for (int i = 0; i < b->roomCount; i++) {
                if (controllable(&b->rooms[i]) && i != SERVER_ROOM) b->rooms[i].sensors.targetTemp = target;
            }
            b->rules[RULE_SCHEDULE].fired++;
            log_event(b, mode == 1 ? "AUTO-SCHEDULE: night setback, thermostats to 18 C"
                                   : "AUTO-SCHEDULE: comfort mode, thermostats to 21 C");
        }
    }

    /* AUTO-LEAK: a smart water shut-off */
    if (b->rules[RULE_LEAK].enabled && b->mainOpen) {
        for (int i = 0; i < b->roomCount; i++) {
            if (b->rooms[i].leak) {
                building_set_main_valve(b, false);
                b->rules[RULE_LEAK].fired++;
                log_event(b, "AUTO-LEAK: main valve closed (leak in %s)", b->rooms[i].name);
                break;
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/*  The simulation step                                                */
/* ------------------------------------------------------------------ */

void building_update(Building *b, float dt)
{
    if (b->state != GAME_PLAYING) return;

    /* the night clock */
    b->nightTime += dt;
    if (b->nightTime >= NIGHT_HOURS * SECONDS_PER_HOUR) {
        b->nightTime = NIGHT_HOURS * SECONDS_PER_HOUR;
        b->state = GAME_WON;
        log_event(b, "6 AM: the night is over");
        return;
    }
    int hour = building_hour(b);

    /* power: drains faster the more is switched on; at zero everything fails */
    if (!b->powerOut) {
        float use = building_power_usage(b);
        b->power  -= use * dt;
        b->energy += use * dt * KWH_PER_POWER;
        b->cost   += use * dt * KWH_PER_POWER * building_tariff(b);
        if (b->power <= 0.0f) {
            b->power = 0.0f;
            b->powerOut = true;
            for (int i = 0; i < b->roomCount; i++) {
                Actuators *a = &b->rooms[i].actuators;
                a->lightsOn   = false;
                a->doorLocked = false;
                if (!b->rooms[i].headFused) a->sprinklers = false;   /* the valves are electric; fused heads stay open */
            }
            log_alert(b, "POWER OUT: lights and doors are dead");
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

    /* random events, then fire, smoke and sprinklers */
    update_events(b, dt);
    update_hazards(b, dt);
    if (b->state != GAME_PLAYING) return;

    /* rooms: motion sensors, temperature and alerts */
    bool hot = b->heatwave > 0.0f;
    for (int i = 0; i < b->roomCount; i++) {
        Room *r = &b->rooms[i];
        r->sensors.secondsSinceMotion += dt;
        r->sensors.motionDetected = !r->breakerTripped && (r->sensors.secondsSinceMotion < MOTION_SECONDS);

        r->sensors.temperature = temp_step(r->sensors.temperature, r->sensors.targetTemp, SEALED_GOAL[i],
                                           r->actuators.hvacOn, hot, dt);
        room_evaluate_alert(r);
    }

    update_network(b, dt);

    /* scene cooldowns and the end of a lockdown */
    for (int i = 0; i < MAX_SCENES; i++) {
        if (b->sceneCooldown[i] > 0.0f) b->sceneCooldown[i] -= dt;
    }
    if (b->lockdownTimer > 0.0f) {
        b->lockdownTimer -= dt;
        if (b->lockdownTimer <= 0.0f) {
            b->lockdownTimer = 0.0f;
            for (int i = 0; i < b->roomCount; i++) {
                Room *r = &b->rooms[i];
                if (!r->contained && controllable(r)) {
                    set_door(r, false);
                    r->actuators.hvacOn = true;
                }
            }
            log_event(b, "Lockdown ended: doors unlocked, vents open");
        }
    }

    if (b->noticeTimer > 0.0f) b->noticeTimer -= dt;

    /* the automation rules run twice a second */
    b->ruleTimer -= dt;
    if (b->ruleTimer <= 0.0f) {
        b->ruleTimer += 0.5f;
        update_rules(b);
    }
}

/* ------------------------------------------------------------------ */
/*  The smart advisor                                                  */
/* ------------------------------------------------------------------ */

const char *building_advice(const Building *b)
{
    static char buf[110];

    if (b->state != GAME_PLAYING) return "The night is over";
    if (b->powerOut) return "POWER OUT: nothing responds until morning";

    for (int i = 0; i < b->roomCount; i++) {
        const Room *r = &b->rooms[i];
        if (r->fire <= 0.0f) continue;
        if (r->contained && !r->actuators.hvacOn && r->actuators.doorLocked) {
            snprintf(buf, sizeof buf, "%s is sealed: keep the door SHUT until the fire is out", r->name);
        } else if (r->fireKind == FIRE_GREASE) {
            snprintf(buf, sizeof buf, "Grease fire in %s: water makes it worse, press C to smother it", r->name);
        } else if (r->fireKind == FIRE_ELECTRICAL) {
            snprintf(buf, sizeof buf, "Electrical fire in %s: cut its power (press C)", r->name);
        } else {
            snprintf(buf, sizeof buf, "FIRE in %s: press C (fire response)", r->name);
        }
        return buf;
    }
    for (int i = 0; i < b->roomCount; i++) {
        const Room *r = &b->rooms[i];
        if (!r->sensors.smokeDetected) continue;
        if (r->sensors.smoke < 0.03f && r->sensors.temperature < 35.0f) {
            snprintf(buf, sizeof buf, "%s alarm, but no smoke or heat: probably a false alarm", r->name);
        } else {
            snprintf(buf, sizeof buf, "Smoke in %s: seal the vent and keep the door shut", r->name);
        }
        return buf;
    }
    for (int i = 0; i < b->roomCount; i++) {
        if (b->rooms[i].leak) {
            snprintf(buf, sizeof buf, "Burst pipe in %s: close the main water valve", b->rooms[i].name);
            return buf;
        }
    }
    for (int i = 0; i < b->roomCount; i++) {
        if (b->rooms[i].breakerTripped) {
            snprintf(buf, sizeof buf, "%s breaker is off: reset it in the control panel", b->rooms[i].name);
            return buf;
        }
    }
    for (int i = 0; i < b->roomCount; i++) {
        const Room *r = &b->rooms[i];
        if (r->actuators.sprinklers && r->fire <= 0.0f && r->sensors.temperature < 40.0f) {
            snprintf(buf, sizeof buf, "Sprinklers still running in %s: shut the valve before it floods", r->name);
            return buf;
        }
    }
    if (b->netOutage > 0.0f) return "Internet is down: the cloud automations are paused";
    if (b->heatwave > 0.0f)  return "HVAC fault: keep the vents open";

    float left  = building_power_left(b);
    float night = NIGHT_HOURS * SECONDS_PER_HOUR - b->nightTime;
    if (left < night) {
        int s = (int)left;
        snprintf(buf, sizeof buf, "Power runs out in %d:%02d, before morning (try ECO)", s / 60, s % 60);
        return buf;
    }

    for (int i = 0; i < b->roomCount; i++) {
        if (b->rooms[i].sensors.temperature > 28.0f) {
            snprintf(buf, sizeof buf, "%s is hot (%.0f C): open its vent", b->rooms[i].name,
                     b->rooms[i].sensors.temperature);
            return buf;
        }
    }
    if (b->rooms[1].sensors.motionDetected) return "Motion in the Hallway: watch the Office door";

    return "All systems normal";
}

/* ------------------------------------------------------------------ */
/*  Control API                                                        */
/* ------------------------------------------------------------------ */

void room_set_lights(Room *r, bool on)
{
    if (controllable(r)) r->actuators.lightsOn = on;
}

void room_set_lock(Room *r, bool locked)
{
    set_door(r, locked);
}

void room_set_target_temp(Room *r, float temp)
{
    if (r->burnt) return;
    r->sensors.targetTemp = clampf(temp, 16.0f, 28.0f);
}

void room_set_hvac(Room *r, bool on)
{
    if (controllable(r)) r->actuators.hvacOn = on;
}

void room_set_sprinklers(Room *r, bool on)
{
    if (!controllable(r)) return;
    r->actuators.sprinklers = on;
    if (!on) r->headFused = false;       /* closing the valve stops the water; the head is replaced later */
}

int building_hour(const Building *b)
{
    int hour = (int)(b->nightTime / SECONDS_PER_HOUR);
    return hour > NIGHT_HOURS ? NIGHT_HOURS : hour;
}

void game_lose(Building *b)
{
    b->state = GAME_OVER;
    b->cause = CAUSE_TEST;
}
