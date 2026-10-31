"""A real project profile links a dependency-owned foreign support artifact."""
import os
from pathlib import Path
import subprocess
import shutil
import tempfile

from toolchain import ZIRAN_ROOT
from project_static_archive_test import request_exit

ROOT=Path(__file__).resolve().parents[1]
ZIRAN=ZIRAN_ROOT/'build/bin/ziran'


def main():
    scratch=ROOT/'work/project-artifact-test'
    subprocess.run(['git','check-ignore',str(scratch)],cwd=ROOT,check=True,stdout=subprocess.DEVNULL)
    scratch.mkdir(parents=True,exist_ok=True)
    env=dict(os.environ,MAKEFLAGS='-j1',CMAKE_BUILD_PARALLEL_LEVEL='1',ZIRAN_ROOT=str(ZIRAN_ROOT),
             ZIRAN_PINNED_LAUNCHER='1',ZIRAN_TEST_ALLOW_LOCAL_GIT='1',
             YUE_DESKTOP_RECOVERY='0',TMPDIR=str(scratch))
    for key in ('DISPLAY','WAYLAND_DISPLAY','XAUTHORITY','DBUS_SESSION_BUS_ADDRESS','SESSION_MANAGER'):
        env.pop(key,None)
    def run(command,cwd):subprocess.run(list(map(str,command)),cwd=cwd,env=env,check=True)
    temporary=tempfile.mkdtemp(dir=scratch)
    try:
        work=Path(temporary);provider=work/'provider';provider.mkdir()
        # Synthetic fixture repository, not a copy of a maintained source tree.
        (provider/'ziran.toml').write_text('''[package]
name = "support"
module_roots = ["."]
[tool]
project = "build/bin/provider"
[artifacts.native]
inputs = ["value.c", "provider.sh", "Makefile"]
outputs = ["support.a", "link.txt"]
[artifacts.extra]
inputs = ["value.c", "provider.sh", "Makefile"]
outputs = ["support.a", "link.txt"]
''')
        (provider/'.gitignore').write_text('/build/\n')
        (provider/'value.c').write_text('int ArtifactValue(void) { return 77; }\n')
        (provider/'Makefile').write_text('build/bin/provider: provider.sh\n\tmkdir -p build/bin\n\tcp provider.sh $@\n\tchmod +x $@\n')
        (provider/'provider.sh').write_text('''#!/bin/sh
set -eu
cc -c "$ZIRAN_PACKAGE_ROOT/value.c" -o "$ZIRAN_ARTIFACT_OUTPUT/value.o"
ar rcs "$ZIRAN_ARTIFACT_OUTPUT/support.a" "$ZIRAN_ARTIFACT_OUTPUT/value.o"
rm "$ZIRAN_ARTIFACT_OUTPUT/value.o"
printf 'archive\\tsupport.a\\n' > "$ZIRAN_ARTIFACT_OUTPUT/link.txt"
''')
        run(['git','init','-q',provider],work);run(['git','add','.'],provider)
        run(['git','-c','user.name=Waozi','-c','user.email=waozi@proton.me',
             '-c','commit.gpgsign=false','commit','-qm','Native support fixture'],provider)
        project=work/'project';(project/'src').mkdir(parents=True)
        (project/'ziran.toml').write_text(f'''[package]
name = "artifact_probe"
entry = "src/app.zi"
module_roots = ["src"]
bridge_modules = ["app"]
[toolchain]
git = "https://github.com/ziranlang/ziran.git"
ref = "ee86330414a2febb0135d86506b505af5e6a660b"
[dependencies.kryon]
git = "https://github.com/kryonlabs/kryon.git"
ref = "master"
[dependencies.support]
git = "{provider.as_uri()}"
ref = "master"
[tool.kryon]
default_profile = "tui"
[tool.kryon.profiles.tui]
backend = "terminal"
native_artifacts = "support:native support:extra"
native_platform = "linux-x86_64-gnu"
''')
        (project/'ziran.local.toml').write_text(f'[overrides]\nziran = "{ZIRAN_ROOT}"\nkryon = "{ROOT}"\nsupport = "{provider}"\n')
        (project/'src/app.zi').write_text('''#import "kryon/Widgets"
support :: #system_library "support";
ArtifactValue :: () -> s32 #foreign support;
#program_export
Frame :: (session: Session, viewport: Rectangle) -> s32 {
    unused viewport
    if TypedCodepointTake(session) == 113 { return ArtifactValue() }
    return 0
}
''')
        run([ZIRAN,'lock'],project)
        run([ZIRAN,'tool','kryon','build','--profile','tui'],project)
        request_exit(project/'build/artifact_probe-tui',project,77)
    except BaseException:
        print('Failed project artifact fixture retained at',temporary,flush=True)
        raise
    else:
        shutil.rmtree(temporary)
    print('PASS actual project tool -> native profile -> package artifact -> verified link -> isolated terminal exit77')


if __name__=='__main__':main()
