#ifndef DRAWABLE_H
#define DRAWABLE_H
namespace arkanoid {

	class Drawable {

        friend class Game;

        char d_ch;
		unsigned int d_id;
		int d_start_row, d_start_col, d_end_row, d_end_col;
		int d_color;
		bool visible;
        
        public:
		Drawable(char ch, int start_row, int start_col, int end_row, int end_col, int color);
		void not_visible();
		bool is_visible();

    };

}
#endif