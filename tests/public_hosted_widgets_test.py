"""Exercise narrow hosted controls as an ordinary downstream package consumer."""
import subprocess
from toolchain import ROOT, ZIRAN, kryon_test_project

project = ROOT / 'build/scratch/public-hosted-widgets/project'
package_id = kryon_test_project(project)
imports = '''#import "kryon/session"
#import "kryon/toggle_props"
#import "kryon/tab_bar_props"
#import "kryon/dropdown_props"
'''
source = project / 'src/public_hosted_widgets.zi'
source.write_text(imports + '''#import "kryon/toggle_widget"
#import "kryon/tab_bar_widget"
#import "kryon/dropdown_widget"
main :: () -> s32 {
    session := Session.{.slot = 4, .epoch = cast(u64)12}
    toggled := Toggle(session, ToggleProps.{.id = 53, .value = true})
    if !toggled.changed || toggled.value { return 1 }
    tabs: [2]Tab
    tabs[1].label = "Two"
    selected := TabBar(session, TabBarProps.{.selected_index = 0}, tabs[:])
    if selected.selected_index != 1 { return 2 }
    options: [1]DropdownOption
    options[0].label = "Choice"
    dropdown := Dropdown(session, DropdownProps.{.selected_index = -1}, options[:])
    if !dropdown.changed || dropdown.selected_index != 0 { return 3 }
    return 0
}
''')
provider = project / 'src/public_hosted_widgets_provider.zi'
provider.write_text(imports + '''#program_export "ToggleHostedHost"
HostedToggle :: (session: Session, props: ToggleProps) -> ToggleValueResult {
    return ToggleValueResult.{.changed = session.slot == 4 &&
        session.epoch == cast(u64)12 && props.id == 53, .value = !props.value}
}
#program_export "TabBarHostedHost"
HostedTabBar :: (session: Session, props: TabBarProps, tabs: []Tab) -> TabBarResult {
    if session.slot != 4 || tabs.count != 2 || tabs[1].label != "Two" {
        return TabBarResult.{.selected_index = -1}
    }
    return TabBarResult.{.selected_index = props.selected_index + 1}
}
#program_export "DropdownHostedHost"
HostedDropdown :: (session: Session, props: DropdownProps,
    options: []DropdownOption) -> DropdownResult {
    return DropdownResult.{.changed = session.slot == 4 && options.count == 1 &&
        options[0].label == "Choice", .selected_index = props.selected_index + 1}
}
''')
common = [str(ZIRAN), '--project', '--offline', '--root', str(source.parent),
          '--define', 'KRYON_HOSTED_UI', '--entry', 'public_hosted_widgets:main']
bindings = []
for module, function, target in [('toggle', 'ToggleHostedHost', 'HostedToggle'),
                                 ('tabs', 'TabBarHostedHost', 'HostedTabBar'),
                                 ('dropdown', 'DropdownHostedHost', 'HostedDropdown')]:
    bindings += ['--bind', f'{package_id}_{module}:{function}=public_hosted_widgets_provider:{target}']
bundle = project.parent / 'widgets.zx'
subprocess.run([common[0], 'bundle', *common[1:], *bindings, '-o', str(bundle),
                str(source), str(provider)], cwd=project, check=True)
assert subprocess.check_output([str(ZIRAN), 'run', str(bundle)],
                               cwd=project, text=True).strip() == '0'
print('Public narrow Toggle, TabBar and Dropdown hosted bundle returned 0', flush=True)
native = project.parent / 'native'
subprocess.run([common[0], 'build', '--target=c', '--exe', *common[1:],
                '-o', str(native), str(source), str(provider)], cwd=project, check=True)
assert not list(native.glob('*paint_queue*')), 'narrow hosted imports loaded native paint queue'
subprocess.run([str(native / 'public_hosted_widgets')], cwd=project, check=True)
print('Public narrow hosted controls native C returned 0; no paint_queue module', flush=True)
