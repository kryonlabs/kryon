import importlib.util
import os
from pathlib import Path
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    'gpui_render_benchmark', ROOT / 'tests/gpui_render_benchmark.py')
BENCHMARK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BENCHMARK)


class GpuiRenderBenchmarkTest(unittest.TestCase):
    DEVICE = {'node': '/dev/dri/renderD129', 'pci': '0000:03:00.0',
              'vendor': '1002', 'device': '744c', 'driver': 'amdgpu'}

    def test_pins_gpui_and_libc_versions(self):
        self.assertIn('gpui = { version = "=0.2.2", features = ["test-support"] }',
                      BENCHMARK.CARGO_TOML)
        self.assertIn('libc = "=0.2.186"', BENCHMARK.CARGO_TOML)

    def test_cpu_scope_is_explicit(self):
        self.assertIn('TestAppContext', BENCHMARK.BENCH_SOURCE)
        self.assertIn('no GPU execution', BENCHMARK.__doc__)
        self.assertIn('presentation or compositor wall time', BENCHMARK.__doc__)

    def test_gpu_probe_compiles_and_keeps_a_private_unix_socket(self):
        self.assertIn('WindowOptions', BENCHMARK.GPU_PROBE)
        self.assertTrue(BENCHMARK.WESTON_RUNTIME.is_relative_to(ROOT / 'build/scratch'))
        longest_socket = BENCHMARK.WESTON_RUNTIME / 'gpui-bench-4194303'
        self.assertLessEqual(len(os.fsencode(longest_socket)), 107)

    def test_generated_project_uses_disposable_paths_and_xkb_sysroot(self):
        with tempfile.TemporaryDirectory(dir=ROOT / 'build/scratch') as temporary:
            saved_build = BENCHMARK.BUILD
            BENCHMARK.BUILD = Path(temporary) / 'gpui-render-benchmark'
            try:
                sysroot = BENCHMARK.write_project()
                BENCHMARK.write_project()
                self.assertTrue((sysroot / 'libxkbcommon-x11.so').is_symlink())
                self.assertTrue((BENCHMARK.BUILD / 'Cargo.toml').is_file())
            finally:
                BENCHMARK.BUILD = saved_build

    def test_parses_one_benchmark_record_per_trial(self):
        record = BENCHMARK.parse_benchmark(
            'GPUI scene=1 frames=120 cpu_draw_p50_ns=105570 '
            'cpu_draw_max_ns=447340 shape_paint_ns=0')

        self.assertEqual(record, {'scene_index': 1, 'frames': 120,
                                  'cpu_draw_p50_ns': 105570,
                                  'cpu_draw_max_ns': 447340,
                                  'shape_paint_ns': 0})

    def test_rejects_partial_benchmark_record(self):
        self.assertIsNone(BENCHMARK.parse_benchmark('GPUI scene=1 frames=120'))

    def test_expected_gpui_failure_records_wl_seat_limitation(self):
        result = BENCHMARK.classify_gpu_probe(
            101,
            'panicked at gpui-0.2.2/src/platform/linux/wayland/client.rs:503:18: '
            'called `Option::unwrap()` on a `None` value',
            '[foo] gl-renderer.so: OpenGL ES renderer',
            self.DEVICE, Path('/ignored/logs'))

        self.assertEqual(result['status'], 'wl_seat_limitation')
        self.assertIn('missing wl_seat global', result['limitation'])

    def test_missing_weston_gl_evidence_is_not_a_pass(self):
        result = BENCHMARK.classify_gpu_probe(
            101,
            'platform/linux/wayland/client.rs:503 '
            'called `Option::unwrap()` on a `None` value',
            'renderer: no-op', self.DEVICE, Path('/ignored/logs'))

        self.assertEqual(result['status'], 'weston_gl_not_verified')

    def test_weston_evidence_reports_the_actual_gl_renderer(self):
        result = BENCHMARK.classify_gpu_probe(
            101, '', '[start] Command line: weston --renderer=gl\n'
            '[gl] GL renderer: AMD Radeon Graphics', self.DEVICE, Path('/ignored/logs'))

        self.assertEqual(result['weston_renderer_evidence'],
                         '[gl] GL renderer: AMD Radeon Graphics')


if __name__ == '__main__':
    unittest.main()
