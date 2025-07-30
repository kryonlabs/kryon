# Kryon Audio Module

if(KRYON_ENABLE_AUDIO)
    add_library(kryon_audio OBJECT
        playback/audio_player.c
        recording/audio_recorder.c
        synthesis/synthesizer.c
        effects/audio_effects.c
    )

    target_include_directories(kryon_audio PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/internal
    )

    target_compile_definitions(kryon_audio PRIVATE KRYON_AUDIO_MODULE=1)
endif()