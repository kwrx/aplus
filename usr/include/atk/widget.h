#ifndef _ATK_WIDGET_H
#define _ATK_WIDGET_H

#include <atk.h>

typedef struct atk_widget atk_widget_t;

#define ATK_WIDGET_FLAGS_ENABLED			0x01
#define ATK_WIDGET_FLAGS_FOCUS				0x02
#define ATK_WIDGET_FLAGS_TOPMOST			0x04


struct __atk_widget_private {
	void* buffer;

	void (*draw) (atk_widget_t*);
	void (*event) (atk_widget_t*, int, void*);
};

struct atk_widget {
	atk_v2si position;
	atk_v2si size;
	
	atk_color_t backcolor;
	atk_color_t forecolor;

	char* title;
	char* text;

	int flags;
	int value;

	atk_list_t* childs;
	struct atk_widget* parent;

	struct __atk_widget_private __data;
};

#endif
