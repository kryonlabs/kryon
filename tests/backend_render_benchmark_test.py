"""Synthetic protocol tests for the benchmark device qualification gate."""
import importlib.util
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    'backend_render_benchmark', ROOT / 'tests/backend_render_benchmark.py')
BENCHMARK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BENCHMARK)


class DrmDeviceTests(unittest.TestCase):
    def test_groups_fdinfo_fields_by_file_descriptor(self):
        records = [
            'IDENTITY llvmpipe (LLVM 19.1.0, 256 bits)',
            'DEVICE 7 drm-pdev:0000:03:00.0',
            'PARITY accepted',
            'DEVICE 7 drm-engine-gfx:125 ns',
            'DEVICE 9 drm-pdev:0000:04:00.0',
            'DEVICE 9 drm-engine-gfx:0 ns',
            'DEVICE 10 drm-pdev:0000:05:00.0',
        ]

        clients = BENCHMARK.drm_devices(records)

        self.assertEqual(clients, [
            {'drm-pdev': '0000:03:00.0', 'drm-engine-gfx': '125 ns'},
            {'drm-pdev': '0000:04:00.0', 'drm-engine-gfx': '0 ns'},
            {'drm-pdev': '0000:05:00.0'},
        ])


class QualifyDeviceTests(unittest.TestCase):
    PCI = '0000:03:00.0'
    DEVICE = {'pci': PCI}

    def hardware_records(self, clients, identity='IDENTITY AMD Radeon RX 7900 XTX'):
        records = [identity]
        for fd, client in enumerate(clients, start=1):
            for key, value in client.items():
                records.append(f'DEVICE {fd} {key}:{value}')
        return records

    def test_accepts_requested_amd_pci_with_nonzero_gfx_execution(self):
        records = self.hardware_records([
            {'drm-pdev': self.PCI, 'drm-engine-gfx': '125 ns'},
        ])

        self.assertEqual(BENCHMARK.qualify_device(records, self.DEVICE), [
            'IDENTITY AMD Radeon RX 7900 XTX',
        ])

    def test_accepts_multiple_active_clients_on_requested_pci(self):
        records = self.hardware_records([
            {'drm-pdev': self.PCI, 'drm-engine-gfx': '125 ns'},
            {'drm-pdev': self.PCI, 'drm-engine-gfx': '640 ns'},
        ])

        self.assertEqual(BENCHMARK.qualify_device(records, self.DEVICE), [
            'IDENTITY AMD Radeon RX 7900 XTX',
        ])

    def test_rejects_merely_open_requested_node_without_gfx_counter(self):
        records = self.hardware_records([
            {'drm-pdev': self.PCI},
        ])

        with self.assertRaisesRegex(
                ValueError, r"actual DRM gfx devices \[\] differ from requested"):
            BENCHMARK.qualify_device(records, self.DEVICE)

    def test_rejects_zero_gfx_counter_on_requested_pci(self):
        records = self.hardware_records([
            {'drm-pdev': self.PCI, 'drm-engine-gfx': '0 ns'},
        ])

        with self.assertRaisesRegex(
                ValueError, r"actual DRM gfx devices \[\] differ from requested"):
            BENCHMARK.qualify_device(records, self.DEVICE)

    def test_rejects_active_other_pci(self):
        records = self.hardware_records([
            {'drm-pdev': '0000:04:00.0', 'drm-engine-gfx': '125 ns'},
        ])

        with self.assertRaisesRegex(
                ValueError, r"actual DRM gfx devices \['0000:04:00.0'\] differ"):
            BENCHMARK.qualify_device(records, self.DEVICE)

    def test_rejects_requested_pci_plus_active_second_gpu(self):
        records = self.hardware_records([
            {'drm-pdev': self.PCI, 'drm-engine-gfx': '125 ns'},
            {'drm-pdev': '0000:04:00.0', 'drm-engine-gfx': '64 ns'},
        ])

        with self.assertRaisesRegex(
                ValueError, r"actual DRM gfx devices \['0000:03:00.0', '0000:04:00.0'\] differ"):
            BENCHMARK.qualify_device(records, self.DEVICE)

    def test_rejects_nonzero_gfx_without_drm_pdev(self):
        records = self.hardware_records([
            {'drm-engine-gfx': '125 ns'},
        ])

        with self.assertRaisesRegex(
                ValueError, r"actual DRM gfx devices \['None'\] differ"):
            BENCHMARK.qualify_device(records, self.DEVICE)

    def test_rejects_software_renderers_in_hardware_mode(self):
        for renderer in ('llvmpipe', 'lavapipe', 'softpipe', 'swiftshader',
                         'Software Vulkan Device'):
            with self.subTest(renderer=renderer):
                records = self.hardware_records([
                    {'drm-pdev': self.PCI, 'drm-engine-gfx': '125 ns'},
                ], identity=f'IDENTITY {renderer}')

                with self.assertRaisesRegex(
                        ValueError, 'CPU/software renderer rejected'):
                    BENCHMARK.qualify_device(records, self.DEVICE)

    def test_rejects_missing_identity(self):
        records = [
            f'DEVICE 1 drm-pdev:{self.PCI}',
            'DEVICE 1 drm-engine-gfx:125 ns',
        ]

        with self.assertRaisesRegex(
                ValueError, 'missing/ambiguous actual renderer identity'):
            BENCHMARK.qualify_device(records, self.DEVICE)

    def test_rejects_ambiguous_identity(self):
        records = [
            'IDENTITY AMD Radeon RX 7900 XTX',
            'IDENTITY AMD Radeon RX 7600',
            f'DEVICE 1 drm-pdev:{self.PCI}',
            'DEVICE 1 drm-engine-gfx:125 ns',
        ]

        with self.assertRaisesRegex(
                ValueError, 'missing/ambiguous actual renderer identity'):
            BENCHMARK.qualify_device(records, self.DEVICE)

    def test_software_mode_accepts_llvmpipe(self):
        records = ['IDENTITY llvmpipe (LLVM 19.1.0, 256 bits)']

        self.assertEqual(BENCHMARK.qualify_device(records, None), records)

    def test_software_mode_rejects_non_llvmpipe_renderers(self):
        for renderer in ('AMD Radeon RX 7900 XTX', 'lavapipe (LLVM 19.1.0)',
                         'softpipe', 'SwiftShader', 'Software Vulkan Device'):
            with self.subTest(renderer=renderer):
                records = [f'IDENTITY {renderer}']

                with self.assertRaisesRegex(
                        ValueError, 'actual llvmpipe device not verified'):
                    BENCHMARK.qualify_device(records, None)


if __name__ == '__main__':
    unittest.main()
