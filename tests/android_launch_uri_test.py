#!/usr/bin/env python3
"""Compile the complete scaffold against Android; run its URI/lifecycle methods
on the JVM with unrelated window, native startup and bundle I/O stubbed.
No device, display, network or URI handler is used.
"""
import os
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / "work/android-launch-uri"
JAVA = ROOT / "templates/droid/droid/app/src/main/java"
SOURCE = (JAVA / "__PACKAGE_PATH__/__ACTIVITY__.java").read_text()


def run(*args):
    subprocess.run(list(map(str, args)), check=True, timeout=120)


def method(name):
    """Extract a whole production method, including lifecycle ordering."""
    match = re.search(r"^    (?:public|private|protected)[^\n]*\b" + name
                      + r"\([^\n]*\) \{", SOURCE, re.M)
    assert match, name
    end = match.end()
    depth = 1
    while depth:
        depth += (SOURCE[end] == "{") - (SOURCE[end] == "}")
        end += 1
    return SOURCE[match.start():end]


FIXTURE = r'''
class Intent {
    String uri; boolean broken;
    Intent(String value) { uri = value; }
    String getDataString() {
        if (broken) throw new IllegalArgumentException("malformed intent");
        return uri;
    }
}
class Bundle {}
class SystemClock { static long elapsedRealtime() { return 1; } }
class NativeActivity {
    Intent intent; int creates, newIntents; boolean pollAtStartup;
    String startupUri;
    Intent getIntent() { return intent; }
    void setIntent(Intent value) { intent = value; }
    protected void onCreate(Bundle state) {
        creates++;
        if (pollAtStartup) startupUri = LaunchActivity.takeLaunchUri();
    }
    protected void onNewIntent(Intent value) { newIntents++; }
}
public class LaunchActivity extends NativeActivity {
    private long startupStarted;
    private final int[] cachedInsets = new int[9];
    int staged, flags, bars, listeners, configs;
    Intent stagedIntent;
    private void stageBundle(Intent intent) { staged++; stagedIntent = intent; }
    private void configureWindowFlags() { flags++; }
    private void configureSystemBars() { bars++; }
    private void setupInsetsListener() { listeners++; }
    private void pushDeviceConfiguration() { configs++; }
__PRODUCTION__
    static void equal(String expected, String actual) {
        if (!expected.equals(actual)) throw new AssertionError("URI mismatch");
    }
    static void require(boolean value) { if (!value) throw new AssertionError(); }
    static String repeat(String s, int n) {
        StringBuilder b = new StringBuilder();
        for (int i = 0; i < n; i++) b.append(s);
        return b.toString();
    }
    public static void main(String[] args) throws Exception {
        equal("", takeLaunchUri());
        LaunchActivity cold = new LaunchActivity();
        cold.intent = new Intent("https://example.org/invite?code=A%2FB#fragment");
        cold.pollAtStartup = true;
        cold.onCreate(null);
        equal(cold.intent.uri, cold.startupUri); // available during native startup
        equal("", takeLaunchUri());
        require(cold.creates == 1 && cold.flags == 1 && cold.bars == 1
            && cold.listeners == 1 && cold.configs == 1 && cold.staged == 1
            && cold.stagedIntent == cold.intent);

        LaunchActivity recreated = new LaunchActivity();
        recreated.intent = cold.intent;
        recreated.onCreate(new Bundle());
        equal("", takeLaunchUri()); // consumed launch is not replayed
        Intent first = new Intent("custom:raw/\uD83D\uDE80?x=%252F&y=+&z=\u00e9");
        cold.onNewIntent(first);
        require(cold.newIntents == 1 && cold.getIntent() == first
            && cold.stagedIntent == first && cold.staged == 2);
        recreated.onCreate(new Bundle()); // unread pending survives recreation
        equal(first.uri, takeLaunchUri());
        equal("", takeLaunchUri());
        cold.onNewIntent(new Intent("one"));
        cold.onNewIntent(new Intent("two"));
        equal("two", takeLaunchUri());

        cold.onNewIntent(new Intent("retained"));
        cold.onNewIntent(null);
        cold.onNewIntent(new Intent(null));
        cold.onNewIntent(new Intent(""));
        cold.onNewIntent(new Intent(repeat("x", 4097)));
        Intent broken = new Intent("ignored"); broken.broken = true;
        cold.onNewIntent(broken);
        equal("retained", takeLaunchUri());
        equal("", takeLaunchUri());
        String limit = repeat("x", 4096);
        cold.onNewIntent(new Intent(limit)); equal(limit, takeLaunchUri());
        String unicodeLimit = repeat("\uD83D\uDE80", 2048);
        cold.onNewIntent(new Intent(unicodeLimit));
        equal(unicodeLimit, takeLaunchUri());
        cold.onNewIntent(new Intent(unicodeLimit + "x"));
        equal("", takeLaunchUri()); // reject whole oversized value

        cold.onNewIntent(new Intent("concurrent"));
        java.util.concurrent.atomic.AtomicInteger consumed =
            new java.util.concurrent.atomic.AtomicInteger();
        java.util.concurrent.atomic.AtomicInteger unexpected =
            new java.util.concurrent.atomic.AtomicInteger();
        Thread[] threads = new Thread[16];
        for (int i = 0; i < threads.length; i++) {
            threads[i] = new Thread(() -> {
                String value = takeLaunchUri();
                if (value.equals("concurrent")) consumed.incrementAndGet();
                else if (!value.isEmpty()) unexpected.incrementAndGet();
            });
            threads[i].start();
        }
        for (Thread t : threads) t.join();
        require(consumed.get() == 1 && unexpected.get() == 0);
        equal("", takeLaunchUri());
        System.out.println("Android launch URI: cold/warm lifecycle, recreation, bounds, raw Unicode, consume-once concurrency passed");
    }
}
'''


def main():
    WORK.mkdir(parents=True, exist_ok=True)
    sdk = Path(os.environ.get("ANDROID_HOME", str(Path.home() / "Android/Sdk")))
    jars = sorted(sdk.glob("platforms/*/android.jar"))
    assert jars, "Android SDK android.jar required to compile complete activity"
    full = WORK / "full/org/kryonlabs/fixture/LaunchActivity.java"
    full.parent.mkdir(parents=True, exist_ok=True)
    full.write_text(SOURCE.replace("__PACKAGE__", "org.kryonlabs.fixture")
                    .replace("__ACTIVITY__", "LaunchActivity"))
    run("javac", "-cp", jars[-1], "-d", WORK / "full-classes", full,
        JAVA / "org/kryonlabs/platform/DocumentTypes.java",
        JAVA / "org/kryonlabs/notifications/NotificationReceiver.java")
    fields = [re.search(r"^    private static .*\b" + name + r" = .*;$",
                        SOURCE, re.M).group(0)
              for name in ("MAX_LAUNCH_URI_LENGTH", "pendingLaunchUri")]
    production = "\n".join(fields + [method(name) for name in (
        "captureLaunchUri", "takeLaunchUri", "onCreate", "onNewIntent")])
    fixture = WORK / "LaunchActivity.java"
    fixture.write_text(FIXTURE.replace("__PRODUCTION__", production))
    run("javac", "-d", WORK / "classes", fixture)
    run("java", "-cp", WORK / "classes", "LaunchActivity")
    print("Complete Android activity and DocumentTypes compile against " + str(jars[-1]))


if __name__ == "__main__":
    main()
