#ifndef _WC_COLOR_H
#define _WC_COLOR_H


typedef struct wc_color {
    float r;
    float g;
    float b;
    float a;
} wc_color_t;



#define WC_COLOR_TO_ARGB(color)                 \
    (uint32_t) (                                \
        (uint32_t) (color.a * 255.0f) << 24 |   \
        (uint32_t) (color.r * 255.0f) << 16 |   \
        (uint32_t) (color.g * 255.0f) << 8  |   \
        (uint32_t) (color.b * 255.0f) << 0      \
    )

#define WC_COLOR_TO_RGB24(color)                \
    (uint32_t) (                                \
        (uint32_t) (color.r * 255.0f) << 16 |   \
        (uint32_t) (color.g * 255.0f) << 8  |   \
        (uint32_t) (color.b * 255.0f) << 0      \
    )

#define WC_COLOR_TO_RGB16(color)                \
    (uint16_t) (                                \
        (uint16_t) (color.r * 31.0f) << 11 |    \
        (uint16_t) (color.g * 63.0f) << 5  |    \
        (uint16_t) (color.b * 31.0f) << 0       \
    )


#define WC_COLOR_FROM_ARGB(a, r, g, b)          \
    (wc_color_t) {                              \
        .a = (float) (a) / 255.0f,              \
        .r = (float) (r) / 255.0f,              \
        .g = (float) (g) / 255.0f,              \
        .b = (float) (b) / 255.0f               \
    }

#define WC_COLOR_FROM_RGB24(r, g, b)            \
    (wc_color_t) {                              \
        .a = 1.0f,                              \
        .r = (float) (r) / 255.0f,              \
        .g = (float) (g) / 255.0f,              \
        .b = (float) (b) / 255.0f               \
    }

#define WC_COLOR_FROM_RGB16(r, g, b)            \
    (wc_color_t) {                              \
        .a = 1.0f,                              \
        .r = (float) (r) / 31.0f,               \
        .g = (float) (g) / 63.0f,               \
        .b = (float) (b) / 31.0f                \
    }



#endif