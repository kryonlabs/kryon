"""Execute Kryon's selected Frame capability from source and saved IR."""
from pathlib import Path
import tempfile

from toolchain import ROOT, ZIRAN, ZIRAN_ROOT
from project_entry_test import run


def main():
    scratch = ROOT / 'build/scratch'
    scratch.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='frame-binding-', dir=scratch) as temporary:
        project = Path(temporary)
        host = project / 'entry_host.zi'
        selected = project / 'selected.zi'
        host.write_text('''#import "session"
#import "geometry"
#import "frame_entry"
#program_export
main :: () -> s32 {
    session: Session
    session.slot = 1
    viewport: Rectangle
    viewport.width = 41.0
    if Frame(session, viewport) != 42 { return 1 }
    return 0
}
''')
        selected.write_text('''#import "session"
#import "geometry"
#program_export
Frame :: (session: Session, viewport: Rectangle) -> s32 {
    return session.slot + cast(s32)viewport.width
}
''')
        sources = [host, selected, ROOT / 'src/backend/frame_entry.zi']
        paths = ['--root', project, '--module-path', ROOT / 'src/ui',
                 '--module-path', ROOT / 'src/backend', '--module-path', ZIRAN_ROOT / 'std']
        binding = ['--bind', 'frame_entry:ProjectFrame=selected:Frame']
        entry = ['--entry', 'entry_host:main']
        run([ZIRAN, 'build', '--target=c', '--exe', *paths, *entry, *binding,
             '-o', project / 'source', *sources], ROOT)
        run([project / 'source/entry_host'], ROOT)
        ir = project / 'ir'
        run([ZIRAN, 'ir', *paths, *entry, *binding, '-o', ir, *sources], ROOT)
        saved = sorted(ir.glob('*.zir'))
        run([ZIRAN, 'build', '--target=c', '--exe', '--root', ir, *entry,
             '-o', project / 'saved', *saved], ROOT)
        run([project / 'saved/entry_host'], ROOT)
        bundle = project / 'entry.zx'
        run([ZIRAN, 'bundle', '--root', ir, *entry, '-o', bundle, *saved], ROOT)
        result = run([ZIRAN, 'run', bundle], ROOT)
        assert result.stdout.strip() == '0', result.stdout
    print('Frame binding native source, saved IR and portable local tests passed')


if __name__ == '__main__':
    main()
