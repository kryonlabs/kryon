"""Exercise the production Android system-bar policy on old and new APIs."""
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / 'templates/droid/droid/app/src/main/java/__PACKAGE_PATH__/__ACTIVITY__.java').read_text()
start = re.search(r'    private void configureSystemBars\(\) \{', source).start()
end = source.index('\n    private void pushDeviceConfiguration', start)
method = source[start:end]
fixture = r'''
class Build {
    static class VERSION { static int SDK_INT; }
    static class VERSION_CODES { static final int R = 30; }
}
class View {
    static final int SYSTEM_UI_FLAG_LAYOUT_STABLE=1, SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION=2,
        SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN=4, SYSTEM_UI_FLAG_FULLSCREEN=8,
        SYSTEM_UI_FLAG_IMMERSIVE_STICKY=16, SYSTEM_UI_FLAG_HIDE_NAVIGATION=32;
    int flags = SYSTEM_UI_FLAG_HIDE_NAVIGATION;
    int getSystemUiVisibility() { return flags; }
    void setSystemUiVisibility(int value) { flags=value; }
}
class WindowInsets {
    static class Type {
        static int statusBars() { return 1; }
        static int navigationBars() { return 2; }
    }
}
class WindowInsetsController {
    int hidden, shown;
    void hide(int value) { hidden |= value; }
    void show(int value) { shown |= value; }
}
class Window {
    boolean fits=true;
    View decor=new View();
    WindowInsetsController controller=new WindowInsetsController();
    void setDecorFitsSystemWindows(boolean value) { fits=value; }
    WindowInsetsController getInsetsController() { return controller; }
    View getDecorView() { return decor; }
}
public class SystemBarsPolicy {
    Window window=new Window();
    Window getWindow() { return window; }
__METHOD__
    static void require(boolean value) { if (!value) throw new AssertionError(); }
    public static void main(String[] args) {
        for (int api : new int[]{27,28,29,30,36}) {
            Build.VERSION.SDK_INT=api;
            SystemBarsPolicy policy=new SystemBarsPolicy();
            policy.configureSystemBars();
            if (api<30) {
                int flags=policy.window.decor.flags;
                require((flags & View.SYSTEM_UI_FLAG_FULLSCREEN)!=0);
                require((flags & View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN)!=0);
                require((flags & View.SYSTEM_UI_FLAG_HIDE_NAVIGATION)==0);
            } else {
                require(!policy.window.fits);
                require(policy.window.controller.hidden==WindowInsets.Type.statusBars());
                require(policy.window.controller.shown==WindowInsets.Type.navigationBars());
            }
        }
        System.out.println("Android API 27/28/29/30/36: status bar hidden and navigation retained");
    }
}
'''
folder = ROOT / 'build/scratch/android-system-bars'
folder.mkdir(parents=True, exist_ok=True)
java = folder / 'SystemBarsPolicy.java'
java.write_text(fixture.replace('__METHOD__', method))
subprocess.run(['javac', '-d', str(folder), str(java)], check=True)
subprocess.run(['java', '-cp', str(folder), 'SystemBarsPolicy'], check=True)
