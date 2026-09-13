/*
 * GPL3 License
 *
 * Author(s):
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 *
 * Copyright (c) 2013-2019 Antonino Natale
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

/*
 * A four-function calculator, and the first thing to be built out of libui's widgets
 * rather than out of raw pixels.
 *
 * Arithmetic is immediate-execution -- an operator commits whatever is pending before it
 * takes its place -- which is what a pocket calculator does and what the single
 * accumulator below is enough for. Nothing here knows how anything is drawn: the keypad
 * is twenty buttons and a layout callback, and the display is two labels on a panel.
 */

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <aplus/input.h>
#include <aplus/ui-widgets.h>
#include <aplus/ui.h>


#define CALC_WINDOW_WIDTH  300
#define CALC_WINDOW_HEIGHT 420

//? Room for a 17-digit %.10g, a sign and a decimal point, with the typed-digit limit well
//? inside it so that appending never has to be refused for want of space.
#define CALC_ENTRY_MAX    32
#define CALC_ENTRY_DIGITS 16

#define CALC_COLUMNS 4
#define CALC_ROWS    5

#define CALC_MARGIN 12
#define CALC_GAP    8


typedef enum {

    CALC_KEY_0 = 0,
    CALC_KEY_1,
    CALC_KEY_2,
    CALC_KEY_3,
    CALC_KEY_4,
    CALC_KEY_5,
    CALC_KEY_6,
    CALC_KEY_7,
    CALC_KEY_8,
    CALC_KEY_9,

    CALC_KEY_DOT,

    CALC_KEY_ADD,
    CALC_KEY_SUB,
    CALC_KEY_MUL,
    CALC_KEY_DIV,

    CALC_KEY_EQUALS,
    CALC_KEY_CLEAR,
    CALC_KEY_DELETE,
    CALC_KEY_SIGN,
    CALC_KEY_PERCENT,

    CALC_KEY_COUNT,

} calc_key_t;


static const struct {

    calc_key_t key;

    const char* label;

    int column;
    int row;

    ui_button_style_t style;

} calc_keypad[] = {

    {CALC_KEY_CLEAR,   "C",        0, 0, UI_BUTTON_STYLE_DANGER },
    {CALC_KEY_SIGN,    "\xC2\xB1", 1, 0, UI_BUTTON_STYLE_DEFAULT},
    {CALC_KEY_PERCENT, "%",        2, 0, UI_BUTTON_STYLE_DEFAULT},
    {CALC_KEY_DIV,     "\xC3\xB7", 3, 0, UI_BUTTON_STYLE_PRIMARY},

    {CALC_KEY_7,       "7",        0, 1, UI_BUTTON_STYLE_DEFAULT},
    {CALC_KEY_8,       "8",        1, 1, UI_BUTTON_STYLE_DEFAULT},
    {CALC_KEY_9,       "9",        2, 1, UI_BUTTON_STYLE_DEFAULT},
    {CALC_KEY_MUL,     "\xC3\x97", 3, 1, UI_BUTTON_STYLE_PRIMARY},

    {CALC_KEY_4,       "4",        0, 2, UI_BUTTON_STYLE_DEFAULT},
    {CALC_KEY_5,       "5",        1, 2, UI_BUTTON_STYLE_DEFAULT},
    {CALC_KEY_6,       "6",        2, 2, UI_BUTTON_STYLE_DEFAULT},
    {CALC_KEY_SUB,     "-",        3, 2, UI_BUTTON_STYLE_PRIMARY},

    {CALC_KEY_1,       "1",        0, 3, UI_BUTTON_STYLE_DEFAULT},
    {CALC_KEY_2,       "2",        1, 3, UI_BUTTON_STYLE_DEFAULT},
    {CALC_KEY_3,       "3",        2, 3, UI_BUTTON_STYLE_DEFAULT},
    {CALC_KEY_ADD,     "+",        3, 3, UI_BUTTON_STYLE_PRIMARY},

    {CALC_KEY_DELETE,  "DEL",      0, 4, UI_BUTTON_STYLE_DEFAULT},
    {CALC_KEY_0,       "0",        1, 4, UI_BUTTON_STYLE_DEFAULT},
    {CALC_KEY_DOT,     ".",        2, 4, UI_BUTTON_STYLE_DEFAULT},
    {CALC_KEY_EQUALS,  "=",        3, 4, UI_BUTTON_STYLE_PRIMARY},
};


static struct {

    ui_view_t* view;

    ui_widget_t* display;
    ui_widget_t* expression;
    ui_widget_t* value;

    ui_widget_t* keys[CALC_KEY_COUNT];

    //? The number on screen, held as the text of it rather than as a double. A calculator
    //? has to show "3." and "0.50" while they are being typed, and neither survives a trip
    //? through a double and back.
    char entry[CALC_ENTRY_MAX];

    //? Whether `entry` is something being typed or the result of something. The difference
    //? is what the next digit does: extend it, or start over.
    bool typing;

    //? Latched until the next clear. Arithmetic on a value that is already wrong only
    //? produces a different wrong value, so nothing runs until it is cleared.
    bool error;

    double accumulator;

    calc_key_t pending;
    bool has_pending;

    bool shift;

} calc;


static bool calc_key_is_digit(calc_key_t key) {
    return key <= CALC_KEY_9;
}


static const char* calc_operator_symbol(calc_key_t key) {

    switch (key) {

        case CALC_KEY_ADD:
            return "+";

        case CALC_KEY_SUB:
            return "-";

        case CALC_KEY_MUL:
            return "\xC3\x97";

        case CALC_KEY_DIV:
            return "\xC3\xB7";

        default:
            return "";
    }
}


/* %.10g rather than a fixed number of decimals: it drops the trailing zeros a calculator
 * has no reason to show, keeps small integers exact, and falls back to an exponent for the
 * magnitudes where anything else would be a screenful of digits. Ten is short of a
 * double's seventeen on purpose -- the digits past it are the ones that turn 0.1 + 0.2
 * into 0.30000000000000004.
 */
static void calc_format(double value, char* out, size_t size) {

    if (!isfinite(value)) {

        calc.error = true;

        snprintf(out, size, "Error");

        return;
    }

    /* Negative zero is a real double and printf prints it as "-0", which is a strange
       thing to be shown after subtracting a number from itself. */
    if (value == 0.0) {
        value = 0.0;
    }

    snprintf(out, size, "%.10g", value);
}


static double calc_entry_value(void) {
    return atof(calc.entry);
}


static void calc_set_entry_value(double value) {

    calc_format(value, calc.entry, sizeof(calc.entry));

    calc.typing = false;
}


static void calc_reset(void) {

    memset(calc.entry, 0, sizeof(calc.entry));

    strcpy(calc.entry, "0");

    calc.typing      = false;
    calc.error       = false;
    calc.accumulator = 0.0;
    calc.pending     = CALC_KEY_EQUALS;
    calc.has_pending = false;
}


static void calc_refresh(void) {

    ui_label_set_text(calc.value, calc.error ? "Error" : calc.entry);


    char pending[CALC_ENTRY_MAX + 8] = {0};

    /* The line above the result, showing what is waiting on the operand being typed. It
       is the one place the accumulator is visible, and without it a chain of operations
       gives no sign that anything was carried forward. */
    if (calc.has_pending && !calc.error) {

        char left[CALC_ENTRY_MAX];

        calc_format(calc.accumulator, left, sizeof(left));

        snprintf(pending, sizeof(pending), "%s %s", left, calc_operator_symbol(calc.pending));
    }

    ui_label_set_text(calc.expression, pending);
}


static double calc_apply(double left, calc_key_t op, double right) {

    switch (op) {

        case CALC_KEY_ADD:
            return left + right;

        case CALC_KEY_SUB:
            return left - right;

        case CALC_KEY_MUL:
            return left * right;

        case CALC_KEY_DIV:

            if (right == 0.0) {

                calc.error = true;

                return 0.0;
            }

            return left / right;

        default:
            return right;
    }
}


static void calc_digit(calc_key_t key) {

    if (calc.error) {
        calc_reset();
    }

    if (!calc.typing) {

        calc.entry[0] = '\0';
        calc.typing   = true;
    }

    /* A leading zero is a placeholder, not a digit, so the first real one replaces it.
       "0" itself and "0." both have to survive, hence the check for the point. */
    if (strcmp(calc.entry, "0") == 0) {
        calc.entry[0] = '\0';
    }

    if (strlen(calc.entry) >= CALC_ENTRY_DIGITS) {
        return;
    }


    const size_t at = strlen(calc.entry);

    calc.entry[at]     = (char)('0' + (int)key);
    calc.entry[at + 1] = '\0';
}


static void calc_dot(void) {

    if (calc.error) {
        calc_reset();
    }

    if (!calc.typing) {

        strcpy(calc.entry, "0");

        calc.typing = true;
    }

    if (strchr(calc.entry, '.')) {
        return;
    }

    if (strlen(calc.entry) >= CALC_ENTRY_DIGITS) {
        return;
    }


    const size_t at = strlen(calc.entry);

    calc.entry[at]     = '.';
    calc.entry[at + 1] = '\0';
}


static void calc_delete(void) {

    if (calc.error) {

        calc_reset();

        return;
    }

    /* Only what is being typed can be taken back. Backspacing a result would have to
       invent digits for whatever it left behind. */
    if (!calc.typing) {
        return;
    }


    size_t length = strlen(calc.entry);

    if (length > 0) {
        calc.entry[--length] = '\0';
    }

    if (length == 0 || strcmp(calc.entry, "-") == 0) {
        strcpy(calc.entry, "0");
    }
}


static void calc_sign(void) {

    if (calc.error) {
        return;
    }

    if (strcmp(calc.entry, "0") == 0) {
        return;
    }


    if (calc.entry[0] == '-') {

        memmove(calc.entry, calc.entry + 1, strlen(calc.entry));

        return;
    }

    if (strlen(calc.entry) + 1 >= sizeof(calc.entry)) {
        return;
    }

    memmove(calc.entry + 1, calc.entry, strlen(calc.entry) + 1);

    calc.entry[0] = '-';
}


static void calc_percent(void) {

    if (calc.error) {
        return;
    }

    calc_set_entry_value(calc_entry_value() / 100.0);
}


static void calc_operator(calc_key_t key) {

    if (calc.error) {
        return;
    }


    /* An operator arriving on the heels of another one replaces it rather than applying
       it: "5 + -" is someone changing their mind, not an operation with no operand. */
    if (calc.has_pending && calc.typing) {

        calc.accumulator = calc_apply(calc.accumulator, calc.pending, calc_entry_value());

    } else if (!calc.has_pending) {

        calc.accumulator = calc_entry_value();
    }

    calc_set_entry_value(calc.accumulator);

    calc.pending     = key;
    calc.has_pending = true;
}


static void calc_equals(void) {

    if (calc.error) {
        return;
    }

    if (calc.has_pending) {
        calc.accumulator = calc_apply(calc.accumulator, calc.pending, calc_entry_value());
    } else {
        calc.accumulator = calc_entry_value();
    }

    calc.has_pending = false;

    calc_set_entry_value(calc.accumulator);
}


static void calc_press(ui_widget_t* widget, void* user) {

    (void)widget;

    const calc_key_t key = (calc_key_t)(intptr_t)user;


    if (calc_key_is_digit(key)) {

        calc_digit(key);

    } else {

        switch (key) {

            case CALC_KEY_DOT:
                calc_dot();
                break;

            case CALC_KEY_ADD:
            case CALC_KEY_SUB:
            case CALC_KEY_MUL:
            case CALC_KEY_DIV:
                calc_operator(key);
                break;

            case CALC_KEY_EQUALS:
                calc_equals();
                break;

            case CALC_KEY_CLEAR:
                calc_reset();
                break;

            case CALC_KEY_DELETE:
                calc_delete();
                break;

            case CALC_KEY_SIGN:
                calc_sign();
                break;

            case CALC_KEY_PERCENT:
                calc_percent();
                break;

            default:
                break;
        }
    }

    calc_refresh();
}


/* The keyboard as the same twenty keys rather than as a second set of actions: a binding
 * resolves to a button, and pressing it does exactly what clicking it does, lighting up
 * included. Shift is tracked here because the server does not own a keymap -- it forwards
 * raw KEY_* codes and leaves the question of what they mean to whoever needs characters.
 */
static calc_key_t calc_key_from_vkey(uint16_t vkey, bool shift) {

    switch (vkey) {

        case KEY_0:
        case KEY_KP0:
            return shift && vkey == KEY_0 ? CALC_KEY_COUNT : CALC_KEY_0;

        case KEY_1:
        case KEY_KP1:
            return CALC_KEY_1;

        case KEY_2:
        case KEY_KP2:
            return CALC_KEY_2;

        case KEY_3:
        case KEY_KP3:
            return CALC_KEY_3;

        case KEY_4:
        case KEY_KP4:
            return CALC_KEY_4;

        case KEY_5:
            return shift ? CALC_KEY_PERCENT : CALC_KEY_5;

        case KEY_KP5:
            return CALC_KEY_5;

        case KEY_6:
        case KEY_KP6:
            return CALC_KEY_6;

        case KEY_7:
        case KEY_KP7:
            return CALC_KEY_7;

        case KEY_8:
            return shift ? CALC_KEY_MUL : CALC_KEY_8;

        case KEY_KP8:
            return CALC_KEY_8;

        case KEY_9:
        case KEY_KP9:
            return CALC_KEY_9;

        case KEY_DOT:
        case KEY_KPDOT:
        case KEY_KPCOMMA:
            return CALC_KEY_DOT;

        case KEY_EQUAL:
            return shift ? CALC_KEY_ADD : CALC_KEY_EQUALS;

        case KEY_KPPLUS:
            return CALC_KEY_ADD;

        case KEY_MINUS:
        case KEY_KPMINUS:
            return CALC_KEY_SUB;

        case KEY_KPASTERISK:
            return CALC_KEY_MUL;

        case KEY_SLASH:
        case KEY_KPSLASH:
            return CALC_KEY_DIV;

        case KEY_ENTER:
        case KEY_KPENTER:
            return CALC_KEY_EQUALS;

        case KEY_BACKSPACE:
            return CALC_KEY_DELETE;

        case KEY_ESC:
        case KEY_DELETE:
            return CALC_KEY_CLEAR;

        default:
            return CALC_KEY_COUNT;
    }
}


static bool calc_on_key(ui_view_t* view, uint16_t vkey, bool down, void* user) {

    (void)view;
    (void)user;


    if (vkey == KEY_LEFTSHIFT || vkey == KEY_RIGHTSHIFT) {

        calc.shift = down;

        return true;
    }


    const calc_key_t key = calc_key_from_vkey(vkey, calc.shift);

    if (key == CALC_KEY_COUNT) {
        return false;
    }


    ui_widget_t* button = calc.keys[key];

    if (down) {

        /* A held key repeats, and each repeat arrives as another press. Acting only on the
           first is what keeps leaning on "5" from filling the display with fives. */
        if (ui_button_held(button)) {
            return true;
        }

        ui_button_set_held(button, true);
        ui_button_activate(button);

    } else {

        ui_button_set_held(button, false);
    }

    return true;
}


/* Everything positional, run once at startup and again after every resize. The display
 * takes a fixed share of the height and the keypad takes the rest, so the window scales
 * as a whole instead of growing a band of empty space at one end.
 */
static void calc_layout(ui_view_t* view, int width, int height, void* user) {

    (void)view;
    (void)user;


    ui_rect_t content = {

        .x      = CALC_MARGIN,
        .y      = CALC_MARGIN,
        .width  = width - 2 * CALC_MARGIN,
        .height = height - 2 * CALC_MARGIN,
    };

    if (content.width <= 0 || content.height <= 0) {
        return;
    }


    int display_height = content.height / 4;

    if (display_height < 56) {
        display_height = 56;
    }

    if (display_height > content.height / 2) {
        display_height = content.height / 2;
    }


    ui_rect_t display = {content.x, content.y, content.width, display_height};

    ui_widget_place(calc.display, display);


    ui_rect_t inner = ui_rect_inset(display, 12);

    const int expression_height = inner.height / 3;

    ui_rect_t expression = {inner.x, inner.y, inner.width, expression_height};
    ui_rect_t value      = {inner.x, inner.y + expression_height, inner.width, inner.height - expression_height};

    ui_widget_place(calc.expression, expression);
    ui_widget_place(calc.value, value);

    /* Type scales with the box it sits in rather than with the window, so the two lines
       keep their relationship to each other and to the panel at any size. */
    ui_label_set_font(calc.expression, UI_FONT_REGULAR, expression_height * 0.62);
    ui_label_set_font(calc.value, UI_FONT_BOLD, value.height * 0.66);


    ui_rect_t keypad = {

        .x      = content.x,
        .y      = display.y + display.height + 2 * CALC_GAP,
        .width  = content.width,
        .height = content.y + content.height - (display.y + display.height + 2 * CALC_GAP),
    };

    if (keypad.height <= 0) {
        return;
    }


    ui_grid_t grid = ui_grid(keypad, CALC_COLUMNS, CALC_ROWS, CALC_GAP);

    double size = ui_grid_cell(&grid, 0, 0, 1, 1).height * 0.44;

    if (size < 11.0) {
        size = 11.0;
    }

    if (size > 26.0) {
        size = 26.0;
    }


    for (size_t i = 0; i < sizeof(calc_keypad) / sizeof(calc_keypad[0]); i++) {

        ui_widget_t* button = calc.keys[calc_keypad[i].key];

        ui_widget_place(button, ui_grid_cell(&grid, calc_keypad[i].column, calc_keypad[i].row, 1, 1));
        ui_button_set_font(button, UI_FONT_BOLD, size);
    }
}


static int calc_build(ui_view_t* view) {

    calc.view = view;


    if ((calc.display = ui_panel_create(view)) == NULL) {
        return -1;
    }

    ui_panel_set_color(calc.display, ui_theme()->surface_sunken);
    ui_panel_set_radius(calc.display, 10.0);
    ui_panel_set_border(calc.display, ui_theme()->border, 1.0);


    if ((calc.expression = ui_label_create(view, "")) == NULL) {
        return -1;
    }

    ui_label_set_align(calc.expression, UI_ALIGN_RIGHT);
    ui_label_set_color(calc.expression, ui_theme()->text_muted);


    if ((calc.value = ui_label_create(view, calc.entry)) == NULL) {
        return -1;
    }

    ui_label_set_align(calc.value, UI_ALIGN_RIGHT);
    ui_label_set_color(calc.value, ui_theme()->text);


    for (size_t i = 0; i < sizeof(calc_keypad) / sizeof(calc_keypad[0]); i++) {

        /* The key is its own callback argument. Twenty buttons doing the same thing to
           different values do not need twenty callbacks. */
        ui_widget_t* button = ui_button_create(view, calc_keypad[i].label, calc_press, (void*)(intptr_t)calc_keypad[i].key);

        if (!button) {
            return -1;
        }

        ui_button_set_style(button, calc_keypad[i].style);

        calc.keys[calc_keypad[i].key] = button;
    }

    return 0;
}


int main(int argc, char** argv) {

    (void)argc;
    (void)argv;

    setvbuf(stdout, NULL, _IONBF, 0);


    calc_reset();


    ui_connection_t* conn = ui_connect(NULL, 5000);

    if (!conn) {
        fprintf(stderr, "aplus-calculator: ui_connect() failed: %s\n", strerror(errno));
        return 1;
    }


    ui_window_t* window = ui_window_create(conn, CALC_WINDOW_WIDTH, CALC_WINDOW_HEIGHT, "aplus-calculator");

    if (!window) {
        fprintf(stderr, "aplus-calculator: ui_window_create() failed: %s\n", strerror(errno));
        ui_disconnect(conn);
        return 1;
    }


    ui_view_t* view = ui_view_create(window);

    if (!view) {
        fprintf(stderr, "aplus-calculator: ui_view_create() failed: %s\n", strerror(errno));
        ui_window_destroy(window);
        ui_disconnect(conn);
        return 1;
    }


    if (calc_build(view) < 0) {
        fprintf(stderr, "aplus-calculator: cannot create the widgets\n");
        ui_view_destroy(view);
        ui_window_destroy(window);
        ui_disconnect(conn);
        return 1;
    }

    ui_view_on_key(view, calc_on_key, NULL);
    ui_view_on_layout(view, calc_layout, NULL);

    calc_refresh();


    const int status = ui_view_run(view);

    if (status < 0) {
        fprintf(stderr, "aplus-calculator: ui_view_run() failed: %s\n", strerror(errno));
    }

    ui_view_destroy(view);
    ui_window_destroy(window);
    ui_disconnect(conn);

    return status < 0 ? 1 : 0;
}
