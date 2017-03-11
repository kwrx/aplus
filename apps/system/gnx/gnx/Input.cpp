#include <gnx/InputController.h>
#include <gnx/Window.h>
#include <gnx/Server.h>
#include <gnx/Screen.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/sched.h>

extern "C" int clone(int (*)(void*), void*, int, void*);

using namespace std;
using namespace GNX;



InputController::InputController() {
    this->kb_fd = open(PATH_KBDEV, O_RDONLY);
    if(this->kb_fd < 0) {
        fprintf(stderr, "%s::%s(%d): could not open %s\n", __FILE__, __func__, __LINE__, PATH_KBDEV);
        exit(-1);
    }

    this->ms_fd = open(PATH_MOUSEDEV, O_RDONLY);
    if(this->ms_fd < 0) {
        fprintf(stderr, "%s::%s(%d): could not open %s\n", __FILE__, __func__, __LINE__, PATH_MOUSEDEV);
        exit(-1);
    }
}

InputController::~InputController() {
    close(this->kb_fd);
    close(this->ms_fd);
}


void InputController::PollKeyboard(std::function<void(uint8_t)> fn) {
    uint8_t ch = 0;
    if(read(this->kb_fd, &ch, sizeof(uint8_t)) == sizeof(uint8_t))
        if(likely(fn))
            fn(ch);
}

void InputController::PollMouse(std::function<void(mouse_t*)> fn) {
    mouse_t e;
    if(read(this->ms_fd, &e, sizeof(e)) == sizeof(e))
        if(likely(fn))
            fn(&e);
}


