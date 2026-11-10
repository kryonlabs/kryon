#!/usr/bin/env python3
"""Execute the real Ziran scheduler against Java Calendar and OS test doubles.
This verifies JNI signatures and persistent state transitions, not Android OS delivery.
"""
import os
from pathlib import Path
import subprocess
import shutil
from toolchain import UI_MODULE_ARGS, ROOT, ZIRAN, ZIRAN_ROOT

WORK = ROOT / 'work/android-notification-jvm'
FILES = {
'org/json/JSONArray.java': r'''package org.json; import java.util.*; public class JSONArray {
 private final List<Integer> values=new ArrayList<>(); public JSONArray(){}
 public JSONArray(String s){String t=s.substring(1,s.length()-1).trim();if(!t.isEmpty())for(String v:t.split(","))values.add(Integer.parseInt(v.trim()));}
 public JSONArray put(int v){values.add(v);return this;}public int length(){return values.size();}public int getInt(int i){return values.get(i);}public String toString(){return values.toString();}}
''',
'android/os/Build.java': '''package android.os; public class Build { public static class VERSION { public static int SDK_INT=33; }}''',
'android/net/Uri.java': '''package android.net; public class Uri { public final String value; Uri(String v){value=v;} public static String encode(String s){return s;} public static Uri parse(String s){return new Uri(s);} }''',
'android/content/Intent.java': '''package android.content; import android.net.Uri; public class Intent {
 public String key; public Uri uri;
 public Intent setClassName(String p,String c){return this;} public Intent setAction(String a){return this;}
 public Intent setData(Uri d){uri=d; return this;} public Intent putExtra(String k,String v){key=v; return this;}
 public String getStringExtra(String k){return key;}
}''',
'android/content/SharedPreferences.java': '''package android.content; import java.util.*; public class SharedPreferences {
 public final Map<String,Object> data=new HashMap<>(); public int commits=0;
 public Map<String,?> getAll(){return new HashMap<>(data);} public boolean contains(String s){return data.containsKey(s);}
 public int getInt(String s,int d){return (Integer)data.getOrDefault(s,d);}
 public long getLong(String s,long d){return (Long)data.getOrDefault(s,d);}
 public String getString(String s,String d){return (String)data.getOrDefault(s,d);}
 public Editor edit(){return new Editor();} public class Editor {
 private final Map<String,Object> changes=new HashMap<>(); private final Set<String> removed=new HashSet<>();
 public Editor putInt(String s,int v){changes.put(s,v);return this;} public Editor putLong(String s,long v){changes.put(s,v);return this;}
 public Editor putString(String s,String v){changes.put(s,v);return this;} public Editor putBoolean(String s,boolean v){changes.put(s,v);return this;}
 public Editor clear(){removed.addAll(data.keySet());return this;} public Editor remove(String s){removed.add(s);return this;} public boolean commit(){for(String s:removed)data.remove(s);data.putAll(changes);commits++;return true;}
 }}''',
'android/content/Context.java': '''package android.content; import java.util.*; import android.app.*; import android.content.pm.*;
 public class Context { public final Map<String,SharedPreferences> preferences=new HashMap<>();
 public final AlarmManager alarm=new AlarmManager(); public final NotificationManager notification=new NotificationManager(); public boolean granted=true;
 public SharedPreferences getSharedPreferences(String s,int m){return preferences.computeIfAbsent(s,k->new SharedPreferences());}
 public Object getSystemService(String s){return s.equals("alarm")?alarm:notification;}
 public String getPackageName(){return "fixture";} public ApplicationInfo getApplicationInfo(){return new ApplicationInfo();}
 public PackageManager getPackageManager(){return new PackageManager();} public int checkSelfPermission(String p){return granted?0:-1;}
 }''',
'android/content/pm/ApplicationInfo.java': '''package android.content.pm; public class ApplicationInfo {public CharSequence loadLabel(PackageManager p){return "Fixture";}}''',
'android/content/pm/PackageManager.java': '''package android.content.pm; import android.content.Intent; public class PackageManager {public Intent getLaunchIntentForPackage(String p){return new Intent();}}''',
'android/app/Activity.java': '''package android.app; public class Activity extends android.content.Context {public int requests=0; public void requestPermissions(String[] p,int r){requests++;}}''',
'android/app/PendingIntent.java': '''package android.app; import android.content.*; public class PendingIntent {public Intent intent; public static PendingIntent getBroadcast(Context c,int r,Intent i,int f){PendingIntent p=new PendingIntent();p.intent=i;return p;} public static PendingIntent getActivity(Context c,int r,Intent i,int f){return getBroadcast(c,r,i,f);}}''',
'android/app/AlarmManager.java': '''package android.app; public class AlarmManager {public long at=0;public int arms=0;public PendingIntent pending; public void setAndAllowWhileIdle(int t,long at,PendingIntent p){this.at=at;pending=p;arms++;} public void set(int t,long at,PendingIntent p){setAndAllowWhileIdle(t,at,p);} public void cancel(PendingIntent p){pending=null;}}''',
'android/app/NotificationChannel.java': '''package android.app; public class NotificationChannel {public int importance;public NotificationChannel(String id,CharSequence name,int importance){this.importance=importance;} public int getImportance(){return importance;}}''',
'android/app/Notification.java': '''package android.app; import android.content.Context; public class Notification {public static class Builder {
 public Builder(Context c){} public Builder(Context c,String s){} public Builder setSmallIcon(int i){return this;} public Builder setContentTitle(CharSequence s){return this;} public Builder setContentText(CharSequence s){return this;} public Builder setAutoCancel(boolean b){return this;} public Builder setContentIntent(PendingIntent p){return this;} public Notification build(){return new Notification();}}}''',
'android/app/NotificationManager.java': '''package android.app; public class NotificationManager {public NotificationChannel channel;public boolean allowed=true;public String lastTag="";public int lastId=0;public int posted=0,cancelled=0;public boolean areNotificationsEnabled(){return allowed;} public NotificationChannel getNotificationChannel(String s){return channel;}public void createNotificationChannel(NotificationChannel c){if(channel==null)channel=c;} public void notify(String tag,int id,Notification n){posted++;}public void cancel(String tag,int id){lastTag=tag;lastId=id;cancelled++;}}''',
'NotificationTest.java': '''import android.app.*; import android.content.*; import java.util.*;
public class NotificationTest {
 static {System.load(System.getProperty("fixture.library"));} static native void init(Context c);
 static native boolean schedule(int hour,int minute,long notBefore,boolean suppress,int exactDate);
 static native void request();static native boolean dates(int[] dates);static native boolean open();static native boolean openId(int id);static native boolean cycleId(int id);static native boolean cancelUnknown(); static native boolean cancel(); static native void fire(Context c,Intent i);
 static void require(boolean b,String s){if(!b)throw new AssertionError(s);}
 static int date(Calendar c){return c.get(Calendar.YEAR)*10000+(c.get(Calendar.MONTH)+1)*100+c.get(Calendar.DAY_OF_MONTH);}
 public static void main(String[] args){
  TimeZone.setDefault(TimeZone.getTimeZone("UTC")); Activity c=new Activity();init(c);
  Calendar now=Calendar.getInstance(); int today=date(now); int hour=now.get(Calendar.HOUR_OF_DAY),minute=now.get(Calendar.MINUTE);
  require(schedule(19,0,0,true,0),"initial schedule"); require(c.alarm.at>System.currentTimeMillis(),"future alarm");
  require(c.getSharedPreferences("kryon.daily.index",0).getAll().size()==1,"one rule");
  require(schedule(20,0,0,true,0),"replace");require(c.getSharedPreferences("kryon.daily.index",0).getAll().size()==1,"replacement identity");
  require(open(),"open"); SharedPreferences prefs=c.getSharedPreferences("kryon.daily.slot.0",0);require(prefs.getInt("opened",0)==today,"opened persisted");
  int commits=prefs.commits,arms=c.alarm.arms;require(open(),"repeat open");require(prefs.commits==commits&&c.alarm.arms==arms,"idempotent date");
  Intent alarm=new Intent().putExtra("key","game.chest:1");fire(c,alarm);require(c.notification.posted==0,"opened suppresses");
  // Simulate persisted rule from a previous process, due now, no activity callbacks.
  prefs.edit().putInt("suppress",0).putInt("hour",hour).putInt("minute",minute).commit();
  c.granted=false;request();request();require(c.requests==1,"permission refusal not reprompted");fire(c,alarm);require(c.notification.posted==0&&prefs.getInt("delivered",0)==0,"denied does not mark delivered");
  c.granted=true;fire(c,alarm);require(c.notification.posted==1&&prefs.getInt("delivered",0)==today,"delivery and durable duplicate guard");
  fire(c,alarm);require(c.notification.posted==1,"duplicate alarm ignored");
  prefs.data.remove("delivered");c.notification.channel.importance=0;
  fire(c,alarm);require(c.notification.posted==1&&prefs.getInt("delivered",0)==0,"blocked channel respected");
  c.notification.channel.importance=3;prefs.edit().putInt("delivered",today).commit();
  c.alarm.pending=null;fire(c,new Intent());require(c.alarm.pending!=null,"boot rearm from preferences");
  require(cancel(),"cancel");require(c.getSharedPreferences("kryon.daily.index",0).getAll().isEmpty(),"cancel persistent index");
  fire(c,alarm);require(c.notification.posted==1,"stale cancelled alarm ignored");
  require(open(),"mark cancelled rule");require(c.alarm.pending==null,"mark never resurrects cancel");
  // Exercise platform21 fallback and local calendar/DST normalization.
  android.os.Build.VERSION.SDK_INT=21; TimeZone.setDefault(TimeZone.getTimeZone("America/New_York"));
  Calendar future=Calendar.getInstance();future.set(2030,Calendar.MARCH,10,0,0,0);future.set(Calendar.MILLISECOND,0);
  require(schedule(2,30,future.getTimeInMillis(),false,0),"API21 set and DST");
  Calendar expected=(Calendar)future.clone();expected.set(Calendar.HOUR_OF_DAY,2);expected.set(Calendar.MINUTE,30);
  require(c.alarm.at==expected.getTimeInMillis(),"DST local normalization");
  require(cancel(),"cancel final");TimeZone.setDefault(TimeZone.getTimeZone("UTC"));require(!schedule(hour,minute,0,false,today),"elapsed one shot rejected");
  // Interrupted cleanup cannot post under a default or different identity.
  SharedPreferences index=c.getSharedPreferences("kryon.daily.index",0);
  index.edit().putInt("stale:7",1).commit();prefs.edit().clear().commit();
  int posted=c.notification.posted,oldArms=c.alarm.arms;fire(c,new Intent());
  require(c.notification.posted==posted&&c.alarm.arms==oldArms&&!index.contains("stale:7"),"incomplete active rule rejected");
  prefs.edit().putString("owner","prior:owner").putInt("delivered",99999999).putInt("opened",99999999).commit();
  require(schedule(19,0,0,false,0),"reuse abandoned slot");
  require(prefs.getInt("delivered",0)==0&&prefs.getInt("opened",0)==0,"slot reuse clears prior owner highwaters");
  require(cancel(),"cancel reuse");
  // One finite sequence reserves one alarm and supports the maximum bound.
  Calendar sequence=Calendar.getInstance();sequence.add(Calendar.DATE,1);int[] dates=new int[3660];
  for(int i=0;i<dates.length;i++){dates[i]=date(sequence);sequence.add(Calendar.DATE,1);}
  require(dates(dates),"maximum finite queue");require(index.getAll().size()==1,"one queue one rule");
  require(c.getSharedPreferences("kryon.daily.slot.0",0).getString("dates","").length()>30000,"full sequence persisted");
  require(cancel(),"cancel queue");
  int accepted=0;for(int i=0;i<1000;i++)if(openId(i))accepted++;
  require(accepted==64,"open-only quota exactly64");
  require(c.preferences.size()<=67,"bounded fixed slot preference files");
  int before=c.preferences.size();for(int i=0;i<100;i++)require(cycleId(i),"schedule/cancel cycle");
  require(c.preferences.size()<=before+1,"cancel cycles reuse slots");
  require(cancelUnknown(),"unknown own rule cancel");
  require(c.notification.lastTag.equals("hosted:unknown:rule")&&c.notification.lastId==79,"unknown cancel cannot hit hostdefault");
  System.out.println("Android notification JNI: replacement, permission, open, duplicate, reboot, cancellation, API21 and DST passed");
 }
}'''
}

def run(command, **kwargs):
    subprocess.run([str(x) for x in command], check=True, **kwargs)

if __name__ == '__main__':
    run(['git', 'check-ignore', str(WORK)])
    WORK.mkdir(parents=True, exist_ok=True)
    for name in ("c", "classes"):
        shutil.rmtree(WORK / name, ignore_errors=True)
    for name, body in FILES.items():
        path=WORK/'java'/name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(body)
    run([ZIRAN, 'build','--target=c','--no-main','--define','ANDROID_BUILD','--root',ROOT/'tests/notification_android',
         '--module-path',ROOT/'src/backend',*UI_MODULE_ARGS,'--module-path',ZIRAN_ROOT/'std',
         '-o',WORK/'c',ROOT/'tests/notification_android/fixture.zi'])
    run([os.environ.get('CC','cc'),'-std=c11','-shared','-fPIC','-O1','-I'+str(ZIRAN_ROOT/'include'),
         *sorted((WORK/'c').glob('*.c')),'-lpthread','-lm','-o',WORK/'libfixture.so'])
    run(['javac','-d',WORK/'classes',*sorted((WORK/'java').rglob('*.java'))])
    run(['java','-Xcheck:jni','-Dfixture.library='+str(WORK/'libfixture.so'),'-cp',WORK/'classes','NotificationTest'])
