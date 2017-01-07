import System.Zeta.*;

public class zCore extends Activity {
    
    @Override
    public void OnStart() {
        ZDebug.Print("Hello World");
        ZDebug.Print("Hel21321323");
        ZDebug.Print("System2");
        
        Activity A = Activity.Load("zDesktop");
        A.OnStop();
    }
}
