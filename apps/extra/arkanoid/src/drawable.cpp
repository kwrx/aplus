#include "../include/drawable.h"

arkanoid::Drawable::Drawable(char ch, int start_row, int start_col, int end_row, int end_col, int color):
 d_ch(ch), d_id(0), d_start_row(start_row), d_start_col(start_col), d_end_row(end_row), d_end_col(end_col), d_color(color), visible(true){
	 
 }

 void arkanoid::Drawable::not_visible(){
     visible = false;
 }

 bool arkanoid::Drawable::is_visible(){
    return visible;
 }
