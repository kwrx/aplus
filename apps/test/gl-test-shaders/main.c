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

#define GL_VERSION_3_0          1
#define GL_GLEXT_PROTOTYPES     1


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>

#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/osmesa.h>

#include <aplus/fb.h>



// vertex shader for a triangle
static char* vertex_shader = 
    "#version 120                                   \n"
    "                                               \n"
    "attribute vec3 position;                       \n"
    "attribute vec3 color;                          \n"
    "                                               \n"
    "varying vec3 v_color;                          \n"
    "                                               \n"
    "void main() {                                  \n"
    "    gl_Position = vec4(position, 1.0);         \n"
    "    v_color = color;                           \n"
    "}                                              \n";

// fragment shader for a triangle
static char* fragment_shader = 
    "#version 120                                   \n"
    "                                               \n"
    "varying vec3 v_color;                          \n"
    "                                               \n"
    "void main() {                                  \n"
    "    gl_FragColor = vec4(v_color, 1.0);         \n"
    "}                                              \n";



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





    static const GLfloat v[3][3] = {
        { -1.0,  1.0, 0.0 },
        {  1.0,  1.0, 0.0 },
        {  0.0, -1.0, 0.0 }
    };

    static const GLfloat c[3][3] = {
        { 1.0, 0.0, 0.0 },
        { 0.0, 1.0, 0.0 },
        { 0.0, 0.0, 1.0 }
    };

    static const GLubyte indices[3] = { 0, 1, 2 };

    GLuint prog = glCreateProgram();
    GLuint vert = glCreateShader(GL_VERTEX_SHADER);
    GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(vert, 1, (const GLchar**) &vertex_shader, NULL);
    glShaderSource(frag, 1, (const GLchar**) &fragment_shader, NULL);

    glCompileShader(vert);
    glCompileShader(frag);

    glAttachShader(prog, vert);
    glAttachShader(prog, frag);

    glLinkProgram(prog);



    do {


        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


        glPushMatrix(); {

            glUseProgram(prog);

            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, v);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, c);

            glEnableVertexAttribArray(0);
            glEnableVertexAttribArray(1);

            glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_BYTE, indices);

        } glPopMatrix();


        glFinish();

        memcpy((void*) fix.smem_start, backbuffer, var.yres * fix.line_length);


    } while(1);


    glDeleteShader(vert);
    glDeleteShader(frag);
    glDeleteProgram(prog);


    OSMesaDestroyContext(ctx);

    return 0;

}

#else

#include <stdio.h>

int main(int argc, char** argv) {
    return fprintf(stderr, "OSMesa not available\n"), 1;
}

#endif