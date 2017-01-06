import System.Zeta.*;

public class System extends Activity {
    
    @Override
    public void OnStart() {
        //ZDebug.Print("Hello World");
        
        Activity A = Activity.Load("System2");
        A.OnStop();
    }
}
