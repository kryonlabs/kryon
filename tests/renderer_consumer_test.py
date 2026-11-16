"""Renderer metadata contract; descriptor integrity belongs to Ziran."""
import copy
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import tomllib
import unittest


ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('renderer_artifact', ROOT / 'scripts/renderer_artifact.py')
renderer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(renderer)


class VerifiedArtifact:
    def __init__(self, root, meta):
        self.root = root
        self.meta = meta
        self.declared = {'renderer.json', 'lib/second.a', 'lib/first.a',
                         'font.ttf', 'font-license', 'web/glue.js', 'lib/libkryonavif.so'}

    def metadata(self, name):
        assert name == 'renderer.json'
        return copy.deepcopy(self.meta)

    def output(self, name):
        if name not in self.declared:
            raise ValueError('undeclared output')
        return self.root / name

    def path(self, name):
        if name != 'include':
            raise ValueError('unexpected include directory')
        return self.root / name


class RendererConsumerTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.scratch = ROOT / 'work/renderer-consumer-test'
        subprocess.run(['git', 'check-ignore', str(cls.scratch)], cwd=ROOT, check=True,
                       stdout=subprocess.DEVNULL)
        cls.scratch.mkdir(parents=True, exist_ok=True)

    def setUp(self):
        temporary = tempfile.TemporaryDirectory(dir=self.scratch)
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        (self.root / 'include').mkdir()
        self.meta = dict(schema=1, artifact='renderer-raylib', platform='linux-x86_64-gnu',
            compile_flags=['-I/sdk path'], link_flags=['-lm', '-pthread'], defines=['PLATFORM_DESKTOP_SDL'],
            archives=['lib/second.a', 'lib/first.a'], include_directories=['include'],
            resources=dict(font='font.ttf', font_license='font-license'))

    def consume(self):
        return renderer.renderer_metadata(VerifiedArtifact(self.root, self.meta))

    def test_local_provider_identity_uses_the_root_locked_kryon_edges(self):
        first = dict(id='old', name='kryon', dependencies={'raylib': 'old-raylib'})
        selected = dict(id='selected', name='kryon', dependencies={'raylib': 'raylib'})
        lock = dict(packages=[first, selected], root=dict(dependencies={'kryon': 'selected'}))
        self.assertIs(renderer.locked_provider(lock, 'selected'), selected)
        self.assertIs(renderer.locked_provider(lock, 'local-override', local=True), selected)
        with self.assertRaisesRegex(ValueError, 'absent'):
            renderer.locked_provider(lock, 'local-override')

    def test_local_override_skips_and_locked_build_checks_the_commit(self):
        repo = self.root / 'dep'
        repo.mkdir()
        git = lambda *a: subprocess.run(['git', '-C', str(repo), '-c', 'user.name=t',
            '-c', 'user.email=t@t', *a], check=True, capture_output=True)
        git('init', '-q')
        git('commit', '-q', '--allow-empty', '-m', 'x')
        renderer.verify_locked_commit(repo, 'older-locked-commit', True, 'avif')
        with self.assertRaisesRegex(ValueError, 'does not match'):
            renderer.verify_locked_commit(repo, 'older-locked-commit', False, 'avif')

    def test_native_order_resources_and_compile_options(self):
        result = self.consume()
        self.assertEqual(renderer.renderer_link(result), [str(self.root / 'lib/second.a'),
            str(self.root / 'lib/first.a'), '-lm', '-pthread'])
        self.assertEqual(result['compile_flags'], ['-I/sdk path'])
        self.assertEqual(result['defines'], ['PLATFORM_DESKTOP_SDL'])
        self.assertEqual(result['include_directories'], [str(self.root / 'include')])
        self.assertEqual(result['resources']['font'], str(self.root / 'font.ttf'))
        self.assertEqual(self.meta['archives'][0], 'lib/second.a')

    def test_packaged_cpu_codec_is_retained_for_installers(self):
        self.meta.update(artifact='renderer-desktop', shared_libraries=['lib/libkryonavif.so'])
        result = self.consume()
        flags = renderer.renderer_link(result)
        self.assertEqual(flags[-5:], ['-Wl,--no-as-needed',
            str(self.root / 'lib/libkryonavif.so'), '-Wl,--as-needed',
            '-Wl,-rpath,' + str(self.root / 'lib'), '-Wl,-rpath,$ORIGIN/../lib'])

    def test_web_glue_and_virtual_preloads(self):
        self.meta.update(artifact='renderer-web', platform='emscripten-wasm32-emscripten', archives=[],
            include_directories=[], js_libraries=['web/glue.js'],
            preload_files=[dict(path='font.ttf', mount='/kryon-font.ttf')])
        result = self.consume()
        base = ['-lm', '-pthread', '--js-library', str(self.root / 'web/glue.js')]
        self.assertEqual(renderer.renderer_link(result, preloads=False), base)
        self.assertEqual(renderer.renderer_link(result), [*base, '--preload-file',
                         str(self.root / 'font.ttf') + '@/kryon-font.ttf'])

    def test_unsupported_renderer_target(self):
        self.meta['platform'] = 'emscripten-wasm32-emscripten'
        with self.assertRaisesRegex(ValueError, 'unsupported'):
            self.consume()

    def test_bad_field_types_fail(self):
        for field in ('compile_flags', 'link_flags', 'defines', 'archives', 'shared_libraries', 'include_directories', 'js_libraries'):
            for value in (None, 'one string', [False], [''], ['bad\nargument']):
                original = self.meta.get(field, [])
                self.meta[field] = value
                with self.subTest(field=field, value=value), self.assertRaises(ValueError):
                    self.consume()
                self.meta[field] = original

    def test_resources_and_includes_must_exist(self):
        self.meta['resources']['font'] = 'undeclared-font'
        with self.assertRaisesRegex(ValueError, 'undeclared'):
            self.consume()
        self.meta['resources']['font'] = 'font.ttf'
        del self.meta['resources']['font_license']
        with self.assertRaisesRegex(ValueError, 'resources'):
            self.consume()
        self.meta['resources']['font_license'] = 'font-license'
        (self.root / 'include').rmdir()
        with self.assertRaisesRegex(ValueError, 'directory'):
            self.consume()

    def test_recipe_inputs_are_the_files_the_provider_reads(self):
        # Ziran refuses to publish an artifact whose inputs changed while it
        # built, so an input the provider never reads only fails downstream
        # builds whenever another change touches it.
        manifest = tomllib.loads((ROOT / 'ziran.toml').read_text())
        for name in (*renderer.SUPPORTED_RENDERERS, 'native-sdl3'):
            inputs = manifest['artifacts'][name]['inputs']
            with self.subTest(artifact=name):
                self.assertNotIn('Makefile', inputs)
                self.assertTrue(all((ROOT / path).is_file() for path in inputs))
        # The Linux raylib recipe runs mk/renderer-artifact.mk; every make
        # fragment it includes outside a conditional is an input.
        inputs = manifest['artifacts']['renderer-raylib']['inputs']
        pending, read = ['mk/renderer-artifact.mk'], set()
        while pending:
            path = pending.pop()
            read.add(path)
            depth = 0
            for line in (ROOT / path).read_text().splitlines():
                word = line.split(maxsplit=1)[0] if line.strip() else ''
                depth += word in ('ifeq', 'ifneq', 'ifdef', 'ifndef')
                depth -= word == 'endif'
                if depth == 0 and word == 'include':
                    included = line.split()[1].replace('$(KRYON_DIR)/', '')
                    if included not in read:
                        pending.append(included)
        self.assertEqual(read, {'mk/renderer-artifact.mk', 'mk/raylib-library.mk', 'mk/codecs.mk'})
        self.assertLessEqual(read, set(inputs))

    def test_invalid_virtual_mounts_are_rejected(self):
        for mount in ('relative', '/../outside', '/first/../outside', '/first/./second',
                      '/first\\second', '/first@/second', ''):
            self.meta['preload_files'] = [dict(path='font.ttf', mount=mount)]
            with self.subTest(mount=mount), self.assertRaises(ValueError):
                self.consume()


if __name__ == '__main__':
    unittest.main()
