"""Pillow, Pillow-SIMD or libvips Lanczos-3, test-only oracles for
tests/lanczos_benchmark.py:

    lanczos_benchmark_oracle.py TOOL IN WIDTH HEIGHT OUT_WIDTH OUT_HEIGHT ITERATIONS OUT

TOOL is pillow (whichever Pillow this Python has, Pillow-SIMD in its venv)
or vips. IN and OUT are raw RGBA. Prints the library, the median and fastest
microseconds and the peak resident KiB (VmHWM).
"""
import sys
import time


def peak_kib():
    with open('/proc/self/status') as status:
        for line in status:
            if line.startswith('VmHWM:'):
                return int(line.split()[1])
    return -1


def main():
    tool, path = sys.argv[1], sys.argv[2]
    width, height, out_width, out_height, iterations = map(int, sys.argv[3:8])
    out = sys.argv[8]
    with open(path, 'rb') as source:
        data = source.read()
    if tool == 'pillow':
        from PIL import Image
        import PIL
        name = f'pillow-{PIL.__version__}'
        picture = Image.frombytes('RGBA', (width, height), data)

        def resize():
            return picture.resize((out_width, out_height), Image.LANCZOS).tobytes()
    elif tool == 'vips':
        import pyvips
        # Every call must compute, not answer from the operation cache.
        pyvips.cache_set_max(0)
        name = f'libvips-{pyvips.version(0)}.{pyvips.version(1)}.{pyvips.version(2)}'
        picture = pyvips.Image.new_from_memory(data, width, height, 4, 'uchar')

        def resize():
            return picture.resize(out_width / width, vscale=out_height / height, kernel='lanczos3').write_to_memory()
    else:
        sys.exit(f'unknown tool {tool}')
    samples = []
    result = b''
    for _ in range(iterations):
        start = time.perf_counter_ns()
        result = resize()
        samples.append(time.perf_counter_ns() - start)
    if iterations:
        if len(result) != out_width * out_height * 4:
            sys.exit(f'{name} made {len(result)} bytes')
        with open(out, 'wb') as target:
            target.write(result)
        samples.sort()
        print(name, samples[len(samples) // 2] // 1000, samples[0] // 1000, peak_kib())
    else:
        print(name, 0, 0, peak_kib())


if __name__ == '__main__':
    main()
