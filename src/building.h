#ifndef BUILDING_H
#define BUILDING_H

#include <stdbool.h>
#include <stddef.h>

#define MAX_ROOMS         8
#define ROOM_NAME_LEN     32
#define MAX_INTRUDERS     5
#define INTRUDER_NAME_LEN 16
#define MAX_SCENES        4
#define MAX_RULES         7
#define MAX_LOG           256
#define LOG_TEXT_LEN      72
#define NOTICE_LEN        80

/* The Office is where you sit. An intruder getting in here ends the night. */
#define GOAL_ROOM         0

/* Night runs 12 AM to 6 AM. Lower SECONDS_PER_HOUR to test quickly. */
#define NIGHT_HOURS       6
#define SECONDS_PER_HOUR  90.0f

typedef struct {
    float temperature;
    float targetTemp;       /* the thermostat setpoint */
    bool  motionDetected;
    float secondsSinceMotion;
    bool  doorOpen;
    bool  smokeDetected;    /* the smoke alarm: set when smoke passes a threshold */
    float smoke;            /* 0.0 clear air ... 1.0 thick smoke */
    bool  leakDetected;     /* the water sensor on the floor */
    float lightLevel;
} SensorData;

typedef struct {
    bool lightsOn;
    bool doorLocked;
    bool hvacOn;            /* true = vent open */
    bool sprinklers;        /* true = the room's sprinkler valve is open */
} Actuators;

typedef enum {
    ALERT_NONE,
    ALERT_WARNING,
    ALERT_CRITICAL
} AlertLevel;

typedef enum {
    GAME_PLAYING,
    GAME_OVER,
    GAME_WON
} GameState;

typedef enum {
    CAUSE_NONE,
    CAUSE_INTRUDER,
    CAUSE_FIRE,
    CAUSE_SMOKE,
    CAUSE_TEST
} GameCause;

typedef enum {
    FIRE_ORDINARY,      /* wood, paper, plastic: water works */
    FIRE_GREASE,        /* the Kitchen: water makes it flare up, smother it instead */
    FIRE_ELECTRICAL     /* the Server Room: water shorts the circuit, cutting the power puts it out */
} FireKind;

typedef enum {
    INTRUDER_DRIFTER,   /* walks toward the Office, one room at a time */
    INTRUDER_SHADE,     /* freezes while a camera is on it */
    INTRUDER_CRAWLER,   /* uses the vents: can skip a room and its locked door */
    INTRUDER_GLITCH,    /* kills the lights in whatever room it is in */
    INTRUDER_LEECH      /* drains extra power while it is loose */
} IntruderType;

/* One-click presets (see scene_activate) */
typedef enum {
    SCENE_LOCKDOWN,     /* every door locked, every vent sealed, for 20 seconds */
    SCENE_ECO,          /* lights off except the Office, half the standby power */
    SCENE_FIRE,         /* contain every burning room: sprinklers, sealed vent, locked door */
    SCENE_RESET         /* back to normal */
} SceneId;

/* Automation rules the building runs by itself (see rule_name) */
typedef enum {
    RULE_VENT,          /* hot room: open its vent */
    RULE_FIRE,          /* smoke alarm: seal the room; heat as well: sprinklers */
    RULE_ECO,           /* power low: switch to the ECO scene */
    RULE_LIGHTS,        /* motion in a dark room: lights on */
    RULE_DOOR,          /* villain close to the Office: lock the Office door */
    RULE_LEAK,          /* water sensor wet: shut the main valve */
    RULE_SCHEDULE       /* night setback: thermostats to 18 C until 5 AM, then 21 C */
} RuleId;

/* Things that go wrong during the night (see building_start_event) */
typedef enum {
    EVENT_SURGE,        /* a breaker trips: the room goes dead until you reset it */
    EVENT_LEAK,         /* a pipe bursts: floods a room and drains the water tank */
    EVENT_HEATWAVE,     /* an HVAC fault: the plant struggles for a while */
    EVENT_NETFAIL,      /* the internet drops: cloud automations stop */
    EVENT_FALSEALARM,   /* a smoke detector cries wolf */
    EVENT_DOORFAULT,    /* a door controller stops answering */
    EVENT_COUNT
} EventId;

typedef struct {
    IntruderType type;
    char  name[INTRUDER_NAME_LEN];
    int   room;         /* index into Building.rooms */
    int   home;         /* where it starts, and where it retreats to when blocked */
    int   startHour;    /* wakes up at this hour of the night */
    bool  active;
    float timer;        /* seconds until its next move attempt */
    float interval;     /* seconds between move attempts */
    int   aggression;   /* 0-20: chance out of 20 that an attempt succeeds */
} Intruder;

typedef struct {
    int        id;
    char       name[ROOM_NAME_LEN];
    SensorData sensors;
    Actuators  actuators;
    AlertLevel alert;

    /* fire */
    float      fire;        /* 0.0 no fire ... 1.0 fully ablaze */
    FireKind   fireKind;
    float      fuel;        /* 1.0 plenty to burn ... 0.0 nothing left */
    float      burnTimer;   /* seconds spent fully ablaze */
    bool       burnt;       /* burnt out for good: no camera, no lights, no door */
    bool       contained;   /* the fire response has sealed this room */

    /* water and electrics */
    float      wet;         /* 0.0 dry ... 1.0 flooded */
    bool       headFused;   /* the sprinkler's heat link has melted: it stays open until the valve is shut */
    bool       breakerTripped;
    bool       leak;        /* a burst pipe is flooding this room */

    /* faults */
    float      falseAlarm;  /* seconds of false smoke alarm left */
    float      lockFault;   /* seconds the door controller is offline */
    bool       wasLocked;   /* last frame's door state (to spot a backdraft) */
} Room;

typedef struct {
    bool enabled;
    int  fired;             /* how many times it has acted this night */
} Rule;

typedef struct {
    float time;             /* night seconds when it happened */
    char  text[LOG_TEXT_LEN];
} LogEntry;

typedef struct {
    LogEntry entries[MAX_LOG];   /* the newest MAX_LOG events */
    int      count;              /* events logged in total */
} EventLog;

typedef struct {
    Room      rooms[MAX_ROOMS];
    int       roomCount;
    int       selectedRoom;
    GameState state;
    GameCause cause;
    float     nightTime;

    Intruder  intruders[MAX_INTRUDERS];
    int       intruderCount;
    float     power;        /* 100.0 down to 0.0 */
    bool      powerOut;
    int       killer;       /* index of the intruder that got in, or -1 */
    float     energy;       /* kWh used tonight */
    float     cost;         /* what it cost, in euro, at the tariff of the hour */

    float     integrity;    /* 100.0 down to 0.0: damage from fire */
    float     fireTimer;    /* seconds until the next random fire */
    int       fireCount;    /* fires this night */
    float     smokeDeath;   /* seconds the Office has been full of smoke */

    float     water;        /* the sprinkler tank, 100.0 down to 0.0 */
    bool      mainOpen;     /* the main water valve */

    float     eventTimer;   /* seconds until the next random event */
    int       eventCount;
    float     heatwave;     /* seconds left */
    float     netOutage;    /* seconds left */
    float     netSpeed;     /* Mbps right now */

    bool      ecoMode;
    int       scheduleMode; /* 0 not applied yet, 1 night setback, 2 comfort */
    float     lockdownTimer;
    float     sceneCooldown[MAX_SCENES];
    bool      doorByRule;   /* the Office door was locked by RULE_DOOR */

    Rule      rules[MAX_RULES];
    float     ruleTimer;
    EventLog  log;

    /* things the screen reacts to: sounds and shakes */
    int       moveSeq;      /* counts villain moves */
    int       lastMoveRoom; /* where the latest move ended */
    int       bangSeq;      /* counts villains banging on the Office door */
    int       boomSeq;      /* counts backdrafts and flashovers */
    int       zapSeq;       /* counts breaker trips */
    char      notice[NOTICE_LEN];   /* the banner on screen */
    float     noticeTimer;
} Building;

/* ---- Simulation layer ---- */
void  building_init(Building *b);
void  building_update(Building *b, float dt);
void  room_evaluate_alert(Room *r);
int   building_hour(const Building *b);
float building_power_usage(const Building *b);   /* power lost per second */
float room_load(const Building *b, int room);    /* how much of that one room uses */
float building_outside_temp(const Building *b); /* the weather outside, degrees C */
float building_tariff(const Building *b);       /* euro per kWh right now */
const char *building_tariff_name(const Building *b);
void  game_lose(Building *b);

bool        building_start_fire(Building *b, int room);   /* returns false if it is already burning */
bool        building_start_event(Building *b, EventId id);/* returns false if it cannot start now */
bool        scene_activate(Building *b, SceneId id);      /* returns false if refused */
const char *scene_name(SceneId id);
const char *rule_name(RuleId id);
const char *rule_info(RuleId id);
bool        rule_is_cloud(RuleId id);                      /* cloud rules stop while the internet is down */
const char *event_name(EventId id);

const char *building_advice(const Building *b);           /* one-line tip from the smart advisor */
float       building_power_left(const Building *b);       /* seconds until the power runs out at the current usage */
int         building_heads_running(const Building *b);    /* sprinkler heads spraying right now */
float       building_head_flow(const Building *b);        /* 0.0 .. 1.0: how strongly each head sprays */

void            log_event(Building *b, const char *fmt, ...);
void            log_alert(Building *b, const char *fmt, ...);   /* logs it and shows a banner */
const LogEntry *log_recent(const Building *b, int back);  /* 0 = newest, NULL when there is none */
void            clock_text(float nightTime, char *out, size_t size);   /* "2:05 AM" */

/* ---- Control API (the only way the UI changes state) ---- */
void room_set_lights(Room *r, bool on);
void room_set_lock(Room *r, bool locked);
void room_set_target_temp(Room *r, float temp);
void room_set_hvac(Room *r, bool on);
void room_set_sprinklers(Room *r, bool on);
bool building_set_breaker(Building *b, int room, bool on);/* on=false cuts the room's power, on=true resets it (false if it cannot be reset yet) */
void building_set_main_valve(Building *b, bool open);

/* ---- UI layer ---- */
void ui_init(void);                 /* call once after InitWindow */
void ui_close(void);                /* call once before CloseWindow */
void ui_handle_input(Building *b);
void ui_draw(const Building *b);
void ui_click_sound(void);          /* the panel uses this */
bool ui_paused(void);               /* the game stands still while paused or while the help is open */

#endif /* BUILDING_H */
