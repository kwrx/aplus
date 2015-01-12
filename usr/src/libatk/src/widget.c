#include <atk.h>
#include <stdint.h>
#include <assert.h>

#include "config.h"


int atk_absolute_position(atk_t* atk, atk_widget_t* widget, int* x, int* y) {
	if(!atk)
		ATK_ERROR("Invalid ATK context");

	if(!widget)
		ATK_ERROR("Invalid Widget context");

	atk_widget_t* tmp = widget->parent;
	while(tmp) {
		*x += tmp->window.x;
		*y += tmp->window.y;		

		tmp = tmp->next;
	}

	return 0;
}


int atk_relative_position(atk_t* atk, atk_widget_t* widget, int* x, int* y) {
	if(!atk)
		ATK_ERROR("Invalid ATK context");

	if(!widget)
		ATK_ERROR("Invalid Widget context");

	atk_widget_t* tmp = widget->parent;
	while(tmp) {
		*x -= tmp->window.x;
		*y -= tmp->window.y;		

		tmp = tmp->next;
	}

	return 0;
}


int atk_create_widget_from(atk_t* atk, atk_widget_t* parent, atk_widget_t* widget, int width, int height) {
	if(!atk)
		ATK_ERROR("Invalid ATK context");

	if(!widget)
		ATK_ERROR("Invalid Widget context");

	if(width < 0 || height < 0 || width > atk->window.w || height > atk->window.h)
		ATK_ERROR("Invalid size of widget");

	memset(widget, 0, sizeof(atk_widget_t));
	widget->surface = SDL_CreateRGBSurface (
		0,
		width,
		height,
		atk->surface->format->BitsPerPixel,
		ATK_MASK_ARGB
	);

	if(widget->surface == NULL)
		ATK_ERROR("SDL_CreateRGBSurface failed");

	widget->renderer = SDL_CreateSoftwareRenderer(widget->surface);
	widget->parent = parent;
	widget->next = NULL;
	widget->draw = NULL;
	widget->event = NULL;
	widget->emask = ATK_EVENT_DEFAULT_MASK;
	widget->window.x = 0;
	widget->window.y = 0;
	widget->window.w = width;
	widget->window.h = height;


	widget->next = atk->widgets;
	atk->widgets = widget;

	__atk_initialize_surface(widget->surface);
	__atk_initialize_renderer(widget->renderer);

	return 0;
}

int atk_create_widget(atk_t* atk, atk_widget_t* widget, int width, int height) {
	return atk_create_widget_from(atk, NULL, widget, width, height);
}

int atk_resize_widget(atk_t* atk, atk_widget_t* widget, int width, int height) {
	if(!atk)
		ATK_ERROR("Invalid ATK context");

	if(!widget)
		ATK_ERROR("Invalid Widget context");

	if(width < 0 || height < 0 || width > atk->window.w || height > atk->window.h)
		ATK_ERROR("Invalid size of widget");


	ATK_ERROR("Resize not supported for now");
}

int atk_move_widget(atk_t* atk, atk_widget_t* widget, int x, int y) {
	if(!atk)
		ATK_ERROR("Invalid ATK context");

	if(!widget)
		ATK_ERROR("Invalid Widget context");

	widget->window.x = x;
	widget->window.y = y;

	return 0;
}

int atk_destroy_widget(atk_t* atk, atk_widget_t* widget) {
	if(!atk)
		ATK_ERROR("Invalid ATK context");

	if(!widget)
		ATK_ERROR("Invalid Widget context");


	atk_widget_t* tmp = atk->widgets;
	while(tmp->next) {
		if(tmp->next == widget) {
			tmp->next = widget->next;
			break;
		}

		tmp = tmp->next;
	}

	SDL_DestroyRenderer(widget->renderer);
	SDL_FreeSurface(widget->surface);

	memset(widget, 0, sizeof(atk_widget_t));
	return 0;
}



int atk_draw_widget(atk_t* atk, atk_widget_t* widget) {
	if(!widget)
		ATK_ERROR("Invalid Widget context");

	if(!widget->draw)
		ATK_ERROR("No draw handler for this widget");

	return widget->draw(atk, widget);
}

int atk_raise_event_widget(atk_t* atk, atk_widget_t* widget, atk_event_t* e) {
	if(!widget)
		ATK_ERROR("Invalid Widget context");

	if(!widget->event)
		ATK_ERROR("No event handler for this widget");

	return widget->event(atk, widget, e);
}

int atk_present_widget(atk_t* atk, atk_widget_t* widget, atk_rect_t* rect) {
	SDL_Rect w;
	w.x = rect->x;
	w.y = rect->y;
	w.w = rect->w;
	w.h = rect->h;
	atk_absolute_position(atk, widget, &w.x, &w.y);

	atk_render_blit(widget->surface, NULL, atk->surface, &w);
	return 0;
}
