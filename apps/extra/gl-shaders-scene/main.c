/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2019 Antonino Natale
 *
 *
 * This file is part of aplus.
 *
 * aplus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * aplus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with aplus.  If not, see <http://www.gnu.org/licenses/>.
 */


#define GL_VERSION_3_0      1
#define GL_GLEXT_PROTOTYPES 1


#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/osmesa.h>

#include <aplus/input.h>
#include <aplus/ui.h>


#define SCENE_DEFAULT_WIDTH  480
#define SCENE_DEFAULT_HEIGHT 360

#define SCENE_FPS_INTERVAL_MS 1000

/**
 * @brief How much of the window the shader actually runs over, before the result is blown back up.
 */
#define SCENE_DEFAULT_PIXEL_SCALE 4
#define SCENE_MIN_PIXEL_SCALE     1
#define SCENE_MAX_PIXEL_SCALE     16

/**
 * @brief The raymarching budget, which has to stay at or below MARCH_STEPS_MAX in the shader.
 */
#define SCENE_DEFAULT_MARCH_STEPS 96
#define SCENE_MIN_MARCH_STEPS     16
#define SCENE_MAX_MARCH_STEPS     128

#define SCENE_ATTRIB_POSITION 0



/**
 * @brief Nothing but a fullscreen quad, handing the fragment stage the point it is shading.
 */
static const char* vertex_shader = "#version 120                                                                                         \n"
                                   "                                                                                                     \n"
                                   "attribute vec2 position;                                                                             \n"
                                   "                                                                                                     \n"
                                   "varying vec2 v_uv;                                                                                   \n"
                                   "                                                                                                     \n"
                                   "void main() {                                                                                        \n"
                                   "    v_uv        = position;                                                                          \n"
                                   "    gl_Position = vec4(position, 0.0, 1.0);                                                          \n"
                                   "}                                                                                                    \n";

/**
 * @brief The whole scene, geometry included: every surface is a signed distance function.
 *
 * Written against GLSL 1.20, which is as much as the OSMesa build here offers.
 */
static const char* fragment_shader = "#version 120                                                                                         \n"
                                     "                                                                                                     \n"
                                     "uniform float u_time;                                                                                \n"
                                     "uniform float u_aspect;                                                                              \n"
                                     "uniform int   u_steps;                                                                               \n"
                                     "                                                                                                     \n"
                                     "varying vec2 v_uv;                                                                                   \n"
                                     "                                                                                                     \n"
                                     "const float PI = 3.14159265;                                                                         \n"
                                     "                                                                                                     \n"
                                     "const vec3 SUN_DIR   = vec3(0.5977, 0.6737, 0.4347);                                                 \n"
                                     "const vec3 SUN_COLOR = vec3(1.30, 1.05, 0.78);                                                       \n"
                                     "const vec3 SKY_LOW   = vec3(0.58, 0.68, 0.82);                                                       \n"
                                     "const vec3 SKY_HIGH  = vec3(0.10, 0.22, 0.46);                                                       \n"
                                     "                                                                                                     \n"
                                     "const float MAT_FLOOR  = 0.0;                                                                        \n"
                                     "const float MAT_CORE   = 1.0;                                                                        \n"
                                     "const float MAT_RING   = 2.0;                                                                        \n"
                                     "const float MAT_PILLAR = 3.0;                                                                        \n"
                                     "                                                                                                     \n"
                                     "const int MARCH_STEPS_MAX = 128;                                                                     \n"
                                     "const int SHADOW_STEPS    = 24;                                                                      \n"
                                     "const int REFLECT_STEPS   = 40;                                                                      \n"
                                     "const int AO_TAPS         = 5;                                                                       \n"
                                     "const int BLOBS           = 4;                                                                       \n"
                                     "const int FBM_OCTAVES     = 4;                                                                       \n"
                                     "                                                                                                     \n"
                                     "const float MARCH_FAR = 46.0;                                                                        \n"
                                     "const float MARCH_EPS = 0.0018;                                                                      \n"
                                     "                                                                                                     \n"
                                     "                                                                                                     \n"
                                     "mat2 rot(float a) {                                                                                  \n"
                                     "    float c = cos(a);                                                                                \n"
                                     "    float s = sin(a);                                                                                \n"
                                     "    return mat2(c, s, -s, c);                                                                        \n"
                                     "}                                                                                                    \n"
                                     "                                                                                                     \n"
                                     "float hash21(vec2 p) {                                                                               \n"
                                     "    p  = fract(p * vec2(127.1, 311.7));                                                              \n"
                                     "    p += dot(p, p + 34.56);                                                                          \n"
                                     "    return fract(p.x * p.y * 95.43);                                                                 \n"
                                     "}                                                                                                    \n"
                                     "                                                                                                     \n"
                                     "float noise21(vec2 p) {                                                                              \n"
                                     "    vec2 i = floor(p);                                                                               \n"
                                     "    vec2 f = fract(p);                                                                               \n"
                                     "    vec2 w = f * f * (3.0 - 2.0 * f);                                                                \n"
                                     "    float a = hash21(i);                                                                             \n"
                                     "    float b = hash21(i + vec2(1.0, 0.0));                                                            \n"
                                     "    float c = hash21(i + vec2(0.0, 1.0));                                                            \n"
                                     "    float d = hash21(i + vec2(1.0, 1.0));                                                            \n"
                                     "    return mix(mix(a, b, w.x), mix(c, d, w.x), w.y);                                                 \n"
                                     "}                                                                                                    \n"
                                     "                                                                                                     \n"
                                     "float fbm(vec2 p) {                                                                                  \n"
                                     "    float v = 0.0;                                                                                   \n"
                                     "    float a = 0.5;                                                                                   \n"
                                     "    for (int i = 0; i < FBM_OCTAVES; i++) {                                                          \n"
                                     "        v += a * noise21(p);                                                                         \n"
                                     "        p  = p * 2.07 + vec2(1.7, 9.2);                                                              \n"
                                     "        a *= 0.5;                                                                                    \n"
                                     "    }                                                                                                \n"
                                     "    return v;                                                                                        \n"
                                     "}                                                                                                    \n"
                                     "                                                                                                     \n"
                                     "float sdSphere(vec3 p, float r) {                                                                    \n"
                                     "    return length(p) - r;                                                                            \n"
                                     "}                                                                                                    \n"
                                     "                                                                                                     \n"
                                     "float sdRoundBox(vec3 p, vec3 b, float r) {                                                          \n"
                                     "    vec3 q = abs(p) - b;                                                                             \n"
                                     "    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0) - r;                              \n"
                                     "}                                                                                                    \n"
                                     "                                                                                                     \n"
                                     "float sdTorus(vec3 p, float major, float minor) {                                                    \n"
                                     "    vec2 q = vec2(length(p.xz) - major, p.y);                                                        \n"
                                     "    return length(q) - minor;                                                                        \n"
                                     "}                                                                                                    \n"
                                     "                                                                                                     \n"
                                     "float smin(float a, float b, float k) {                                                              \n"
                                     "    float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);                                              \n"
                                     "    return mix(b, a, h) - k * h * (1.0 - h);                                                         \n"
                                     "}                                                                                                    \n"
                                     "                                                                                                     \n"
                                     "                                                                                                     \n"
                                     "// Distance to the nearest surface, and which surface it was.                                        \n"
                                     "vec2 map(vec3 p) {                                                                                   \n"
                                     "                                                                                                     \n"
                                     "    vec2 hit = vec2(p.y + 1.0, MAT_FLOOR);                                                           \n"
                                     "                                                                                                     \n"
                                     "    vec3 q = p;                                                                                      \n"
                                     "    q.xz   = rot(u_time * 0.31) * q.xz;                                                              \n"
                                     "    q.xy   = rot(u_time * 0.17) * q.xy;                                                              \n"
                                     "                                                                                                     \n"
                                     "    float core = sdRoundBox(q, vec3(0.62), 0.14);                                                    \n"
                                     "                                                                                                     \n"
                                     "    for (int i = 0; i < BLOBS; i++) {                                                                \n"
                                     "        float f = float(i);                                                                          \n"
                                     "        float a = u_time * (0.63 + 0.11 * f) + f * (2.0 * PI / float(BLOBS));                        \n"
                                     "        vec3  c = vec3(cos(a) * 1.25, sin(a * 1.7) * 0.62, sin(a) * 1.25);                           \n"
                                     "        core    = smin(core, sdSphere(p - c, 0.36), 0.42);                                           \n"
                                     "    }                                                                                                \n"
                                     "                                                                                                     \n"
                                     "    if (core < hit.x) {                                                                              \n"
                                     "        hit = vec2(core, MAT_CORE);                                                                  \n"
                                     "    }                                                                                                \n"
                                     "                                                                                                     \n"
                                     "    vec3 t = p;                                                                                      \n"
                                     "    t.xz   = rot(u_time * -0.23) * t.xz;                                                             \n"
                                     "    t.yz   = rot(0.62 + sin(u_time * 0.27) * 0.22) * t.yz;                                           \n"
                                     "                                                                                                     \n"
                                     "    float ring = sdTorus(t, 2.35, 0.085);                                                            \n"
                                     "                                                                                                     \n"
                                     "    if (ring < hit.x) {                                                                              \n"
                                     "        hit = vec2(ring, MAT_RING);                                                                  \n"
                                     "    }                                                                                                \n"
                                     "                                                                                                     \n"
                                     "    // Five pillars repeated along x and mirrored across z: one box, fourteen times,                 \n"
                                     "    // and the clamp is what keeps the repetition from swallowing the middle.                        \n"
                                     "    vec3 r = p;                                                                                      \n"
                                     "    r.x   -= 6.0 * clamp(floor(r.x / 6.0 + 0.5), -2.0, 2.0);                                         \n"
                                     "    r.z    = abs(r.z) - 10.0;                                                                        \n"
                                     "                                                                                                     \n"
                                     "    float pillar = sdRoundBox(r - vec3(0.0, 0.7, 0.0), vec3(0.34, 1.7, 0.34), 0.06);                 \n"
                                     "                                                                                                     \n"
                                     "    if (pillar < hit.x) {                                                                            \n"
                                     "        hit = vec2(pillar, MAT_PILLAR);                                                              \n"
                                     "    }                                                                                                \n"
                                     "                                                                                                     \n"
                                     "    return hit;                                                                                      \n"
                                     "}                                                                                                    \n"
                                     "                                                                                                     \n"
                                     "                                                                                                     \n"
                                     "// Four taps of the field around p, in the tetrahedron pattern -- two fewer map()                    \n"
                                     "// calls than sampling each axis in both directions, for a normal just as usable.                    \n"
                                     "vec3 calcNormal(vec3 p) {                                                                            \n"
                                     "    vec2 e = vec2(1.0, -1.0) * 0.0015;                                                               \n"
                                     "    return normalize(e.xyy * map(p + e.xyy).x + e.yyx * map(p + e.yyx).x +                           \n"
                                     "                     e.yxy * map(p + e.yxy).x + e.xxx * map(p + e.xxx).x);                           \n"
                                     "}                                                                                                    \n"
                                     "                                                                                                     \n"
                                     "                                                                                                     \n"
                                     "// Walk the ray forward by whatever the field says is safe. Returns the distance                     \n"
                                     "// travelled and the material hit, or a negative material when nothing was.                          \n"
                                     "vec2 march(vec3 ro, vec3 rd, float far, int steps) {                                                 \n"
                                     "                                                                                                     \n"
                                     "    float t  = 0.02;                                                                                 \n"
                                     "    float id = -1.0;                                                                                 \n"
                                     "                                                                                                     \n"
                                     "    for (int i = 0; i < MARCH_STEPS_MAX; i++) {                                                      \n"
                                     "                                                                                                     \n"
                                     "        if (i >= steps) {                                                                            \n"
                                     "            break;                                                                                   \n"
                                     "        }                                                                                            \n"
                                     "                                                                                                     \n"
                                     "        vec2 h = map(ro + rd * t);                                                                   \n"
                                     "                                                                                                     \n"
                                     "        // Scaled with t, so a surface at the far end is not asked to resolve to the                 \n"
                                     "        // same absolute precision as one under the eye -- it cannot, at one sample.                 \n"
                                     "        if (h.x < MARCH_EPS * t) {                                                                   \n"
                                     "            id = h.y;                                                                                \n"
                                     "            break;                                                                                   \n"
                                     "        }                                                                                            \n"
                                     "                                                                                                     \n"
                                     "        t += h.x;                                                                                    \n"
                                     "                                                                                                     \n"
                                     "        if (t > far) {                                                                               \n"
                                     "            break;                                                                                   \n"
                                     "        }                                                                                            \n"
                                     "    }                                                                                                \n"
                                     "                                                                                                     \n"
                                     "    return vec2(t, id);                                                                              \n"
                                     "}                                                                                                    \n"
                                     "                                                                                                     \n"
                                     "                                                                                                     \n"
                                     "// How much of the sun reaches p. The penumbra comes free: how near the ray passed                   \n"
                                     "// to an occluder, divided by how far it had gone, is already the shape of one.                      \n"
                                     "float softShadow(vec3 ro, vec3 rd, float far) {                                                      \n"
                                     "                                                                                                     \n"
                                     "    float res = 1.0;                                                                                 \n"
                                     "    float t   = 0.05;                                                                                \n"
                                     "                                                                                                     \n"
                                     "    for (int i = 0; i < SHADOW_STEPS; i++) {                                                         \n"
                                     "                                                                                                     \n"
                                     "        float h = map(ro + rd * t).x;                                                                \n"
                                     "                                                                                                     \n"
                                     "        res = min(res, 10.0 * h / t);                                                                \n"
                                     "        t  += clamp(h, 0.04, 0.6);                                                                   \n"
                                     "                                                                                                     \n"
                                     "        if (res < 0.004 || t > far) {                                                                \n"
                                     "            break;                                                                                   \n"
                                     "        }                                                                                            \n"
                                     "    }                                                                                                \n"
                                     "                                                                                                     \n"
                                     "    return clamp(res, 0.0, 1.0);                                                                     \n"
                                     "}                                                                                                    \n"
                                     "                                                                                                     \n"
                                     "                                                                                                     \n"
                                     "// Ambient occlusion by disagreement: step along the normal and compare where the                    \n"
                                     "// surface should be with where the field says it is. The gap is the crowding.                       \n"
                                     "float calcAO(vec3 p, vec3 n) {                                                                       \n"
                                     "                                                                                                     \n"
                                     "    float occ = 0.0;                                                                                 \n"
                                     "    float sca = 1.0;                                                                                 \n"
                                     "                                                                                                     \n"
                                     "    for (int i = 0; i < AO_TAPS; i++) {                                                              \n"
                                     "        float h = 0.02 + 0.14 * float(i);                                                            \n"
                                     "        occ    += (h - map(p + n * h).x) * sca;                                                      \n"
                                     "        sca    *= 0.82;                                                                              \n"
                                     "    }                                                                                                \n"
                                     "                                                                                                     \n"
                                     "    return clamp(1.0 - 1.6 * occ, 0.0, 1.0);                                                         \n"
                                     "}                                                                                                    \n"
                                     "                                                                                                     \n"
                                     "                                                                                                     \n"
                                     "vec3 sky(vec3 rd) {                                                                                  \n"
                                     "                                                                                                     \n"
                                     "    vec3 col = mix(SKY_LOW, SKY_HIGH, pow(max(rd.y, 0.0), 0.55));                                    \n"
                                     "                                                                                                     \n"
                                     "    float s = max(dot(rd, SUN_DIR), 0.0);                                                            \n"
                                     "                                                                                                     \n"
                                     "    col += SUN_COLOR * (pow(s, 256.0) * 1.2 + pow(s, 6.0) * 0.18);                                   \n"
                                     "                                                                                                     \n"
                                     "    // Cloud cover, as the view ray hits a plane overhead: dividing by rd.y is what                  \n"
                                     "    // stretches the noise towards the horizon the way distance would.                               \n"
                                     "    if (rd.y > 0.02) {                                                                               \n"
                                     "        vec2  cp = rd.xz / rd.y * 1.1 + vec2(u_time * 0.045, u_time * 0.02);                         \n"
                                     "        float c  = smoothstep(0.42, 0.95, fbm(cp));                                                  \n"
                                     "        col      = mix(col, vec3(0.92, 0.94, 0.98), c * smoothstep(0.02, 0.28, rd.y) * 0.75);        \n"
                                     "    }                                                                                                \n"
                                     "                                                                                                     \n"
                                     "    return col;                                                                                      \n"
                                     "}                                                                                                    \n"
                                     "                                                                                                     \n"
                                     "                                                                                                     \n"
                                     "vec3 albedo(float id, vec3 p) {                                                                      \n"
                                     "                                                                                                     \n"
                                     "    if (id < 0.5) {                                                                                  \n"
                                     "        return mix(vec3(0.09, 0.10, 0.12), vec3(0.42, 0.43, 0.45), mod(floor(p.x) + floor(p.z), 2.0));\n"
                                     "    }                                                                                                \n"
                                     "                                                                                                     \n"
                                     "    // The blob takes its colour from where it is rather than from which sphere it                   \n"
                                     "    // came from, so the fused shape reads as one body instead of five.                              \n"
                                     "    if (id < 1.5) {                                                                                  \n"
                                     "        return 0.5 + 0.5 * cos(vec3(0.0, 2.1, 4.2) + length(p) * 1.6 + u_time * 0.6);                \n"
                                     "    }                                                                                                \n"
                                     "                                                                                                     \n"
                                     "    if (id < 2.5) {                                                                                  \n"
                                     "        return vec3(0.92, 0.68, 0.26);                                                               \n"
                                     "    }                                                                                                \n"
                                     "                                                                                                     \n"
                                     "    return vec3(0.52, 0.50, 0.46);                                                                   \n"
                                     "}                                                                                                    \n"
                                     "                                                                                                     \n"
                                     "                                                                                                     \n"
                                     "vec3 shade(vec3 p, vec3 n, vec3 rd, float id) {                                                      \n"
                                     "                                                                                                     \n"
                                     "    vec3 base = albedo(id, p);                                                                       \n"
                                     "                                                                                                     \n"
                                     "    float sha = softShadow(p + n * 0.02, SUN_DIR, 16.0);                                             \n"
                                     "    float ao  = calcAO(p, n);                                                                        \n"
                                     "                                                                                                     \n"
                                     "    float sun     = max(dot(n, SUN_DIR), 0.0) * sha;                                                 \n"
                                     "    float ambient = 0.5 + 0.5 * n.y;                                                                 \n"
                                     "    float bounce  = max(-n.y, 0.0);                                                                  \n"
                                     "                                                                                                     \n"
                                     "    vec3 col = base * (SUN_COLOR * sun + SKY_LOW * ambient * ao * 0.45 + vec3(0.30, 0.22, 0.16) * bounce * 0.25);\n"
                                     "                                                                                                     \n"
                                     "    float gloss = id < 0.5 ? 40.0 : (id < 1.5 ? 90.0 : (id < 2.5 ? 150.0 : 18.0));                   \n"
                                     "    float spec  = pow(max(dot(n, normalize(SUN_DIR - rd)), 0.0), gloss) * sun;                       \n"
                                     "                                                                                                     \n"
                                     "    return col + SUN_COLOR * spec * (id < 2.5 ? 0.9 : 0.15);                                         \n"
                                     "}                                                                                                    \n"
                                     "                                                                                                     \n"
                                     "                                                                                                     \n"
                                     "// How much of what a surface sees in a mirror comes back. The floor is polished                     \n"
                                     "// enough to carry the scene and the ring is metal; the rest of it reflects nothing.                 \n"
                                     "float reflectance(float id) {                                                                        \n"
                                     "    if (id < 0.5) {                                                                                  \n"
                                     "        return 0.35;                                                                                 \n"
                                     "    }                                                                                                \n"
                                     "    if (id > 1.5 && id < 2.5) {                                                                      \n"
                                     "        return 0.55;                                                                                 \n"
                                     "    }                                                                                                \n"
                                     "    return 0.0;                                                                                      \n"
                                     "}                                                                                                    \n"
                                     "                                                                                                     \n"
                                     "                                                                                                     \n"
                                     "vec3 render(vec3 ro, vec3 rd) {                                                                      \n"
                                     "                                                                                                     \n"
                                     "    vec3 background = sky(rd);                                                                       \n"
                                     "    vec2 hit        = march(ro, rd, MARCH_FAR, u_steps);                                             \n"
                                     "                                                                                                     \n"
                                     "    if (hit.y < 0.0) {                                                                               \n"
                                     "        return background;                                                                           \n"
                                     "    }                                                                                                \n"
                                     "                                                                                                     \n"
                                     "    vec3 p = ro + rd * hit.x;                                                                        \n"
                                     "    vec3 n = calcNormal(p);                                                                          \n"
                                     "                                                                                                     \n"
                                     "    vec3 col = shade(p, n, rd, hit.y);                                                               \n"
                                     "                                                                                                     \n"
                                     "    // One mirror bounce, and only off the two surfaces that should have one. The                    \n"
                                     "    // bounce is shaded without a shadow or an occlusion term: both cost another                     \n"
                                     "    // march each, and neither survives being mixed in at a quarter strength.                        \n"
                                     "    float refl = reflectance(hit.y);                                                                 \n"
                                     "                                                                                                     \n"
                                     "    if (refl > 0.0) {                                                                                \n"
                                     "                                                                                                     \n"
                                     "        vec3 ro2 = p + n * 0.03;                                                                     \n"
                                     "        vec3 rd2 = reflect(rd, n);                                                                   \n"
                                     "        vec2 h2  = march(ro2, rd2, MARCH_FAR, REFLECT_STEPS);                                        \n"
                                     "                                                                                                     \n"
                                     "        vec3 rcol = sky(rd2);                                                                        \n"
                                     "                                                                                                     \n"
                                     "        if (h2.y >= 0.0) {                                                                           \n"
                                     "            vec3 p2 = ro2 + rd2 * h2.x;                                                              \n"
                                     "            vec3 n2 = calcNormal(p2);                                                                \n"
                                     "            rcol    = albedo(h2.y, p2) * (SUN_COLOR * max(dot(n2, SUN_DIR), 0.0) +                   \n"
                                     "                                          SKY_LOW * (0.5 + 0.5 * n2.y) * 0.4);                       \n"
                                     "        }                                                                                            \n"
                                     "                                                                                                     \n"
                                     "        // Grazing angles mirror, head-on angles mostly do not.                                      \n"
                                     "        float fres = 0.04 + 0.96 * pow(1.0 - max(dot(n, -rd), 0.0), 5.0);                            \n"
                                     "                                                                                                     \n"
                                     "        col = mix(col, rcol, refl * (0.25 + 0.75 * fres));                                           \n"
                                     "    }                                                                                                \n"
                                     "                                                                                                     \n"
                                     "    return mix(col, background, 1.0 - exp(-0.0016 * hit.x * hit.x));                                 \n"
                                     "}                                                                                                    \n"
                                     "                                                                                                     \n"
                                     "                                                                                                     \n"
                                     "void main() {                                                                                        \n"
                                     "                                                                                                     \n"
                                     "    // The eye orbits the scene and breathes a little; everything else is the shader.                \n"
                                     "    float a = u_time * 0.16;                                                                         \n"
                                     "                                                                                                     \n"
                                     "    vec3 ro = vec3(sin(a) * 6.8, 1.75 + sin(u_time * 0.43) * 0.45, cos(a) * 6.8);                    \n"
                                     "    vec3 ta = vec3(0.0, 0.35, 0.0);                                                                  \n"
                                     "                                                                                                     \n"
                                     "    vec3 fw = normalize(ta - ro);                                                                    \n"
                                     "    vec3 rt = normalize(cross(fw, vec3(0.0, 1.0, 0.0)));                                             \n"
                                     "    vec3 up = cross(rt, fw);                                                                         \n"
                                     "                                                                                                     \n"
                                     "    vec3 rd = normalize(v_uv.x * u_aspect * rt + v_uv.y * up + 1.7 * fw);                            \n"
                                     "                                                                                                     \n"
                                     "    vec3 col = render(ro, rd);                                                                       \n"
                                     "                                                                                                     \n"
                                     "    // The march works in linear light and the window does not, so: tone map, gamma.                 \n"
                                     "    col = col / (1.0 + col);                                                                         \n"
                                     "    col = pow(col, vec3(0.4545));                                                                    \n"
                                     "    col = col * (1.0 - 0.18 * dot(v_uv, v_uv));                                                      \n"
                                     "                                                                                                     \n"
                                     "    gl_FragColor = vec4(col, 1.0);                                                                   \n"
                                     "}                                                                                                    \n";



/**
 * @brief Points OSMesa at the window's own pixels, and sizes the rendering to the pixel scale.
 *
 * The viewport is the top-left corner of the window, which scene_upscale() spreads over the rest.
 *
 * @param ctx The OSMesa context to bind.
 * @param win The window to render into.
 * @param scale How much smaller than the window the rendering is.
 * @return 0 on success, or -1.
 */

static int scene_bind(OSMesaContext ctx, ui_window_t* win, int scale) {

    const int width  = ui_window_width(win);
    const int height = ui_window_height(win);

    if (!OSMesaMakeCurrent(ctx, ui_window_pixels(win), GL_UNSIGNED_BYTE, width, height)) {
        return -1;
    }

    OSMesaPixelStore(OSMESA_Y_UP, 0);

    const int vw = width / scale > 0 ? width / scale : 1;
    const int vh = height / scale > 0 ? height / scale : 1;

    glViewport(0, height - vh, vw, vh);

    return 0;
}


/**
 * @brief Spreads the corner the shader rendered into over the whole window, in place and nearest-neighbour.
 *
 * @param win The window holding the surface.
 * @param scale How many times to repeat each pixel in each direction.
 */

static void scene_upscale(ui_window_t* win, int scale) {

    if (scale <= 1) {
        return;
    }

    const int width  = ui_window_width(win);
    const int height = ui_window_height(win);

    uint32_t* pixels    = ui_window_pixels(win);
    const size_t stride = ui_window_stride(win) / sizeof(uint32_t);

    for (int y = height - 1; y >= 0; y--) {

        const uint32_t* src = pixels + (size_t)(y / scale) * stride;
        uint32_t* dst       = pixels + (size_t)y * stride;

        for (int x = width - 1; x >= 0; x--) {
            dst[x] = src[x / scale];
        }
    }
}


/**
 * @brief Compiles one shader stage, and says why if it would not.
 *
 * @param type The stage to compile.
 * @param source The shader source.
 * @param name The stage's name, for the error message.
 * @return The shader, or 0.
 */

static GLuint scene_shader(GLenum type, const char* source, const char* name) {

    GLuint shader = glCreateShader(type);

    if (!shader) {
        fprintf(stderr, "gl-shaders-scene: glCreateShader(%s) failed\n", name);
        return 0;
    }

    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

    if (!compiled) {

        GLchar log[2048] = {0};
        glGetShaderInfoLog(shader, sizeof(log) - 1, NULL, log);

        fprintf(stderr, "gl-shaders-scene: %s shader did not compile:\n%s\n", name, log);

        glDeleteShader(shader);
        return 0;
    }

    return shader;
}


static GLuint scene_program(void) {

    GLuint vert = scene_shader(GL_VERTEX_SHADER, vertex_shader, "vertex");
    GLuint frag = scene_shader(GL_FRAGMENT_SHADER, fragment_shader, "fragment");

    if (!vert || !frag) {
        glDeleteShader(vert);
        glDeleteShader(frag);
        return 0;
    }

    GLuint program = glCreateProgram();

    glAttachShader(program, vert);
    glAttachShader(program, frag);

    glBindAttribLocation(program, SCENE_ATTRIB_POSITION, "position");

    glLinkProgram(program);

    glDeleteShader(vert);
    glDeleteShader(frag);

    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);

    if (!linked) {

        GLchar log[2048] = {0};
        glGetProgramInfoLog(program, sizeof(log) - 1, NULL, log);

        fprintf(stderr, "gl-shaders-scene: program did not link:\n%s\n", log);

        glDeleteProgram(program);
        return 0;
    }

    return program;
}


static uint64_t now_ms(void) {

    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) {
        return 0;
    }

    return ((uint64_t)ts.tv_sec * 1000ULL) + ((uint64_t)ts.tv_nsec / 1000000ULL);
}


/**
 * @brief Reports the frame rate once a second, and starts a fresh interval when it does.
 *
 * @param frames In/out. The frames counted since the last report.
 * @param since In/out. When the interval started.
 */

static void scene_fps_report(unsigned* frames, uint64_t* since) {

    const uint64_t now     = now_ms();
    const uint64_t elapsed = now - *since;

    if (elapsed < SCENE_FPS_INTERVAL_MS) {
        return;
    }

    printf("gl-shaders-scene: %.2f fps\n", (double)*frames * 1000.0 / (double)elapsed);

    *frames = 0;
    *since  = now;
}


static int clampi(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}


int main(int argc, char** argv) {

    setvbuf(stdout, NULL, _IONBF, 0);


    int width  = argc > 1 ? atoi(argv[1]) : SCENE_DEFAULT_WIDTH;
    int height = argc > 2 ? atoi(argv[2]) : SCENE_DEFAULT_HEIGHT;
    int scale  = argc > 3 ? atoi(argv[3]) : SCENE_DEFAULT_PIXEL_SCALE;
    int steps  = argc > 4 ? atoi(argv[4]) : SCENE_DEFAULT_MARCH_STEPS;

    width  = clampi(width, 32, 4096);
    height = clampi(height, 32, 4096);
    scale  = clampi(scale, SCENE_MIN_PIXEL_SCALE, SCENE_MAX_PIXEL_SCALE);
    steps  = clampi(steps, SCENE_MIN_MARCH_STEPS, SCENE_MAX_MARCH_STEPS);


    ui_connection_t* conn = ui_connect(NULL, 5000);

    if (!conn) {
        fprintf(stderr, "gl-shaders-scene: ui_connect() failed: %s\n", strerror(errno));
        return 1;
    }

    ui_window_t* win = ui_window_create(conn, width, height, "gl-shaders-scene");

    if (!win) {
        fprintf(stderr, "gl-shaders-scene: ui_window_create() failed: %s\n", strerror(errno));
        return 1;
    }


    OSMesaContext ctx;

    #if OSMESA_MAJOR_VERSION * 100 + OSMESA_MINOR_VERSION >= 305
    ctx = OSMesaCreateContextExt(OSMESA_BGRA, 0, 0, 0, NULL);
    #else
    ctx = OSMesaCreateContext(OSMESA_BGRA, NULL);
    #endif

    if (!ctx) {
        fprintf(stderr, "gl-shaders-scene: OSMesaCreateContext() failed\n");
        return 1;
    }

    if (scene_bind(ctx, win, scale) < 0) {
        fprintf(stderr, "gl-shaders-scene: OSMesaMakeCurrent() failed\n");
        return 1;
    }


    fprintf(stderr, "GL_RENDERER    = %s\n", glGetString(GL_RENDERER));
    fprintf(stderr, "GL_VERSION     = %s\n", glGetString(GL_VERSION));
    fprintf(stderr, "GL_VENDOR      = %s\n", glGetString(GL_VENDOR));


    GLuint program = scene_program();

    if (!program) {
        return 1;
    }

    const GLint u_time   = glGetUniformLocation(program, "u_time");
    const GLint u_aspect = glGetUniformLocation(program, "u_aspect");
    const GLint u_steps  = glGetUniformLocation(program, "u_steps");

    static const GLfloat quad[4][2] = {
        {-1.0f, -1.0f},
        {1.0f,  -1.0f},
        {-1.0f, 1.0f },
        {1.0f,  1.0f }
    };

    glUseProgram(program);

    glVertexAttribPointer(SCENE_ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE, 0, quad);
    glEnableVertexAttribArray(SCENE_ATTRIB_POSITION);


    printf("gl-shaders-scene: %dx%d window, shaded at 1/%d, %d march steps\n", ui_window_width(win), ui_window_height(win), scale, steps);
    printf("gl-shaders-scene: space pauses, '-' and '=' change the pixel scale, esc quits\n");


    bool running = true;
    bool paused  = false;
    bool redraw  = true;

    uint64_t clock_ms = 0;
    uint64_t last     = now_ms();

    unsigned fps_frames = 0;
    uint64_t fps_since  = now_ms();

    while (running) {

        const bool was_paused = paused;

        int timeout = (was_paused && !redraw) ? -1 : 0;

        for (;;) {

            ui_event_t ev;

            int e = ui_next_event(conn, &ev, timeout);

            timeout = 0;

            if (e < 0) {
                fprintf(stderr, "gl-shaders-scene: ui_next_event() failed: %s\n", strerror(errno));
                running = false;
                break;
            }

            if (e == 0) {
                break;
            }


            if (ev.type == UI_EVENT_CLOSE) {
                running = false;
                break;
            }

            if (ev.type == UI_EVENT_CONFIGURE) {

                if (ui_window_apply_configure(win) < 0) {
                    fprintf(stderr, "gl-shaders-scene: ui_window_apply_configure() failed: %s\n", strerror(errno));
                    running = false;
                    break;
                }

                if (scene_bind(ctx, win, scale) < 0) {
                    fprintf(stderr, "gl-shaders-scene: OSMesaMakeCurrent() failed\n");
                    running = false;
                    break;
                }

                redraw = true;
            }

            if (ev.type != UI_EVENT_KEY || !ev.key.down) {
                continue;
            }

            switch (ev.key.vkey) {

                case KEY_ESC:
                    running = false;
                    break;

                case KEY_SPACE:

                    paused = !paused;

                    fps_frames = 0;
                    fps_since  = now_ms();

                    break;

                case KEY_MINUS:
                case KEY_EQUAL:

                    scale = clampi(ev.key.vkey == KEY_EQUAL ? scale - 1 : scale + 1, SCENE_MIN_PIXEL_SCALE, SCENE_MAX_PIXEL_SCALE);

                    if (scene_bind(ctx, win, scale) < 0) {
                        fprintf(stderr, "gl-shaders-scene: OSMesaMakeCurrent() failed\n");
                        running = false;
                        break;
                    }

                    printf("gl-shaders-scene: shading at 1/%d\n", scale);

                    redraw = true;
                    break;
            }

            if (!running) {
                break;
            }
        }

        if (!running) {
            break;
        }

        const uint64_t now = now_ms();

        if (!was_paused) {
            clock_ms += now - last;
        }

        last = now;

        if (paused && !redraw) {
            continue;
        }

        redraw = false;


        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUniform1f(u_time, (GLfloat)clock_ms / 1000.0f);
        glUniform1f(u_aspect, (GLfloat)ui_window_width(win) / (GLfloat)ui_window_height(win));
        glUniform1i(u_steps, steps);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glFinish();


        scene_upscale(win, scale);

        ui_window_damage_all(win);

        if (ui_window_commit(win) < 0) {
            fprintf(stderr, "gl-shaders-scene: ui_window_commit() failed: %s\n", strerror(errno));
            break;
        }

        fps_frames++;

        if (!paused) {
            scene_fps_report(&fps_frames, &fps_since);
        }
    }


    glDeleteProgram(program);

    OSMesaDestroyContext(ctx);

    ui_window_destroy(win);
    ui_disconnect(conn);

    return 0;
}

