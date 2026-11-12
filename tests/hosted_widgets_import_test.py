"""Exercise the hosted aggregate with real provider results, native and portable."""
import os
import re
import subprocess
from toolchain import ROOT, ZIRAN

for name in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER'):
    os.environ.pop(name, None)
os.environ['YUE_DESKTOP_RECOVERY'] = '0'
work = ROOT / 'build/scratch/hosted-widgets'
work.mkdir(parents=True, exist_ok=True)
minimal = work / 'minimal.zx'
subprocess.run([str(ZIRAN), 'bundle', '--project', '--offline', '--root', str(ROOT),
                '--define', 'KRYON_HOSTED_UI', '--entry', 'hosted_widgets_import:Main',
                '-o', str(minimal), str(ROOT / 'tests/hosted_widgets_import.zi')],
               cwd=ROOT, check=True)
assert subprocess.check_output([str(ZIRAN), 'run', str(minimal)], cwd=ROOT, text=True).strip() == '0'
fixture = (ROOT / 'tests/hosted_ui.zi').read_text()
fixture = re.sub(r'^#import .*\n', '', fixture, flags=re.M)
fixture = '#import "kryon/Widgets"\n#import "pointer_input"\n#import "toggle_measure"\n' + fixture
extra = '''    time := TimePicker(session, TimePickerProps.{.value = 71})
    if !time.changed || time.value != 72 { return 26 }
    spin := Spinbox(session, SpinboxProps.{.value = 13})
    if !spin.changed || spin.value != 14 { return 27 }
'''
fixture = fixture.replace('    return 42', extra + '    return 42')
source = work / 'hosted_widgets_contract.zi'
source.write_text(fixture)
provider = work / 'hosted_widgets_extra.zi'
provider.write_text('''#import "kryon/Widgets"
#program_export "TimePickerHostedHost"
Time :: (session: Session, props: TimePickerProps) -> TimePickerResult {
    return TimePickerResult.{.value = props.value + 1, .changed = session.slot == 4}
}
#program_export "SpinboxHostedHost"
Spin :: (session: Session, props: SpinboxProps) -> SpinboxStepResult {
    return SpinboxStepResult.{.value = props.value + 1, .changed = session.slot == 4}
}
''')
# Reuse the established host contract bindings, adding the two app-required calls.
script = (ROOT / 'tests/ziran_hosted_ui_test.sh').read_text()
bindings = []
for line in script.splitlines():
    if '--bind ' not in line: continue
    binding = line.split('--bind ', 1)[1].split()[0]
    bindings += ['--bind', binding]
bindings += ['--bind', 'timepicker:TimePickerHostedHost=hosted_widgets_extra:Time',
             '--bind', 'spinbox:SpinboxHostedHost=hosted_widgets_extra:Spin']
def run(args):
    return subprocess.run([str(x) for x in args], cwd=ROOT, check=True)
options = ['--project', '--offline', '--root', work, '--module-path', ROOT / 'tests',
           '--define', 'KRYON_HOSTED_UI', '--entry', 'hosted_widgets_contract:main']
sources = [source, ROOT / 'tests/hosted_ui_provider.zi', provider]
bundle = work / 'contracts.zx'
run([ZIRAN, 'bundle', *options, *bindings, '-o', bundle, *sources])
result = subprocess.check_output([str(ZIRAN), 'run', str(bundle)], cwd=ROOT, text=True)
assert result.strip() == '42', result
native = work / 'c'
run([ZIRAN, 'build', *options, '--target=c', '--exe', '-o', native, *sources])
result = subprocess.run([str(native / 'hosted_widgets_contract')], cwd=ROOT)
assert result.returncode == 42, result.returncode
print('Hosted Widgets aggregate: actual pointer/widgets including TimePicker and Spinbox passed portable and native C')
