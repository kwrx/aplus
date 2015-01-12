#ifndef _ATK_H
#define _ATK_H

#include <SDL2/SDL.h>
#include <stdint.h>

#define ATK_ZINDEX_TOP					1000000
#define ATK_ZINDEX_BOTTOM				-1000000
#define ATK_ZINDEX_DEFAULT				0


#define ATK_TTF_STYLE_BOLD				TTF_STYLE_BOLD
#define ATK_TTF_STYLE_ITALIC			TTF_STYLE_ITALIC
#define ATK_TTF_STYLE_UNDERLINE			TTF_STYLE_UNDERLINE
#define ATK_TTF_STYLE_STRIKETHROUGH		TTF_STYLE_STRIKETHROUGH

#define ATK_TTF_ENCODING_ASCII			127
#define ATK_TTF_ENCODING_UTF8			255
#define ATK_TTF_ENCODING_UNICODE		65535
#define ATK_TTF_ENCODING_DEFAULT		ATK_TTF_ENCODING_UTF8


#define ATK_EVENT_INIT				1
#define ATK_EVENT_DNIT				2

#define ATK_EVENT_DEFAULT_MASK		\
	ATK_EVENT_INIT				|	\
	ATK_EVENT_DNIT




typedef void atk_image_t;
typedef void atk_font_t;
typedef void atk_render_t;

typedef SDL_Color atk_color_t;
typedef SDL_Point atk_point_t;
typedef SDL_Rect atk_rect_t;

typedef struct atk atk_t;
typedef struct atk_widget atk_widget_t;
typedef struct atk_event atk_event_t;

typedef uint64_t atk_emask_t;

struct atk {
	SDL_Surface* surface;
	SDL_Renderer* renderer;
	SDL_Rect window;

	atk_widget_t* widgets;
};


struct atk_widget {
	atk_image_t* surface;
	atk_render_t* renderer;
	atk_rect_t window;

	atk_emask_t emask;
	int zindex;

	int (*draw) (atk_t* atk, atk_widget_t* widget);
	int (*event) (atk_t* atk, atk_widget_t* widget, atk_event_t* e);

	struct atk_widget* parent;
	struct atk_widget* next;
};


struct atk_event {
	int type;
	void* param;
};





#ifdef NDEBUG
#define ATK_ASSERT(a)				a
#else
#define ATK_ASSERT(a)																\
	if((a) != 0) {																	\
		printf("ATK_ASSERT failed: %s (%s)\n", atk_error(), SDL_GetError());		\
		exit(-1);																	\
	}
#endif



#define atk_render_blit(src, srcrect, dst, dstrect)	\
	SDL_BlitSurface((SDL_Surface*) src, (SDL_Rect*) srcrect, (SDL_Surface*) dst, (SDL_Rect*) dstrect)

#define atk_render_blit_scaled(src, srcrect, dst, dstrect)	\
	SDL_BlitScaled((SDL_Surface*) src, (SDL_Rect*) srcrect, (SDL_Surface*) dst, (SDL_Rect*) dstrect)

#define atk_render_clear(r)	\
	SDL_RenderClear((SDL_Renderer*) r)

#define atk_render_line(r, x1, y1, x2, y2)	\
	SDL_RenderDrawLine((SDL_Renderer*) r, x1, y1, x2, y2)

#define atk_render_lines(r, points, count)	\
	SDL_RenderDrawLines((SDL_Renderer*) r, (const SDL_Point*) points, count)

#define atk_render_point(r, x, y)	\
	SDL_RenderDrawPoint((SDL_Renderer*) r, x, y)

#define atk_render_points(r, points, count)	\
	SDL_RenderDrawPoints((SDL_Renderer*) r, (const SDL_Point*) points, count)

#define atk_render_stroke_rect(r, rect)	\
	SDL_RenderDrawRect((SDL_Renderer*) r, (SDL_Rect*) rect)

#define atk_render_stroke_rects(r, rects, count)	\
	SDL_RenderDrawRects((SDL_Renderer*) r, (const SDL_Rect*) rects, count)

#define atk_render_fill_rect(r, rect)	\
	SDL_RenderFillRect((SDL_Renderer*) r, (SDL_Rect*) rect)

#define atk_render_fill_rects(r, rects, count)	\
	SDL_RenderFillRects((SDL_Renderer*) r, (const SDL_Rect*) rects, count)

#define atk_render_scale(r, x, y)	\
	SDL_RenderSetScale((SDL_Renderer*) r, x, y)


static inline void atk_render_color(atk_render_t* re, int r, int g, int b, int a) {
	SDL_SetRenderDrawColor((SDL_Renderer*) re, r, g, b, a);
}

#endif
