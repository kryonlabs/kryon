#!/usr/bin/env python3
"""Existing Android apps retain routes/settings while gaining OS callbacks."""
from pathlib import Path
import sys
import tempfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from android_notifications_scaffold import ensure_notifications, ANDROID, NAME, RECEIVER

with tempfile.TemporaryDirectory(prefix='kryon-notification-scaffold-') as directory:
    app = Path(directory)
    manifest = app / 'droid/app/src/main/AndroidManifest.xml'
    manifest.parent.mkdir(parents=True)
    manifest.write_text('''<manifest xmlns:android="http://schemas.android.com/apk/res/android" xmlns:tools="http://schemas.android.com/tools" package="xyz.waozi.game">
<!-- Owner routing comment -->
<uses-permission android:name="android.permission.INTERNET"/>
<application android:allowBackup="false" tools:replace="android:allowBackup"><activity android:name=".Custom" android:exported="true"><intent-filter><action android:name="android.intent.action.VIEW"/><data android:scheme="game" android:host="route"/></intent-filter></activity><service android:name=".OwnerService"/></application></manifest>''')
    assert ensure_notifications(app, ROOT)
    tree = ET.parse(manifest)
    application = tree.getroot().find('application')
    assert application.get('{' + ANDROID + '}allowBackup') == 'false'
    assert application.get('{http://schemas.android.com/tools}replace') == 'android:allowBackup'
    assert application.find('activity/intent-filter/data').get('scheme') is None
    assert application.find('activity/intent-filter/data').get('{' + ANDROID + '}scheme') == 'game'
    assert application.find('service').get(NAME) == '.OwnerService'
    assert 'Owner routing comment' in manifest.read_text()
    receiver = [entry for entry in application.findall('receiver') if entry.get(NAME) == RECEIVER]
    assert len(receiver) == 1 and receiver[0].get('{' + ANDROID + '}exported') == 'false'
    before = manifest.read_bytes()
    assert not ensure_notifications(app, ROOT)
    assert manifest.read_bytes() == before
    callback = app / 'droid/app/src/main/java/org/kryonlabs/notifications/NotificationReceiver.java'
    callback.write_text('// stale callback')
    assert ensure_notifications(app, ROOT)
    assert 'native void receive' in callback.read_text()
    assert manifest.read_bytes() == before
print('Android notification scaffold: routes, permissions, callback refresh and idempotence passed')
