

public class Game {

	public int X;
	public int Y;

	public void DrawSprite() {
		AVM.VMDebug.Print("DrawSprite ");
	}

	

	static void main(String args[]) throws Exception {
		Game G[] = new Game[10];
		for(int i = 0; i < G.length; i++)
			G[i] = new Game();
	
		int e = 0;
		do {
			for(int i = 0; i < G.length; i++) {
				G[i].X++;
				G[i].Y++;

				G[i].DrawSprite();
			}

			e++;

			if(e > 100)
				break;			
			//	throw new Exception("Good Work!");
		} while(true);
	}
}
