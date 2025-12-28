#ifndef KRYON_ANDROID_SHADERS_H
#define KRYON_ANDROID_SHADERS_H

// ============================================================================
// GLSL ES 3.0 Shader Programs
// ============================================================================

// ============================================================================
// Basic Color Shader (for rectangles, lines)
// ============================================================================

static const char* VERTEX_SHADER_COLOR =
    "#version 300 es\n"
    "precision highp float;\n"
    "\n"
    "layout(location = 0) in vec2 aPosition;\n"
    "layout(location = 1) in vec4 aColor;\n"
    "\n"
    "uniform mat4 uProjection;\n"
    "\n"
    "out vec4 vColor;\n"
    "\n"
    "void main() {\n"
    "    gl_Position = uProjection * vec4(aPosition, 0.0, 1.0);\n"
    "    vColor = aColor;\n"
    "}\n";

static const char* FRAGMENT_SHADER_COLOR =
    "#version 300 es\n"
    "precision mediump float;\n"
    "\n"
    "in vec4 vColor;\n"
    "out vec4 fragColor;\n"
    "\n"
    "void main() {\n"
    "    fragColor = vColor;\n"
    "}\n";

// ============================================================================
// Texture Shader (for images, text glyphs)
// ============================================================================

static const char* VERTEX_SHADER_TEXTURE =
    "#version 300 es\n"
    "precision highp float;\n"
    "\n"
    "layout(location = 0) in vec2 aPosition;\n"
    "layout(location = 1) in vec2 aTexCoord;\n"
    "layout(location = 2) in vec4 aColor;\n"
    "\n"
    "uniform mat4 uProjection;\n"
    "\n"
    "out vec2 vTexCoord;\n"
    "out vec4 vColor;\n"
    "\n"
    "void main() {\n"
    "    gl_Position = uProjection * vec4(aPosition, 0.0, 1.0);\n"
    "    vTexCoord = aTexCoord;\n"
    "    vColor = aColor;\n"
    "}\n";

static const char* FRAGMENT_SHADER_TEXTURE =
    "#version 300 es\n"
    "precision mediump float;\n"
    "\n"
    "in vec2 vTexCoord;\n"
    "in vec4 vColor;\n"
    "out vec4 fragColor;\n"
    "\n"
    "uniform sampler2D uTexture;\n"
    "\n"
    "void main() {\n"
    "    vec4 texColor = texture(uTexture, vTexCoord);\n"
    "    fragColor = texColor * vColor;\n"
    "}\n";

// ============================================================================
// Text Shader (grayscale texture with color tint)
// ============================================================================

static const char* VERTEX_SHADER_TEXT =
    "#version 300 es\n"
    "precision highp float;\n"
    "\n"
    "layout(location = 0) in vec2 aPosition;\n"
    "layout(location = 1) in vec2 aTexCoord;\n"
    "layout(location = 2) in vec4 aColor;\n"
    "\n"
    "uniform mat4 uProjection;\n"
    "\n"
    "out vec2 vTexCoord;\n"
    "out vec4 vColor;\n"
    "\n"
    "void main() {\n"
    "    gl_Position = uProjection * vec4(aPosition, 0.0, 1.0);\n"
    "    vTexCoord = aTexCoord;\n"
    "    vColor = aColor;\n"
    "}\n";

static const char* FRAGMENT_SHADER_TEXT =
    "#version 300 es\n"
    "precision mediump float;\n"
    "\n"
    "in vec2 vTexCoord;\n"
    "in vec4 vColor;\n"
    "out vec4 fragColor;\n"
    "\n"
    "uniform sampler2D uTexture;\n"
    "\n"
    "void main() {\n"
    "    float alpha = texture(uTexture, vTexCoord).r;\n"
    "    fragColor = vec4(vColor.rgb, vColor.a * alpha);\n"
    "}\n";

// ============================================================================
// Rounded Rectangle Shader (with smooth corners)
// ============================================================================

static const char* VERTEX_SHADER_ROUNDED_RECT =
    "#version 300 es\n"
    "precision highp float;\n"
    "\n"
    "layout(location = 0) in vec2 aPosition;\n"
    "layout(location = 1) in vec2 aLocalPos;\n"
    "layout(location = 2) in vec4 aColor;\n"
    "\n"
    "uniform mat4 uProjection;\n"
    "\n"
    "out vec2 vLocalPos;\n"
    "out vec4 vColor;\n"
    "\n"
    "void main() {\n"
    "    gl_Position = uProjection * vec4(aPosition, 0.0, 1.0);\n"
    "    vLocalPos = aLocalPos;\n"
    "    vColor = aColor;\n"
    "}\n";

static const char* FRAGMENT_SHADER_ROUNDED_RECT =
    "#version 300 es\n"
    "precision mediump float;\n"
    "\n"
    "in vec2 vLocalPos;\n"
    "in vec4 vColor;\n"
    "out vec4 fragColor;\n"
    "\n"
    "uniform vec2 uSize;\n"
    "uniform float uRadius;\n"
    "\n"
    "float roundedBoxSDF(vec2 pos, vec2 size, float radius) {\n"
    "    vec2 q = abs(pos) - size + radius;\n"
    "    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - radius;\n"
    "}\n"
    "\n"
    "void main() {\n"
    "    vec2 center = uSize * 0.5;\n"
    "    vec2 pos = vLocalPos - center;\n"
    "    \n"
    "    float dist = roundedBoxSDF(pos, uSize * 0.5, uRadius);\n"
    "    float alpha = 1.0 - smoothstep(-1.0, 1.0, dist);\n"
    "    \n"
    "    fragColor = vec4(vColor.rgb, vColor.a * alpha);\n"
    "}\n";

// ============================================================================
// Gradient Shader (linear gradients)
// ============================================================================

static const char* VERTEX_SHADER_GRADIENT =
    "#version 300 es\n"
    "precision highp float;\n"
    "\n"
    "layout(location = 0) in vec2 aPosition;\n"
    "layout(location = 1) in float aGradientT;\n"
    "\n"
    "uniform mat4 uProjection;\n"
    "\n"
    "out float vGradientT;\n"
    "\n"
    "void main() {\n"
    "    gl_Position = uProjection * vec4(aPosition, 0.0, 1.0);\n"
    "    vGradientT = aGradientT;\n"
    "}\n";

static const char* FRAGMENT_SHADER_GRADIENT =
    "#version 300 es\n"
    "precision mediump float;\n"
    "\n"
    "in float vGradientT;\n"
    "out vec4 fragColor;\n"
    "\n"
    "uniform vec4 uColorStart;\n"
    "uniform vec4 uColorEnd;\n"
    "\n"
    "void main() {\n"
    "    fragColor = mix(uColorStart, uColorEnd, vGradientT);\n"
    "}\n";

// ============================================================================
// Shadow Shader (for drop shadows and box shadows)
// ============================================================================

static const char* VERTEX_SHADER_SHADOW =
    "#version 300 es\n"
    "precision highp float;\n"
    "\n"
    "layout(location = 0) in vec2 aPosition;\n"
    "layout(location = 1) in vec2 aLocalPos;\n"
    "\n"
    "uniform mat4 uProjection;\n"
    "\n"
    "out vec2 vLocalPos;\n"
    "\n"
    "void main() {\n"
    "    gl_Position = uProjection * vec4(aPosition, 0.0, 1.0);\n"
    "    vLocalPos = aLocalPos;\n"
    "}\n";

static const char* FRAGMENT_SHADER_SHADOW =
    "#version 300 es\n"
    "precision mediump float;\n"
    "\n"
    "in vec2 vLocalPos;\n"
    "out vec4 fragColor;\n"
    "\n"
    "uniform vec2 uShadowOffset;\n"
    "uniform float uShadowBlur;\n"
    "uniform vec4 uShadowColor;\n"
    "uniform vec2 uBoxSize;\n"
    "uniform float uBoxRadius;\n"
    "\n"
    "float roundedBoxSDF(vec2 pos, vec2 size, float radius) {\n"
    "    vec2 q = abs(pos) - size + radius;\n"
    "    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - radius;\n"
    "}\n"
    "\n"
    "void main() {\n"
    "    vec2 center = uBoxSize * 0.5;\n"
    "    vec2 pos = vLocalPos - center - uShadowOffset;\n"
    "    \n"
    "    float dist = roundedBoxSDF(pos, uBoxSize * 0.5, uBoxRadius);\n"
    "    \n"
    "    // Smooth shadow falloff\n"
    "    float shadow = 1.0 - smoothstep(-uShadowBlur, uShadowBlur, dist);\n"
    "    \n"
    "    fragColor = vec4(uShadowColor.rgb, uShadowColor.a * shadow);\n"
    "}\n";

// ============================================================================
// Shader Program Types
// ============================================================================

typedef enum {
    SHADER_PROGRAM_COLOR = 0,
    SHADER_PROGRAM_TEXTURE,
    SHADER_PROGRAM_TEXT,
    SHADER_PROGRAM_ROUNDED_RECT,
    SHADER_PROGRAM_GRADIENT,
    SHADER_PROGRAM_SHADOW,
    SHADER_PROGRAM_COUNT
} ShaderProgramType;

// ============================================================================
// Shader Source Lookup
// ============================================================================

static inline const char* get_vertex_shader_source(ShaderProgramType type) {
    switch (type) {
        case SHADER_PROGRAM_COLOR:
            return VERTEX_SHADER_COLOR;
        case SHADER_PROGRAM_TEXTURE:
            return VERTEX_SHADER_TEXTURE;
        case SHADER_PROGRAM_TEXT:
            return VERTEX_SHADER_TEXT;
        case SHADER_PROGRAM_ROUNDED_RECT:
            return VERTEX_SHADER_ROUNDED_RECT;
        case SHADER_PROGRAM_GRADIENT:
            return VERTEX_SHADER_GRADIENT;
        case SHADER_PROGRAM_SHADOW:
            return VERTEX_SHADER_SHADOW;
        default:
            return NULL;
    }
}

static inline const char* get_fragment_shader_source(ShaderProgramType type) {
    switch (type) {
        case SHADER_PROGRAM_COLOR:
            return FRAGMENT_SHADER_COLOR;
        case SHADER_PROGRAM_TEXTURE:
            return FRAGMENT_SHADER_TEXTURE;
        case SHADER_PROGRAM_TEXT:
            return FRAGMENT_SHADER_TEXT;
        case SHADER_PROGRAM_ROUNDED_RECT:
            return FRAGMENT_SHADER_ROUNDED_RECT;
        case SHADER_PROGRAM_GRADIENT:
            return FRAGMENT_SHADER_GRADIENT;
        case SHADER_PROGRAM_SHADOW:
            return FRAGMENT_SHADER_SHADOW;
        default:
            return NULL;
    }
}

#endif // KRYON_ANDROID_SHADERS_H
