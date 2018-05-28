#include "../include/game.h"

void arkanoid::Game::introduction(){

	getmaxyx(stdscr, actual_max_row, actual_max_col);
	use_color(5);
	mvprintw(actual_max_row / 2, (actual_max_col / 2) - 9, "Benvenuti su Arzanoid");
	mvprintw(round(actual_max_row * (static_cast<float>(3)/static_cast<float>(4))), round(actual_max_col * (static_cast<float>(1)/static_cast<float>(4))) - 15, "Developed by Antonio Agostino");
	mvprintw(round(actual_max_row * (static_cast<float>(3)/static_cast<float>(4))), round(actual_max_col * (static_cast<float>(3)/static_cast<float>(4))) - 12, "https://github.com/Zane83");
	use_color(2);
	mvprintw(round(actual_max_row * (static_cast<float>(9)/static_cast<float>(10))), (actual_max_col / 2) - 14, "Premi un tasto per continuare");
	refresh();
	getch();
	clear();
}

void arkanoid::Game::ending_message(){

	clear();
	use_color(5);
	mvprintw(actual_max_row / 2, (actual_max_col / 2) - 6, "Hai perso! :(");
	use_color(2);
	mvprintw(round(actual_max_row * (static_cast<float>(9)/static_cast<float>(10))), (actual_max_col / 2) - 12, "Premi un tasto per uscire");
	refresh();
	nodelay(stdscr, FALSE);
	getch();
	clear();
}

void arkanoid::Game::new_level_message(){
	clear();
	use_color(5);
	mvprintw(actual_max_row / 2, (actual_max_col / 2) - 5, "Livello: %d", level_nr + 1);
	use_color(2);
	mvprintw(round(actual_max_row * (static_cast<float>(9)/static_cast<float>(10))), (actual_max_col / 2) - 15, "Premi un tasto per continuare");
	refresh();
	nodelay(stdscr, FALSE);
	getch();
	clear();
}

void arkanoid::Game::init_screen_settings(){

	initscr();
	start_color();
	init_colors();
	cbreak();
	noecho();
	introduction();
	keypad(stdscr, TRUE);
	srand (time(NULL));
	default_max_col = 80;
	default_max_row = 24;
	health = 3;
	level_nr = 0;

}

void arkanoid::Game::init_game_settings(){

	nodelay(stdscr, TRUE);
	getmaxyx(stdscr, actual_max_row, actual_max_col);
	stop = false;
	dr_counter = 0;
	is_ball_moving = false;
	ticks = 0;
	if(level_nr < 50)
		level_nr++;
		
	bar_speed = 4;
	if(rand() % 2 == 0)
		col_mod = -1;
	else
		col_mod = 1;

	row_mod = -1;
	draw_border();
	load_level();
	draw_object(bar);
	draw_object(ball1);
	draw_health();
	draw_level_nr();

}

bool arkanoid::Game::is_scrsize_changed(){
	int n_max_row, n_max_col;
	getmaxyx(stdscr, n_max_row, n_max_col);
	if(n_max_row != actual_max_row || n_max_col != actual_max_col)
		return true;

	return false;
}

void arkanoid::Game::init_colors(){
	init_pair(1, COLOR_RED, COLOR_BLACK);
	init_pair(2, COLOR_YELLOW, COLOR_BLACK);
	init_pair(3, COLOR_WHITE, COLOR_BLACK);
	init_pair(4, COLOR_BLUE, COLOR_BLACK);
	init_pair(5, COLOR_GREEN, COLOR_BLACK);
}

void arkanoid::Game::use_color(int color){
	attrset(COLOR_PAIR(color));
}

void arkanoid::Game::draw_border(){

	use_color(4);
	for(unsigned i = 0; i < actual_max_row; i++){
		mvprintw(i, 0, "+");
		mvprintw(i, actual_max_col - 1, "+");
		refresh();
	}	

	for(unsigned j = 0; j < actual_max_col; j++){
		mvprintw(0, j, "+");
		mvprintw(actual_max_row - 1, j, "+");
		refresh();
	}
	
}

void arkanoid::Game::delete_border(){
	for(unsigned i = 0; i < actual_max_row; i++){
		mvprintw(i, 0, " ");
		mvprintw(i, actual_max_col - 1, " ");
		refresh();
	}	

	for(unsigned j = 0; j < actual_max_col; j++){
		mvprintw(0, j, " ");
		mvprintw(actual_max_row - 1, j, " ");
		refresh();
	}
}

unsigned arkanoid::Game::update_ball_position(){

	if(!is_ball_moving && g_input == ' '){

		is_ball_moving = true;

	}

	int ob_id;
	bool anglex = false, angley = false;

	if(is_ball_moving){
		if(ticks % (200000 - (level_nr * 2500)) == 0){
			Drawable *bar = &drawed[drawed.size() - 2];
			Drawable *ball = &drawed[drawed.size() - 1];

			ball_px = ball->d_start_col;
			ball_py = ball->d_start_row;
			erase_object(*ball);
			ball->d_start_col += col_mod;
			ball->d_end_col += col_mod;
			ball->d_start_row += row_mod;
			ball->d_end_row += row_mod;
			if(alevel->M[ball->d_start_row][ball->d_start_col] == 0){
				if((ball->d_start_row >= bar->d_start_row) && (ball->d_start_col >= bar->d_start_col) && (ball->d_start_col <= bar->d_end_col)){
					row_mod = -row_mod;
					ball->d_start_col += col_mod;
					ball->d_end_col += col_mod;
					ball->d_start_row += row_mod;
					ball->d_end_row += row_mod;
					draw_object(*ball);

				} else if((ball->d_start_row >= bar->d_start_row) && !((ball->d_start_col >= bar->d_start_col) && (ball->d_start_col <= bar->d_end_col))){
					if(health > 1){
						is_ball_moving = false;
						ball->d_start_col = (bar->d_end_col + bar->d_start_col) / 2;
						ball->d_end_col = (bar->d_end_col + bar->d_start_col) / 2;
						ball->d_start_row = 21;
						ball->d_end_row = 21;
						draw_object(*ball);
						if(rand() % 2 == 0)
							col_mod = -1;
						else
							col_mod = 1;
						row_mod = -1;
						health--;
						draw_health();
						
					} else {
						return 1;
					}
				} else {
					draw_object(*ball);
				}
			} else {
				ob_id = alevel->M[ball->d_start_row][ball->d_start_col];
				if(alevel->M[ball_py + row_mod][ball_px] != 0){
					row_mod = -row_mod;
				} else {
					angley = true;
				}

				if(alevel->M[ball_py][ball_px + col_mod] != 0){
					col_mod = -col_mod;
				} else {
					anglex = true;
				}

				if(anglex && angley){
					row_mod = -row_mod;
					col_mod = -col_mod;
				}

				if(ob_id != -1){
					erase_object(drawed[ob_id - 1]);
					drawed[ob_id - 1].not_visible();
					alevel->mark_element(drawed[ob_id - 1].d_start_row, drawed[ob_id - 1].d_start_col, 0);
					if(ob_id == 1){
						return 2;
					}
				}
				
				ball->d_start_col += col_mod;
				ball->d_end_col += col_mod;
				ball->d_start_row += row_mod;
				ball->d_end_row += row_mod;

				if(alevel->M[ball->d_start_row][ball->d_start_col] != -1){
					if((ball->d_start_row == bar->d_start_row) && (ball->d_start_col >= bar->d_start_col) && (ball->d_start_col <= bar->d_end_col)){
						if(anglex){
							col_mod = -col_mod;
							ball->d_start_col += col_mod;
							ball->d_end_col += col_mod;
							ball->d_start_row += row_mod;
							ball->d_end_row += row_mod;
							draw_object(*ball);
						} else if(angley){
							row_mod = -row_mod;
							ball->d_start_col += col_mod;
							ball->d_end_col += col_mod;
							ball->d_start_row += row_mod;
							ball->d_end_row += row_mod;
							draw_object(*ball);
						}
					} else if((ball->d_start_row > bar->d_start_row) && (ball->d_start_col >= bar->d_start_col) && (ball->d_start_col <= bar->d_end_col)){
						if(angley)
							row_mod = -row_mod;
						else if(anglex)
							col_mod = -col_mod;
							
						ball->d_start_col = ball_px + col_mod;
						ball->d_end_col = ball_px + col_mod;
						ball->d_start_row = ball_py + row_mod;
						ball->d_end_row = ball_py + row_mod;
					} else if((ball->d_start_row >= bar->d_start_row) && !((ball->d_start_col >= bar->d_start_col) && (ball->d_start_col <= bar->d_end_col))){
						is_ball_moving = false;
						ball->d_start_col = (bar->d_end_col + bar->d_start_col) / 2;
						ball->d_end_col = (bar->d_end_col + bar->d_start_col) / 2;
						ball->d_start_row = 21;
						ball->d_end_row = 21;
						draw_object(*ball);
						if(rand() % 2 == 0)
							col_mod = -1;
						else
							col_mod = 1;
						row_mod = -1;
						health--;
						draw_health();
					} else {
						draw_object(*ball);
					}
				} else {
					if(anglex){
						col_mod = -col_mod;
						ball->d_start_col += col_mod;
						ball->d_end_col += col_mod;
						ball->d_start_row += row_mod;
						ball->d_end_row += row_mod;
						draw_object(*ball);
					} else if(angley){
						row_mod = -row_mod;
						ball->d_start_col += col_mod;
						ball->d_end_col += col_mod;
						ball->d_start_row += row_mod;
						ball->d_end_row += row_mod;
						draw_object(*ball);
					}

				}
				
			}
		}
	}
	return 0;
}

void arkanoid::Game::update_bar_position(){

	float xdiff;
	Drawable *bar = &drawed[drawed.size() - 2];
	Drawable *ball = &drawed[drawed.size() - 1];
	if(g_input == KEY_LEFT){
		if(bar->d_start_col > bar_speed){
			erase_object(*bar);
			bar->d_start_col -= bar_speed;
			bar->d_end_col -= bar_speed;
			draw_object(*bar);
			if(!is_ball_moving){
				erase_object(*ball);
				ball->d_start_col -= bar_speed;
				ball->d_end_col -= bar_speed;
				draw_object(*ball);
			}
		} else {
			if(bar->d_start_col > 1){
				erase_object(*bar);
				bar->d_start_col --;
				bar->d_end_col --;
				draw_object(*bar);
				if(!is_ball_moving){
					erase_object(*ball);
					ball->d_start_col --;
					ball->d_end_col --;
					draw_object(*ball);
				}
			}
		}
	} else if(g_input == KEY_RIGHT){
		
		if(bar->d_end_col < default_max_col - bar_speed - 2){
			erase_object(*bar);
			bar->d_start_col += bar_speed;
			bar->d_end_col += bar_speed;
			draw_object(*bar);
			if(!is_ball_moving){
				erase_object(*ball);
				ball->d_start_col += bar_speed;
				ball->d_end_col += bar_speed;
				draw_object(*ball);
			}
		} else {
			if(bar->d_end_col < default_max_col - 2){
				erase_object(*bar);
				bar->d_start_col ++;
				bar->d_end_col ++;
				draw_object(*bar);
				if(!is_ball_moving){
					erase_object(*ball);
					ball->d_start_col ++;
					ball->d_end_col ++;
					draw_object(*ball);
				}
			}
		}
	}
}

void arkanoid::Game::draw_health(){
	use_color(4);
	mvprintw(actual_max_row - 1, (actual_max_col / 4) - 6, "Vite: %d", health);
	refresh();
}

void arkanoid::Game::draw_level_nr(){
	use_color(4);
	mvprintw(actual_max_row - 1, round(actual_max_col * (static_cast<float>(3)/static_cast<float>(4))) - 9, "Livello: %d", level_nr);
	refresh();
}

arkanoid::Game::Game() {

	alevel = new arkanoid::Level();
	init_screen_settings();
	init_game_settings();
	
}

arkanoid::Game::~Game(){
	delete alevel;
}

void arkanoid::Game::new_game(){
	new_level_message();
	alevel->init_level(level_nr);
	init_game_settings();
}

void arkanoid::Game::end_actual_game(){
	clear();
	drawed.erase(drawed.begin(), drawed.end());
	bricks.erase(bricks.begin(), bricks.end());
	ball1.d_id = 0;
	bar.d_id = 0;
}

void arkanoid::Game::get_input(){
	g_input = getch();
}

bool arkanoid::Game::is_running(){
	
	if(g_input != 27)
		return true;

	return false;
}

void arkanoid::Game::exit(){
	endwin();
}

bool arkanoid::Game::load_level(){

	unsigned br_count = 0;
	for(int k = 1; k < alevel->brick_nr; k++){
		for(int i = 1; i < arkanoid::Level::map_num_rows; i+= arkanoid::Brick::height){
			for(int j = 1; j < arkanoid::Level::map_num_cols; j+= arkanoid::Brick::width){
				if(alevel->M[i][j] == k){
					if(k == 1)
						bricks.push_back(WinningBrick(i,j));
					else
						bricks.push_back(NormalBrick(i,j));
					draw_object(bricks[br_count]);
					br_count++;
				}
			}
		}
	}
}

void arkanoid::Game::update_all(){

	int r = update_scene();
	
	if(r == 2){
		end_actual_game();
		new_game();
	} else if(r == 1){
		ending_message();
		g_input = 27;
	} else {
		g_input = 0;
		get_input();
		ticks++;
	}
}

unsigned arkanoid::Game::update_scene(){

	if(is_scrsize_changed()){
		delete_border();
		for(unsigned i = 0; i < drawed.size(); i++){
			erase_object(drawed[i]);
		}

		getmaxyx(stdscr, actual_max_row, actual_max_col);
		
		draw_border();

		for(unsigned i = 0; i < drawed.size(); i++){
			draw_object(drawed[i]);
		}
		draw_health();
		draw_level_nr();
	}

	update_bar_position();

	return update_ball_position();
}

void arkanoid::Game::draw_object(Drawable &dr){
	if(dr.is_visible()){
		if(dr.d_id == 0){
			dr_counter++;
			dr.d_id = dr_counter;
			drawed.push_back(dr);
		}
		

		float xdiff = static_cast<float>(actual_max_col) / static_cast<float>(default_max_col);
		float ydiff = static_cast<float>(actual_max_row) / static_cast<float>(default_max_row);
		unsigned start_r = round(dr.d_start_row * ydiff);
		unsigned end_r = round(dr.d_end_row * ydiff);
		unsigned start_c = round(dr.d_start_col * xdiff);
		unsigned end_c = round(dr.d_end_col* xdiff);

		use_color(dr.d_color);

		for(unsigned i = start_r; i <= end_r; i++)
		{
			for(unsigned j = start_c; j <= end_c; j++)
			{
		
				mvprintw(i, j, "%c", dr.d_ch);
				refresh();
		
			}
		}
	}
}

void arkanoid::Game::erase_object(Drawable dr){
	if(dr.is_visible()){
		float xdiff = static_cast<float>(actual_max_col) / static_cast<float>(default_max_col);
		float ydiff = static_cast<float>(actual_max_row) / static_cast<float>(default_max_row);
		unsigned start_r = round(dr.d_start_row * ydiff);
		unsigned end_r = round(dr.d_end_row * ydiff);
		unsigned start_c = round(dr.d_start_col * xdiff);
		unsigned end_c = round(dr.d_end_col* xdiff);

		for(unsigned i = start_r; i <= end_r; i++)
		{
			for(unsigned j = start_c; j <= end_c; j++)
			{
		
				mvprintw(i, j, " ");
				refresh();
		
			}
		}
	}
}