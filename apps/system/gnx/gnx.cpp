#include <iostream>
#include <unistd.h>
#include <fcntl.h>

#define GSHM_INCLUDE_OPERATOR
#include <aplus/gshm.h>

#include <aplus/fbdev.h>
#include <aplus/sysconfig.h>
#include "gnx.h"

using namespace std;
using namespace GNX;

int Screen::Width = 800;
int Screen::Height = 600;
int Screen::Bpp = 32;
int Screen::Stride = 3200;
void* Screen::FrameBuffer = NULL;
