#include <gnxserver.h>

WindowServer::WindowServer(WindowServer* p, uint16_t w, uint16_t h)
    : Parent(p), Context.Width(w), Context.Height(h) {

        
}