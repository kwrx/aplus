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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>

#include <GL/gl.h>
#include <GL/osmesa.h>

#include <aplus/fb.h>



static void gear(GLfloat inner_radius, GLfloat outer_radius, GLfloat width, GLint teeth, GLfloat tooth_depth) {
  
    GLint i;
    GLfloat r0, r1, r2;
    GLfloat angle, da;
    GLfloat u, v, len;

    r0 = inner_radius;
    r1 = outer_radius - tooth_depth / 2.f;
    r2 = outer_radius + tooth_depth / 2.f;

    da = 2.f * (float) M_PI / teeth / 4.f;

    glShadeModel(GL_FLAT);

    glNormal3f(0.f, 0.f, 1.f);

    /* draw front face */
    glBegin(GL_QUAD_STRIP);
    for (i = 0; i <= teeth; i++) {
        angle = i * 2.f * (float) M_PI / teeth;
        glVertex3f(r0 * (float) cos(angle), r0 * (float) sin(angle), width * 0.5f);
        glVertex3f(r1 * (float) cos(angle), r1 * (float) sin(angle), width * 0.5f);
        if (i < teeth) {
            glVertex3f(r0 * (float) cos(angle), r0 * (float) sin(angle), width * 0.5f);
            glVertex3f(r1 * (float) cos(angle + 3 * da), r1 * (float) sin(angle + 3 * da), width * 0.5f);
        }
    }
    glEnd();

    /* draw front sides of teeth */
    glBegin(GL_QUADS);
    da = 2.f * (float) M_PI / teeth / 4.f;
    for (i = 0; i < teeth; i++) {
        angle = i * 2.f * (float) M_PI / teeth;

        glVertex3f(r1 * (float) cos(angle), r1 * (float) sin(angle), width * 0.5f);
        glVertex3f(r2 * (float) cos(angle + da), r2 * (float) sin(angle + da), width * 0.5f);
        glVertex3f(r2 * (float) cos(angle + 2 * da), r2 * (float) sin(angle + 2 * da), width * 0.5f);
        glVertex3f(r1 * (float) cos(angle + 3 * da), r1 * (float) sin(angle + 3 * da), width * 0.5f);
    }
    glEnd();

    glNormal3f(0.0, 0.0, -1.0);

    /* draw back face */
    glBegin(GL_QUAD_STRIP);
    for (i = 0; i <= teeth; i++) {
        angle = i * 2.f * (float) M_PI / teeth;
        glVertex3f(r1 * (float) cos(angle), r1 * (float) sin(angle), -width * 0.5f);
        glVertex3f(r0 * (float) cos(angle), r0 * (float) sin(angle), -width * 0.5f);
        if (i < teeth) {
            glVertex3f(r1 * (float) cos(angle + 3 * da), r1 * (float) sin(angle + 3 * da), -width * 0.5f);
            glVertex3f(r0 * (float) cos(angle), r0 * (float) sin(angle), -width * 0.5f);
        }
    }
    glEnd();

    /* draw back sides of teeth */
    glBegin(GL_QUADS);
    da = 2.f * (float) M_PI / teeth / 4.f;
    for (i = 0; i < teeth; i++) {
        angle = i * 2.f * (float) M_PI / teeth;

        glVertex3f(r1 * (float) cos(angle + 3 * da), r1 * (float) sin(angle + 3 * da), -width * 0.5f);
        glVertex3f(r2 * (float) cos(angle + 2 * da), r2 * (float) sin(angle + 2 * da), -width * 0.5f);
        glVertex3f(r2 * (float) cos(angle + da), r2 * (float) sin(angle + da), -width * 0.5f);
        glVertex3f(r1 * (float) cos(angle), r1 * (float) sin(angle), -width * 0.5f);
    }
    glEnd();

    /* draw outward faces of teeth */
    glBegin(GL_QUAD_STRIP);
    for (i = 0; i < teeth; i++) {
        angle = i * 2.f * (float) M_PI / teeth;

        glVertex3f(r1 * (float) cos(angle), r1 * (float) sin(angle), width * 0.5f);
        glVertex3f(r1 * (float) cos(angle), r1 * (float) sin(angle), -width * 0.5f);
        u = r2 * (float) cos(angle + da) - r1 * (float) cos(angle);
        v = r2 * (float) sin(angle + da) - r1 * (float) sin(angle);
        len = (float) sqrt(u * u + v * v);
        u /= len;
        v /= len;
        glNormal3f(v, -u, 0.0);
        glVertex3f(r2 * (float) cos(angle + da), r2 * (float) sin(angle + da), width * 0.5f);
        glVertex3f(r2 * (float) cos(angle + da), r2 * (float) sin(angle + da), -width * 0.5f);
        glNormal3f((float) cos(angle), (float) sin(angle), 0.f);
        glVertex3f(r2 * (float) cos(angle + 2 * da), r2 * (float) sin(angle + 2 * da), width * 0.5f);
        glVertex3f(r2 * (float) cos(angle + 2 * da), r2 * (float) sin(angle + 2 * da), -width * 0.5f);
        u = r1 * (float) cos(angle + 3 * da) - r2 * (float) cos(angle + 2 * da);
        v = r1 * (float) sin(angle + 3 * da) - r2 * (float) sin(angle + 2 * da);
        glNormal3f(v, -u, 0.f);
        glVertex3f(r1 * (float) cos(angle + 3 * da), r1 * (float) sin(angle + 3 * da), width * 0.5f);
        glVertex3f(r1 * (float) cos(angle + 3 * da), r1 * (float) sin(angle + 3 * da), -width * 0.5f);
        glNormal3f((float) cos(angle), (float) sin(angle), 0.f);
    }

    glVertex3f(r1 * (float) cos(0), r1 * (float) sin(0), width * 0.5f);
    glVertex3f(r1 * (float) cos(0), r1 * (float) sin(0), -width * 0.5f);

    glEnd();

    glShadeModel(GL_SMOOTH);

    /* draw inside radius cylinder */
    glBegin(GL_QUAD_STRIP);
    for (i = 0; i <= teeth; i++) {
        angle = i * 2.f * (float) M_PI / teeth;
        glNormal3f(-(float) cos(angle), -(float) sin(angle), 0.f);
        glVertex3f(r0 * (float) cos(angle), r0 * (float) sin(angle), -width * 0.5f);
        glVertex3f(r0 * (float) cos(angle), r0 * (float) sin(angle), width * 0.5f);
    }
    glEnd();

}


int main(int argc, char** argv) {


    int fd = open("/dev/fb0", O_RDWR);

    if(fd < 0) {
        return perror("open /dev/fb0"), 1;
    }


    struct fb_var_screeninfo var;
    struct fb_fix_screeninfo fix;

    if(ioctl(fd, FBIOGET_VSCREENINFO, &var) < 0) {
        return perror("ioctl FBIOGET_VSCREENINFO"), 1;
    }

    if(ioctl(fd, FBIOGET_FSCREENINFO, &fix) < 0) {
        return perror("ioctl FBIOGET_FSCREENINFO"), 1;
    }


    fprintf(stderr, "fb0: %dx%d, %d bpp, %d bytes per line ", var.xres, var.yres, var.bits_per_pixel, fix.line_length);


    
    OSMesaContext ctx;

#if OSMESA_MAJOR_VERSION * 100 + OSMESA_MINOR_VERSION >= 305
    ctx = OSMesaCreateContextExt(OSMESA_RGBA, var.bits_per_pixel, 0, 0, NULL);
#else
    ctx = OSMesaCreateContext(OSMESA_RGBA, NULL);
#endif

    if(!ctx) {
        return perror("OSMesaCreateContext"), 1;
    }


    void* backbuffer = malloc(var.yres * fix.line_length);

    if(!backbuffer) {
        return perror("malloc"), 1;
    }

    if(!OSMesaMakeCurrent(ctx, backbuffer, GL_UNSIGNED_BYTE, var.xres, var.yres)) {
        return perror("OSMesaMakeCurrent"), 1;
    }


    fprintf(stderr, "GL_RENDERER    = %s\n", glGetString(GL_RENDERER));
    fprintf(stderr, "GL_VERSION     = %s\n", glGetString(GL_VERSION));
    fprintf(stderr, "GL_VENDOR      = %s\n", glGetString(GL_VENDOR));
    fprintf(stderr, "GL_EXTENSIONS  = %s\n", glGetString(GL_EXTENSIONS));



    GLfloat ratio = (GLfloat) var.yres / (GLfloat) var.xres;
    GLfloat xmax  = 2.5f;

    glViewport(0, 0, var.xres, var.yres);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-xmax, xmax, -xmax * ratio, xmax * ratio, 5.0f, 60.0f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, -20.0f);




    static GLfloat pos[4]   = { 5.0f, 5.0f, 10.0f, 0.0f };
    static GLfloat red[4]   = { 0.8f, 0.1f, 0.0f, 1.0f };
    static GLfloat green[4] = { 0.0f, 0.8f, 0.2f, 1.0f };
    static GLfloat blue[4]  = { 0.2f, 0.2f, 1.0f, 1.0f };


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
    GLfloat angle = 0.0f;

    do {


        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


        glPushMatrix(); {

            glRotatef(view_rotx, 1.0f, 0.0f, 0.0f);
            glRotatef(view_roty, 0.0f, 1.0f, 0.0f);
            glRotatef(view_rotz, 0.0f, 0.0f, 1.0f);

            glPushMatrix(); {

                glTranslatef(-3.0f, -2.0f, 0.0f);
                glRotatef(angle, 0.0f, 0.0f, 1.0f);
                glCallList(gear1);
            
            } glPopMatrix();

            glPushMatrix(); {

                glTranslatef(3.1f, -2.0f, 0.0f);
                glRotatef(-2.0f * angle - 9.0f, 0.0f, 0.0f, 1.0f);
                glCallList(gear2);
            
            } glPopMatrix();

            glPushMatrix(); {

                glTranslatef(-3.1f, 4.2f, 0.0f);
                glRotatef(-2.0f * angle - 25.0f, 0.0f, 0.0f, 1.0f);
                glCallList(gear3);
            
            } glPopMatrix();

        } glPopMatrix();

        glFinish();


        memcpy((void*) fix.smem_start, backbuffer, var.yres * fix.line_length);

        angle += 2.0f;

    } while(1);


    OSMesaDestroyContext(ctx);

    return 0;

}

#else

#include <stdio.h>

int main(int argc, char** argv) {
    return fprintf(stderr, "OSMesa not available\n"), 1;
}

#endif