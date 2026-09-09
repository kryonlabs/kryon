#include "runtime/surface.h"
#include <assert.h>

static void assert_track_equal(MotionTrack actual, MotionTrack expected)
{
    assert(actual.value == expected.value);
    assert(actual.origin == expected.origin);
    assert(actual.target == expected.target);
    assert(actual.elapsed_ms == expected.elapsed_ms);
}

int main(void)
{
    for (int step = 0; step <= 4; step++) {
        float hover = step / 4.0f;
        SurfaceLayer bevel = LightfieldLayer(5, 72, 38, 8, 1,
            0x006cffff, 0x006cffff, 0x006cffff, 0x409cffff,
            hover, 0, 0, false, 1, 0x00172dff);
        assert(bevel.y == FaceOffset(hover, 0, false) + 1 - hover);
        assert(bevel.y + bevel.height == FaceOffset(hover, 0, false) + 37);
        unsigned int light = LiftColor(0x006cffff, hover);
        float strength = 0.20f + 0.28f * 0.375f + 0.50f * hover + 0.25f * hover;
        assert(bevel.end_color == Opacity(GradientColor(light, 0xffffffff, 0.72f), strength));
    }
    const unsigned int bevel_lights[] = {0xff0000ff, 0x0000ffff, 0x00ff00ff, 0x808080ff};
    for (int index = 0; index < 4; index++) {
        SurfaceLayer bevel = LightfieldLayer(5, 72, 48, 8, 1,
            0x006cffff, 0x006cffff, bevel_lights[index], bevel_lights[index],
            0, 0, 0, false, 1, 0x00172dff);
        assert((bevel.end_color & 255) == (index < 2 ? 249 : 165));
        SurfaceLayer pressed = LightfieldLayer(5, 72, 48, 8, 1,
            0x006cffff, 0x006cffff, bevel_lights[index], bevel_lights[index],
            0, 1, 0, false, 1, 0x00172dff);
        assert((pressed.end_color & 255) == 0);
    }
    SurfaceLayer reflection = LightfieldLayer(8, 720, 40, 8, 1,
        0x006cff80, 0x006cffff, 0x006cffff, 0x409cffff,
        0, 0, 0, false, 1, 0x00172dff);
    unsigned int lifted = LiftColor(LiftColor(0x006cffff, 1), 1);
    SurfaceLayer borderless_hover = LightfieldLayer(5, 72, 36, 8, 1,
        0x092039ff, 0, 0x409cffff, 0x409cffff, 1, 0, 0, false, 1, 0x00172dff);
    assert(borderless_hover.end_color == Opacity(ChromaColor(0x409cffff, 255), 0.5775f));
    SurfaceLayer broad_face = LightfieldLayer(3, 720, 40, 8, 1,
        0x006cffff, 0x006cffff, 0x006cffff, 0x409cffff,
        0, 0, 0, false, 1, 0xffffffff);
    assert(broad_face.color == 0x006cffff);
    SurfaceLayer compact_face = LightfieldLayer(3, 72, 40, 8, 1,
        0x006cffff, 0x006cffff, 0x006cffff, 0x409cffff,
        0, 0, 0, false, 1, 0xffffffff);
    assert(compact_face.color == GradientColor(0x006cffff,
        GradientColor(0x006cffff, 0xffffffff, 0.3f), 0.5f));
    SurfaceLayer tall_light = LightfieldLayer(8, 72, 48, 8, 1,
        0x006cffff, 0x006cffff, 0x006cffff, 0x409cffff,
        0, 0, 0, false, 1, 0xffffffff);
    assert(tall_light.end_color == Opacity(GradientColor(lifted, 0xffffffff, 0.22f), 0.76f));
    assert(tall_light.blur == 0 && tall_light.inner_blur > 0);
    assert(SampleCoverage(tall_light, -1, 10, 1) == 0);
    assert(reflection.end_color == Opacity(GradientColor(lifted, 0xffffffff, 0.11f), 0.38f * (128.0f / 255)));
    assert(SampleCoverage(reflection, -1, 10, 1) == 0);
    assert(LiftColor(0x006cff80, 0) == 0x006cff80);
    assert(LiftColor(0x006cff80, 0.5f) == 0x008bff80);
    assert(LiftColor(0x006cff80, 1) == 0x00aaff80);
    assert(LiftColor(0x006cff00, 2) == 0x00aaff00);
    assert(LiftColor(0x80808080, 1) == 0x80808080);
    for (int step = 0; step <= 4; step++) {
        float press = step / 4.0f;
        SurfaceLayer field = LightfieldLayer(0, 720, 40, 8, 1,
            0x006cffff, 0x006cffff, 0x006cffff, 0x409cffff,
            0, press, 0, false, 1, 0x00172dff);
        float strength = 0.05f * (1 - press * 0.8f) + 0.25f * (1 - press);
        assert(field.color == Opacity(0x006cffff, strength));
    }
    for (int step = 0; step <= 4; step++) {
        float focus = step / 4.0f;
        SurfaceLayer shadow = LightfieldLayer(1, 72, 40, 8, 1,
            0xeef5ffff, 0x064cffff, 0x064cffff, 0x064cffff,
            0, 0, focus, false, 1, 0xffffffff);
        assert(shadow.color == (unsigned int)(24 * (1 - 0.5f * focus)));
    }
    for (int step = 0; step < 4; step++) {
        double phase = 997 + step * 0.25;
        Ring early = LoadingRing(72, 40, 18, phase, 0x006cff80, 0x092039ff);
        Ring late = LoadingRing(72, 40, 18, 150000000000.0 + phase, 0x006cff80, 0x092039ff);
        assert(late.start_angle == early.start_angle && late.end_angle == early.end_angle);
    }
    for (int step = 0; step <= 4; step++) {
        float press = step / 4.0f;
        SurfaceLayer volume = LightfieldLayer(8, 72, 40, 8, 1,
            0x006cffff, 0x006cffff, 0x006cffff, 0x409cffff,
            0, press, 0, false, 1, 0x00172dff);
        assert(volume.inner_blur == 20 + 12 * press && volume.blur == 0);
        assert(SampleCoverage(volume, -1, 10, 1) == 0);
        assert(SampleCoverage(volume, volume.width + 1, 10, 1) == 0);
    }
    for (int step = 0; step <= 4; step++) {
        float width = 160 + 120 * step;
        float depth = 0.35f + 0.13f * step / 4;
        SurfaceLayer face = LightfieldLayer(3, width, 40, 8, 1,
            0x006cff80, 0x006cff80, 0x006cff80, 0x006cff80,
            0, 0, 0, false, 1, 0xffffffff);
        assert(face.end_color == DepthColor(0x006cff80, depth));
        assert((face.color & 255) == 128);
    }
    for (int press_step = 0; press_step <= 2; press_step++) {
        float press = press_step * 0.5f;
        const unsigned int backgrounds[] = {0x00172dffu, 0x000050ffu, 0x006cffffu};
        const float biases[] = {1.0f, 0.5f, 0.0f};
        for (int sample = 0; sample < 3; sample++) {
            SurfaceLayer reflection = LightfieldLayer(8, 72, 40, 8, 1,
                backgrounds[sample], 0x006cffffu, 0x006cff80u, 0x409cffffu,
                0.5f, press, 0, false, 1, 0x00172dffu);
            assert(reflection.gradient_bias == biases[sample] * (1.0f - press));
            assert(SampleColor(reflection, 0) == reflection.color);
            assert(SampleColor(reflection, 1) == reflection.end_color);
            assert(reflection.blur == 0 && reflection.inner_blur > 0);
            assert(SampleCoverage(reflection, -1, 10, 1) == 0);
        }
    }
    for (int hover_step = 0; hover_step <= 2; hover_step++) {
        for (int press_step = 0; press_step <= 2; press_step++) {
            float hover = hover_step * 0.5f;
            float press = press_step * 0.5f;
            SurfaceLayer borderless = LightfieldLayer(1, 72, 40, 8, 1,
                0xf8fbffffu, 0, 0x064cffffu, 0x064cffffu,
                hover, press, 0, false, 1, 0xffffffffu);
            SurfaceLayer raised = LightfieldLayer(1, 72, 40, 8, 1,
                0xf8fbffffu, 0x064cffffu, 0x064cffffu, 0x064cffffu,
                hover, press, 0, false, 1, 0xffffffffu);
            float strength = 24.0f + 10.0f * hover - 20.0f * hover * (1.0f - press) - 18.0f * press;
            assert(borderless.color == (unsigned int)(strength * (0.5f + 0.5f * press)));
            assert(raised.color == (unsigned int)strength);
            assert(borderless.blur == raised.blur);
            assert(borderless.y == raised.y);
        }
    }
    const unsigned int alphas[] = {0, 128, 255};
    const unsigned int shadow_bodies[] = {0xffffffff, 0xd0d0d0ff, 0xffe7ffff, 0xffdfffff, 0x006cffff, 0x101828ff};
    const unsigned int shadow_opacities[] = {14, 24, 24, 34, 34, 34};
    for (int index = 0; index < 6; index++) {
        SurfaceLayer shadow = LightfieldLayer(1, 72, 40, 8, 1,
            shadow_bodies[index], 0x064cffff, 0x064cffff, 0x064cffff,
            1, 0, 0, false, 1, 0xffffffff);
        assert(shadow.color == shadow_opacities[index]);
    }
    for (int alpha_index = 0; alpha_index < 3; alpha_index++) {
        unsigned int alpha = alphas[alpha_index];
        SurfaceLayer base = LightfieldLayer(3, 72, 40, 8, 1,
            0xeef5ff00u | alpha, 0x064cffffu, 0x064cffffu, 0x064cff80u,
            0, 0, 0, false, 1, 0xffffffffu);
        unsigned int middle = GradientColor(base.color, base.end_color, 0.5f);
        for (int focus_step = 0; focus_step <= 2; focus_step++) {
            float focus = focus_step * 0.5f;
            SurfaceLayer focused = LightfieldLayer(3, 72, 40, 8, 1,
                0xeef5ff00u | alpha, 0x064cffffu, 0x064cffffu, 0x064cff80u,
                0, 0, focus, false, 1, 0xffffffffu);
            assert(focused.color == GradientColor(base.color, middle, 0.75f * focus));
            assert(focused.end_color == GradientColor(base.end_color, middle, 0.75f * focus));
            assert((focused.color & 255) == alpha && (focused.end_color & 255) == alpha);
        }
    }
    for (int alpha_index = 0; alpha_index < 3; alpha_index++) {
        unsigned int alpha = alphas[alpha_index];
        unsigned int border = 0x064cff00u | alpha;
        for (int hover_step = 0; hover_step <= 2; hover_step++) {
            for (int press_step = 0; press_step <= 2; press_step++) {
                float hover = hover_step * 0.5f;
                float press = press_step * 0.5f;
                float amount = hover * (1.0f - press);
                SurfaceLayer edge = LightfieldLayer(4, 72, 40, 8, 1,
                    0xeef5ffffu, border, border, 0x064cff80u,
                    hover, press, 0, false, 1, 0xffffffffu);
                assert(edge.color == GradientColor(border, 0xeef5ff00u | alpha, 0.65f * amount));
                assert(edge.end_color == GradientColor(border, 0xffffff00u | alpha, 0.30f * amount));
                assert(edge.gradient && (edge.color & 255) == alpha && (edge.end_color & 255) == alpha);
            }
        }
    }
    for(unsigned int index = 0; index < 3; index++) {
        unsigned int alpha = alphas[index];
        unsigned int light = 0x409cff00u | alpha;
        SurfaceLayer reflection = LightfieldLayer(8, 72, 40, 8, 1,
            0x0a223b00u | alpha, 0, light, light, 1, 0, 0, false, 1, 0x092039ffu);
        float boost = 15.0f / 16.0f;
        float strength = (0.38f + 0.32f) * (0.15f + 0.45f * boost);
        strength *= (float)alpha / 255.0f;
        unsigned int reflected = GradientColor(light, ChromaColor(light, 255), boost);
        unsigned int expected = Opacity(GradientColor(reflected, 0xffffff00u | alpha,
            0.22f * (1.0f - boost)), strength);
        assert(reflection.end_color == expected);
        assert(reflection.blur == 0 && reflection.inner_blur > 0 && reflection.gradient);
        assert(InnerBlurCoverage(-1, 10, reflection.width, reflection.height,
            reflection.radius, reflection.inner_blur) == 0);
    }
    assert(MaterialLayerCount(0) == 12 && MaterialLayerCount(1) == 3);
    for(int index = 0; index < MaterialLayerCount(0); index++) {
        SurfaceLayer flat = MaterialLayer(1, index, 80, 40, 8, 2,
            0x12345680u, 0x789abc80u, 0xffffffffu, 0xff000080u,
            1, 1, 1, false, 1, 0x092039ffu);
        assert(flat.blur == 0 && flat.inner_blur == 0 && flat.x == 0 && flat.y == 0);
        assert(index < 3 ? (flat.color & 255) == 128 : flat.color == 0);
        SurfaceLayer standard = MaterialLayer(0, index, 80, 40, 8, 2,
            0x12345680u, 0x789abc80u, 0xffffffffu, 0xff000080u,
            1, 1, 1, false, 1, 0x092039ffu);
        SurfaceLayer expected = LightfieldLayer(index, 80, 40, 8, 2,
            0x12345680u, 0x789abc80u, 0xffffffffu, 0xff000080u,
            1, 1, 1, false, 1, 0x092039ffu);
        assert(standard.color == expected.color && standard.end_color == expected.end_color);
        assert(standard.blur == expected.blur && standard.inner_blur == expected.inner_blur);
    }
    assert(MaterialOffset(1, 1, 1, false) == 0);
    const unsigned int face_alphas[] = {0, 128, 255};
    for(int index = 0; index < 3; index++) {
        unsigned int alpha = face_alphas[index];
        unsigned int background = 0x072f6100u | alpha;
        SurfaceLayer rest = LightfieldLayer(3, 72, 38, 8, 1,
            background, 0x064292ffu, 0x064292ffu, 0x409cffffu,
            0, 0, 0, true, 1, 0x092039ffu);
        assert(rest.color == GradientColor(background, alpha, 0.15f));
        assert(rest.end_color == ((GradientColor(background, 0x064292ffu, 0.30f) & 0xffffff00u) | alpha));
        for(int step = 1; step < 3; step++) {
            float amount = step * 0.5f;
            SurfaceLayer interacting = LightfieldLayer(3, 72, 38, 8, 1,
                background, 0x064292ffu, 0x064292ffu, 0x409cffffu,
                amount, amount, amount, true, 1, 0x092039ffu);
            assert(interacting.color == rest.color && interacting.end_color == rest.end_color);
        }
        SurfaceLayer bloom = LightfieldLayer(0, 72, 38, 8, 1,
            background, 0x064292ffu, 0x064292ffu, 0x409cffffu,
            1, 1, 1, true, 1, 0x092039ffu);
        assert((bloom.color & 255) == 0);
    }
    for(int a = 0; a < 3; a++) {
        for(int step = 0; step <= 2; step++) {
            float press = step * 0.5f;
            SurfaceLayer borderless = LightfieldLayer(3, 72, 40, 8, 1,
                0xf8fbff00u | face_alphas[a], 0, 0x064cffffu, 0x064cffffu,
                0, press, 0, false, 1, 0xffffffffu);
            SurfaceLayer bordered = LightfieldLayer(3, 72, 40, 8, 1,
                0xf8fbff00u | face_alphas[a], 0x064cffffu, 0x064cffffu, 0x064cffffu,
                0, press, 0, false, 1, 0xffffffffu);
            assert((borderless.color & 255) == face_alphas[a]);
            assert((borderless.end_color & 255) == face_alphas[a]);
            assert((bordered.end_color & 255) == face_alphas[a]);
            if(step == 0)
                assert(borderless.end_color == bordered.end_color);
            else
                assert((borderless.end_color >> 24) > (bordered.end_color >> 24));
        }
    }
    SurfaceLayer standard_rim = LightfieldLayer(5, 72, 40, 8, 1,
        0x006cffff, 0x006cffff, 0x006cffff, 0x409cffff,
        0, 0, 0, false, 1, 0x092039ff);
    SurfaceLayer tall_rim = LightfieldLayer(5, 72, 48, 8, 1,
        0x006cffff, 0x006cffff, 0x006cffff, 0x409cffff,
        0, 0, 0, false, 1, 0x092039ff);
    assert((standard_rim.end_color & 255) == 86);
    assert((tall_rim.end_color & 255) == 222);
    int faces = 0;
    for (int index = 0; index < MaterialLayerCount(0); index++) {
        SurfaceLayer layer = LightfieldLayer(index, 72, 40, 8, 1,
            0x006cffff, 0x006cffff, 0x006cffff, 0x409cff80,
            0.5f, 0.25f, 0.75f, false, 0, 0x092039ff);
        if (layer.is_face) {
            faces++;
            assert(layer.gradient && layer.stroke == 0 && layer.blur == 0);
            assert(layer.inner_blur == 0 && !layer.outside_only);
            assert(FillGradient(layer, true, 0xff000080, 0x0000ff00, 1).is_face);
        }
    }
    assert(faces == 1);
    SurfaceLayer focus_bloom = LightfieldLayer(11, 72, 40, 8, 1,
        0x006cffff, 0x006cffff, 0x006cffff, 0x409cff80,
        0, 0, 1, false, 1, 0x092039ff);
    assert(MaterialLayerCount(0) == 12 && focus_bloom.outside_only);
    assert((focus_bloom.color & 255) == 38);
    assert(SampleCoverage(focus_bloom, 36, 20, 1) == 0);
    assert(SampleCoverage(focus_bloom, -3, 20, 1) > 0);
    assert(SampleCoverage(focus_bloom, -9, 20, 1) == 0);
    const unsigned int focus_chromas[] = {0, 128, 160, 192, 255};
    const float focus_strengths[] = {0.15f, 0.15f, 0.225f, 0.30f, 0.30f};
    for (int edge = 0; edge < 5; edge++) {
        for (int visible = 0; visible <= 1; visible++) {
            unsigned int border = (focus_chromas[edge] << 24) | (visible ? 255 : 0);
            for (int alpha = 0; alpha <= 255; alpha += 85) {
                unsigned int focus_color = 0x409cff00 | (unsigned int)alpha;
                for (int step = 0; step <= 4; step++) {
                    float focus = step / 4.0f;
                    float strength = visible ? focus_strengths[edge] * focus : 0.15f * focus * 0.35f;
                    SurfaceLayer bloom = LightfieldLayer(11, 72, 40, 8, 1,
                        0x006cffff, border, border, focus_color,
                        0, 0, focus, false, 1, 0x092039ff);
                    assert(bloom.color == Opacity(DepthColor(focus_color, 1), strength));
                    assert(bloom.outside_only && SampleCoverage(bloom, 36, 20, 1) == 0);
                }
            }
        }
    }
    unsigned int previous_rim_alpha = 0;
    for (int height = 40; height <= 48; height += 2) {
        for (int step = 0; step <= 4; step++) {
            float press = step / 4.0f;
            float raised = (height - 40) / 8.0f;
            raised *= raised * (1 - press);
            SurfaceLayer contact = LightfieldLayer(2, 72, height, 8, 1,
                0x006cffff, 0x006cffff, 0x006cffff, 0x409cffff,
                0, press, 0, false, 1, 0x092039ff);
            assert(contact.blur == 2 + 3 * (1 - press) + 3 * raised);
            assert((contact.color & 255) == (unsigned int)(34 - 26 * press + 70 * raised));
        }
    }
    for (int height = 32; height <= 48; height += 4) {
        SurfaceLayer rim = LightfieldLayer(5, 72, height, 8, 1,
            0x006cffff, 0x006cffff, 0x006cffff, 0x409cffff,
            0, 0, 0, false, 1, 0xffffffff);
        assert((rim.end_color & 255) > previous_rim_alpha);
        assert(rim.x == 1 && rim.y == 1 && rim.width == 70);
        assert(rim.height == height - 2 && rim.radius == 7 && rim.stroke == 1);
        previous_rim_alpha = rim.end_color & 255;
    }
    unsigned int previous_contact_alpha = 256;
    for (int step = 0; step <= 4; step++) {
        SurfaceLayer contact = LightfieldLayer(2, 72, 40, 8, 1,
            0x006cffff, 0x006cffff, 0x006cffff, 0x409cffff,
            step / 4.0f, 0, 0, false, 1, 0xffffffff);
        unsigned int alpha = contact.end_color & 255;
        assert(alpha > 0 && alpha < previous_contact_alpha);
        assert(contact.blur == 8 + step);
        assert((contact.end_color >> 8) == (DepthColor(0x006cffff, 0.65f - 0.30f * (step / 4.0f)) >> 8));
        previous_contact_alpha = alpha;
    }
    for (int step = 0; step <= 4; step++) {
        float focused = step / 4.0f;
        SurfaceLayer contact = LightfieldLayer(2, 72, 40, 8, 1,
            0x006cffff, 0x006cffff, 0x006cffff, 0x409cffff,
            0, 0, focused, false, 1, 0xffffffff);
        assert((contact.end_color & 255) == (unsigned int)(110 - 40 * focused));
        assert((contact.color & 255) == 0 && contact.blur == 8 + 4 * focused);
    }
    for (int hover_step = 0; hover_step <= 4; hover_step++) {
        for (int focus_step = 0; focus_step <= 4; focus_step++) {
            float hover = hover_step / 4.0f;
            float focused = focus_step / 4.0f;
            float active = hover + focused;
            if (active > 1) {
                active = 1;
            }
            SurfaceLayer contact = LightfieldLayer(2, 72, 40, 8, 1,
                0x006cffff, 0x006cffff, 0x006cffff, 0x409cffff,
                hover, 0, focused, false, 1, 0xffffffff);
            assert(contact.blur == 8 + 4 * active);
            assert(contact.gradient && (contact.color & 255) == 0);
        }
    }
    for (int step = 0; step <= 4; step++) {
        float hover = step / 4.0f;
        SurfaceLayer reflection = LightfieldLayer(8, 72, 40, 8, 1,
            0x006cffff, 0x006cffff, 0x006cff80, 0x006cff80,
            hover, 0, 0, false, 1, 0x092039ff);
        unsigned int expected = GradientColor(0x006cff80, 0xffffff80,
            0.22f * (1 - 0.64f * (1 - hover)));
        assert((reflection.end_color >> 8) == (expected >> 8));
        assert((reflection.end_color & 255) <= 128);
        assert(SampleCoverage(reflection, -1, 20, 1) == 0);
    }
    SurfaceLayer light_focus_halo = LightfieldLayer(6, 72, 40, 8, 1, 0xeef5ffff,
        0x12882280, 0x12882280, 0x8844cc80, 0, 0, 1, false, 1, 0xffffffff);
    assert((light_focus_halo.color & 255) == 0);
    SurfaceLayer light_focus_border = LightfieldLayer(4, 72, 40, 8, 1, 0xeef5ffff,
        0x12882200, 0x12882200, 0x8844cc80, 0, 0, 1, false, 1, 0xffffffff);
    assert((light_focus_border.color & 255) == 0);
    SurfaceLayer light_hover = LightfieldLayer(3, 72, 36, 8, 1, 0x90c6fe80,
        0x006cffff, 0x006cffff, 0x006cffff, 1, 0, 0, false, 1, 0xffffffff);
    assert(light_hover.end_color == GradientColor(0x90c6fe80, 0xffffff80, 0.12f));
    assert((light_hover.color & 255) == 128 && (light_hover.end_color & 255) == 128);
    SurfaceLayer hover_reflection = LightfieldLayer(8, 72, 38, 8, 1, 0x006cffff,
        0x006cffff, 0x006cffff, 0x006cffff, 1, 0, 0, false, 1, 0x092039ff);
    assert(hover_reflection.x == 2 && hover_reflection.y == 1);
    assert(hover_reflection.width == 68 && hover_reflection.height == 34);
    assert(SampleCoverage(hover_reflection, 30, hover_reflection.height, 1) == 0);
    SurfaceLayer hover_contact = LightfieldLayer(2, 72, 38, 8, 1, 0x006cffff,
        0x006cffff, 0x006cffff, 0x006cffff, 1, 0, 0, false, 1, 0x092039ff);
    assert(hover_contact.y - FaceOffset(1, 0, false) == 1);
    assert(hover_contact.blur == 7);
    float previous_contact = 1;
    for(int distance = 0; distance < 7; distance++) {
        float coverage = SampleCoverage(hover_contact, 30, hover_contact.height + distance, 1);
        assert(coverage < previous_contact && coverage >= 0);
        previous_contact = coverage;
    }
    for (unsigned int alpha = 0; alpha <= 255; alpha += 255) {
        SurfaceLayer narrow = LightfieldLayer(3, 72, 40, 8, 1, 0x18385800 | alpha,
            0x183858ff, 0x183858ff, 0x006cffff, 0, 0, 0, false, 1, 0x092039ff);
        SurfaceLayer wide = LightfieldLayer(3, 720, 40, 8, 1, 0x18385800 | alpha,
            0x183858ff, 0x183858ff, 0x006cffff, 0, 0, 0, false, 1, 0x092039ff);
        assert(wide.color != narrow.color && wide.end_color != narrow.end_color);
        assert((wide.color & 255) == alpha && (wide.end_color & 255) == alpha);
    }
    SurfaceLayer sample = {.color = 0x112233ff, .end_color = 0x8899aa00};
    for (int step = -2; step <= 12; step++) {
        float position = step / 10.0f;
        sample.gradient = false;
        assert(SampleColor(sample, position) == sample.color);
        sample.gradient = true;
        assert(SampleColor(sample, position) ==
            GradientColor(sample.color, sample.end_color, position));
    }
    assert(SampleColor(sample, -1) == sample.color);
    assert(SampleColor(sample, 2) == sample.end_color);
    sample.gradient_bias = 1;
    assert(SampleColor(sample, 0.5f) == GradientColor(sample.color, sample.end_color, 0.25f));
    assert(SampleColor(sample, -1) == sample.color);
    assert(SampleColor(sample, 2) == sample.end_color);
    SurfaceLayer custom = FillGradient(sample, true, 0xff0000ff, 0x0000ff00, 1);
    assert(custom.gradient_bias == 0);
    assert(SampleColor(custom, 0.5f) == GradientColor(custom.color, custom.end_color, 0.5f));
    FillStates curve_states = {.hover = true, .hover_amount = 0.5f};
    assert(ApplyFillStates(sample, curve_states, 1).gradient_bias == 0.5f);
    curve_states.hover_amount = 1;
    assert(ApplyFillStates(sample, curve_states, 1).gradient_bias == 0);
    curve_states.hover_amount = 0;
    assert(ApplyFillStates(sample, curve_states, 1).gradient_bias == 1);
    for (int hover = 0; hover <= 1; hover++) {
        SurfaceLayer green = LightfieldLayer(3, 72, 40, 8, 1,
            0x008063ff, 0x00ffbbff, 0x00ffbbff, 0x409cffff,
            hover, 0, 0, false, 1, 0x092039ff);
        assert((green.color >> 24) <= 10);
        assert((green.end_color >> 24) <= 10);
        assert(((green.color >> 16) & 255) >= 128);
        assert((green.color & 255) == 255 && (green.end_color & 255) == 255);
    }
    InteractionMotion seed = {.hover = {.value = 0.25f, .target = 1},
        .press = {.value = 0.5f, .target = 1}, .focus = {.value = 0.75f, .target = 1}};
    for (int flags = 0; flags < 128; flags++) {
        bool hovered = (flags & 1) != 0, pressed = (flags & 2) != 0;
        bool focused = (flags & 4) != 0, enabled = (flags & 8) != 0;
        bool explicit_state = (flags & 16) != 0, disabled = (flags & 32) != 0;
        bool loading = (flags & 64) != 0;
        InteractionMotion actual = AdvanceInteractionMotion(seed, hovered, pressed, focused,
            enabled, explicit_state, disabled, loading, 40, 140, 80);
        MotionTrack hover = AdvanceInteraction(seed.hover, 0, hovered, pressed, focused,
            enabled, explicit_state, disabled, loading, 40, 140, 80);
        MotionTrack press = AdvanceInteraction(seed.press, 1, hovered, pressed, focused,
            enabled, explicit_state, disabled, loading, 40, 140, 80);
        MotionTrack focus = AdvanceInteraction(seed.focus, 2, hovered, pressed, focused,
            enabled, explicit_state, disabled, loading, 40, 140, 80);
        assert_track_equal(actual.hover, hover);
        assert_track_equal(actual.press, press);
        assert_track_equal(actual.focus, focus);
        assert(actual.active == (MotionActive(hover) || MotionActive(press) || MotionActive(focus)));
    }
    unsigned int borderless_top[] = {7, 15, 7, 5};
    unsigned int borderless_bottom[] = {21, 160, 47, 5};
    for (int state = 0; state < 4; state++) {
        SurfaceLayer reflection = LightfieldLayer(5, 72, 40, 8, 1,
            0x092039ff, 0, 0x006cffff, 0x409cffff,
            state == 1, 0, state == 2, state == 3, 1, 0x092039ff);
        assert((reflection.color & 255) == borderless_top[state]);
        assert((reflection.end_color & 255) == borderless_bottom[state]);
    }
    for (int mode = 0; mode < 5; mode++) {
        for (int scale_index = 0; scale_index < 3; scale_index++) {
            float scale = scale_index == 0 ? 0.75f : (float)scale_index;
            SurfaceLayer layer = {0};
            layer.width = 12;
            layer.height = 8;
            layer.radius = 3;
            layer.stroke = mode == 1 || mode == 3 ? 1 : 0;
            layer.blur = mode == 2 || mode == 3 ? 2 : 0;
            layer.inner_blur = mode >= 3 ? 2 : 0;
            for (int y = -5; y <= 20; y++) {
                for (int x = -5; x <= 28; x++) {
                    float width = layer.width * scale;
                    float height = layer.height * scale;
                    float radius = layer.radius * scale;
                    float expected;
                    if (mode == 3)
                        expected = BlurStrokeCoverage(x, y, width, height, radius,
                            layer.stroke * scale, layer.blur * scale);
                    else if (mode == 2)
                        expected = BlurCoverage(x, y, width, height, radius, layer.blur * scale);
                    else if (mode == 4)
                        expected = InnerBlurCoverage(x, y, width, height, radius, layer.inner_blur * scale);
                    else
                        expected = LayerCoverage(x, y, width, height, radius, layer.stroke * scale);
                    assert(SampleCoverage(layer, x, y, scale) == expected);
                }
            }
        }
    }
    for (int light = 0; light < 2; light++) {
        for (int alpha = 0; alpha <= 255; alpha += 85) {
            Ring ring = LoadingRing(72, 40, 18, 375, 0x006cff00u | alpha,
                light ? 0xffffffffu : 0x092039ffu);
            assert(ring.glow_blur == (light ? 0 : 6));
            assert((LoadingSample(ring, -0.5f, -0.5f).glow & 255) == 0);
            assert((LoadingSample(ring, 16, 0).glow & 255) == 0);
            for (int y = -12; y <= 12; y++) {
                for (int x = -12; x <= 12; x++) {
                    RingSample sample = LoadingSample(ring, x, y);
                    if (light || alpha == 0)
                        assert((sample.glow & 255) == 0);
                    assert(sample.track == Opacity(ring.track_color,
                        ArcCoverage(x, y, ring.inner_radius, ring.outer_radius, 0, 360)));
                    assert(sample.arc == Opacity(LoadingArcColor(ring, x + 0.5f, y + 0.5f),
                        ArcCoverage(x, y, ring.inner_radius, ring.outer_radius,
                            ring.start_angle, ring.end_angle)));
                    assert(sample.tip == Opacity(ring.tip_color, LoadingTipCoverage(ring, x, y)));
                }
            }
        }
    }
    unsigned int reflection_alpha[] = {96, 49, 178, 191};
    for (int state = 0; state < 4; state++) {
        SurfaceLayer reflection = LightfieldLayer(8, 72, 40, 8, 1,
            0x006cffff, 0x006cffff, 0x006cffff, 0x409cffff,
            state / 2, 0, state % 2, false, 1, 0x092039ff);
        assert((reflection.end_color & 255) == reflection_alpha[state]);
    }
    SurfaceLayer dark_crown = LightfieldLayer(9, 72, 40, 8, 1,
        0x006cffff, 0x006cffff, 0x006cffff, 0x409cffff,
        0, 0, 0, false, 1, 0x092039ff);
    SurfaceLayer light_crown = LightfieldLayer(9, 72, 40, 8, 1,
        0x006cffff, 0x006cffff, 0x006cffff, 0x409cffff,
        0, 0, 0, false, 1, 0xffffffff);
    assert((dark_crown.color & 255) == 10);
    assert((light_crown.color & 255) == 23);
    for (int channel = 0; channel < 3; channel++) {
        MotionTrack track = AdvanceInteraction((MotionTrack){0}, channel, true, true, true,
            true, false, false, false, channel == 1 ? 40 : 70, 140, 80);
        assert(track.value == 0.875f);
        assert(AdvanceInteraction(track, channel, true, true, true,
            true, false, true, false, 0, 140, 80).value == 0);
        assert(AdvanceInteraction(track, channel, true, true, true,
            true, false, false, true, 0, 140, 80).value == 0);
    }
    FillStates assembled = FillTransition(FillState(0, 0x112233ff, 0x445566ff),
        FillState(8192, 0x77889900, 0), FillState(0, 0, 0),
        FillState(8192, 0xabcdef80, 0x12345600), 0.25f, 0.5f, 0.75f);
    assert(!assembled.normal && assembled.hover && !assembled.press && assembled.focus);
    assert(assembled.hover_start == 0x77889900 && assembled.hover_end == 0);
    assert(assembled.focus_end == 0x12345600);
    assert(assembled.hover_amount == 0.25f && assembled.press_amount == 0.5f && assembled.focus_amount == 0.75f);
    assert(DefaultMotionEnabled());
    assert(BlurStrokeCoverage(36, 0, 72, 40, 8, 1, 12) == 1);
    assert(BlurStrokeCoverage(36, 20, 72, 40, 8, 1, 12) == 0);
    assert(BlurStrokeCoverage(36, -13, 72, 40, 8, 1, 12) == 0);
    assert(BlurStrokeCoverage(36, -2, 72, 40, 8, 1, 12) >
        BlurStrokeCoverage(36, -6, 72, 40, 8, 1, 12));
    assert(BlurStrokeCoverage(36, 0, 72, 40, 8, 1, 0) ==
        LayerCoverage(36, 0, 72, 40, 8, 1));
    assert(BlurStrokeCoverage(36, 0, 72, 40, 8, 0, 12) == 0);
    SurfaceLayer focus_volume = LightfieldLayer(6, 72, 40, 8, 1,
        0x006cffff, 0x006cffff, 0x006cffff, 0x409cff80,
        0, 0, 1, false, 1, 0x092039ff);
    assert(focus_volume.blur == 0 && focus_volume.inner_blur == 12);
    assert(focus_volume.stroke == 0 && focus_volume.x == 0 && focus_volume.y == 0);
    assert(focus_volume.width == 72 && focus_volume.height == 40);
    assert(InnerBlurCoverage(-1, 20, 72, 40, 8, focus_volume.inner_blur) == 0);
    assert(InnerBlurCoverage(36, 1, 72, 40, 8, focus_volume.inner_blur) > 0);
    assert((focus_volume.color & 255) == 128);
    const uint32_t materials[] = {0xff0017ffu, 0x00ffc3ffu, 0xffb100ffu};
    const uint32_t focus_alphas[] = {0, 128, 255};
    for (int material_index = 0; material_index < 3; material_index++) {
        for (int alpha_index = 0; alpha_index < 3; alpha_index++) {
            uint32_t material = materials[material_index];
            uint32_t focus = 0x409cff00u | focus_alphas[alpha_index];
            SurfaceLayer volume = LightfieldLayer(6, 72, 40, 8, 1,
                0x808080ffu, material, material, focus,
                0, 0, 1, false, 1, 0x092039ffu);
            uint32_t expected = DepthColor(DepthColor(
                (material & 0xffffff00u) | focus_alphas[alpha_index], 1), 0.4f);
            assert(volume.color == expected);
            assert(volume.inner_blur == 12 && volume.blur == 0);
            assert(InnerBlurCoverage(-1, 20, 72, 40, 8, volume.inner_blur) == 0);
            SurfaceLayer edge = LightfieldLayer(7, 72, 40, 8, 1,
                0x808080ffu, material, material, focus,
                0, 0, 1, false, 1, 0x092039ffu);
            assert(edge.color == GradientColor(focus,
                0xffffff00u | focus_alphas[alpha_index], 0.50f));
        }
    }
    const uint32_t body_chromas[] = {0, 128, 160, 192, 255};
    const float edge_whites[] = {0.5f, 0.5f, 0.61f, 0.72f, 0.72f};
    for (int sample = 0; sample < 5; sample++) {
        for (int alpha_index = 0; alpha_index < 3; alpha_index++) {
            uint32_t focus = 0x409cff00u | focus_alphas[alpha_index];
            for (int step = 0; step <= 4; step++) {
                float amount = (float)step / 4;
                SurfaceLayer edge = LightfieldLayer(7, 72, 40, 8, 1,
                    (body_chromas[sample] << 8) | 255u, 0x006cffffu,
                    0x006cffffu, focus, 0, 0, amount, false, 1, 0x092039ffu);
                assert(edge.color == Opacity(GradientColor(focus,
                    0xffffff00u | focus_alphas[alpha_index], edge_whites[sample]), amount));
                assert(edge.stroke == 1.5f);
            }
        }
    }
    SurfaceLayer resting_focus_face = LightfieldLayer(3, 72, 38, 8, 1,
        0x0055d4ffu, 0x006cffffu, 0x006cffffu, 0x409cffffu,
        0, 0, 0, false, 1, 0x092039ffu);
    SurfaceLayer focused_face = LightfieldLayer(3, 72, 38, 8, 1,
        0x0055d4ffu, 0x006cffffu, 0x006cffffu, 0x409cffffu,
        0, 0, 1, false, 1, 0x092039ffu);
    assert((focused_face.color >> 24) < (resting_focus_face.color >> 24));
    assert(((focused_face.color >> 16) & 255) < ((resting_focus_face.color >> 16) & 255));
    assert((focused_face.color & 255) == 255 && (focused_face.end_color & 255) == 255);
    assert(InteractionValue(4, 12, 2, 10, 0.875f, 0, 0) == 11);
    assert(InteractionValue(1, 0.25f, 0, 0.5f, 0.875f, 0, 0) == 0.34375f);
    assert(InteractionValue(4, 12, 2, 10, 0, 0, 0.5f) == 7);
    assert(InteractionValue(4, 12, 2, 10, 1, 0, 1) == 12);
    assert(InteractionValue(4, 12, 2, 10, 1, 1, 1) == 2);
    assert(InteractionValue(4, 12, 2, 10, -1, -1, -1) == 4);
    SurfaceLayer contact = LightfieldLayer(2, 72, 40, 8, 1,
        0x006cffffu, 0x006cffffu, 0x006cffffu, 0x409cffffu,
        0, 0, 0, false, 1, 0xffffffffu);
    assert(contact.gradient && (contact.color & 255) == 0);
    assert(contact.blur == 8 && (contact.end_color & 255) == 110);
    assert((contact.end_color >> 24) == 0);
    for (int step = 0; step <= 3; step++) {
        SurfaceLayer raised_contact = LightfieldLayer(2, 72, 40 + step * 4, 8, 1,
            0x006cffffu, 0x006cffffu, 0x006cffffu, 0x409cffffu,
            0, 0, 0, false, 1, 0xffffffffu);
        unsigned int expected_alpha = step < 2 ? 110 + step * 20 : 150;
        assert((raised_contact.end_color & 255) == expected_alpha);
        assert((raised_contact.color & 255) == 0 && raised_contact.blur == 8);
    }
    SurfaceLayer material = FillGradient(FlatLayer(0, 72, 40, 8, 0, 0, 0, 1),
        true, 0x102030ffu, 0x405060ffu, 1);
    FillStates fills = {.hover = true, .hover_start = 0xff0000ffu,
        .hover_end = 0x0000ff00u};
    SurfaceLayer fade = ApplyFillStates(material, fills, 1);
    assert(fade.color == material.color && fade.end_color == material.end_color);
    fills.hover_amount = 0.5f;
    fade = ApplyFillStates(material, fills, 1);
    assert(fade.color == GradientColor(material.color, fills.hover_start, 0.5f));
    assert(fade.end_color == GradientColor(material.end_color, fills.hover_end, 0.5f));
    fills.hover_amount = 1;
    fade = ApplyFillStates(material, fills, 1);
    assert(fade.color == fills.hover_start && fade.end_color == fills.hover_end);
    fills.press_amount = 1;
    fade = ApplyFillStates(material, fills, 1);
    assert(fade.color == material.color && fade.end_color == material.end_color);
    SurfaceLayer hover_face = LightfieldLayer(3, 72, 40, 8, 1,
        0x006cffffu, 0x006cffffu, 0x006cffffu, 0x409cffffu,
        1, 0, 0, false, 1, 0x092039ffu);
    SurfaceLayer resting_face = LightfieldLayer(3, 72, 40, 8, 1,
        0x006cffffu, 0x006cffffu, 0x006cffffu, 0x409cffffu,
        0, 0, 0, false, 1, 0x092039ffu);
    assert(((hover_face.end_color >> 16) & 255) >
           ((resting_face.end_color >> 16) & 255));
    SurfaceLayer hover_border = LightfieldLayer(4, 72, 40, 8, 1,
        0x006cffffu, 0x006cff80u, 0x006cffffu, 0x409cffffu,
        1, 0, 0, false, 1, 0x092039ffu);
    assert(hover_border.gradient && (hover_border.end_color & 255) == 128);
    assert((hover_border.end_color >> 24) > (hover_border.color >> 24));
    assert(RoundedDistance(9.5f, 9.5f, 20, 20, 0) == -10);
    assert(RoundedDistance(21.5f, 9.5f, 20, 20, 0) == 2);
    assert(InnerBlurCoverage(-1, 10, 20, 20, 4, 6) == 0);
    assert(InnerBlurCoverage(10, 10, 20, 20, 4, 6) == 0);
    assert(InnerBlurCoverage(0, 10, 20, 20, 4, 6) >
           InnerBlurCoverage(2, 10, 20, 20, 4, 6));
    SurfaceLayer focus_edge = LightfieldLayer(7, 72, 40, 8, 1,
        0x006cffffu, 0x006cffffu, 0x006cffffu, 0x409cff80u,
        0, 0, 1, false, 1, 0x092039ffu);
    assert((focus_edge.color >> 24) > 64 && (focus_edge.color & 255) == 128);
    SurfaceLayer light_focus_edge = LightfieldLayer(7, 72, 40, 8, 1,
        0x006cffffu, 0x006cffffu, 0x006cffffu, 0x409cff80u,
        0, 0, 1, false, 1, 0xffffffffu);
    assert(focus_edge.stroke == 1.5f && light_focus_edge.stroke == 1);
    assert(focus_edge.x == light_focus_edge.x && focus_edge.width == light_focus_edge.width);
    Ring loading = LoadingRing(72, 40, 18, 0, 0x006cff80u, 0x092039ffu);
    for (int hover_step = 0; hover_step < 3; hover_step++) {
        for (int press_step = 0; press_step < 3; press_step++) {
            float hover = hover_step * 0.5f;
            float press = press_step * 0.5f;
            SurfaceLayer contact = LightfieldLayer(2, 72, 40, 8, 1,
                0x006cffffu, 0x006cffffu, 0x006cffffu, 0x409cffffu,
                hover, press, 0, false, 1, 0x092039ffu);
            assert(contact.blur == 2 + (3 + 2 * hover) * (1 - press));
            SurfaceLayer borderless = LightfieldLayer(2, 72, 40, 8, 1,
                0x006cffffu, 0, 0x006cffffu, 0x409cffffu,
                hover, press, 0, false, 1, 0x092039ffu);
            assert(borderless.blur == 2 + 5 * hover);
        }
    }
    for(int light = 0; light < 2; light++) {
        unsigned int ambient = light ? 0xffffffffu : 0x092039ffu;
        Ring reference = LoadingRing(72, 40, 18, 997, 0x006cff80u, ambient);
        Ring elapsed = LoadingRing(72, 40, 18, 15000997, 0x006cff80u, ambient);
        assert(elapsed.start_angle == reference.start_angle);
        assert(elapsed.end_angle == reference.end_angle);
        const unsigned int alphas[] = {0, 128, 255};
        const float times[] = {0, 375, 997, 1500};
        for (int alpha = 0; alpha < 3; alpha++) {
            for (int time = 0; time < 4; time++) {
                Ring ring = LoadingRing(72, 40, 18, times[time], 0x006cff00u | alphas[alpha], ambient);
                float radius = (ring.inner_radius + ring.outer_radius) * 0.5f;
                float x = radius * SinDegrees(ring.end_angle + 90);
                float y = radius * SinDegrees(ring.end_angle);
                assert(LoadingArcColor(ring, x, y) == GradientColor(ring.color, ring.tip_color, 0.75f));
                assert((LoadingArcColor(ring, x, y) & 255) == alphas[alpha]);
                float trail = light ? 0.55f : 0.8f;
                assert(ring.trail_opacity == trail);
                assert(LoadingArcColor(ring, -x, -y) == Opacity(ring.color, trail));
            }
        }
    }
    assert(loading.tip_color == 0xffffff80u);
    Ring neutral_loading = LoadingRing(72, 40, 18, 0, 0xf0f8ff80u, 0x092039ffu);
    Ring subdued_loading = LoadingRing(72, 40, 18, 0, 0x00306080u, 0x092039ffu);
    assert(neutral_loading.trail_opacity == 0.55f);
    assert(subdued_loading.trail_opacity == 0.675f);
    assert(loading.start_angle == 110 && loading.end_angle == 425);
    assert(LoadingPaintRadius(loading) == 16.0f);
    float tip_x = 8.75f * SinDegrees(loading.end_angle + 90);
    float tip_y = 8.75f * SinDegrees(loading.end_angle);
    assert(LoadingTipCoverage(loading, tip_x - 0.5f, tip_y - 0.5f) == 1);
    assert(LoadingTipCoverage(loading, -tip_x - 0.5f, -tip_y - 0.5f) == 0);
    assert((LoadingArcColor(loading, tip_x, tip_y) & 255) >
           (LoadingArcColor(loading, -tip_x, -tip_y) & 255));
    assert(InteractionColor(0x000000ffu, 0xff0000ffu, 0x0000ffffu,
        0xffffffffu, 0, 0, 0.5f) == 0x7f7f7fffu);
    assert(InteractionColor(0x000000ffu, 0xff0000ffu, 0x0000ffffu,
        0xffffffffu, 1, 0, 1) == 0xff0000ffu);
    assert(InteractionColor(0x000000ffu, 0xff0000ffu, 0x0000ffffu,
        0xffffffffu, 1, 1, 1) == 0x0000ffffu);
    assert(InteractionColor(0xff000080u, 0x00ff00c0u, 0,
        0x0000ff40u, 0, 0, 0.875f) == 0x1f00df48u);
    assert(InteractionColor(0xff000080u, 0x00ff00c0u, 0,
        0x0000ff40u, 0.875f, 0, 1) == 0x00df1fb0u);
    assert(InteractionColor(0xff000080u, 0x00ff00c0u, 0,
        0x0000ff40u, 1, 1, 1) == 0);
    SurfaceLayer gradient = FillGradient(FlatLayer(0, 80, 40, 8, 1, 0, 0, 1),
        true, 0xff000000u, 0x0000ffffu, 0.5f);
    assert(gradient.gradient && gradient.color == 0xff000000u);
    assert(gradient.end_color == 0x0000ff7fu && gradient.radius == 8);
    SurfaceLayer inner = LightfieldLayer(8, 72, 40, 8, 1,
        0x006cffffu, 0x006cffffu, 0x006cffffu, 0x409cffffu,
        1, 0, 0, false, 1, 0x00172dffu);
    assert(inner.blur == 0 && inner.x > 0 && inner.y >= 0);
    assert(inner.x + inner.width < 72 && inner.y + inner.height < 40);
    assert(inner.gradient && (inner.color & 255) == 0);
    assert((inner.end_color & 255) > 0);
    SurfaceLayer crown = LightfieldLayer(9, 72, 40, 8, 1,
        0x006cffffu, 0x006cffffu, 0x006cffffu, 0x409cffffu,
        1, 0, 0, false, 1, 0x00172dffu);
    assert(crown.blur == 0 && crown.inner_blur > 0);
    assert(crown.x > 0 && crown.x + crown.width < 72);
    assert(crown.y >= 0 && crown.y + crown.height < 40);
    assert(crown.gradient && (crown.color & 255) > 0);
    assert((crown.end_color & 255) == 0);
    assert(InnerBlurCoverage(-2, 20, crown.width, crown.height,
        crown.radius, crown.inner_blur) == 0);
    for (int light = 0; light < 2; light++) {
        unsigned int ambient = light ? 0xffffffffu : 0x092039ffu;
        SurfaceLayer rest = LightfieldLayer(10, 72, 40, 8, 1,
            0x006cffff, 0x006cffff, 0x006cffff, 0x409cffff,
            0, 0, 0, false, 1, ambient);
        SurfaceLayer middle = LightfieldLayer(10, 72, 40, 8, 1,
            0x006cffff, 0x006cffff, 0x006cffff, 0x409cffff,
            0, 0.5f, 0, false, 1, ambient);
        SurfaceLayer pressed = LightfieldLayer(10, 72, 40, 8, 1,
            0x006cffff, 0x006cffff, 0x006cffff, 0x409cffff,
            0, 1, 0, false, 1, ambient);
        assert((rest.color & 255) == 0);
        assert((middle.color & 255) > 0 && (middle.color & 255) < (pressed.color & 255));
        assert(pressed.blur == 0 && pressed.inner_blur == 8 && pressed.gradient);
        assert(pressed.x == 1 && pressed.y == FaceOffset(0, 1, false) + 1);
        assert(pressed.end_color == 0);
        assert(InnerBlurCoverage(-1, 20, pressed.width, pressed.height,
            pressed.radius, pressed.inner_blur) == 0);
        assert(InnerBlurCoverage(35, 19, pressed.width, pressed.height,
            pressed.radius, pressed.inner_blur) == 0);
    }
    SurfaceLayer flat = FlatLayer(1, 80, 40, 12, 2, 0x102030ffu, 0x50607080u, 0.5f);
    assert(flat.radius == 12 && flat.stroke == 2 && flat.color == 0x50607040u);
    assert(FlatLayer(0, 80, 40, 12, 2, 0, 0xffffffffu, 1).color == 0);
    assert(FlatLayer(1, 80, 40, 12, 0, 0, 0xffffffffu, 1).color == 0);
    assert(IconCoverage(1, 11, 11, 24, 24) == 1);
    assert(IconCoverage(3, 11, 11, 24, 24) == 0);
    assert(IconCoverage(99, 0, 0, 24, 24) == 0);
    assert(ArcCoverage(0, 0, 7, 9, 0, 360) == 0);
    assert(ArcCoverage(8, 0, 7, 9, 0, 90) == 1);
    assert(ArcCoverage(-9, 0, 7, 9, 0, 90) == 0);
    assert(ArcCoverage(8, 0, 7, 9, 315, 585) ==
           ArcCoverage(8, 0, 7, 9, -45, 225));
    for(int index = 0; index < MaterialLayerCount(0); index++) {
        SurfaceLayer transparent = LightfieldLayer(index, 80, 30, 8, 1,
            0, 0, 0, 0, 1, 0, 0, false, 1, 0);
        assert((transparent.color & 255) == 0);
        assert((transparent.end_color & 255) == 0);
    }
    assert(DepthColor(0x006cff80u, 0) == 0x006cff80u);
    assert((DepthColor(0x006cff80u, 0.3f) & 0xffffu) == 0xff80u);
    assert(DepthColor(0x000000ffu, 1) == 0x000000ffu);
    assert(ChevronCoverage(8, 10, 18) > 0);
    assert(ChevronCoverage(8, 2, 18) == 0);
    assert(ChevronCoverage(0, 0, 0) == 0);
    assert(SegmentCoverage(-4, 0, 60, 100) == 1);
    assert(SegmentCoverage(60, 0, 60, 100) == 0);
    assert(SegmentCoverage(59.5f, 0, 60, 100) == 0.5f);
    assert(SegmentCoverage(59.5f, 60, 40, 100) == 0.5f);
    assert(SegmentCoverage(104, 60, 40, 100) == 1);
    assert(BlurFalloff(0, 10) == 1);
    assert(BlurFalloff(5, 10) == 0.1875f);
    assert(BlurFalloff(10, 10) == 0);
    assert(BlurCoverage(-5.5f, 5, 20, 12, 0, 10) == 0.1875f);
    assert(BlurCoverage(-10.5f, 5, 20, 12, 0, 10) == 0);
    assert(LayerCoverage(-0.5f, 4, 20, 12, 0, 0) == 0.5f);
    assert(LayerCoverage(5, 5, 20, 12, 4, 1) == 0);
    assert(LayerCoverage(0, 5, 20, 12, 4, 1) == 1);
    float corner = LayerCoverage(1, 1, 20, 12, 4, 0);
    assert(corner > 0 && corner < 1);
    assert(corner == LayerCoverage(18, 1, 20, 12, 4, 0));
    assert(LayerCoverage(1, 1, -2, 12, 4, 0) == 0);
    MotionTrack track = {0};
    track = AdvanceMotion(track, 1, 50, 100, false);
    assert(track.value == 0.875f);
    MotionTrack reversed = AdvanceMotion(track, 0, 0, 100, false);
    assert(reversed.value == track.value);
    assert(reversed.origin == track.value);
    reversed = AdvanceMotion(reversed, 0, 50, 100, false);
    assert(reversed.value == 0.109375f);
    reversed = AdvanceMotion(reversed, 0, 50, 100, false);
    assert(reversed.value == 0);
    MotionTrack ten_frames = {0};
    for(int i = 0; i < 10; i++)
        ten_frames = AdvanceMotion(ten_frames, 1, 10, 100, false);
    assert(ten_frames.value == 1);
    assert(AdvanceMotion(track, 0, 0, 100, true).value == 0);
    assert(AdvanceMotion(track, 0, 0, 0, false).value == 0);
    return 0;
}
