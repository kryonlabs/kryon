#!/usr/bin/env python3
"""Source/saved native file dates against Python calendar and installed gettext."""
import datetime as dt
import gettext
import locale
import os
from pathlib import Path
import shutil
import subprocess
import time
from zoneinfo import ZoneInfo

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/scratch/file-date-linux'

def run(args, **kw):
    return subprocess.run(list(map(str, args)), cwd=ROOT, check=True, **kw)

def main():
    run(['git', 'check-ignore', OUT / 'source'])
    OUT.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    for key in ('DISPLAY', 'WAYLAND_DISPLAY', 'WAYLAND_SOCKET', 'XAUTHORITY',
                'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER', 'ICEAUTHORITY'):
        env.pop(key, None)
    env.update(YUE_DESKTOP_RECOVERY='0', LC_ALL='C.UTF-8', LANGUAGE='en', TZ='UTC')
    env['LDLIBS'] = env.get('LDLIBS', '') + ' ' + subprocess.check_output(['pkg-config', '--libs', 'glib-2.0'], text=True).strip()
    compiler = os.environ.get('ZIRAN_BIN') or shutil.which('ziran')
    assert compiler, 'Ziran is required'
    source = ROOT / 'tests/file_date_linux_test.zi'
    ir = OUT / 'ir'
    common = ['--module-path', ROOT / 'src/backend']
    run(['build-queue', '--', compiler, 'ir', '--root', ROOT / 'tests', *common,
         '--entry', 'file_date_linux_test:main', '-o', ir, source], env=env)
    locale.setlocale(locale.LC_ALL, 'C.UTF-8')
    available_locales = subprocess.check_output(['locale', '-a'], text=True).splitlines()
    message_locale = next((name for name in available_locales
                           if name.lower().endswith(('utf8', 'utf-8')) and
                           not name.lower().startswith('c.')), None)
    assert message_locale, 'Translation tests require an installed UTF-8 locale other than C'
    catalogs = [language for language in ('en', 'de', 'es', 'fr', 'ru', 'zh_CN')
                if language == 'en' or gettext.find('thunar', languages=[language])]
    total = 0
    for form in ('source', 'saved'):
        target = OUT / form
        input_ = source if form == 'source' else ir / 'file_date_linux_test.zir'
        root = ROOT / 'tests' if form == 'source' else ir
        run(['build-queue', '--', compiler, 'build', '--target=c', '--exe',
             '--entry', 'file_date_linux_test:main', '--root', root, *common,
             '-o', target, input_], env=env)
        executable = target / 'file_date_linux_test'
        for zone in ('UTC', 'America/New_York', 'America/Asuncion', 'Pacific/Kiritimati'):
            tz = ZoneInfo(zone)
            # Both DST transitions, year boundary, leap day, and midnight.
            for date in ((2026, 11, 8), (2026, 3, 9), (2026, 11, 2),
                         (2026, 1, 1), (2024, 3, 1)):
                current = dt.datetime(*date, 0, 5, tzinfo=tz)
                now = int(current.timestamp())
                for age in (-2, 0, 1, 2, 6, 7, 8, 365):
                    value = current - dt.timedelta(days=age)
                    stamp = int(value.timestamp())
                    for style in ('locale', 'relative'):
                        expected = value.strftime('%x')
                        if style == 'relative':
                            expected = 'Today' if age == 0 else 'Yesterday' if age == 1 else value.strftime('%A') if 2 <= age <= 6 else expected
                        case = dict(env, TZ=zone, DATE_STAMP=str(stamp), DATE_NOW=str(now),
                                    DATE_STYLE=style, DATE_DOMAIN='', DATE_EXPECTED=expected)
                        run([executable], env=case, capture_output=True, text=True)
                        total += 1
        current = dt.datetime(2026, 11, 8, 12, tzinfo=dt.timezone.utc)
        now = int(current.timestamp())
        for language in catalogs:
            catalog = gettext.translation('thunar', languages=[language], fallback=language == 'en')
            for age, label in ((0, 'Today'), (1, 'Yesterday'), (None, 'Unknown')):
                stamp = 0 if age is None else int((current - dt.timedelta(days=age)).timestamp())
                case = dict(env, LC_ALL=message_locale, LANGUAGE=language, DATE_STAMP=str(stamp), DATE_NOW=str(now),
                            DATE_STYLE='relative', DATE_DOMAIN='thunar', DATE_EXPECTED=catalog.gettext(label))
                run([executable], env=case, capture_output=True, text=True)
                total += 1
        for stamp in (0, 2**63 - 1, -(2**63)):
            run([executable], env=dict(env, DATE_STAMP=str(stamp), DATE_NOW=str(now),
                DATE_STYLE='relative', DATE_DOMAIN='', DATE_EXPECTED='Unknown'),
                capture_output=True, text=True)
            total += 1
        for stamp, expected in ((0, dt.datetime.fromtimestamp(0, dt.timezone.utc).strftime('%x')),
                                (2**63 - 1, 'ERROR'), (-(2**63), 'ERROR')):
            run([executable], env=dict(env, DATE_STAMP=str(stamp), DATE_NOW=str(now),
                DATE_STYLE='locale', DATE_DOMAIN='', DATE_EXPECTED=expected), capture_output=True, text=True)
            total += 1
        run([executable], env=dict(env, DATE_STAMP=str(now), DATE_NOW=str(2**63 - 1),
            DATE_STYLE='relative', DATE_DOMAIN='', DATE_EXPECTED='ERROR'), capture_output=True, text=True)
        total += 1
        # Production-clock branch, independent UTC expectation.
        run([executable], env=dict(env, DATE_STAMP=str(int(time.time())), DATE_NOW='-1',
            DATE_STYLE='relative', DATE_DOMAIN='', DATE_EXPECTED='Today'),
            capture_output=True, text=True)
        total += 1
    print(f'File dates: {total} source/saved native cases passed; calendar/DST, installed translations {catalogs} and output bounds')

if __name__ == '__main__':
    try:
        main()
    except subprocess.CalledProcessError as error:
        print(error.stdout or '')
        print(error.stderr or '')
        raise
