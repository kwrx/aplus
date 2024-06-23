#ifndef _WC_WINDOW_H
#define _WC_WINDOW_H

#include <cairo/cairo.h>
#include <wc/wc.h>
#include <wc/wc_cursor.h>
#include <wc/wc_display.h>
#include <wc/wc_font.h>
#include <wc/wc_renderer.h>

#ifdef __cplusplus
extern "C" {
#endif


#define WC_WINDOW_FLAGS_NONE          0x00000000
#define WC_WINDOW_FLAGS_FULLSCREEN    0x00000001
#define WC_WINDOW_FLAGS_RESIZABLE     0x00000002
#define WC_WINDOW_FLAGS_BORDERLESS    0x00000004
#define WC_WINDOW_FLAGS_HIDDEN        0x00000008
#define WC_WINDOW_FLAGS_ALWAYS_ON_TOP 0x00000040
#define WC_WINDOW_FLAGS_FOCUSED       0x00000080
#define WC_WINDOW_FLAGS_DIRTY         0x00000100
#define WC_WINDOW_FLAGS_DRAG          0x00000200

#define WC_WINDOW_TITLE_MAX 256


#define WC_WINDOW_COLOR_BORDER_FOCUSED 0.5, 0.5, 0.5, 1.0
#define WC_WINDOW_COLOR_BORDER         0.2, 0.2, 0.2, 1.0
#define WC_WINDOW_COLOR_TITLEBAR_BG    0.3, 0.3, 0.3, 1.0
#define WC_WINDOW_COLOR_TITLEBAR_FG    1.0, 1.0, 1.0, 1.0


#define WC_WINDOW_MEASURES_TITLEBAR_HEIGHT   32
#define WC_WINDOW_MEASURES_TITLEBAR_FONTSIZE 14
#define WC_WINDOW_MEASURES_TITLEBAR_OFFSET_X 10
#define WC_WINDOW_MEASURES_TITLEBAR_OFFSET_Y 20



typedef struct wc_window {

        struct wc_window *parent;
        struct wc_window *root;
        struct wc_window *next;

        wc_ref_t ref;
        wc_font_t *font;

        int x;
        int y;
        int width;
        int height;

        int min_width;
        int min_height;
        int max_width;
        int max_height;

        int drag_x;
        int drag_y;

        int flags;

        char title[WC_WINDOW_TITLE_MAX];

} wc_window_t;


int wc_window_create(wc_window_t **window, wc_window_t *parent);
int wc_window_destroy(wc_window_t *window);
void wc_window_set_title(wc_window_t *window, const char *title);
void wc_window_set_position(wc_window_t *window, int x, int y);
void wc_window_set_size(wc_window_t *window, int width, int height);
void wc_window_set_min_size(wc_window_t *window, int width, int height);
void wc_window_set_max_size(wc_window_t *window, int width, int height);
void wc_window_set_flags(wc_window_t *window, int flags);
void wc_window_set_font(wc_window_t *window, wc_font_t *font);

void wc_window_enqueue(wc_window_t *window);
void wc_window_dequeue(wc_window_t *window);

void wc_window_draw(wc_window_t *window, wc_renderer_t *renderer);
void wc_window_draw_all(wc_renderer_t *renderer);

#ifdef __cplusplus
}
#endif

#endif
