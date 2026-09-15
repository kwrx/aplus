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

    #define GL_VERSION_3_0      1
    #define GL_GLEXT_PROTOTYPES 1


    #include <errno.h>
    #include <math.h>
    #include <stdbool.h>
    #include <stdint.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include <time.h>
    #include <unistd.h>

    #include <GL/gl.h>
    #include <GL/glext.h>
    #include <GL/osmesa.h>

    #include <aplus/input.h>
    #include <aplus/ui.h>


    #define TRIANGLE_DEFAULT_WIDTH  480
    #define TRIANGLE_DEFAULT_HEIGHT 360

    #define TRIANGLE_FPS_INTERVAL_MS 1000



// vertex shader for a triangle
static char* vertex_shader = "#version 120                                   \n"
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
static char* fragment_shader = "#version 120                                   \n"
                               "                                               \n"
                               "varying vec3 v_color;                          \n"
                               "                                               \n"
                               "void main() {                                  \n"
                               "    gl_FragColor = vec4(v_color, 1.0);         \n"
                               "}                                              \n";



/* Point OSMesa at the window's own pixels.
 *
 * There is no blit anywhere in this program: the window buffer libui hands out is a tight
 * run of 32-bit pixels, which is exactly what OSMesa wants to render into, so GL draws
 * straight into the surface that gets committed. OSMESA_BGRA is what makes that work -- it
 * lays each pixel down as B, G, R, A, which read back as the 0xAARRGGBB the server expects.
 *
 * Called again after every resize, because applying a configure can move the buffer.
 */

static int triangle_bind(OSMesaContext ctx, ui_window_t* win) {

    const int width  = ui_window_width(win);
    const int height = ui_window_height(win);

    if (!OSMesaMakeCurrent(ctx, ui_window_pixels(win), GL_UNSIGNED_BYTE, width, height)) {
        return -1;
    }

    /* GL counts rows up from the bottom and a window counts them down from the top. Saying
       so here is cheaper than flipping every frame by hand. */
    OSMesaPixelStore(OSMESA_Y_UP, 0);

    glViewport(0, 0, width, height);

    return 0;
}


static uint64_t now_ms(void) {

    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) {
        return 0;
    }

    return ((uint64_t)ts.tv_sec * 1000ULL) + ((uint64_t)ts.tv_nsec / 1000000ULL);
}


/* Report the frame rate once a second, and start a fresh interval when it does.
 *
 * Frames are counted between reports rather than timed one by one: a single frame is close
 * enough to the clock's granularity that timing it says more about the clock than about the
 * renderer, and the average over a second is the figure worth having. The division is by the
 * interval actually measured, not by TRIANGLE_FPS_INTERVAL_MS, so a report that arrives late
 * is still an honest rate rather than an inflated one.
 *
 * This window only redraws when it is mapped or resized, so the usual reading here is 0.00
 * fps. That is the answer, not a missing one -- an idle window costs nothing to composite.
 */

static void triangle_fps_report(unsigned* frames, uint64_t* since) {

    const uint64_t now     = now_ms();
    const uint64_t elapsed = now - *since;

    if (elapsed < TRIANGLE_FPS_INTERVAL_MS) {
        return;
    }

    printf("gl-shaders-triangle: %.2f fps\n", (double)*frames * 1000.0 / (double)elapsed);

    *frames = 0;
    *since  = now;
}


int main(int argc, char** argv) {

    setvbuf(stdout, NULL, _IONBF, 0);


    int width  = argc > 1 ? atoi(argv[1]) : TRIANGLE_DEFAULT_WIDTH;
    int height = argc > 2 ? atoi(argv[2]) : TRIANGLE_DEFAULT_HEIGHT;


    ui_connection_t* conn = ui_connect(NULL, 5000);

    if (!conn) {
        fprintf(stderr, "gl-shaders-triangle: ui_connect() failed: %s\n", strerror(errno));
        return 1;
    }

    ui_window_t* win = ui_window_create(conn, width, height, "gl-shaders-triangle");

    if (!win) {
        fprintf(stderr, "gl-shaders-triangle: ui_window_create() failed: %s\n", strerror(errno));
        return 1;
    }


    OSMesaContext ctx;

    #if OSMESA_MAJOR_VERSION * 100 + OSMESA_MINOR_VERSION >= 305
    ctx = OSMesaCreateContextExt(OSMESA_BGRA, 24, 0, 0, NULL);
    #else
    ctx = OSMesaCreateContext(OSMESA_BGRA, NULL);
    #endif

    if (!ctx) {
        fprintf(stderr, "gl-shaders-triangle: OSMesaCreateContext() failed\n");
        return 1;
    }

    if (triangle_bind(ctx, win) < 0) {
        fprintf(stderr, "gl-shaders-triangle: OSMesaMakeCurrent() failed\n");
        return 1;
    }


    fprintf(stderr, "GL_RENDERER    = %s\n", glGetString(GL_RENDERER));
    fprintf(stderr, "GL_VERSION     = %s\n", glGetString(GL_VERSION));
    fprintf(stderr, "GL_VENDOR      = %s\n", glGetString(GL_VENDOR));


    static const GLfloat v[3][3] = {
        {-1.0, 1.0,  0.0},
        {1.0,  1.0,  0.0},
        {0.0,  -1.0, 0.0}
    };

    static const GLfloat c[3][3] = {
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
        {0.0, 0.0, 1.0}
    };

    static const GLubyte indices[3] = {0, 1, 2};

    GLuint prog = glCreateProgram();
    GLuint vert = glCreateShader(GL_VERTEX_SHADER);
    GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(vert, 1, (const GLchar**)&vertex_shader, NULL);
    glShaderSource(frag, 1, (const GLchar**)&fragment_shader, NULL);

    glCompileShader(vert);
    glCompileShader(frag);

    glAttachShader(prog, vert);
    glAttachShader(prog, frag);

    glLinkProgram(prog);


    bool running = true;
    bool redraw  = true;

    unsigned fps_frames = 0;
    uint64_t fps_since  = now_ms();

    while (running) {

        if (redraw) {

            redraw = false;

            /* Opaque black: the window buffer is nominally ARGB, and leaving the alpha at
               zero would be asking anything that does blend it to drop the frame. */
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glUseProgram(prog);

            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, v);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, c);

            glEnableVertexAttribArray(0);
            glEnableVertexAttribArray(1);

            glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_BYTE, indices);

            glFinish();


            ui_window_damage_all(win);

            if (ui_window_commit(win) < 0) {
                fprintf(stderr, "gl-shaders-triangle: ui_window_commit() failed: %s\n", strerror(errno));
                break;
            }

            fps_frames++;
        }


        triangle_fps_report(&fps_frames, &fps_since);


        /* The picture does not move, so this waits rather than spinning: a frame is drawn
           when the window is first mapped and again whenever it is resized, and never
           otherwise. The wait is bounded rather than indefinite only so that the report
           above keeps its schedule while nothing is happening; whatever is left of the
           current interval is exactly how long there is to wait. */
        ui_event_t ev;

        const uint64_t elapsed = now_ms() - fps_since;
        const int timeout      = elapsed >= TRIANGLE_FPS_INTERVAL_MS ? 0 : (int)(TRIANGLE_FPS_INTERVAL_MS - elapsed);

        int e = ui_next_event(conn, &ev, timeout);

        if (e < 0) {
            fprintf(stderr, "gl-shaders-triangle: ui_next_event() failed: %s\n", strerror(errno));
            break;
        }

        if (e == 0) {
            continue;
        }


        if (ev.type == UI_EVENT_CLOSE) {
            break;
        }

        if (ev.type == UI_EVENT_KEY && ev.key.down && ev.key.vkey == KEY_ESC) {
            break;
        }

        if (ev.type == UI_EVENT_CONFIGURE) {

            if (ui_window_apply_configure(win) < 0) {
                fprintf(stderr, "gl-shaders-triangle: ui_window_apply_configure() failed: %s\n", strerror(errno));
                break;
            }

            if (triangle_bind(ctx, win) < 0) {
                fprintf(stderr, "gl-shaders-triangle: OSMesaMakeCurrent() failed\n");
                break;
            }

            redraw = true;
        }
    }


    glDeleteShader(vert);
    glDeleteShader(frag);
    glDeleteProgram(prog);

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
