#ifndef _ATK_H
#define _ATK_H

#include <SDL2/SDL.h>
#include <stdint.h>

typedef SDL_Surface atk_bitmap_t;
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
	SDL_Surface* surface;
	SDL_Renderer* renderer;
	SDL_Rect window;

	atk_emask_t emask;

	int (*draw) (atk_t* atk, atk_widget_t* widget);
	int (*event) (atk_t* atk, atk_widget_t* widget, atk_event_t* e);

	struct atk_widget* parent;
	struct atk_widget* next;
};


struct atk_event {
	int type;
	void* param;
};


#define ATK_EVENT_INIT				1
#define ATK_EVENT_DNIT				2

#define ATK_EVENT_DEFAULT_MASK		\
	ATK_EVENT_INIT				|	\
	ATK_EVENT_DNIT


#endif
