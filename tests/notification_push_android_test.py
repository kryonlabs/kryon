#!/usr/bin/env python3
"""Exercise generated production JNI with a JVM and deterministic OS/provider fakes.

No display, account, device, network or external registrations. Also compile
the callback scaffolds against the actual cached Android/provider libraries.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
from zipfile import ZipFile

REPO = Path(__file__).resolve().parent.parent


def run(*args, **kwargs):
    subprocess.run([str(arg) for arg in args], check=True, **kwargs)


FIXTURES = {
    "PushColdStartTest.java": r'''
public class PushColdStartTest {
    public static void main(String[] args) {
        // Android can start the service before any Activity/PushCallbacks.
        org.kryonlabs.notifications.push.SharedFcmService service = new org.kryonlabs.notifications.push.SharedFcmService();
        service.onNewToken("cold-token");
        service.handleIntent(new android.content.Intent("com.google.android.c2dm.intent.RECEIVE").putExtra("gcm.n.e","1"));
        if (com.google.firebase.messaging.FirebaseMessagingService.autoDisplayed != 0) throw new AssertionError("cold notification bypass");
        if (!android.content.Context.PREFS.getString("endpoint", "").isEmpty()) throw new AssertionError("cold token revived inactive scope");
        System.out.println("gplay cold service start and notification ingress passed");
    }
}
''',
    "android/content/Intent.java": r'''
package android.content;
import java.util.*;
public class Intent {
    final String action;
    public final Map<String,String> extras = new HashMap<>();
    public Intent(String action) { this.action = action; }
    public String getAction() { return action; }
    public String getStringExtra(String name) { return extras.get(name); }
    public Intent putExtra(String name, String value) { extras.put(name,value); return this; }
}
''',
    "android/content/Context.java": r'''
package android.content;
public class Context {
    public static final SharedPreferences PREFS = new SharedPreferences();
    public SharedPreferences getSharedPreferences(String name, int mode) { return PREFS; }
    public ClassLoader getClassLoader() { return getClass().getClassLoader(); }
    public Context getApplicationContext() { return this; }
}
''',
    "android/content/SharedPreferences.java": r'''
package android.content;
import java.util.*;
public class SharedPreferences {
    final Map<String,Object> data = new HashMap<>();
    public String getString(String key, String fallback) { return (String)data.getOrDefault(key, fallback); }
    public int getInt(String key, int fallback) { return (Integer)data.getOrDefault(key, fallback); }
    public long getLong(String key, long fallback) { return (Long)data.getOrDefault(key, fallback); }
    public Editor edit() { return new Editor(); }
    public class Editor {
        final Map<String,Object> pending = new HashMap<>();
        public Editor putString(String k, String v) { pending.put(k,v); return this; }
        public Editor putInt(String k, int v) { pending.put(k,v); return this; }
        public Editor putLong(String k, long v) { pending.put(k,v); return this; }
        public boolean commit() { data.putAll(pending); return true; }
    }
}
''',
    "org/json/JSONObject.java": r'''
package org.json;
import java.util.*;
public class JSONObject {
    final Map<String,Object> data = new TreeMap<>();
    public JSONObject put(String k,int v) { data.put(k,v); return this; }
    public JSONObject put(String k,long v) { data.put(k,v); return this; }
    public JSONObject put(String k,Object v) { data.put(k,v); return this; }
    public String toString() { return data.toString(); }
}
''',
    "org/unifiedpush/android/connector/data/ResolvedDistributor.java": r'''
package org.unifiedpush.android.connector.data;
public class ResolvedDistributor {
    public static class Found extends ResolvedDistributor {
        final String name;
        public Found(String name) { this.name = name; }
        public String getPackageName() { return name; }
    }
}
''',
    "org/unifiedpush/android/connector/UnifiedPush.java": r'''
package org.unifiedpush.android.connector;
import android.content.Context;
import java.util.*;
import org.unifiedpush.android.connector.data.ResolvedDistributor;
public class UnifiedPush {
    public static final List<String> installed = new ArrayList<>();
    public static String saved, preferred, lastWire;
    public static int registrations;
    public static List<String> getDistributors(Context c) { return installed; }
    public static String getSavedDistributor(Context c) { return saved; }
    public static String getAckDistributor(Context c) { return saved; }
    public static ResolvedDistributor resolveDefaultDistributor(Context c) {
        return preferred == null ? new ResolvedDistributor() : new ResolvedDistributor.Found(preferred);
    }
    public static void saveDistributor(Context c, String name) { saved = name; }
    public static void register(Context c, String wire, String message, String vapid) {
        if (!installed.contains(saved)) throw new IllegalStateException("missing distributor");
        registrations++; lastWire = wire;
    }
    public static void unregister(Context c, String wire) {}
}
''',
    "com/google/firebase/FirebaseApp.java": r'''
package com.google.firebase;
public class FirebaseApp {
    public static boolean configured;
    public static FirebaseApp getInstance() {
        if (!configured) throw new IllegalStateException("unconfigured");
        return new FirebaseApp();
    }
}
''',
    "com/google/android/gms/tasks/OnCompleteListener.java": r'''
package com.google.android.gms.tasks;
public interface OnCompleteListener<T> { void onComplete(Task<T> task); }
''',
    "com/google/android/gms/tasks/Task.java": r'''
package com.google.android.gms.tasks;
public class Task<T> {
    public boolean success;
    public T result;
    public OnCompleteListener<T> listener;
    public Task<T> addOnCompleteListener(OnCompleteListener<T> cb) { listener = cb; return this; }
    public boolean isSuccessful() { return success; }
    public T getResult() { return result; }
    public void finish(T value, boolean ok) { result = value; success = ok; listener.onComplete(this); }
}
''',
    "com/google/firebase/messaging/FirebaseMessaging.java": r'''
package com.google.firebase.messaging;
import com.google.android.gms.tasks.Task;
public class FirebaseMessaging {
    public static final FirebaseMessaging INSTANCE = new FirebaseMessaging();
    public static Task<String> pending;
    public static int requests;
    public static FirebaseMessaging getInstance() { return INSTANCE; }
    public Task<String> getToken() { requests++; pending = new Task<>(); return pending; }
}
''',
    "com/google/firebase/messaging/FirebaseMessagingService.java": r'''
package com.google.firebase.messaging;
public class FirebaseMessagingService extends android.content.Context {
    public static int autoDisplayed;
    // Mirrors the actual SDK ordering: background notification auto-display
    // precedes (and bypasses) onMessageReceived. Shared service must gate first.
    public void handleIntent(android.content.Intent intent) {
        if ("com.google.firebase.messaging.NEW_TOKEN".equals(intent.getAction())) {
            onNewToken(intent.getStringExtra("token")); return;
        }
        if ("1".equals(intent.getStringExtra("gcm.n.e")) || "1".equals(intent.getStringExtra("gcm.notification.e"))) {
            autoDisplayed++; return;
        }
        onMessageReceived(new RemoteMessage(intent.extras));
    }
    public void onNewToken(String token) {}
    public void onMessageReceived(RemoteMessage message) {}
}
''',
    "com/google/firebase/messaging/RemoteMessage.java": r'''
package com.google.firebase.messaging;
public class RemoteMessage {
    final java.util.Map<String,String> data;
    public RemoteMessage(java.util.Map<String,String> data) { this.data = data; }
    public java.util.Map<String,String> getData() { return data; }
}
''',
    "PushLifecycleTest.java": r'''
import android.content.Context;
import android.content.SharedPreferences;
import org.kryonlabs.notifications.push.PushCallbacks;
import org.unifiedpush.android.connector.UnifiedPush;
import com.google.firebase.FirebaseApp;
import com.google.firebase.messaging.FirebaseMessaging;
import com.google.android.gms.tasks.Task;
public class PushLifecycleTest {
    static class TestService extends org.kryonlabs.notifications.push.SharedFcmService {
        int received;
        @Override protected void onPushMessage(com.google.firebase.messaging.RemoteMessage message) { received++; }
    }
    static final Context C = new Context();
    static final SharedPreferences P = Context.PREFS;
    static void require(boolean pass, String name) { if (!pass) throw new AssertionError(name); }
    static String text(String key) { return P.getString(key, ""); }
    static int status() { return P.getInt("status",0); }
    static long revision() { return P.getLong("revision",0); }
    static boolean endpoint(String wire, String value) { return PushCallbacks.endpoint(C,1,wire,value,"auth","key"); }
    static void unified() {
        int first = PushCallbacks.start(C,"account","App");
        require(first == 2, "no distributor status: " + first + " stored: " + status());
        require(UnifiedPush.registrations == 0, "no false registration");
        UnifiedPush.installed.add("org.unifiedpush.distributor.sunup");
        UnifiedPush.installed.add("org.example.other");
        UnifiedPush.preferred = "org.example.other";
        require(PushCallbacks.start(C,"account","App") == 4, "start default");
        require(UnifiedPush.saved.equals(UnifiedPush.preferred), "honor system default");
        String wire = text("wire");
        require(!endpoint("previous", "https://push.example/endpoint"), "stale endpoint rejected");
        require(!PushCallbacks.endpoint(C,2,wire,"token",null,null), "other provider rejected");
        require(!endpoint(wire,"http://push.example/endpoint"), "insecure endpoint rejected");
        require(endpoint(wire,"https://push.example/endpoint"), "endpoint accepted");
        long rev = revision();
        require(endpoint(wire,"https://push.example/endpoint") && revision() == rev, "duplicate endpoint stable");
        require(endpoint(wire,"https://push.example/rotated") && revision() > rev, "endpoint rotation");
        rev = revision();
        require(PushCallbacks.endpoint(C,1,wire,"https://push.example/rotated","new-auth","new-key") && revision() > rev, "encryption key rotation");
        require(PushCallbacks.accepts(C,1,wire), "ready payload gate");
        require(PushCallbacks.start(C,"account","App") == 5 && text("wire").equals(wire), "idempotent re-registration");
        require(!PushCallbacks.cancel(C,"other"), "foreign scope cancel rejected");
        require(PushCallbacks.cancel(C,"account"), "cancel");
        require(status() == 1 && text("endpoint").isEmpty() && !PushCallbacks.accepts(C,1,wire), "cancel invalidates all data");
        require(!endpoint(wire,"https://push.example/late"), "late canceled callback");
        require(PushCallbacks.start(C,"account","App") == 4, "re-enable");
        String newWire = text("wire");
        require(!newWire.equals(wire) && !endpoint(wire,"https://push.example/stale"), "new generation");
        require(endpoint(newWire,"https://push.example/live"), "new endpoint");
        UnifiedPush.installed.clear();
        PushCallbacks.snapshot(C,"account");
        require(status() == 2 && text("endpoint").isEmpty(), "removed distributor invalidates endpoint");
        require(!endpoint(newWire,"https://push.example/stale"), "removed distributor stale callback");
        UnifiedPush.installed.add("org.unifiedpush.distributor.sunup");
        require(PushCallbacks.start(C,"account","App") == 4, "Sunup fallback");
        require(text("distributor").equals("org.unifiedpush.distributor.sunup"), "actual Sunup selected");
        String sunup = text("wire");
        PushCallbacks.failed(C,1,sunup,true);
        require(status() == 7 && text("endpoint").isEmpty(), "temporary unavailable");
        require(endpoint(sunup,"https://push.example/recovered"), "temporary recovery");
        PushCallbacks.failed(C,1,sunup,false);
        require(status() == 6 && !endpoint(sunup,"https://push.example/late"), "terminal failure");
        PushCallbacks.start(C,"new-account","App");
        require(!endpoint(sunup,"https://push.example/old-account"), "account isolation");
        require(PushCallbacks.snapshot(C,"account").equals("{}"), "foreign scope snapshot empty");
        require(PushCallbacks.start(C,"","App") == 8, "invalid scope rejected");
    }
    static void fcm() {
        require(PushCallbacks.start(C,"account","App") == 3, "FCM configuration required");
        require(FirebaseMessaging.requests == 0, "no token without existing default Firebase app");
        FirebaseApp.configured = true;
        require(PushCallbacks.start(C,"account","App") == 4, "FCM request");
        require(UnifiedPush.registrations == 0, "gplay never registers UnifiedPush");
        Task<String> stale = FirebaseMessaging.pending;
        PushCallbacks.cancel(C,"account");
        stale.finish("stale-token",true);
        require(status() == 1 && text("endpoint").isEmpty(), "canceled FCM completion rejected");
        PushCallbacks.start(C,"new-account","App");
        stale.finish("stale-token",true);
        require(text("endpoint").isEmpty(), "old FCM account rejected");
        Task<String> superseded = FirebaseMessaging.pending;
        PushCallbacks.start(C,"new-account","App");
        superseded.finish("older-request-token",true);
        require(text("endpoint").isEmpty(), "superseded request rejected");
        FirebaseMessaging.pending.finish("current-token",true);
        require(status() == 5 && text("endpoint").equals("current-token"), "token accepted");
        long rev = revision();
        PushCallbacks.start(C,"new-account","App");
        Task<String> beforeRefresh = FirebaseMessaging.pending;
        TestService service = new TestService();
        service.onNewToken("rotated-token");
        require(text("endpoint").equals("rotated-token") && revision() > rev, "token refresh persisted");
        beforeRefresh.finish("older-token",true);
        require(text("endpoint").equals("rotated-token"), "older getToken completion cannot overwrite refresh");
        beforeRefresh.finish(null,false);
        require(status() == 5, "older getToken failure cannot invalidate refresh");
        String wire = text("wire");
        android.content.Intent data = new android.content.Intent("com.google.android.c2dm.intent.RECEIVE")
            .putExtra("kryon_scope","new-account").putExtra("kryon_instance",wire);
        service.handleIntent(data);
        require(service.received == 1, "active data payload reaches app handler");
        service.handleIntent(new android.content.Intent("com.google.android.c2dm.intent.RECEIVE")
            .putExtra("kryon_scope","old-account").putExtra("kryon_instance",wire));
        service.handleIntent(new android.content.Intent("com.google.android.c2dm.intent.RECEIVE")
            .putExtra("kryon_scope","new-account").putExtra("kryon_instance","old-generation"));
        service.handleIntent(new android.content.Intent("com.google.android.c2dm.intent.RECEIVE"));
        require(service.received == 1, "foreign or missing data audience rejected");
        data.putExtra("gcm.n.e","1"); service.handleIntent(data);
        data.extras.remove("gcm.n.e"); data.putExtra("gcm.notification.e","1"); service.handleIntent(data);
        require(com.google.firebase.messaging.FirebaseMessagingService.autoDisplayed == 0, "SDK notification auto-display blocked before super");
        data.extras.remove("gcm.notification.e");
        PushCallbacks.cancel(C,"new-account");
        service.handleIntent(data);
        require(service.received == 1, "cancel blocks data ingress");
        data.putExtra("gcm.n.e","1"); service.handleIntent(data);
        require(com.google.firebase.messaging.FirebaseMessagingService.autoDisplayed == 0, "cancel blocks notification ingress");
        service.onNewToken("late-token");
        require(status() == 1 && text("endpoint").isEmpty(), "refresh cannot revive cancellation");
        PushCallbacks.start(C,"new-account","App");
        FirebaseMessaging.pending.finish(null,false);
        require(status() == 6 && text("endpoint").isEmpty(), "token failure");
    }
    public static void main(String[] args) {
        if (args[0].equals("gplay")) fcm(); else unified();
        System.out.println(args[0] + " production JNI lifecycle passed");
    }
}
''',
}


def cached_aar(group, artifact, version=None):
    base = Path.home() / ".gradle/caches/modules-2/files-2.1" / group / artifact
    pattern = f"{version}/*/*.aar" if version else "*/*/*.aar"
    candidates = sorted(base.glob(pattern))
    if not candidates:
        raise RuntimeError(f"required cached Android API missing: {group}:{artifact}")
    return candidates[-1]


def compile_real_callbacks(work):
    sdk = Path(os.environ.get("ANDROID_HOME", str(Path.home() / "Android/Sdk")))
    android_jars = sorted(sdk.glob("platforms/*/android.jar"))
    if not android_jars:
        print("Android SDK unavailable; real callback API compilation skipped", flush=True)
        return
    android = android_jars[-1]
    jars = [android]
    for group, artifact, version in [
        ("org.unifiedpush.android", "connector", "3.3.3-rc1"),
        ("com.google.firebase", "firebase-messaging", "25.1.2"),
        ("com.google.android.gms", "play-services-tasks", None),
    ]:
        try:
            aar = cached_aar(group, artifact, version)
        except RuntimeError as error:
            print(f"{error}; real callback API compilation skipped", flush=True)
            return
        jar = work / f"{artifact}.jar"
        with ZipFile(aar) as archive:
            jar.write_bytes(archive.read("classes.jar"))
        jars.append(jar)
    sources = sorted((REPO / "templates/android-notifications-push").glob("*/java/**/*.java"))
    run("javac", "-cp", os.pathsep.join(map(str,jars)), "-d", work / "real-callbacks", *sources)
    print("Callback scaffolds compile against actual Android, connector and FCM APIs", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ziran-root", type=Path, required=True)
    parser.add_argument("--work", type=Path, required=True)
    args = parser.parse_args()
    if not shutil.which("javac") or not shutil.which("java"):
        print("JDK unavailable; Android JNI callback tests skipped", flush=True)
        return
    work = args.work.resolve()
    sources = work / "java"
    for name, text in FIXTURES.items():
        path = sources / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
    compile_real_callbacks(work)
    production = REPO / "templates/android-notifications-push"
    # JVM fixtures emulate just the OS and provider APIs; the lifecycle under
    # test is production Ziran generated into the native library, unchanged.
    run("javac", "-d", work / "classes", *sorted(sources.rglob("*.java")),
        *sorted((production / "common/java").rglob("*.java")),
        *sorted((production / "gplay/java").rglob("*.java")))
    host = work / "jni_host.zi"
    host.write_text('#import "notification_push"\n#program_export "GetAndroidApp"\nTestApp :: () -> *void { return null }\n#program_export "ANativeActivity_finish"\nTestFinish :: (activity: *void) { unused activity }\n')
    for edition in ("direct", "fdroid", "gplay"):
        target = work / edition
        definitions = ["--define", "ANDROID_BUILD"]
        if edition == "gplay": definitions += ["--define", "GOOGLE_PLAY"]
        if edition == "fdroid": definitions += ["--define", "FDROID"]
        run(args.ziran_root / "build/bin/zi2c", "--no-main", *definitions,
            "--root", work, "--module-path", REPO / "src/backend", "-o", target / "c", host)
        headers = sorted({p.parent for p in (target / "c").rglob("*.h")})
        run(os.environ.get("CC", "cc"), "-std=c11", "-fPIC", "-shared", "-pthread",
            f"-I{args.ziran_root / 'include'}", *[f"-I{p}" for p in headers],
            *sorted((target / "c").rglob("*.c")), "-o", target / "libmain.so")
        # Check emitted runtime classes and native symbols as well as policy.
        if edition != "gplay":
            for source in (target / "c").rglob("*.c"):
                with source.open() as emitted:
                    assert all("com/google/" not in line and "PTokenComplete" not in line for line in emitted), edition
        run("java", "-Xcheck:jni", f"-Djava.library.path={target}", "-cp", work / "classes", "PushLifecycleTest", edition)
        if edition == "gplay":
            run("java", "-Xcheck:jni", f"-Djava.library.path={target}", "-cp", work / "classes", "PushColdStartTest")
    if shutil.which("groovy"):
        run("groovy", REPO / "tests/notification_push_gradle_test.groovy", REPO)
    else:
        print("Groovy unavailable; Gradle DSL isolation test skipped", flush=True)


if __name__ == "__main__":
    main()
