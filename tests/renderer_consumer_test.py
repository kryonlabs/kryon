"""Renderer metadata contract; descriptor integrity belongs to Ziran."""
import copy
import importlib.util
from pathlib import Path
import subprocess
import tempfile
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
                         'font.ttf', 'font-license', 'web/glue.js'}

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

    def test_native_order_resources_and_compile_options(self):
        result = self.consume()
        self.assertEqual(renderer.renderer_link(result), [str(self.root / 'lib/second.a'),
            str(self.root / 'lib/first.a'), '-lm', '-pthread'])
        self.assertEqual(result['compile_flags'], ['-I/sdk path'])
        self.assertEqual(result['defines'], ['PLATFORM_DESKTOP_SDL'])
        self.assertEqual(result['include_directories'], [str(self.root / 'include')])
        self.assertEqual(result['resources']['font'], str(self.root / 'font.ttf'))
        self.assertEqual(self.meta['archives'][0], 'lib/second.a')

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
        for field in ('compile_flags', 'link_flags', 'defines', 'archives', 'include_directories', 'js_libraries'):
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

    def test_invalid_virtual_mounts_are_rejected(self):
        for mount in ('relative', '/../outside', '/first/../outside', '/first/./second',
                      '/first\\second', '/first@/second', ''):
            self.meta['preload_files'] = [dict(path='font.ttf', mount=mount)]
            with self.subTest(mount=mount), self.assertRaises(ValueError):
                self.consume()


if __name__ == '__main__':
    unittest.main()
