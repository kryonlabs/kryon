#!/usr/bin/env python3
"""Fixed-view viewport crops on CPU and real GLES, source and saved IR."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

import numpy as np
import scene_gpu_test as gpu


def pixels(prefix, label, size, state, width, height):
    return np.fromfile(f'{prefix}-{label}-{size}-{state}.rgba', dtype=np.uint8).reshape(height, width, 4)


def difference(actual, expected):
    delta = np.abs(actual.astype(np.int16) - expected.astype(np.int16))
    return {'mae': float(delta.mean()), 'fraction_over_1': float((delta > 1).mean()),
            'fraction_over_16': float((delta > 16).mean()), 'max': int(delta.max())}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, help='Pre-change zero-shift RGBA prefix')
    args = parser.parse_args()
    shifts = [(0, 0), (-1.5, 0), (1.5, 0), (0, -1.5), (0, 1.5),
              (-1.5, -1.5), (1.5, 1.5), (-1.5, 1.5), (1.5, -1.5)]
    results = []
    for saved in (False, True):
        executable = gpu.build(gpu.ROOT / 'tests/scene_camera_shift_test.zi',
                               'scene_camera_shift_test:main', saved=saved)
        prefix = gpu.OUT / ('shift-saved' if saved else 'shift-source')
        subprocess.run([str(executable), str(prefix)], cwd=gpu.ROOT,
                       env=gpu.private_env(), check=True)
        for size, (width, height) in enumerate(((96, 64), (64, 96))):
            hashes = set()
            reference = {kind: pixels(prefix, f'wide-{kind}', size, 0, width * 3, height * 3)
                         for kind in ('cpu', 'gpu')}
            for state, (shift_x, shift_y) in enumerate(shifts):
                images = {kind: pixels(prefix, kind, size, state, width, height)
                          for kind in ('cpu', 'gpu')}
                hashes.add(hashlib.sha256(images['cpu'].tobytes()).hexdigest())
                row = {'saved': saved, 'width': width, 'height': height, 'shift': [shift_x, shift_y]}
                # Wider FOV, same aspect and fixed pose: each small viewport is
                # an integer-aligned crop. Positive NDC Y moves pixels upwards.
                x = round(width - shift_x * width / 2)
                y = round(height + shift_y * height / 2)
                for kind in ('cpu', 'gpu'):
                    crop = reference[kind][y:y + height, x:x + width]
                    stats = difference(images[kind], crop)
                    assert stats['mae'] <= 0.75 and stats['fraction_over_1'] <= 0.01, (row, kind, stats)
                    row[f'{kind}_crop'] = stats
                    if state == 0:
                        zero = pixels(prefix, f'zero-{kind}', size, 0, width, height)
                        assert np.array_equal(zero, images[kind]), (row, kind, 'explicit zero differs')
                        if args.baseline:
                            old = pixels(args.baseline, f'zero-{kind}', size, 0, width, height)
                            assert np.array_equal(zero, old), (row, kind, 'pre-change zero pixels differ')
                stats = difference(images['cpu'], images['gpu'])
                assert stats['mae'] <= 1.0 and stats['fraction_over_16'] <= 0.01, (row, stats)
                row['cpu_gpu'] = stats
                results.append(row)
            assert len(hashes) == len(shifts), 'Each pan must expose a different viewport'
    output = gpu.OUT / 'camera-shift-results.json'
    output.write_text(json.dumps(results, indent=2) + '\n')
    print(f'PASS: {len(results)} source/saved portrait/landscape camera views; CPU/GPU crop oracles, '
          'shifted frustum clipping and exact async pixels/metadata')
    if args.baseline:
        print('PASS: zero-shift CPU/GPU pixels exactly match the pre-change symmetric renderer')


if __name__ == '__main__':
    main()
