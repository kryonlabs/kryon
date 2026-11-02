#!/usr/bin/env python3
"""Refresh library-owned local notification callbacks in an existing scaffold."""
import argparse
from pathlib import Path
import shutil
import xml.etree.ElementTree as ET

ANDROID = 'http://schemas.android.com/apk/res/android'
NAME = '{' + ANDROID + '}name'
RECEIVER = 'org.kryonlabs.notifications.NotificationReceiver'
ACTIONS = ('BOOT_COMPLETED', 'TIME_SET', 'TIMEZONE_CHANGED', 'MY_PACKAGE_REPLACED')


def ensure_notifications(root: Path, kryon: Path) -> bool:
    manifest = root / 'droid/app/src/main/AndroidManifest.xml'
    if not manifest.is_file():
        raise ValueError(f'Missing Android manifest: {manifest}')
    tree = ET.parse(manifest, parser=ET.XMLParser(target=ET.TreeBuilder(insert_comments=True)))
    document = tree.getroot()
    application = document.find('application')
    if application is None:
        raise ValueError(f'Missing Android application: {manifest}')
    changed = False
    for permission in ('POST_NOTIFICATIONS', 'RECEIVE_BOOT_COMPLETED'):
        name = 'android.permission.' + permission
        if not any(item.get(NAME) == name for item in document.findall('uses-permission')):
            document.insert(0, ET.Element('uses-permission', {NAME: name}))
            changed = True
    receiver = next((item for item in application.findall('receiver') if item.get(NAME) == RECEIVER), None)
    if receiver is None:
        receiver = ET.SubElement(application, 'receiver', {NAME: RECEIVER})
        changed = True
    exported = '{' + ANDROID + '}exported'
    if receiver.get(exported) != 'false':
        receiver.set(exported, 'false')
        changed = True
    present = {item.get(NAME) for group in receiver.findall('intent-filter') for item in group.findall('action')}
    missing = ['android.intent.action.' + action for action in ACTIONS if 'android.intent.action.' + action not in present]
    if missing:
        group = ET.SubElement(receiver, 'intent-filter')
        for action in missing:
            ET.SubElement(group, 'action', {NAME: action})
        changed = True
    if changed:
        ET.register_namespace('android', ANDROID)
        ET.indent(tree, space='    ')
        tree.write(manifest, encoding='utf-8', xml_declaration=True)
    relative = Path('droid/app/src/main/java/org/kryonlabs/notifications/NotificationReceiver.java')
    source = kryon / 'templates/droid' / relative
    target = root / relative
    if not target.is_file() or target.read_bytes() != source.read_bytes():
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
        changed = True
    return changed


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--kryon', type=Path, required=True)
    args = parser.parse_args()
    print('Updated notification scaffold' if ensure_notifications(args.root.resolve(), args.kryon.resolve()) else 'Notification scaffold current')
