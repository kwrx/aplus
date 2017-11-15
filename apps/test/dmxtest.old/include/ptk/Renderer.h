#pragma once

class Renderer {
    public:
        static double MarginLeft = 5.0;
        static double MarginTop = 32.0;
        static double MarginRight = 10.0;
        static double MarginBottom = 37.0;
        static double BorderRadius = 10.0;

        void DrawWindowBorders(Window* W);
};