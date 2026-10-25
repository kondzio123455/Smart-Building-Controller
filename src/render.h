#ifndef RENDER_H
#define RENDER_H

#include <stdbool.h>
#include "raylib.h"

/*
 * The picture-making toolbox: a lighting shader (point lights, fog, shiny wet floors),
 * a filmic post-processing pass (glow, grain, colour fringes), and soft glowing sprites.
 * If a shader cannot be loaded on someone's computer, everything falls back to plain drawing.
 */

typedef struct {
    Vector3 pos[4];
    Vector3 col[4];      /* colour times brightness; (0,0,0) means the light is off */
    Vector3 ambient;     /* the light that is everywhere */
    float   fog;         /* thickness of the air: fog per metre */
    Vector3 fogColour;
    float   wet;         /* 0 dry ... 1 soaked: how shiny the floor is */
} SceneLight;

void render_init(int width, int height);   /* after InitWindow */
void render_close(void);
bool render_lit(void);                     /* the lighting shader is working and switched on */
bool render_post(void);                    /* the post-processing shader is working and switched on */
void render_toggle_quality(void);          /* the G key: fancy graphics on or off */

/* 1. start the picture and draw all the solid, lit things */
void render_scene_begin(Camera3D cam, const SceneLight *light);
/* 2. stop lighting, then draw glows and smoke as sprites */
void render_lit_end(void);
void sprites_glow_begin(void);             /* additive: fire, lamps, eyes, sparks */
void sprites_glow_end(void);
void sprites_smoke_begin(void);            /* normal blending: smoke, mist */
void sprites_smoke_end(void);
void sprite_glow(Camera3D cam, Vector3 pos, float size, Color tint);
void sprite_smoke(Camera3D cam, Vector3 pos, float size, Color tint);
/* glows can be asked for while the solid things are drawn, and appear in the glow pass */
void glow_queue(Vector3 pos, float size, Color tint);
void glow_flush(Camera3D cam);
/* 3. finish the picture and put it on the screen */
void render_scene_end(float time, float strength);

#endif
