#!/usr/bin/env python3
"""Fail when a normal or F-Droid Android build holds a Google library.

Every app ships the normal APK, -fdroid and -gplay; only -gplay may contain
Google libraries or services: Play services, Firebase, Play Billing, Play
Core, Play licensing, install referrer, ads, ML Kit. Open-source libraries
that only carry Google's name (AndroidX, Material, ZXing, Protobuf, Guava)
are not Google services.

    check-google-free.py [--source DIR] [--expect-google] [PACKAGE...]

Each PACKAGE (an APK, or an App Bundle) is searched entry by entry: dex
class names, the binary manifest, resources, native libraries and
META-INF. --source checks DIR's Gradle files the way F-Droid's scanner
does: a dependency or plugin line naming a Google library fails unless its
configuration belongs to the Google Play edition (gplayImplementation) or
the file does (any path part named gplay, such as android-gplay.gradle).
--expect-google checks a -gplay package the other way round, proving the
search still finds Play Billing.
"""
import argparse
import re
import subprocess
import sys
import zipfile
from pathlib import Path

GOOGLE = (
    'com.google.android.gms', 'com.google.gms', 'com.google.firebase', 'com.google.android.play',
    'com.android.billingclient', 'com.android.vending.billing', 'com.android.vending.BILLING',
    'com.android.vending.licensing', 'com.google.android.vending.licensing', 'com.android.vending.CHECK_LICENSE',
    'com.android.installreferrer', 'com.google.android.c2dm', 'com.google.android.datatransport',
    'com.google.android.finsky', 'com.google.android.ads', 'com.google.ads', 'com.google.android.libraries',
    'com.google.mlkit',
)
# Library code shows as class names: dex type descriptors and the JNI class
# paths of native code. A bare package name in code is no library (FOSS
# UnifiedPush asks whether com.google.android.gms is installed), but in the
# manifest it declares Google services and permissions.
CODE = [(name, b'L' + name.replace('.', '/').encode() + b'/') for name in GOOGLE]
NATIVE = [(name, name.replace('.', '/').encode() + b'/') for name in GOOGLE]
MANIFEST = [(name, form) for name in GOOGLE for form in (name.encode(), name.encode('utf-16-le'))]
# Libraries also leave version and properties files in META-INF or the root.
MARKER = re.compile('|'.join([re.escape(name) for name in GOOGLE] + ['firebase', 'play-services', 'billingclient']),
                    re.IGNORECASE)
DECLARATION = re.compile(r'\s*(?:(?P<scope>[a-z]*?)(?:implementation|api|compileonly|runtimeonly|compile|provided|apk|'
                         r'classpath|kapt|annotationprocessor)\b|id\b|apply\s*\(?\s*plugin)', re.IGNORECASE)


def needles(name):
    """What in a package entry would show a Google library."""
    if name.endswith('.dex'):
        return CODE
    if name.endswith('.so'):
        return NATIVE
    if name.endswith('AndroidManifest.xml'):
        return MANIFEST
    return []


def package_findings(path):
    findings = []
    with zipfile.ZipFile(path) as archive:
        for entry in archive.infolist():
            if entry.is_dir():
                continue
            search = needles(entry.filename)
            data = archive.read(entry) if search else b''
            found = sorted({name for name, form in search if form in data})
            if not found and entry.filename.count('/') <= 2 and not entry.filename.startswith(('assets/', 'base/assets/')) \
                    and MARKER.search(entry.filename):
                found = ['a library marker']
            if found:
                findings.append(f'{path}: {entry.filename}: {", ".join(found)}')
    return findings


def gplay_only(path, root):
    return any('gplay' in part.lower() for part in path.relative_to(root).parts)


def gradle_files(root):
    """The Gradle files a checkout holds: Git's tracked files when it is
    one, so local backups and generated projects are left out."""
    listing = subprocess.run(['git', '-C', root, 'ls-files', '-z', '--', '*.gradle', '*.gradle.kts'],
                             capture_output=True)
    if listing.returncode == 0:
        return [root / name for name in listing.stdout.decode().split('\0') if name]
    return [path for path in root.rglob('*') if path.name.endswith(('.gradle', '.gradle.kts'))]


def source_findings(root):
    findings = []
    for path in sorted(gradle_files(root)):
        if not path.is_file() or gplay_only(path, root):
            continue
        for number, line in enumerate(path.read_text(errors='replace').splitlines(), 1):
            declaration = DECLARATION.match(line)
            if not declaration or (declaration.group('scope') or '').lower().startswith('gplay'):
                continue
            names = [name for name in GOOGLE if name in line or name.replace('.', '/') in line]
            if names:
                findings.append(f'{path.relative_to(root)}:{number}: {line.strip()}')
    return findings


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('--source', type=Path, help="check this folder's Gradle files as F-Droid scans them")
    parser.add_argument('--expect-google', action='store_true', help='the packages are -gplay: fail if none holds Play Billing')
    parser.add_argument('packages', nargs='*', type=Path)
    args = parser.parse_args()
    if not args.source and not args.packages:
        parser.error('give --source or a package')
    failed = False
    if args.source:
        findings = source_findings(args.source.resolve())
        for finding in findings:
            print(f'Google library outside the gplay edition: {finding}', file=sys.stderr)
        failed |= bool(findings)
        if not findings:
            print(f'{args.source}: Gradle files name no Google library outside the gplay edition')
    for package in args.packages:
        findings = package_findings(package)
        if args.expect_google:
            if not any('com.android.billingclient' in finding for finding in findings):
                print(f'{package}: no Play Billing found; the -gplay check no longer recognises it', file=sys.stderr)
                failed = True
            else:
                print(f'{package}: Play Billing found, as the -gplay edition should have')
            continue
        for finding in findings[:40]:
            print(f'Google library in a normal or F-Droid package: {finding}', file=sys.stderr)
        if len(findings) > 40:
            print(f'... and {len(findings) - 40} more entries', file=sys.stderr)
        failed |= bool(findings)
        if not findings:
            print(f'{package}: no Google library')
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
