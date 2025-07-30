# Kryon Renderer Examples

This directory contains examples demonstrating the multiple renderer backends in Kryon, all capable of rendering the same .kry UI definition files.

## Available Examples

### 1. `basic_renderer_demo`
**Simple demonstration of software and GPU renderers**

```bash
./basic_renderer_demo
```

Shows:
- Loading and parsing of `hello-world.kry`
- Software renderer (CPU-based pixel buffer)
- GPU renderer (hardware accelerated, if available)
- Basic performance timing
- Success/failure status for each renderer

### 2. `renderer_comparison_demo`
**Comprehensive comparison of all available renderers**

```bash
./renderer_comparison_demo [kry_file]
```

Features:
- Benchmarks multiple renderers with the same .kry file
- Performance metrics (render time, memory usage)
- Side-by-side visual comparison
- Real-time interactive demo
- Saves output images for visual inspection

Example usage:
```bash
./renderer_comparison_demo hello-world.kry
./renderer_comparison_demo complex-ui.kry
```

### 3. `kry_renderer_showcase`
**Loads multiple .kry files and shows them with different renderers**

```bash
./kry_renderer_showcase
```

Capabilities:
- Auto-discovers all .kry files in the examples directory
- Tests renderer capabilities (transparency, text, animations)
- Creates a visual gallery showing all combinations
- Interactive mode for switching between examples and renderers
- Terminal ASCII output (if terminal renderer available)

## Available Renderers

### Software Renderer
- **Type:** CPU-based rasterization
- **Pros:** Always available, predictable performance, no GPU dependencies
- **Cons:** Slower for complex scenes, limited to basic features
- **Best for:** Embedded systems, servers, development/testing

### WebGL Renderer  
- **Type:** GPU-accelerated via WebGL
- **Pros:** Hardware acceleration, modern graphics features, web compatibility
- **Cons:** Requires WebGL context, may not be available on all systems
- **Best for:** Web applications, high-performance graphics

### OpenGL Renderer
- **Type:** GPU-accelerated via native OpenGL
- **Pros:** Full GPU acceleration, mature API, wide platform support
- **Cons:** Requires OpenGL drivers, context management complexity
- **Best for:** Desktop applications, games

### Terminal Renderer
- **Type:** ASCII/Unicode text output
- **Pros:** Works in any terminal, minimal resource usage, debugging friendly
- **Cons:** Limited visual fidelity, text-only output
- **Best for:** CLI tools, server environments, debugging

## KRY Test Files

### `hello-world.kry`
Simple example with:
- Container with styling
- Text element
- Basic layout properties

### `complex-ui.kry`
Advanced example featuring:
- Nested containers and flexbox layout
- Multiple text styles and colors
- Buttons and interactive elements
- Grid layout sections
- Progress bars and visual elements
- Comprehensive styling tests

## Build Instructions

From the project root:

```bash
mkdir build && cd build
cmake -DKRYON_BUILD_EXAMPLES=ON ..
make
```

Examples will be built to `build/bin/examples/`

## Output Files

The demos generate several output files:

- `software_output.ppm` - Software renderer output
- `gpu_output.ppm` - GPU renderer output  
- `gallery_*.ppm` - Comparison gallery images
- `output_*.txt` - Terminal renderer ASCII output

## Performance Analysis

Run the comparison demo to see performance characteristics:

```bash
./renderer_comparison_demo complex-ui.kry
```

Expected output:
```
=== Performance Comparison ===
Renderer              | Time (s) | Memory (KB) | Relative Speed
---------------------|----------|-------------|---------------
Software CPU         |    0.045 |       256.0 | 1.00x
WebGL GPU           |    0.012 |       512.0 | 3.75x
OpenGL GPU          |    0.010 |       480.0 | 4.50x

=== Recommendations ===
• Fastest: OpenGL GPU (0.010s per 100 renders)
• Most efficient: Software CPU (best time/memory ratio)
• Use OpenGL GPU for maximum performance
• Use Software CPU for resource-constrained environments
```

## Renderer Selection Guide

**Choose Software Renderer when:**
- Running on embedded systems
- GPU not available or unreliable
- Debugging layout issues
- Consistent cross-platform behavior needed

**Choose GPU Renderer when:**
- Performance is critical
- Complex graphics/animations
- Large UI elements or high resolution
- Modern hardware available

**Choose Terminal Renderer when:**
- Building CLI applications
- Server-side UI generation
- Debugging without graphics
- Accessibility requirements

## Troubleshooting

**"GPU renderer not available"**
- Install graphics drivers
- Check OpenGL/WebGL support
- Try software renderer as fallback

**"Failed to parse KRY file"**
- Check file syntax against KRY specification
- Ensure all @style blocks are properly closed
- Verify property names use camelCase

**"No output files generated"**
- Check write permissions in current directory
- Ensure renderers initialized successfully
- Try basic_renderer_demo first for simpler testing

## Integration Examples

These demos show how to integrate multiple renderers in your own applications:

```c
// Initialize available renderers
KryonRenderer* renderers[3];
int renderer_count = 0;

// Try GPU first, fallback to software
if ((renderers[renderer_count] = kryon_gpu_renderer_create(&gpu_config, platform))) {
    renderer_count++;
}
if ((renderers[renderer_count] = kryon_software_renderer_create(&software_config))) {
    renderer_count++;
}

// Render with best available
for (int i = 0; i < renderer_count; i++) {
    if (kryon_renderer_render_tree(renderers[i], tree)) {
        printf("Rendered successfully with %s\n", renderer_names[i]);
        break;
    }
}
```

This demonstrates Kryon's key strength: **write once, render anywhere** - the same .kry UI definitions work with any renderer backend.