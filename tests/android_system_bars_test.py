"""Exercise the production Android system-bar policy on old and new APIs."""
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / 'templates/droid/droid/app/src/main/java/__PACKAGE_PATH__/__ACTIVITY__.java').read_text()
start = re.search(r'    private void configureSystemBarColors\(\) \{', source).start()
end = source.index('\n    private void pushDeviceConfiguration', start)
method = source[start:end]
start = source.index('    private void updateInsets(WindowInsets insets) {')
end = source.index('\n    private int inferImeBottom', start)
method += '\n' + source[start:end]
fixture = r'''
class android { static class graphics { static class Color { static final int BLACK=0xff000000; } } }
class Build {
    static class VERSION { static int SDK_INT; }
    static class VERSION_CODES { static final int P = 28, Q = 29, R = 30; }
}
class View {
    static final int SYSTEM_UI_FLAG_LAYOUT_STABLE=1, SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION=2,
        SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN=4, SYSTEM_UI_FLAG_FULLSCREEN=8,
        SYSTEM_UI_FLAG_IMMERSIVE_STICKY=16, SYSTEM_UI_FLAG_HIDE_NAVIGATION=32,
        SYSTEM_UI_FLAG_LIGHT_STATUS_BAR=64, SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR=128;
    int flags = SYSTEM_UI_FLAG_HIDE_NAVIGATION | SYSTEM_UI_FLAG_LIGHT_STATUS_BAR | SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR;
    int getSystemUiVisibility() { return flags; }
    void setSystemUiVisibility(int value) { flags=value; }
}
class WindowInsets {
    int left=0, top=52, right=0, bottom=84;
    DisplayCutout cutout;
    int getSystemWindowInsetLeft() { return left; }
    int getSystemWindowInsetTop() { return top; }
    int getSystemWindowInsetRight() { return right; }
    int getSystemWindowInsetBottom() { return bottom; }
    DisplayCutout getDisplayCutout() { return cutout; }
    Insets getInsets(int type) {
        return type==Type.ime() ? new Insets(0,0,0,300) : new Insets(left,0,right,bottom);
    }
    static class Type {
        static int statusBars() { return 1; }
        static int navigationBars() { return 2; }
        static int systemBars() { return 3; }
        static int ime() { return 4; }
    }
}
class Insets {
    int left,top,right,bottom;
    Insets(int l,int t,int r,int b) { left=l; top=t; right=r; bottom=b; }
}
class DisplayCutout {
    int getSafeInsetLeft() { return 0; }
    int getSafeInsetTop() { return 72; }
    int getSafeInsetRight() { return 0; }
    int getSafeInsetBottom() { return 0; }
}
class DisplayMetrics { float density=2; }
class Display { void getMetrics(DisplayMetrics metrics) {} }
class WindowManager { Display getDefaultDisplay() { return new Display(); } }
class Log { static void e(String tag,String message) { throw new AssertionError(message); } }
class WindowInsetsController {
    static final int APPEARANCE_LIGHT_STATUS_BARS=1, APPEARANCE_LIGHT_NAVIGATION_BARS=2;
    int hidden, shown, appearance=3;
    void setSystemBarsAppearance(int value,int mask) { appearance=(appearance & ~mask) | (value & mask); }
    void hide(int value) { hidden |= value; }
    void show(int value) { shown |= value; }
}
class Window {
    boolean fits=true, statusContrast=true, navigationContrast=true;
    int statusColor=-1, navigationColor=-1, dividerColor=-1;
    void setStatusBarColor(int value) { statusColor=value; }
    void setNavigationBarColor(int value) { navigationColor=value; }
    void setNavigationBarDividerColor(int value) { dividerColor=value; }
    void setStatusBarContrastEnforced(boolean value) { statusContrast=value; }
    void setNavigationBarContrastEnforced(boolean value) { navigationContrast=value; }
    View decor=new View();
    WindowInsetsController controller=new WindowInsetsController();
    void setDecorFitsSystemWindows(boolean value) { fits=value; }
    WindowInsetsController getInsetsController() { return controller; }
    View getDecorView() { return decor; }
}
public class SystemBarsPolicy {
    static final String TAG="test";
    int[] cachedInsets=new int[9], delivered;
    float density;
    Window window=new Window();
    Window getWindow() { return window; }
    WindowManager getWindowManager() { return new WindowManager(); }
    int inferImeBottom(int nav) { return 300; }
    void nativeSetInsets(int... values) { delivered=values; }
    void nativeSetDeviceDensity(float value) { density=value; }
__METHOD__
    static void require(boolean value) { if (!value) throw new AssertionError(); }
    public static void main(String[] args) {
        for (int api : new int[]{27,28,29,30,36}) {
            Build.VERSION.SDK_INT=api;
            SystemBarsPolicy policy=new SystemBarsPolicy();
            policy.configureSystemBars();
            require(policy.window.statusColor==android.graphics.Color.BLACK);
            require(policy.window.navigationColor==android.graphics.Color.BLACK);
            if (api>=28) require(policy.window.dividerColor==android.graphics.Color.BLACK);
            if (api>=29) require(!policy.window.statusContrast && !policy.window.navigationContrast);
            if (api<30) {
                int flags=policy.window.decor.flags;
                require((flags & View.SYSTEM_UI_FLAG_FULLSCREEN)!=0);
                require((flags & View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN)!=0);
                require((flags & View.SYSTEM_UI_FLAG_HIDE_NAVIGATION)==0);
                require((flags & (View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR | View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR))==0);
            } else {
                require(!policy.window.fits);
                require(policy.window.controller.appearance==0);
                require(policy.window.controller.hidden==WindowInsets.Type.statusBars());
                require(policy.window.controller.shown==WindowInsets.Type.navigationBars());
            }
            WindowInsets insets=new WindowInsets();
            policy.updateInsets(insets);
            require(policy.delivered[1]==0);
            require(policy.delivered[3]==84 && policy.delivered[4]==300);
            require(java.util.Arrays.equals(policy.cachedInsets,policy.delivered));
            require(policy.density==2);
            insets.left=84;
            insets.bottom=0;
            policy.updateInsets(insets);
            require(policy.delivered[0]==84 && policy.delivered[3]==0);
            if (api>=28) {
                insets.cutout=new DisplayCutout();
                policy.updateInsets(insets);
                require(policy.delivered[1]==0 && policy.delivered[6]==72);
            }
            if (api<30) {
                policy.window.decor.flags &= ~View.SYSTEM_UI_FLAG_FULLSCREEN;
                policy.updateInsets(insets);
                require(policy.delivered[1]==52);
            }
        }
        System.out.println("Android API 27/28/29/30/36: black system bars with white icons; hidden status inset removed; navigation, keyboard and cutout retained");
    }
}
'''
folder = ROOT / 'build/scratch/android-system-bars'
folder.mkdir(parents=True, exist_ok=True)
java = folder / 'SystemBarsPolicy.java'
java.write_text(fixture.replace('__METHOD__', method))
subprocess.run(['javac', '-d', str(folder), str(java)], check=True)
subprocess.run(['java', '-cp', str(folder), 'SystemBarsPolicy'], check=True)
