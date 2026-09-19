#include <math.h>

#include "lawcheck.h"
#include "runtime/slider.h"

static const float float_edges[] = {
    -1000000.0f, -1000.0f, -100.0f, -10.0f, -1.0f, -0.0f,
    0.0f,        0.001f,   0.25f,   0.5f,   0.75f, 1.0f,
    1.001f,      2.0f,     10.0f,   100.0f, 1000.0f, 1000000.0f,
};

int
main(void)
{
    LawCheck law = {0};

    LAW_BEGIN(&law, "slider.ratio.clamped");
    for(size_t i = 0; i < sizeof(float_edges) / sizeof(float_edges[0]); i++) {
        float ratio = SliderClampRatio(float_edges[i]);
        REQUIRE_FLOAT_BETWEEN(&law, ratio, 0.0f, 1.0f);
    }
    {
        uint32_t state = 0x1a7e5eedu;
        for(int i = 0; i < 512; i++) {
            float input = law_float_between(&state, -10000.0f, 10000.0f);
            float ratio = SliderClampRatio(input);
            REQUIRE_FLOAT_BETWEEN(&law, ratio, 0.0f, 1.0f);
        }
    }

    LAW_BEGIN(&law, "slider.value.ratio.roundtrip.clamped");
    FOR_INT(minimum, -20, 20) {
        FOR_INT(maximum, minimum, 24) {
            for(size_t i = 0; i < sizeof(float_edges) / sizeof(float_edges[0]); i++) {
                float value = float_edges[i];
                float ratio = SliderRatio(value, (float)minimum, (float)maximum);
                float resolved = SliderValue((float)minimum, (float)maximum, ratio);
                REQUIRE_FLOAT_BETWEEN(&law, ratio, 0.0f, 1.0f);
                if(maximum > minimum) {
                    REQUIRE_FLOAT_BETWEEN(&law, resolved, (float)minimum, (float)maximum);
                } else {
                    REQUIRE(&law, fabsf(resolved - (float)minimum) < 0.001f);
                }
            }
        }
    }

    LAW_BEGIN(&law, "slider.pointer.ratio.clamped");
    {
        uint32_t state = 0xa11ce55u;
        for(int i = 0; i < 512; i++) {
            float pointer = law_float_between(&state, -2000.0f, 2000.0f);
            float origin = law_float_between(&state, -500.0f, 500.0f);
            float length = law_float_between(&state, -10.0f, 1000.0f);
            float normal = SliderPointerRatio(pointer, origin, length, false);
            float inverted = SliderPointerRatio(pointer, origin, length, true);
            REQUIRE_FLOAT_BETWEEN(&law, normal, 0.0f, 1.0f);
            REQUIRE_FLOAT_BETWEEN(&law, inverted, 0.0f, 1.0f);
            if(length <= 0.0f) {
                REQUIRE(&law, normal == 0.0f);
                REQUIRE(&law, inverted == 0.0f);
            }
        }
    }

    LAW_BEGIN(&law, "slider.discrete.value.in.range");
    FOR_INT(minimum, -16, 16) {
        FOR_INT(maximum, minimum, 20) {
            for(size_t i = 0; i < sizeof(float_edges) / sizeof(float_edges[0]); i++) {
                int value = SliderDiscreteValue(minimum, maximum, float_edges[i]);
                if(maximum > minimum)
                    REQUIRE(&law, value >= minimum && value <= maximum);
                else
                    REQUIRE(&law, value == minimum);
            }
        }
    }

    return lawcheck_finish(&law);
}
