#!/usr/bin/env python3
"""Validate completeness and statistics: never publish partial or software GPU runs."""
import tempfile
from pathlib import Path
from shader_benchmark import OUT, parse


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=OUT) as directory:
        path = Path(directory) / 'samples.log'
        workload = 'workload 0 640 360 0 2 100000 8 20\n'
        rows = workload + ''.join(f'sample 0 640 360 0 {i} {i+1}000000 0 0 0 0 123\n' for i in range(20))
        path.write_text(rows)
        results, _ = parse(path, {(0, 640, 360, 0): 20})
        assert results[0]['mean_ms'] == 10.5 and results[0]['p95_ms'] == 19
        assert results[0]['instructions'] == 2 and results[0]['compile_ms'] == 0.1
        assert results[0]['warmup_frames'] == 8
        gpu_rows = rows.replace('sample 0 ', 'sample 2 ').replace('workload 0 ', 'workload 2 ')
        cases = [rows.replace('0 19 20000000', '0 18 20000000'), rows.rsplit('sample', 1)[0],
                 rows.replace('1000000 ', '-1000000 ', 1), rows + rows,
                 'identity true llvmpipe OpenGL ES 3.2\n' + gpu_rows,
                 rows.replace('1000000 ', '0 ', 1), rows.removeprefix(workload),
                 rows.replace(' 8 20\n', ' 8 21\n'),
                 rows.replace('1000000 0 ', '1000000 90000000000000000 ', 1)]
        for index, text in enumerate(cases):
            path.write_text(text)
            expected = {(2 if index == 4 else 0, 640, 360, 0): 20}
            try:
                parse(path, expected)
            except ValueError:
                continue
            raise AssertionError('Accepted invalid benchmark case ' + str(index))
        path.write_text(rows.replace('1000000 ', '0 ', 1))
        tiny, _ = parse(path, {(0, 640, 360, 0): 20}, allow_zero=True)
        assert tiny[0]['samples'] == 20
        path.write_text('workload 1 64 36 9 0 0 1 2\n'
                        'sample 1 64 36 9 0 12000000 0 0 0 0 123\n'
                        'sample 1 64 36 9 1 15000000 0 0 0 0 456\n')
        scene, _ = parse(path, {(1, 64, 36, 9): 2})
        assert scene[0]['shader'] == 'skinned-character'
        assert scene[0]['mean_ms'] == 13.5 and scene[0]['hashes'] == [123, 456]
    print('Local shader benchmark statistics tests passed')


if __name__ == '__main__':
    main()
