#include <stddef.h>
#include <math.h>
#include "raylib.h"
#include "rlgl.h"
#include "render.h"

/* ------------------------------------------------------------------ */
/*  The shaders (GLSL 3.30, which raylib uses on Windows, Linux and Mac) */
/* ------------------------------------------------------------------ */

static const char *LIT_VS =
    "#version 330\n"
    "in vec3 vertexPosition;\n"
    "in vec4 vertexColor;\n"
    "uniform mat4 mvp;\n"
    "out vec3 fragPos;\n"
    "out vec4 fragColor;\n"
    "void main()\n"
    "{\n"
    "    fragPos = vertexPosition;\n"            /* raylib has already moved shapes into the room */
    "    fragColor = vertexColor;\n"
    "    gl_Position = mvp*vec4(vertexPosition, 1.0);\n"
    "}\n";

static const char *LIT_FS =
    "#version 330\n"
    "in vec3 fragPos;\n"
    "in vec4 fragColor;\n"
    "uniform vec4 colDiffuse;\n"
    "uniform vec3 viewPos;\n"
    "uniform vec3 ambientLight;\n"
    "uniform vec3 lightPos[4];\n"
    "uniform vec3 lightCol[4];\n"
    "uniform float fogDensity;\n"
    "uniform vec3 fogColour;\n"
    "uniform float wetness;\n"
    "out vec4 finalColor;\n"
    "void main()\n"
    "{\n"
    "    vec3 viewDir = normalize(viewPos - fragPos);\n"
    "    vec3 slope = cross(dFdx(fragPos), dFdy(fragPos));\n"          /* flat normal from the slope of the surface */
    "    vec3 n = (length(slope) > 0.00000001) ? normalize(slope) : vec3(0.0, 1.0, 0.0);\n"
    "    if (dot(n, viewDir) < 0.0) n = -n;\n"
    "    float a = fragColor.a;\n"
    "    bool glowing = (a > 0.98) && (a < 0.995);\n"                  /* alpha 252 marks a light-emitting surface */
    "    vec3 light = ambientLight;\n"
    "    for (int i = 0; i < 4; i++) {\n"
    "        vec3 toLight = lightPos[i] - fragPos;\n"
    "        float d = length(toLight);\n"
    "        vec3 L = toLight/max(d, 0.001);\n"
    "        float atten = 1.0/(1.0 + 0.10*d + 0.045*d*d);\n"
    "        float diff = max(dot(n, L), 0.0);\n"
    "        vec3 H = normalize(L + viewDir);\n"
    "        float shine = pow(max(dot(n, H), 0.0), 40.0)*wetness*smoothstep(0.85, 1.0, n.y);\n"
    "        light += lightCol[i]*(diff + 1.2*shine)*atten;\n"
    "    }\n"
    "    vec3 base = fragColor.rgb*colDiffuse.rgb;\n"
    "    vec3 lit = glowing ? base*1.6 : base*light;\n"
    "    float dist = length(viewPos - fragPos);\n"
    "    float fog = 1.0 - exp(-fogDensity*dist);\n"
    "    vec3 col = mix(lit, fogColour, clamp(fog, 0.0, 1.0));\n"
    "    finalColor = vec4(col, glowing ? 1.0 : a*colDiffuse.a);\n"
    "}\n";

static const char *POST_FS =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec2 resolution;\n"
    "uniform float time;\n"
    "uniform float strength;\n"
    "out vec4 finalColor;\n"
    "void main()\n"
    "{\n"
    "    vec2 c = fragTexCoord - vec2(0.5);\n"
    "    float r2 = dot(c, c);\n"
    "    vec2 uv = vec2(0.5) + c*(1.0 + 0.05*r2*strength);\n"                         /* the lens bulges a little */
    "    vec2 off = c*(0.0035 + 0.006*r2)*strength;\n"                                  /* colour fringes towards the edges */
    "    vec3 col;\n"
    "    col.r = texture(texture0, uv + off).r;\n"
    "    col.g = texture(texture0, uv).g;\n"
    "    col.b = texture(texture0, uv - off).b;\n"
    "    vec3 glow = vec3(0.0);\n"                                                      /* bloom: bright things bleed light */
    "    for (int i = 0; i < 16; i++) {\n"
    "        float ang = 6.2831853*float(i)/16.0;\n"
    "        float rad = (i % 2 == 0) ? 5.0 : 11.0;\n"
    "        vec2 d = vec2(cos(ang), sin(ang))*rad/resolution;\n"
    "        glow += max(texture(texture0, uv + d).rgb - vec3(0.55), vec3(0.0));\n"
    "    }\n"
    "    col += glow*(0.55/16.0)*strength*1.7;\n"
    "    col = vec3(1.0) - exp(-col*1.35);\n"                                          /* soft highlights: nothing burns out to white */
    "    col = (col - vec3(0.5))*1.08 + vec3(0.5);\n"                                   /* a little more contrast */
    "    col *= vec3(0.96, 1.0, 1.04);\n"                                               /* and a cold tint */
    "    col *= 1.0 - 0.55*strength*smoothstep(0.30, 0.85, length(c)*1.25);\n"          /* dark corners */
    "    float n = fract(sin(dot(uv*resolution + vec2(time*37.0, time*11.0), vec2(12.9898, 78.233)))*43758.5453);\n"
    "    col += (n - 0.5)*0.07*strength;\n"                                             /* film grain */
    "    col *= 1.0 - 0.10*strength*(0.5 + 0.5*sin(uv.y*resolution.y*3.14159));\n"      /* scanlines */
    "    float roll = fract(time*0.12);\n"
    "    col += vec3(0.04)*strength*smoothstep(0.02, 0.0, abs(uv.y - roll));\n"         /* a faint rolling bar */
    "    finalColor = vec4(col, 1.0);\n"
    "}\n";

/* ------------------------------------------------------------------ */
/*  State                                                              */
/* ------------------------------------------------------------------ */

static Shader litShader, postShader;
static bool   litOk = false, postOk = false;
static bool   fancy = true;                  /* the G key */
static bool   sceneUsesTexture = false;
static RenderTexture2D target;
static Texture2D softGlow, softSmoke;
static int    texW = 800, texH = 450;

static int locViewPos, locAmbient, locLightPos, locLightCol, locFog, locFogCol, locWet;
static int locRes, locTime, locStrength;

#define MAX_QUEUED_GLOWS 96
static struct { Vector3 pos; float size; Color tint; } queued[MAX_QUEUED_GLOWS];
static int queuedCount = 0;

void render_init(int width, int height)
{
    texW = width;
    texH = height;

    litShader = LoadShaderFromMemory(LIT_VS, LIT_FS);
    locLightPos = GetShaderLocation(litShader, "lightPos");
    locLightCol = GetShaderLocation(litShader, "lightCol");
    locViewPos  = GetShaderLocation(litShader, "viewPos");
    locAmbient  = GetShaderLocation(litShader, "ambientLight");
    locFog      = GetShaderLocation(litShader, "fogDensity");
    locFogCol   = GetShaderLocation(litShader, "fogColour");
    locWet      = GetShaderLocation(litShader, "wetness");
    litOk = (locLightPos >= 0 && locLightCol >= 0 && locViewPos >= 0 && locAmbient >= 0
             && locFog >= 0 && locFogCol >= 0 && locWet >= 0);

    postShader = LoadShaderFromMemory(NULL, POST_FS);
    locRes      = GetShaderLocation(postShader, "resolution");
    locTime     = GetShaderLocation(postShader, "time");
    locStrength = GetShaderLocation(postShader, "strength");
    postOk = (locRes >= 0 && locTime >= 0 && locStrength >= 0);
    if (postOk) target = LoadRenderTexture(width, height);

    /* two soft round sprites: one fades to black (for glow), one fades to see-through (for smoke) */
    Image g = GenImageGradientRadial(64, 64, 0.0f, WHITE, BLACK);
    softGlow = LoadTextureFromImage(g);
    UnloadImage(g);
    Image s = GenImageGradientRadial(64, 64, 0.0f, (Color){ 255, 255, 255, 255 }, (Color){ 255, 255, 255, 0 });
    softSmoke = LoadTextureFromImage(s);
    UnloadImage(s);
    SetTextureFilter(softGlow, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(softSmoke, TEXTURE_FILTER_BILINEAR);
}

void render_close(void)
{
    UnloadTexture(softGlow);
    UnloadTexture(softSmoke);
    if (postOk) UnloadRenderTexture(target);
    UnloadShader(litShader);
    UnloadShader(postShader);
}

bool render_lit(void)  { return litOk && fancy; }
bool render_post(void) { return postOk && fancy; }
void render_toggle_quality(void) { fancy = !fancy; }

/* ------------------------------------------------------------------ */
/*  Drawing a scene                                                    */
/* ------------------------------------------------------------------ */

void render_scene_begin(Camera3D cam, const SceneLight *l)
{
    sceneUsesTexture = render_post();
    if (sceneUsesTexture) {
        BeginTextureMode(target);
        ClearBackground((Color){ 4, 5, 8, 255 });
    }
    BeginMode3D(cam);

    if (render_lit()) {
        float pos[12], col[12];
        for (int i = 0; i < 4; i++) {
            pos[i * 3 + 0] = l->pos[i].x;  pos[i * 3 + 1] = l->pos[i].y;  pos[i * 3 + 2] = l->pos[i].z;
            col[i * 3 + 0] = l->col[i].x;  col[i * 3 + 1] = l->col[i].y;  col[i * 3 + 2] = l->col[i].z;
        }
        float view[3] = { cam.position.x, cam.position.y, cam.position.z };
        float amb[3]  = { l->ambient.x, l->ambient.y, l->ambient.z };
        float fogc[3] = { l->fogColour.x, l->fogColour.y, l->fogColour.z };

        SetShaderValueV(litShader, locLightPos, pos, SHADER_UNIFORM_VEC3, 4);
        SetShaderValueV(litShader, locLightCol, col, SHADER_UNIFORM_VEC3, 4);
        SetShaderValue(litShader, locViewPos, view, SHADER_UNIFORM_VEC3);
        SetShaderValue(litShader, locAmbient, amb, SHADER_UNIFORM_VEC3);
        SetShaderValue(litShader, locFog, &l->fog, SHADER_UNIFORM_FLOAT);
        SetShaderValue(litShader, locFogCol, fogc, SHADER_UNIFORM_VEC3);
        SetShaderValue(litShader, locWet, &l->wet, SHADER_UNIFORM_FLOAT);
        BeginShaderMode(litShader);
    }
    queuedCount = 0;
}

void render_lit_end(void)
{
    if (render_lit()) EndShaderMode();
}

void render_scene_end(float time, float strength)
{
    EndMode3D();
    if (!sceneUsesTexture) return;

    EndTextureMode();
    float res[2] = { (float)texW, (float)texH };
    SetShaderValue(postShader, locRes, res, SHADER_UNIFORM_VEC2);
    SetShaderValue(postShader, locTime, &time, SHADER_UNIFORM_FLOAT);
    SetShaderValue(postShader, locStrength, &strength, SHADER_UNIFORM_FLOAT);
    BeginShaderMode(postShader);
    DrawTextureRec(target.texture, (Rectangle){ 0, 0, (float)texW, -(float)texH }, (Vector2){ 0, 0 }, WHITE);
    EndShaderMode();
}

/* ------------------------------------------------------------------ */
/*  Sprites                                                            */
/* ------------------------------------------------------------------ */

void sprites_glow_begin(void)
{
    rlDisableDepthMask();
    BeginBlendMode(BLEND_ADDITIVE);
}

void sprites_glow_end(void)
{
    EndBlendMode();
    rlEnableDepthMask();
}

void sprites_smoke_begin(void)
{
    rlDisableDepthMask();
}

void sprites_smoke_end(void)
{
    rlEnableDepthMask();
}

/* size is the height and width in metres. A glow fades to black, so with additive blending it only adds light */
void sprite_glow(Camera3D cam, Vector3 pos, float size, Color tint)
{
    DrawBillboard(cam, softGlow, pos, size, tint);
}

/* smoke fades to see-through, so it works with normal blending */
void sprite_smoke(Camera3D cam, Vector3 pos, float size, Color tint)
{
    DrawBillboard(cam, softSmoke, pos, size, tint);
}

void glow_queue(Vector3 pos, float size, Color tint)
{
    if (queuedCount < MAX_QUEUED_GLOWS) {
        queued[queuedCount].pos  = pos;
        queued[queuedCount].size = size;
        queued[queuedCount].tint = tint;
        queuedCount++;
    }
}

void glow_flush(Camera3D cam)
{
    for (int i = 0; i < queuedCount; i++) {
        DrawBillboard(cam, softGlow, queued[i].pos, queued[i].size, queued[i].tint);
    }
    queuedCount = 0;
}
