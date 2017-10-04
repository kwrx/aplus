#include <ptk/Color.h>
using namespace ptk;

#define __(x, r, g, b) \
    Color Color::x (r, g, b)

__(White,       1.00, 1.00, 1.00);
__(Black,       0.00, 0.00, 0.00);
__(Red,         1.00, 0.00, 0.00);
__(Green,       0.00, 1.00, 1.00);
__(Blue,        0.00, 0.00, 1.00);
__(Gray,        0.50, 0.50, 0.50);
__(DarkGray,    0.25, 0.25, 0.25);
__(LightGray,   0.75, 0.75, 0.75);

#undef __
