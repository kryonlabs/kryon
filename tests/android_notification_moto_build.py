#!/usr/bin/env python3
"""Build the explicit notification test APK; does not inspect/control a phone."""
from pathlib import Path
import os
import subprocess
import zipfile
from toolchain import ROOT, ZIRAN, ZIRAN_ROOT

WORK=ROOT/'work/android-notification-moto'
SDK=Path('/home/wao/Android/Sdk')
TOOLS=SDK/'build-tools/37.0.0'
JAR=SDK/'platforms/android-36/android.jar'
CC=SDK/'ndk/28.2.13676358/toolchains/llvm/prebuilt/linux-x86_64/bin/armv7a-linux-androideabi21-clang'
def run(command): subprocess.run([str(x) for x in command],check=True)
def write(name,text):
 p=WORK/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(text)
if __name__=='__main__':
 run(['git','check-ignore',WORK]);WORK.mkdir(parents=True,exist_ok=True)
 # Same native JNI fixture as the JVM test, linked to real Android classes.
 run([ZIRAN,'build','--target=c','--no-main','--define','ANDROID_BUILD','--define','__arm__','--root',ROOT/'tests/notification_android',
      '--module-path',ROOT/'src/backend','--module-path',ROOT/'src/ui','--module-path',ZIRAN_ROOT/'std',
      '-o',WORK/'c',ROOT/'tests/notification_android/fixture.zi'])
 (WORK/'lib/armeabi-v7a').mkdir(parents=True,exist_ok=True)
 run([CC,'-std=c11','-shared','-fPIC','-O1','-I'+str(ZIRAN_ROOT/'include'),*sorted((WORK/'c').glob('*.c')),
      '-Wl,--no-undefined','-lm','-o',WORK/'lib/armeabi-v7a/libmain.so'])
 write('AndroidManifest.xml','''<manifest xmlns:android="http://schemas.android.com/apk/res/android" package="org.kryonlabs.notifications.test">
 <uses-sdk android:minSdkVersion="21" android:targetSdkVersion="28"/>
 <uses-permission android:name="android.permission.POST_NOTIFICATIONS"/><uses-permission android:name="android.permission.RECEIVE_BOOT_COMPLETED"/>
 <application android:label="Kryon notification local test" android:debuggable="true">
 <activity android:name=".Probe" android:exported="true"><intent-filter><action android:name="android.intent.action.MAIN"/><category android:name="android.intent.category.LAUNCHER"/></intent-filter></activity>
 <receiver android:name=".ScheduleProbe" android:exported="true"/>
 <receiver android:name="org.kryonlabs.notifications.NotificationReceiver" android:exported="false"><intent-filter><action android:name="android.intent.action.BOOT_COMPLETED"/><action android:name="android.intent.action.TIMEZONE_CHANGED"/></intent-filter></receiver>
 </application></manifest>''')
 write('java/NotificationTest.java','''import android.content.Context; import java.util.Calendar;
 public class NotificationTest {
 static {System.loadLibrary("main");} public static native void init(Context c);
 public static native boolean schedule(int hour,int minute,long notBefore,boolean suppress,int date);
 public static native boolean open();public static native boolean cancel();
 public static void run(Context c){init(c);Calendar t=Calendar.getInstance();t.add(Calendar.MINUTE,1);
 boolean ok=schedule(t.get(Calendar.HOUR_OF_DAY),t.get(Calendar.MINUTE),0,false,0);
 android.util.Log.i("KryonNotificationTest","scheduled="+ok+" hour="+t.get(Calendar.HOUR_OF_DAY)+" minute="+t.get(Calendar.MINUTE));}
 }''')
 write('java/org/kryonlabs/notifications/test/Probe.java','''package org.kryonlabs.notifications.test;
 public class Probe extends android.app.Activity { @Override public void onCreate(android.os.Bundle b){super.onCreate(b);
 try {Class.forName("NotificationTest").getMethod("run",android.content.Context.class).invoke(null,this);}
 catch(Exception e){android.util.Log.e("KryonNotificationTest","failed",e);} finish();}}
 ''')
 write('java/org/kryonlabs/notifications/test/ScheduleProbe.java','''package org.kryonlabs.notifications.test;
 public class ScheduleProbe extends android.content.BroadcastReceiver {public void onReceive(android.content.Context c,android.content.Intent i){
 try {Class.forName("NotificationTest").getMethod("run",android.content.Context.class).invoke(null,c);}
 catch(Exception e){android.util.Log.e("KryonNotificationTest","failed",e);}}}
 ''')
 receiver=ROOT/'templates/droid/droid/app/src/main/java/org/kryonlabs/notifications/NotificationReceiver.java'
 run(['javac','-source','8','-target','8','-bootclasspath',JAR,'-d',WORK/'classes',*sorted((WORK/'java').rglob('*.java')),receiver])
 run([TOOLS/'d8','--lib',JAR,'--output',WORK,*sorted((WORK/'classes').rglob('*.class'))])
 run([TOOLS/'aapt','package','-f','-M',WORK/'AndroidManifest.xml','-I',JAR,'-F',WORK/'unsigned.apk'])
 with zipfile.ZipFile(WORK/'unsigned.apk','a') as apk:
  apk.write(WORK/'classes.dex','classes.dex');apk.write(WORK/'lib/armeabi-v7a/libmain.so','lib/armeabi-v7a/libmain.so')
 if not (WORK/'test.jks').exists():
  run(['keytool','-genkeypair','-keystore',WORK/'test.jks','-storepass','android','-keypass','android','-alias','test','-keyalg','RSA','-validity','365','-dname','CN=Kryon Local Test'])
 run([TOOLS/'apksigner','sign','--ks',WORK/'test.jks','--ks-pass','pass:android','--key-pass','pass:android','--out',WORK/'notification-test.apk',WORK/'unsigned.apk'])
 print('Test APK:',WORK/'notification-test.apk')
