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

#if defined(CONFIG_HAVE_MESA)

    #include <errno.h>
    #include <math.h>
    #include <stdbool.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include <unistd.h>

    #include <GL/gl.h>
    #include <GL/osmesa.h>

    #include <aplus/input.h>
    #include <aplus/ui.h>


    #define GEARS_DEFAULT_WIDTH  480
    #define GEARS_DEFAULT_HEIGHT 360



static void gear(GLfloat inner_radius, GLfloat outer_radius, GLfloat width, GLint teeth, GLfloat tooth_depth) {

    GLint i;
    GLfloat r0, r1, r2;
    GLfloat angle, da;
    GLfloat u, v, len;

    r0 = inner_radius;
    r1 = outer_radius - tooth_depth / 2.f;
    r2 = outer_radius + tooth_depth / 2.f;

    da = 2.f * (float)M_PI / teeth / 4.f;

    glShadeModel(GL_FLAT);

    glNormal3f(0.f, 0.f, 1.f);

    /* draw front face */
    glBegin(GL_QUAD_STRIP);
    for (i = 0; i <= teeth; i++) {
        angle = i * 2.f * (float)M_PI / teeth;
        glVertex3f(r0 * (float)cos(angle), r0 * (float)sin(angle), width * 0.5f);
        glVertex3f(r1 * (float)cos(angle), r1 * (float)sin(angle), width * 0.5f);
        if (i < teeth) {
            glVertex3f(r0 * (float)cos(angle), r0 * (float)sin(angle), width * 0.5f);
            glVertex3f(r1 * (float)cos(angle + 3 * da), r1 * (float)sin(angle + 3 * da), width * 0.5f);
        }
    }
    glEnd();

    /* draw front sides of teeth */
    glBegin(GL_QUADS);
    da = 2.f * (float)M_PI / teeth / 4.f;
    for (i = 0; i < teeth; i++) {
        angle = i * 2.f * (float)M_PI / teeth;

        glVertex3f(r1 * (float)cos(angle), r1 * (float)sin(angle), width * 0.5f);
        glVertex3f(r2 * (float)cos(angle + da), r2 * (float)sin(angle + da), width * 0.5f);
        glVertex3f(r2 * (float)cos(angle + 2 * da), r2 * (float)sin(angle + 2 * da), width * 0.5f);
        glVertex3f(r1 * (float)cos(angle + 3 * da), r1 * (float)sin(angle + 3 * da), width * 0.5f);
    }
    glEnd();

    glNormal3f(0.0, 0.0, -1.0);

    /* draw back face */
    glBegin(GL_QUAD_STRIP);
    for (i = 0; i <= teeth; i++) {
        angle = i * 2.f * (float)M_PI / teeth;
        glVertex3f(r1 * (float)cos(angle), r1 * (float)sin(angle), -width * 0.5f);
        glVertex3f(r0 * (float)cos(angle), r0 * (float)sin(angle), -width * 0.5f);
        if (i < teeth) {
            glVertex3f(r1 * (float)cos(angle + 3 * da), r1 * (float)sin(angle + 3 * da), -width * 0.5f);
            glVertex3f(r0 * (float)cos(angle), r0 * (float)sin(angle), -width * 0.5f);
        }
    }
    glEnd();

    /* draw back sides of teeth */
    glBegin(GL_QUADS);
    da = 2.f * (float)M_PI / teeth / 4.f;
    for (i = 0; i < teeth; i++) {
        angle = i * 2.f * (float)M_PI / teeth;

        glVertex3f(r1 * (float)cos(angle + 3 * da), r1 * (float)sin(angle + 3 * da), -width * 0.5f);
        glVertex3f(r2 * (float)cos(angle + 2 * da), r2 * (float)sin(angle + 2 * da), -width * 0.5f);
        glVertex3f(r2 * (float)cos(angle + da), r2 * (float)sin(angle + da), -width * 0.5f);
        glVertex3f(r1 * (float)cos(angle), r1 * (float)sin(angle), -width * 0.5f);
    }
    glEnd();

    /* draw outward faces of teeth */
    glBegin(GL_QUAD_STRIP);
    for (i = 0; i < teeth; i++) {
        angle = i * 2.f * (float)M_PI / teeth;

        glVertex3f(r1 * (float)cos(angle), r1 * (float)sin(angle), width * 0.5f);
        glVertex3f(r1 * (float)cos(angle), r1 * (float)sin(angle), -width * 0.5f);
        u   = r2 * (float)cos(angle + da) - r1 * (float)cos(angle);
        v   = r2 * (float)sin(angle + da) - r1 * (float)sin(angle);
        len = (float)sqrt(u * u + v * v);
        u /= len;
        v /= len;
        glNormal3f(v, -u, 0.0);
        glVertex3f(r2 * (float)cos(angle + da), r2 * (float)sin(angle + da), width * 0.5f);
        glVertex3f(r2 * (float)cos(angle + da), r2 * (float)sin(angle + da), -width * 0.5f);
        glNormal3f((float)cos(angle), (float)sin(angle), 0.f);
        glVertex3f(r2 * (float)cos(angle + 2 * da), r2 * (float)sin(angle + 2 * da), width * 0.5f);
        glVertex3f(r2 * (float)cos(angle + 2 * da), r2 * (float)sin(angle + 2 * da), -width * 0.5f);
        u = r1 * (float)cos(angle + 3 * da) - r2 * (float)cos(angle + 2 * da);
        v = r1 * (float)sin(angle + 3 * da) - r2 * (float)sin(angle + 2 * da);
        glNormal3f(v, -u, 0.f);
        glVertex3f(r1 * (float)cos(angle + 3 * da), r1 * (float)sin(angle + 3 * da), width * 0.5f);
        glVertex3f(r1 * (float)cos(angle + 3 * da), r1 * (float)sin(angle + 3 * da), -width * 0.5f);
        glNormal3f((float)cos(angle), (float)sin(angle), 0.f);
    }

    glVertex3f(r1 * (float)cos(0), r1 * (float)sin(0), width * 0.5f);
    glVertex3f(r1 * (float)cos(0), r1 * (float)sin(0), -width * 0.5f);

    glEnd();

    glShadeModel(GL_SMOOTH);

    /* draw inside radius cylinder */
    glBegin(GL_QUAD_STRIP);
    for (i = 0; i <= teeth; i++) {
        angle = i * 2.f * (float)M_PI / teeth;
        glNormal3f(-(float)cos(angle), -(float)sin(angle), 0.f);
        glVertex3f(r0 * (float)cos(angle), r0 * (float)sin(angle), -width * 0.5f);
        glVertex3f(r0 * (float)cos(angle), r0 * (float)sin(angle), width * 0.5f);
    }
    glEnd();
}


/* Point OSMesa at the window's own pixels and set the view up for their size.
 *
 * There is no blit anywhere in this program: the window buffer libui hands out is a tight
 * run of 32-bit pixels, which is exactly what OSMesa wants to render into, so GL draws
 * straight into the surface that gets committed. OSMESA_BGRA is what makes that work -- it
 * lays each pixel down as B, G, R, A, which read back as the 0xAARRGGBB the server expects.
 *
 * Called again after every resize, because applying a configure can move the buffer.
 */

static int gears_bind(OSMesaContext ctx, ui_window_t* win) {

    const int width  = ui_window_width(win);
    const int height = ui_window_height(win);

    if (!OSMesaMakeCurrent(ctx, ui_window_pixels(win), GL_UNSIGNED_BYTE, width, height)) {
        return -1;
    }

    /* GL counts rows up from the bottom and a window counts them down from the top. Saying
       so here is cheaper than flipping every frame by hand. */
    OSMesaPixelStore(OSMESA_Y_UP, 0);


    const GLfloat ratio = (GLfloat)height / (GLfloat)width;
    const GLfloat xmax  = 2.5f;

    glViewport(0, 0, width, height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-xmax, xmax, -xmax * ratio, xmax * ratio, 5.0f, 60.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, -20.0f);

    return 0;
}


int main(int argc, char** argv) {

    setvbuf(stdout, NULL, _IONBF, 0);


    int width  = argc > 1 ? atoi(argv[1]) : GEARS_DEFAULT_WIDTH;
    int height = argc > 2 ? atoi(argv[2]) : GEARS_DEFAULT_HEIGHT;


    ui_connection_t* conn = ui_connect(NULL, 5000);

    if (!conn) {
        fprintf(stderr, "gl-gears: ui_connect() failed: %s\n", strerror(errno));
        return 1;
    }

    ui_window_t* win = ui_window_create(conn, width, height, "gl-gears");

    if (!win) {
        fprintf(stderr, "gl-gears: ui_window_create() failed: %s\n", strerror(errno));
        return 1;
    }


    OSMesaContext ctx;

    #if OSMESA_MAJOR_VERSION * 100 + OSMESA_MINOR_VERSION >= 305
    ctx = OSMesaCreateContextExt(OSMESA_BGRA, 24, 0, 0, NULL);
    #else
    ctx = OSMesaCreateContext(OSMESA_BGRA, NULL);
    #endif

    if (!ctx) {
        fprintf(stderr, "gl-gears: OSMesaCreateContext() failed\n");
        return 1;
    }

    if (gears_bind(ctx, win) < 0) {
        fprintf(stderr, "gl-gears: OSMesaMakeCurrent() failed\n");
        return 1;
    }


    fprintf(stderr, "GL_RENDERER    = %s\n", glGetString(GL_RENDERER));
    fprintf(stderr, "GL_VERSION     = %s\n", glGetString(GL_VERSION));
    fprintf(stderr, "GL_VENDOR      = %s\n", glGetString(GL_VENDOR));


    static GLfloat pos[4]   = {5.0f, 5.0f, 10.0f, 0.0f};
    static GLfloat red[4]   = {0.8f, 0.1f, 0.0f, 1.0f};
    static GLfloat green[4] = {0.0f, 0.8f, 0.2f, 1.0f};
    static GLfloat blue[4]  = {0.2f, 0.2f, 1.0f, 1.0f};


    glLightfv(GL_LIGHT0, GL_POSITION, pos);
    glEnable(GL_CULL_FACE);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_DEPTH_TEST);


    GLint gear1 = glGenLists(1);
    glNewList(gear1, GL_COMPILE);
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, red);
    gear(1.0f, 4.0f, 1.0f, 20, 0.7f);
    glEndList();

    GLint gear2 = glGenLists(1);
    glNewList(gear2, GL_COMPILE);
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, green);
    gear(0.5f, 2.0f, 2.0f, 10, 0.7f);
    glEndList();

    GLint gear3 = glGenLists(1);
    glNewList(gear3, GL_COMPILE);
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, blue);
    gear(1.3f, 2.0f, 0.5f, 10, 0.7f);
    glEndList();


    glEnable(GL_NORMALIZE);


    GLfloat view_rotx = 20.0f;
    GLfloat view_roty = 30.0f;
    GLfloat view_rotz = 0.0f;
    GLfloat angle     = 0.0f;

    bool running = true;

    while (running) {

        /* Drained without waiting, because this window has something to say every frame
           whether or not the server does. A resize has to be picked up before the frame it
           applies to is drawn, which is the only reason the events come first. */
        for (;;) {

            ui_event_t ev;

            int e = ui_next_event(conn, &ev, 0);

            if (e < 0) {
                fprintf(stderr, "gl-gears: ui_next_event() failed: %s\n", strerror(errno));
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

            if (ev.type == UI_EVENT_KEY && ev.key.down && ev.key.vkey == KEY_ESC) {
                running = false;
                break;
            }

            if (ev.type == UI_EVENT_CONFIGURE) {

                if (ui_window_apply_configure(win) < 0) {
                    fprintf(stderr, "gl-gears: ui_window_apply_configure() failed: %s\n", strerror(errno));
                    running = false;
                    break;
                }

                if (gears_bind(ctx, win) < 0) {
                    fprintf(stderr, "gl-gears: OSMesaMakeCurrent() failed\n");
                    running = false;
                    break;
                }
            }
        }

        if (!running) {
            break;
        }


        /* Opaque black: the window buffer is nominally ARGB, and leaving the alpha at zero
           would be asking anything that does blend it to drop the frame entirely. */
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


        glPushMatrix();
        {

            glRotatef(view_rotx, 1.0f, 0.0f, 0.0f);
            glRotatef(view_roty, 0.0f, 1.0f, 0.0f);
            glRotatef(view_rotz, 0.0f, 0.0f, 1.0f);

            glPushMatrix();
            {

                glTranslatef(-3.0f, -2.0f, 0.0f);
                glRotatef(angle, 0.0f, 0.0f, 1.0f);
                glCallList(gear1);
            }
            glPopMatrix();

            glPushMatrix();
            {

                glTranslatef(3.1f, -2.0f, 0.0f);
                glRotatef(-2.0f * angle - 9.0f, 0.0f, 0.0f, 1.0f);
                glCallList(gear2);
            }
            glPopMatrix();

            glPushMatrix();
            {

                glTranslatef(-3.1f, 4.2f, 0.0f);
                glRotatef(-2.0f * angle - 25.0f, 0.0f, 0.0f, 1.0f);
                glCallList(gear3);
            }
            glPopMatrix();
        }
        glPopMatrix();

        glFinish();


        ui_window_damage_all(win);

        if (ui_window_commit(win) < 0) {
            fprintf(stderr, "gl-gears: ui_window_commit() failed: %s\n", strerror(errno));
            break;
        }

        angle += 2.0f;
    }


    OSMesaDestroyContext(ctx);

    ui_window_destroy(win);
    ui_disconnect(conn);

    return 0;
}

#else

    #include <stdio.h>

int main(int argc, char** argv) {
    return fprintf(stderr, "OSMesa not available\n"), 1;
}

#endif
