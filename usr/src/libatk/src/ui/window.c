#include <atk.h>
#include <stdint.h>
#include <assert.h>

#include "../config.h"

#include <atk/window.h>


/* TODO:
	* Draw Handler
	* Event Handler
*/




int atk_window_from(atk_t* atk, atk_window_t* parent, atk_window_t* window, char* title, int x, int y, int w, int h, int flags) {
	if(!atk)
		ATK_ERROR("Invalid ATK context");

	if(!window)
		ATK_ERROR("Invalid Window context");

	if(!title)
		title = "ATK App - Untitled";

	memset(window, 0, sizeof(atk_window_t));
	
	if(parent)
		atk_create_widget_from(atk, &parent->ctx, &window->ctx, w, h);
	else
		atk_create_widget(atk, &window->ctx, w, h);

	atk_window_set_title(atk, window, title);
	atk_window_set_flags(atk, window, flags);
	atk_window_move(atk, window, x, y);

	window->surface = window->ctx.surface;
	window->renderer = window->ctx.renderer;

	return 0;
}

int atk_window(atk_t* atk, atk_window_t* window, char* title, int x, int y, int w, int h, int flags) {
	return atk_window_from(atk, NULL, window, title, x, y, w, h, flags);
}

int atk_window_destroy(atk_t* atk, atk_window_t* window) {
	if(!atk)
		ATK_ERROR("Invalid ATK context");

	if(!window)
		ATK_ERROR("Invalid Window context");

	atk_destroy_widget(atk, &window->ctx);
	
	memset(window, 0, sizeof(atk_window_t));
	return 0;
}

int atk_window_show(atk_t* atk, atk_window_t* window) {
	if(!atk)
		ATK_ERROR("Invalid ATK context");

	if(!window)
		ATK_ERROR("Invalid Window context");
	
	atk_window_set_flags(atk, window, window->flags & ~ATK_WINDOW_FLAGS_HIDDEN);
	atk_window_invalidate(atk, window);

	return 0;
}

int atk_window_hide(atk_t* atk, atk_window_t* window) {
	if(!atk)
		ATK_ERROR("Invalid ATK context");

	if(!window)
		ATK_ERROR("Invalid Window context");
	
	atk_window_set_flags(atk, window, window->flags | ATK_WINDOW_FLAGS_HIDDEN);
	atk_window_invalidate(atk, window);

	return 0;
}


atk_image_t* atk_window_get_surface(atk_t* atk, atk_window_t* window) {
	if(!atk)
		ATK_ERROR_N("Invalid ATK context");

	if(!window)
		ATK_ERROR_N("Invalid Window context");
	
	return window->surface;
}

atk_render_t* atk_window_get_renderer(atk_t* atk, atk_window_t* window) {
	if(!atk)
		ATK_ERROR_N("Invalid ATK context");

	if(!window)
		ATK_ERROR_N("Invalid Window context");
	
	return window->renderer;
}

atk_widget_t* atk_window_get_context(atk_t* atk, atk_window_t* window) {
	if(!atk)
		ATK_ERROR_N("Invalid ATK context");

	if(!window)
		ATK_ERROR_N("Invalid Window context");
	
	return &window->ctx;
}

int atk_window_get_flags(atk_t* atk, atk_window_t* window, int* flags) {
	if(!atk)
		ATK_ERROR("Invalid ATK context");

	if(!window)
		ATK_ERROR("Invalid Window context");
	
	if(!flags)
		ATK_ERROR("Flags Pointer cannot be null");

	*flags = window->flags;
	return 0;
}

int atk_window_set_flags(atk_t* atk, atk_window_t* window, int flags) {
	if(!atk)
		ATK_ERROR("Invalid ATK context");

	if(!window)
		ATK_ERROR("Invalid Window context");
	

	window->flags = flags;
	return 0;
}

int atk_window_set_title(atk_t* atk, atk_window_t* window, char* title) {
	if(!atk)
		ATK_ERROR("Invalid ATK context");

	if(!window)
		ATK_ERROR("Invalid Window context");

	if(!title)
		title = "ATK App - Untitled";


	strcpy(window->title, title);
	return 0;
}

int atk_window_resize(atk_t* atk, atk_window_t* window, int w, int h) {
	if(!atk)
		ATK_ERROR("Invalid ATK context");

	if(!window)
		ATK_ERROR("Invalid Window context");

	return atk_resize_widget(atk, &window->ctx, w, h);
}

int atk_window_move(atk_t* atk, atk_window_t* window, int x, int y) {
	if(!atk)
		ATK_ERROR("Invalid ATK context");

	if(!window)
		ATK_ERROR("Invalid Window context");

	return atk_move_widget(atk, &window->ctx, x, y);
}

int atk_window_add_widget(atk_t* atk, atk_window_t* window, atk_widget_t* widget) {
	if(!atk)
		ATK_ERROR("Invalid ATK context");

	if(!window)
		ATK_ERROR("Invalid Window context");

	if(!widget)
		ATK_ERROR("Invalid Widget context");


	widget->parent = &window->ctx;
	return 0;
}

int atk_window_remove_widget(atk_t* atk, atk_window_t* window, atk_widget_t* widget) {
	if(!atk)
		ATK_ERROR("Invalid ATK context");

	if(!window)
		ATK_ERROR("Invalid Window context");

	if(!widget)
		ATK_ERROR("Invalid Widget context");


	widget->parent = NULL;
	atk_destroy_widget(atk, widget);

	return 0;
}


int atk_window_invalidate_region(atk_t* atk, atk_window_t* window, atk_rect_t* rect) {
	if(!atk)
		ATK_ERROR("Invalid ATK context");

	if(!window)
		ATK_ERROR("Invalid Window context");

	return atk_present_widget(atk, &window->ctx, rect);
}

int atk_window_invalidate(atk_t* atk, atk_window_t* window) {
	return atk_window_invalidate_region(atk, window, &window->ctx.window);
}




