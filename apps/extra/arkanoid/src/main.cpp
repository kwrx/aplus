#include <iostream>
#include "../include/game.h"

int main(){

	arkanoid::Game g;
	
	while(g.is_running()){
		g.update_all();
	}
	g.exit();	
}