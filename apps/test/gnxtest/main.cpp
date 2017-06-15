#include <iostream>
#include <aplus/gnx.h>
#include <unistd.h>


using namespace std;
using namespace Gnx;


int main(int argc, char** argv) {
    LibGnx::Initialize("/tmp/gnxctl");

    Window* W = new Window(NULL, "Hello", 200, 200, 400, 400);
    W->Paint(NULL);

    sleep(5);
    LibGnx::Close(0);
}