#pragma once
#include <Peach.h>


#define CONTROL_SIZE_DEFAULT_WIDTH      400
#define CONTROL_SIZE_DEFAULT_HEIGHT     300
#define CONTROL_TEXT_DEFAULT_VALUE      "UI Control"

namespace Peach::UI {
    class Control {
        public:
            inline Control(Control* Parent, string Text, uint16_t X, uint16_t Y, uint16_t W, uint16_t H) {
                _init(Parent, Text, X, Y, W, H);
            }

            inline Control(string Text, uint16_t X, uint16_t Y, uint16_t W, uint16_t H) {
                _init(NULL, Text, X, Y, W, H);
            }

            inline Control(Control* Parent, string Text) {
                _init(Parent, Text, 0, 0, CONTROL_SIZE_DEFAULT_WIDTH, CONTROL_SIZE_DEFAULT_HEIGHT);
            }

            inline Control(string Text) {
                _init(NULL, Text, 0, 0, CONTROL_SIZE_DEFAULT_WIDTH, CONTROL_SIZE_DEFAULT_HEIGHT);
            }

            inline Control() {
                _init(NULL, CONTROL_TEXT_DEFAULT_VALUE, 0, 0, CONTROL_SIZE_DEFAULT_WIDTH, CONTROL_SIZE_DEFAULT_HEIGHT);
            }

            ~Control();

        protected:
            void _init(Control* Parent, string Text, uint16_t X, uint16_t Y, uint16_t W, uint16_t H);
    };
}