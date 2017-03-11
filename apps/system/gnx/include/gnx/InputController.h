#ifndef _GNX_INPUT_H
#define _GNX_INPUT_H

#include <functional>
#include <aplus/base.h>
#include <aplus/input.h>

namespace GNX {
    class InputController {
        public:
            InputController();
            ~InputController();

            void PollKeyboard(std::function<void(uint8_t)>);
            void PollMouse(std::function<void(mouse_t*)>);

        protected:
            int kb_fd;
            int ms_fd;
    };
}

#endif