#include "../include/level.h"
arkanoid::Level::Level(){
    
    init_level(1);

}

void arkanoid::Level::init_level(unsigned difficulty){
    srand (time(NULL));
    init_map();
    brick_nr = round(difficulty * (static_cast<float>(3) / static_cast<float>(2))) + 10;
    winner_block_positioning();
    
    for(int i = 10; i < brick_nr && i <= combinations; i++){
        random_elements_positioning(i);
    }

}

void arkanoid::Level::winner_block_positioning(){
    int row = rand() % (heights - 2) + 1;
    int col = rand() % (slots - 2) + 1;
    corner_elements_positioning(1, col, row);
    corner_elements_positioning(2, col - 1, row);
    corner_elements_positioning(3, col + 1, row);
    corner_elements_positioning(4, col, row + 1);
    corner_elements_positioning(5, col, row - 1);
    corner_elements_positioning(6, col + 1, row + 1);
    corner_elements_positioning(7, col - 1, row - 1);
    corner_elements_positioning(8, col - 1, row + 1);
    corner_elements_positioning(9, col + 1, row - 1);
}

void arkanoid::Level::init_map(){
    for(int i = 1; i < map_num_rows + 4; i++){
        for(int j = 1; j < map_num_cols; j++){
            M[i][j] = 0;
        }
    }

    for(int i = 0; i < map_num_rows + 4; i++){
        M[i][0] = -1;
        M[i][map_num_cols - 1] = -1;
    }

    for(int j = 0; j < map_num_cols; j++){
        M[0][j] = -1;
    }
    
}

bool arkanoid::Level::corner_elements_positioning(int element_number, int col_shift, int row_shift){

    for(int i = row_shift; i < heights; i++){
        for(int j = col_shift; j < slots; j++){
            if(M[1 + (arkanoid::Brick::height * i)][1 + (arkanoid::Brick::width * j)] == 0){
                mark_element(1 + (i * arkanoid::Brick::height), 1 + (arkanoid::Brick::width * j), element_number);
                return true;
            }
        }  
    }

    return false;
}

void arkanoid::Level::random_elements_positioning(int element_number){

    unsigned i, slot;
    bool positionated = false;

    while(!positionated){
        i = rand() % heights;
        slot = rand() % slots;
        if(M[1 + (i * arkanoid::Brick::height)][1 + (arkanoid::Brick::width * slot)] == 0){
            mark_element(1 + (i * arkanoid::Brick::height), 1 + (arkanoid::Brick::width * slot), element_number);
            positionated = true;
        }
    }
}

void arkanoid::Level::mark_element(int start_r, int start_c, int element_number){
    
    for(int i = start_r; i < start_r + arkanoid::Brick::height; i++){
        for(int j = start_c; j < start_c + arkanoid::Brick::width; j++){
            M[i][j] = element_number;
        }
    }
}