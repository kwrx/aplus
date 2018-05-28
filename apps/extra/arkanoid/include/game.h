#ifndef GAME_H
#define GAME_H
#include <ncurses.h>
#include <vector>
#include <cmath>
#include "drawable.h"
#include "n_brick.h"
#include "w_brick.h"
#include "bar.h"
#include "ball.h"
#include "level.h"


namespace arkanoid {

	class Game {

		int default_max_row, default_max_col, actual_max_row, actual_max_col, g_input, row_mod, col_mod;
		std::vector<arkanoid::Drawable> drawed;
		std::vector<arkanoid::Brick> bricks;
		unsigned int dr_counter, ticks, ball_px, ball_py, health, level_nr, bar_speed;
		bool is_ball_moving, stop;
		Level *alevel;
		Bar bar;
		Ball ball1;

		void introduction();

		void ending_message();

		void new_level_message();

		void init_screen_settings();

		void init_game_settings();

		void init_colors();

		bool is_scrsize_changed();

		void use_color(int color);

		void draw_border();

		void delete_border();

		unsigned update_scene();

		void update_bar_position();

		unsigned update_ball_position();

		bool load_level();

		void draw_health();

		void draw_level_nr();

		public:
		Game();

		~Game();

		bool is_running();

		void exit();

		void get_input();

		void update_all();

		void draw_object(Drawable &dr);

		void end_actual_game();

		void new_game();

		void erase_object(Drawable dr);
	};
}
#endif