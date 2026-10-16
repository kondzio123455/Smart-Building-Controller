#ifndef BUILDING_H
#define BUILDING_H

#include <stdbool.h>

#define MAX_ROOMS     8
#define ROOM_NAME_LEN 32

/* What the room "reads" - updated by the simulation each frame */
typedef struct {
    float temperature;      /* current temp in degrees C */
    float targetTemp;       /* setpoint the HVAC works toward */
    bool  motionDetected;
    float secondsSinceMotion; /* lets the UI fade the motion indicator */
    bool  doorOpen;
    bool  smokeDetected;
    float lightLevel;       /* 0.0 - 1.0 ambient light */
} SensorData;

/* What the user can switch from the camera view */
typedef struct {
    bool lightsOn;
    bool doorLocked;
    bool hvacOn;
} Actuators;

typedef enum {
    ALERT_NONE,
    ALERT_WARNING,   /* e.g. temp out of range */
    ALERT_CRITICAL   /* e.g. smoke */
} AlertLevel;

typedef struct {
    int        id;
    char       name[ROOM_NAME_LEN];
    SensorData sensors;
    Actuators  actuators;
    AlertLevel alert;
} Room;

typedef struct {
    Room rooms[MAX_ROOMS];
    int  roomCount;
    int  selectedRoom;   /* which "camera" the UI is showing */
} Building;

/* ---- Simulation layer (one person owns this) ---- */
void building_init(Building *b);
void building_update(Building *b, float dt);  /* drift temps, random motion, apply actuators */
void room_evaluate_alert(Room *r);

/* ---- Control API (the only way the UI changes state) ---- */
void room_set_lights(Room *r, bool on);
void room_set_lock(Room *r, bool locked);
void room_set_target_temp(Room *r, float temp);

/* ---- UI layer (the other person owns this) ---- */
void ui_handle_input(Building *b);
void ui_draw(const Building *b);

#endif /* BUILDING_H */