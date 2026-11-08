"""Generate and run the pinned GPUI CPU-draw comparison harness.

Run through build-queue. Normal mode is deliberately CPU-scoped: GPUI 0.2.2
TestAppContext render/layout/prepaint/paint only, with no GPU execution,
presentation or compositor wall time. Use --gpu-probe with an AMD render node to
record the current headless Weston/GPUI Wayland limitation without touching the
desktop. Generated Cargo sources and binaries stay under build/scratch; reports
and logs stay under ignored work/backend-render-benchmark.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build/scratch/gpui-render-benchmark'
WESTON_RUNTIME = ROOT / 'build/scratch/gpui-runtime'
WORK = ROOT / 'work/backend-render-benchmark'
SCENES = ['text', 'images', 'shapes', 'scroll', 'zoom']
DEFAULT_CARGO = Path.home() / '.rustup/toolchains/1.94.1-x86_64-unknown-linux-gnu/bin/cargo'
XKB_LIBRARY = Path('/usr/lib/x86_64-linux-gnu/libxkbcommon-x11.so.0')
CARGO_TOML = """[package]
name = "gpui-render-benchmark"
version = "0.1.0"
edition = "2021"

[dependencies]
gpui = { version = "=0.2.2", features = ["test-support"] }
libc = "=0.2.186"

[profile.release]
debug = false
opt-level = 3
lto = "thin"
"""
GPU_PROBE = r"""use gpui::{prelude::*, App, Application, Bounds, Render, Window, WindowBounds, WindowOptions, div, px, size};

struct Probe;

impl Render for Probe {
    fn render(&mut self, _window: &mut Window, _cx: &mut gpui::Context<Self>) -> impl IntoElement {
        div().size_full().bg(gpui::black())
    }
}

fn main() {
    println!("GPUI_PROBE starting");
    Application::new().run(|cx: &mut App| {
        let bounds = Bounds::centered(None, size(px(480.0), px(320.0)), cx);
        let window = cx.open_window(
            WindowOptions {
                window_bounds: Some(WindowBounds::Windowed(bounds)),
                ..Default::default()
            },
            |_, cx| cx.new(|_| Probe),
        );
        println!("GPUI_PROBE opened={}", window.is_ok());
        cx.quit();
    });
    println!("GPUI_PROBE stopped");
}
"""
BENCH_SOURCE = r"""
use gpui::{
    canvas, div, img, point, prelude::*, px, rgb, rgba, size, AnyElement, Context, Image,
    ImageFormat, Render, TestAppContext, Window,
};
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::Arc;
use std::time::Instant;

static SHAPE_PAINT_NS: AtomicU64 = AtomicU64::new(0);

fn crc32(data: &[u8]) -> u32 {
    let mut crc = !0u32;
    for byte in data {
        crc ^= *byte as u32;
        for _ in 0..8 {
            let mask = (crc & 1).wrapping_neg();
            crc = (crc >> 1) ^ (0xedb88320 & mask);
        }
    }
    !crc
}

fn adler32(data: &[u8]) -> u32 {
    let mut low: u32 = 1;
    let mut high: u32 = 0;
    for (index, byte) in data.iter().enumerate() {
        low = (low + *byte as u32) % 65521;
        high = (high + low * ((index % 5552) as u32 + 1)) % 65521;
    }
    (high << 16) | low
}

fn png_chunk(kind: &[u8; 4], data: &[u8]) -> Vec<u8> {
    let mut result = Vec::with_capacity(data.len() + 12);
    result.extend_from_slice(&(data.len() as u32).to_be_bytes());
    result.extend_from_slice(kind);
    result.extend_from_slice(data);
    let mut crc_input = Vec::with_capacity(kind.len() + data.len());
    crc_input.extend_from_slice(kind);
    crc_input.extend_from_slice(data);
    result.extend_from_slice(&crc32(&crc_input).to_be_bytes());
    result
}

fn fixture_png(mirror: bool, tint_alpha: u8) -> Vec<u8> {
    let mut scanlines = Vec::with_capacity(272);
    for y in 0..16 {
        scanlines.push(0);
        for x in 0..16 {
            let source_x = if mirror { 15 - x } else { x };
            let (red, green, blue, alpha) = if (source_x / 4 + y / 4) % 2 == 0 {
                (0xe0, 0x20, 0x50, 0xff)
            } else {
                (0x20, 0xd0, 0xa0, 0x80)
            };
            let tint = tint_alpha as u16;
            scanlines.extend_from_slice(&[
                (red as u16 * tint / 255) as u8,
                (green as u16 * tint / 255) as u8,
                (blue as u16 * tint / 255) as u8,
                (alpha as u16 * tint / 255) as u8,
            ]);
        }
    }

    let mut deflate = Vec::with_capacity(scanlines.len() + 11);
    deflate.extend_from_slice(&[0x78, 0x01]);
    deflate.push(1);
    deflate.extend_from_slice(&(scanlines.len() as u16).to_le_bytes());
    deflate.extend_from_slice(&(!(scanlines.len() as u16)).to_le_bytes());
    deflate.extend_from_slice(&scanlines);
    deflate.extend_from_slice(&adler32(&scanlines).to_be_bytes());

    let mut png = b"\x89PNG\r\n\x1a\n".to_vec();
    png.extend(png_chunk(
        b"IHDR",
        &[0, 0, 0, 16, 0, 0, 0, 16, 8, 6, 0, 0, 0],
    ));
    png.extend(png_chunk(b"IDAT", &deflate));
    png.extend(png_chunk(b"IEND", &[]));
    png
}

fn main() {
    let scene = std::env::args()
        .nth(1)
        .and_then(|value| value.parse::<u8>().ok())
        .unwrap_or(0);
    let warmup = std::env::args()
        .nth(2)
        .and_then(|value| value.parse::<u32>().ok())
        .unwrap_or(120);
    let frames = std::env::args()
        .nth(3)
        .and_then(|value| value.parse::<u32>().ok())
        .unwrap_or(120);

    let mut test = TestAppContext::single();
    let mut images = Vec::new();
    for tint_alpha in [255, 215, 175] {
        for mirror in [false, true] {
            images.push(Arc::new(Image::from_bytes(
                ImageFormat::Png,
                fixture_png(mirror, tint_alpha),
            )));
        }
    }
    let (_, mut visual) = test.add_window_view(|_, _| BenchView {
        scene,
        frame: 0,
        bench: Bench { images },
    });
    visual.simulate_resize(size(px(480.0), px(320.0)));

    for frame in 0..warmup {
        let _ = visual.update(|window, cx| {
            let root = window.root::<BenchView>().unwrap().unwrap();
            root.update(cx, |view, cx| {
                view.frame = frame;
                cx.notify();
            });
            window.draw(cx)
        });
        visual.run_until_parked();
    }

    let mut samples_ns = Vec::with_capacity(frames as usize);
    SHAPE_PAINT_NS.store(0, Ordering::Relaxed);
    for frame in 0..frames {
        let started = Instant::now();
        let _ = visual.update(|window, cx| {
            let root = window.root::<BenchView>().unwrap().unwrap();
            root.update(cx, |view, cx| {
                view.frame = frame;
                cx.notify();
            });
            window.draw(cx)
        });
        samples_ns.push(started.elapsed().as_nanos() as u64);
        visual.run_until_parked();
    }

    samples_ns.sort_unstable();
    let p50 = samples_ns[(samples_ns.len() - 1).min((samples_ns.len() * 50) / 100)];
    println!(
        "GPUI scene={} frames={} cpu_draw_p50_ns={} cpu_draw_max_ns={} shape_paint_ns={}",
        scene,
        frames,
        p50,
        samples_ns[samples_ns.len() - 1],
        SHAPE_PAINT_NS.load(Ordering::Relaxed)
    );
    test.quit();
}

struct Bench {
    images: Vec<Arc<Image>>,
}

impl Bench {
    fn text(&self, frame: u32) -> AnyElement {
        let mut root = div().size(px(480.0)).bg(rgb(0x20252bff)).overflow_hidden();
        for row in 0..14 {
            for column in 0..2 {
                let left = 8.0 + column as f32 * 240.0 - (frame % 8) as f32;
                let top = 8.0 + row as f32 * 22.0;
                let value = if row % 2 == 0 {
                    "Kryon: quick brown fox 012345"
                } else {
                    "UTF-8: café Ω 日"
                };
                root = root.child(
                    div()
                        .absolute()
                        .left(px(left))
                        .top(px(top))
                        .size(px(224.0))
                        .text_size(px(12.0 + (row % 3) as f32 * 2.0))
                        .text_color(rgba(0xe6aa8cdc))
                        .child(value),
                );
            }
        }
        root.into_any_element()
    }

    fn images(&self, frame: u32) -> AnyElement {
        let mut root = div().size(px(480.0)).bg(rgb(0x20252bff)).overflow_hidden();
        for row in 0..6 {
            for column in 0..8 {
                let left = 8.0 + column as f32 * 58.0;
                let top = 8.0 + row as f32 * 50.0;
                let image_index =
                    (row as usize % 3) * 2 + usize::from((row + column + frame) % 2 == 1);
                let mut tile = div()
                    .absolute()
                    .left(px(left))
                    .top(px(top))
                    .size(px(48.0))
                    .h(px(40.0));
                if column % 4 == 3 {
                    tile = tile
                        .left(px(left + 4.0))
                        .top(px(top + 3.0))
                        .size(px(36.0))
                        .h(px(32.0))
                        .rounded(px(8.0))
                        .overflow_hidden();
                }
                root = root.child(tile.child(img(self.images[image_index].clone()).size_full()));
            }
        }
        root.into_any_element()
    }

    fn shapes(&self, frame: u32) -> AnyElement {
        let mut root = div().size(px(480.0)).bg(rgb(0x20252bff)).overflow_hidden();

        root = root
            .child(
                div()
                    .absolute()
                    .left(px(8.0))
                    .top(px(8.0))
                    .size(px(48.0))
                    .h(px(24.0))
                    .bg(rgb(0xc83c28ff)),
            )
            .child(
                div()
                    .absolute()
                    .left(px(32.0))
                    .top(px(8.0))
                    .size(px(48.0))
                    .h(px(24.0))
                    .bg(rgba(0x1464dc80)),
            )
            .child(
                div()
                    .absolute()
                    .left(px(96.0))
                    .top(px(8.0))
                    .size(px(40.0))
                    .h(px(24.0))
                    .rounded(px(12.0))
                    .bg(rgb(0x50c864ff)),
            )
            .child(
                div()
                    .absolute()
                    .left(px(360.0))
                    .top(px(8.0))
                    .size(px(44.0))
                    .h(px(20.0))
                    .overflow_hidden()
                    .text_size(px(18.0))
                    .child("MMMMMMMMMMMM"),
            );

        for row in 0..4 {
            for column in 0..6 {
                let left = 8.0 + column as f32 * 78.0 + (frame % 7) as f32;
                let top = 48.0 + row as f32 * 66.0;
                root = root.child(
                    div()
                        .absolute()
                        .left(px(left + 3.0))
                        .top(px(top + 3.0))
                        .size(px(58.0))
                        .h(px(42.0))
                        .rounded(px(9.0))
                        .bg(rgba(0x3c64d2aa))
                        .border_2()
                        .border_color(rgba(0xf0dc78d2)),
                );
            }
        }

        root.child(
            canvas(
                move |_, _, _| {},
                move |_, _, window, _| {
                    let started = Instant::now();
                    for row in 0..4 {
                        for column in 0..6 {
                            let left = 8.0 + column as f32 * 78.0;
                            let top = 48.0 + row as f32 * 66.0;
                            let mut line = gpui::PathBuilder::stroke(px(2.0));
                            line.move_to(point(px(left), px(top + 48.0)));
                            line.line_to(point(px(left + 64.0), px(top)));
                            if let Ok(path) = line.build() {
                                window.paint_path(path, rgba(0x78f0a0b4));
                            }

                            let mut triangle = gpui::PathBuilder::fill();
                            triangle.add_polygon(
                                &[
                                    point(px(left + 10.0), px(top + 38.0)),
                                    point(px(left + 32.0), px(top + 10.0)),
                                    point(px(left + 54.0), px(top + 38.0)),
                                ],
                                true,
                            );
                            if let Ok(path) = triangle.build() {
                                window.paint_path(path, rgba(0xf0645080));
                            }
                        }
                    }
                    SHAPE_PAINT_NS
                        .fetch_add(started.elapsed().as_nanos() as u64, Ordering::Relaxed);
                },
            )
            .absolute()
            .size(px(480.0))
            .h(px(320.0)),
        )
        .into_any_element()
    }

    fn view(&self, frame: u32, zoom_scene: bool) -> AnyElement {
        let zoom = if zoom_scene { 1.0 + (frame % 5) as f32 * 0.25 } else { 1.0 };
        let scroll_y = if zoom_scene { 0 } else { frame * 2 };
        let mut root = div()
            .size(px(480.0))
            .bg(rgb(0x20252bff))
            .overflow_hidden()
            .child(
                div()
                    .absolute()
                    .left(px(8.0))
                    .top(px(8.0))
                    .text_size(px(18.0))
                    .child("Scroll / zoom: identical content"),
            );
        for row in 0..10 {
            for column in 0..6 {
                let left = 8.0 + column as f32 * 78.0 * zoom;
                let top = 40.0 + (row as f32 * 46.0 - scroll_y as f32) * zoom;
                root = root
                    .child(
                        div()
                            .absolute()
                            .left(px(left))
                            .top(px(top))
                            .size(px(32.0 * zoom))
                            .h(px(24.0 * zoom))
                            .child(img(self.images[0].clone()).size_full()),
                    )
                    .child(
                        div()
                            .absolute()
                            .left(px(left))
                            .top(px(top + 26.0 * zoom))
                            .text_size(px(12.0 * zoom))
                            .child("Tile 0123"),
                    );
            }
        }
        root.into_any_element()
    }
}

struct BenchView {
    scene: u8,
    frame: u32,
    bench: Bench,
}

impl Render for BenchView {
    fn render(&mut self, _: &mut Window, _: &mut Context<Self>) -> AnyElement {
        if self.scene == 0 {
            self.bench.text(self.frame)
        } else if self.scene == 1 {
            self.bench.images(self.frame)
        } else if self.scene == 2 {
            self.bench.shapes(self.frame)
        } else {
            self.bench.view(self.frame, self.scene == 4)
        }
    }
}
"""


def capture(command, env=None):
    return subprocess.check_output(list(map(str, command)), cwd=ROOT, env=env,
                                   text=True).strip()


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def percentile(values, value):
    return sorted(values)[max(0, math.ceil(len(values) * value) - 1)]


def bounded_run(command, *, env, output, timeout):
    process = subprocess.Popen(list(map(str, command)), cwd=ROOT, env=env,
                               stdout=output, stderr=subprocess.STDOUT,
                               start_new_session=True)
    try:
        return process.wait(timeout=timeout)
    except BaseException:
        try:
            os.killpg(process.pid, signal.SIGTERM)
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGKILL)
            process.wait()
        except ProcessLookupError:
            pass
        raise


def validate_cargo(cargo):
    cargo = cargo.resolve()
    if cargo.name != 'cargo' or not cargo.is_file() or not os.access(cargo, os.X_OK):
        raise ValueError(f'--cargo must be an executable cargo binary, not {cargo}')
    version = capture([cargo, '--version'])
    if version != 'cargo 1.94.1 (29ea6fb6a 2026-03-24)':
        raise ValueError('GPUI harness requires cargo 1.94.1: ' + version)
    return cargo, version


def write_project():
    (BUILD / 'src/bin').mkdir(parents=True, exist_ok=True)
    (BUILD / 'Cargo.toml').write_text(CARGO_TOML)
    (BUILD / 'src/main.rs').write_text(BENCH_SOURCE.lstrip())
    (BUILD / 'src/bin/gpu_probe.rs').write_text(GPU_PROBE)
    if not XKB_LIBRARY.is_file():
        raise ValueError(f'missing system library: {XKB_LIBRARY}')
    sysroot = BUILD / 'sysroot'
    sysroot.mkdir(exist_ok=True)
    xkb_link = sysroot / 'libxkbcommon-x11.so'
    if xkb_link.is_symlink() and xkb_link.resolve() == XKB_LIBRARY:
        return sysroot
    if xkb_link.is_symlink() or xkb_link.exists():
        xkb_link.unlink()
    xkb_link.symlink_to(XKB_LIBRARY)
    return sysroot


def base_env(sysroot, cargo):
    env = dict(os.environ)
    for key in list(env):
        if key in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY',
                   'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER', 'DRI_PRIME',
                   'NODEVICE_SELECT', 'EGL_PLATFORM', 'RUSTFLAGS', 'RUSTC') or key.startswith(
                       ('VK_', 'MESA_', 'GALLIUM_', 'LIBGL_', 'BENCH_', 'CARGO_')):
            env.pop(key, None)
    old_library_path = env.get('LD_LIBRARY_PATH', '')
    library_path = str(sysroot) + (os.pathsep + old_library_path if old_library_path else '')
    env.update(YUE_DESKTOP_RECOVERY='0', TMPDIR=str(WORK), MAKEFLAGS='-j1',
               CARGO_BUILD_JOBS='1', RUSTFLAGS='-L ' + str(sysroot),
               RUSTC=str(cargo.with_name('rustc')),
               LD_LIBRARY_PATH=library_path)
    return env


def device_identity(node):
    node = node.resolve()
    if node.parent != Path('/dev/dri') or not re.fullmatch(r'renderD[0-9]+', node.name) or not node.is_char_device():
        raise ValueError('--device must be an existing /dev/dri/renderD* character device')
    pci_path = (Path('/sys/class/drm') / node.name / 'device').resolve()
    identity = {
        'node': str(node), 'pci': pci_path.name,
        'vendor': (pci_path / 'vendor').read_text().strip().removeprefix('0x'),
        'device': (pci_path / 'device').read_text().strip().removeprefix('0x'),
        'driver': (pci_path / 'driver').resolve().name,
    }
    if identity['vendor'] != '1002':
        raise ValueError('GPU probe currently requires the qualified AMD/Linux DRM device')
    return identity


def run_benchmark(cargo, env, trials, warmup, frames, logs):
    command = [cargo, 'build', '--manifest-path', BUILD / 'Cargo.toml',
               '--release', '--bins']
    with (logs / 'build.log').open('w') as output:
        bounded_run(command, env=env, output=output, timeout=900)
    executable = BUILD / 'target/release/gpui-render-benchmark'
    results = {}
    for scene_index, scene in enumerate(SCENES):
        samples = []
        for trial in range(trials):
            log = logs / f'scene-{scene_index}-trial-{trial}.log'
            with log.open('w') as output:
                code = bounded_run([executable, scene_index, warmup, frames],
                                   env=env, output=output, timeout=300)
            if code != 0:
                raise ValueError(f'GPUI {scene} trial {trial} exited {code}; see {log}')
            records = parse_benchmark(log.read_text())
            if not records:
                raise ValueError(f'GPUI {scene} trial {trial} emitted no benchmark record')
            samples.append(records)
        results[scene] = summarize(samples)
    return results, executable


def parse_benchmark(text):
    match = re.search(r'^GPUI scene=([0-9]+) frames=([0-9]+) cpu_draw_p50_ns=([0-9]+) '
                      r'cpu_draw_max_ns=([0-9]+) shape_paint_ns=([0-9]+)$', text, re.M)
    if not match:
        return None
    values = list(map(int, match.groups()))
    return {'scene_index': values[0], 'frames': values[1],
            'cpu_draw_p50_ns': values[2], 'cpu_draw_max_ns': values[3],
            'shape_paint_ns': values[4]}


def summarize(samples):
    return {
        'trials': len(samples),
        'cpu_draw_p50_us_of_trial_p50s': percentile(
            [row['cpu_draw_p50_ns'] / 1000 for row in samples], 0.5),
        'cpu_draw_max_us': max(row['cpu_draw_max_ns'] / 1000 for row in samples),
        'shape_paint_total_us': sum(row['shape_paint_ns'] / 1000 for row in samples),
        'raw_trials': samples,
    }


def start_weston(env, identity, runtime, socket_name, log):
    runtime.mkdir(mode=0o700, parents=True, exist_ok=True)
    socket = runtime / socket_name
    if len(os.fsencode(socket)) >= 108:
        raise ValueError(f'Wayland socket path exceeds Unix limits: {socket}')
    weston_env = dict(
        env, XDG_RUNTIME_DIR=str(runtime),
        DRI_PRIME='pci-' + identity['pci'].replace(':', '_').replace('.', '_'),
        MESA_VK_DEVICE_SELECT=identity['vendor'] + ':' + identity['device'],
        MESA_VK_DEVICE_SELECT_FORCE_DEFAULT_DEVICE='1',
        BENCH_DEVICE_EVIDENCE='1')
    process = subprocess.Popen(['weston', '--backend=headless', '--renderer=gl',
                                '--socket=' + socket_name, '--idle-time=0',
                                '--width=480', '--height=320'],
                               cwd=ROOT, env=weston_env, stdout=log,
                               stderr=subprocess.STDOUT, start_new_session=True)
    deadline = time.monotonic() + 8
    while time.monotonic() < deadline:
        if socket.exists():
            return process, socket
        if process.poll() is not None:
            break
        time.sleep(0.05)
    process.terminate()
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()
    raise ValueError(f'private headless Weston did not expose {socket}; see {log.name}')


def run_gpu_probe(cargo, env, identity, logs):
    runtime = WESTON_RUNTIME
    socket_name = f'gpui-bench-{os.getpid()}'
    weston_log_path = logs / 'weston.log'
    probe_log_path = logs / 'gpu-probe.log'
    with weston_log_path.open('w') as weston_log:
        weston, socket = start_weston(env, identity, runtime, socket_name, weston_log)
    try:
        probe_env = dict(env, XDG_RUNTIME_DIR=str(runtime), WAYLAND_DISPLAY=socket_name,
                         DRI_PRIME='pci-' + identity['pci'].replace(':', '_').replace('.', '_'),
                         MESA_VK_DEVICE_SELECT=identity['vendor'] + ':' + identity['device'],
                         MESA_VK_DEVICE_SELECT_FORCE_DEFAULT_DEVICE='1',
                         BENCH_DEVICE_EVIDENCE='1')
        with probe_log_path.open('w') as output:
            code = bounded_run([BUILD / 'target/release/gpu_probe'], env=probe_env,
                               output=output, timeout=45)
    finally:
        weston.terminate()
        try:
            weston.wait(timeout=5)
        except subprocess.TimeoutExpired:
            weston.kill()
            weston.wait()
        socket.unlink(missing_ok=True)
    weston_text = weston_log_path.read_text()
    probe_text = probe_log_path.read_text()
    return classify_gpu_probe(code, probe_text, weston_text, identity, logs)


def classify_gpu_probe(code, probe_text, weston_text, identity, logs):
    expected = ('platform/linux/wayland/client.rs:503' in probe_text
                and 'called `Option::unwrap()` on a `None` value' in probe_text)
    renderer_lines = [line.strip() for line in weston_text.splitlines()
                      if 'renderer' in line.lower() and 'no-op' not in line.lower()]
    renderer = next((line for line in renderer_lines
                     if 'GL renderer:' in line or 'Using GL renderer' in line),
                    next(iter(renderer_lines), ''))
    result = {
        'status': 'wl_seat_limitation' if expected else 'unexpected_failure' if code else 'passed',
        'exit_code': code,
        'device': identity,
        'weston_log': str(logs / 'weston.log'),
        'gpu_probe_log': str(logs / 'gpu-probe.log'),
        'weston_renderer_evidence': renderer,
        'limitation': ('GPUI 0.2.2 Wayland client.rs:503 unwraps the missing wl_seat global; '
                       'headless Weston exposes no seat, so true GPU presentation cannot start'),
    }
    if expected and 'gl-renderer' not in weston_text and 'OpenGL' not in weston_text:
        result['status'] = 'weston_gl_not_verified'
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--smoke', action='store_true')
    parser.add_argument('--cargo', type=Path, default=DEFAULT_CARGO)
    parser.add_argument('--output', type=Path, default=WORK / 'gpui-render-benchmark.json')
    parser.add_argument('--trials', type=int)
    parser.add_argument('--frames', type=int)
    parser.add_argument('--warmup', type=int)
    parser.add_argument('--gpu-probe', action='store_true')
    parser.add_argument('--device', type=Path,
                        help='gpu probe: exact AMD /dev/dri/renderD* node')
    args = parser.parse_args()
    trials = args.trials or (2 if args.smoke else 5)
    frames = args.frames or (3 if args.smoke else 120)
    warmup = args.warmup or (1 if args.smoke else 120)
    if not 1 <= trials <= 30 or not 1 <= frames <= 10000 or not 1 <= warmup <= 10000:
        parser.error('require 1..30 trials and 1..10000 frames/warmup')
    output = args.output.resolve()
    output.relative_to(ROOT)
    for path in (WORK, output):
        subprocess.run(['git', 'check-ignore', '-q', str(path)], cwd=ROOT, check=True)
    WORK.mkdir(parents=True, exist_ok=True)
    logs = WORK / (output.stem + '-logs')
    logs.mkdir(exist_ok=True)
    identity = None
    if args.gpu_probe:
        if args.device is None:
            parser.error('--gpu-probe requires --device')
        identity = device_identity(args.device)
    elif args.device:
        parser.error('--device applies only to --gpu-probe')
    cargo, cargo_version = validate_cargo(args.cargo)
    sysroot = write_project()
    env = base_env(sysroot, cargo)
    scenes, executable = run_benchmark(cargo, env, trials, warmup, frames, logs)
    report = {
        'schema': 1,
        'status': 'passed',
        'completed_utc': datetime.now(timezone.utc).isoformat(),
        'mode': 'GPUI 0.2.2 TestAppContext CPU draw benchmark',
        'timing_scope': ('one full GPUI root render/layout/prepaint/paint per frame; retained '
                         'scene only, no GPU execution, presentation, or compositor wall time'),
        'comparability_limit': ('Not equivalent to Kryon compose-only timings and not a '
                                'GPU wall-throughput comparison'),
        'workload_limit': ('GPUI workloads mirror node counts and geometry approximately; '
                           'text/font stacks and image filtering differ'),
        'trials': trials,
        'warmup_frames': warmup,
        'frames': frames,
        'gpui_version': '0.2.2',
        'libc_version': '0.2.186',
        'cargo': str(cargo),
        'cargo_version': cargo_version,
        'rustc_version': capture([cargo.with_name('rustc'), '--version'], env),
        'environment': {
            'desktop': 'none',
            'YUE_DESKTOP_RECOVERY': env['YUE_DESKTOP_RECOVERY'],
            'variables_scrubbed': ['DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY',
                                   'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER'],
            'local_xkb_sysroot': str(sysroot),
        },
        'logs': str(logs),
        'scenes': scenes,
        'hashes': {
            str(path.relative_to(BUILD)): digest(path)
            for path in (BUILD / 'Cargo.toml', BUILD / 'src/main.rs',
                         BUILD / 'src/bin/gpu_probe.rs', executable)
        },
    }
    if args.gpu_probe:
        report['gpu_probe'] = run_gpu_probe(cargo, env, identity, logs)
        if report['gpu_probe']['status'] not in ('wl_seat_limitation', 'passed'):
            report['status'] = 'failed'
    output.write_text(json.dumps(report, indent=2, sort_keys=True) + '\n')
    if report['status'] != 'passed':
        raise ValueError('GPUI benchmark failed: ' + json.dumps(report.get('gpu_probe', {}),
                                                               sort_keys=True))
    print(json.dumps({'output': str(output), 'status': report['status']}))


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        sys.exit('GPUI benchmark failed: ' + str(error))
