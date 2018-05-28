#ifndef LEVEL_H
#define LEVEL_H
#include <ctime>
#include <random>
#include <ncurses.h>
#include "drawable.h"
#include "../include/brick.h"
namespace arkanoid {

    class Level {
        friend class Game;

        unsigned brick_nr;
        static const unsigned map_num_rows = 19;
        static const unsigned map_num_cols = 80;
        unsigned slots = (map_num_cols - 2) / arkanoid::Brick::width;
        unsigned heights = (map_num_rows - 1) / arkanoid::Brick::height;
        unsigned combinations = slots * heights;
        int M[map_num_rows + 4][map_num_cols];
        
        void init_map();
        bool corner_elements_positioning(int element_number, int col_shift, int row_shift);
        void random_elements_positioning(int element_number);
        void mark_element(int start_r, int start_c, int element_number);
        void winner_block_positioning();

        public:
        Level();
        void init_level(unsigned difficulty);
    };
}
#endif