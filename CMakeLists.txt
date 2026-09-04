cmake_minimum_required(VERSION 3.24)
project(KlanggeistLyricsStudio VERSION 2.1.1 LANGUAGES CXX)

# Qualitätsregel: Warnungen sind Fehler. Keine Suppression, kein stilles Durchwinken.
set(CMAKE_COMPILE_WARNING_AS_ERROR ON)

add_executable(KlanggeistLyricsStudio WIN32
    src/KlanggeistLyricsStudio.rc
    src/main.cpp
    src/mini_json.cpp
    src/job_controller.cpp
    src/lyrics_document.cpp
    src/audio_player.cpp
    src/audio_analysis.cpp
    src/image_analysis.cpp
    src/song_structure.cpp
    src/production_planner.cpp
    src/tatarus_visual_brain.cpp
    src/video_export_controller.cpp
    src/ffmpeg_video_exporter.cpp
    src/video_renderer.cpp
    src/cover_image.cpp
    src/cover_motion.cpp
    src/lyrics_compositor.cpp
    src/mp4_encoder.cpp
)

target_compile_features(KlanggeistLyricsStudio PRIVATE cxx_std_20)
target_compile_definitions(KlanggeistLyricsStudio PRIVATE
    UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN
)

if(MSVC)
    target_compile_options(KlanggeistLyricsStudio PRIVATE
        /W4 /WX /permissive- /EHsc /utf-8 /Zc:__cplusplus
    )
endif()

target_include_directories(KlanggeistLyricsStudio PRIVATE src)

target_link_libraries(KlanggeistLyricsStudio PRIVATE
    d2d1
    dwrite
    windowscodecs
    ole32
    shell32
    shlwapi
    mf
    mfplat
    mfreadwrite
    mfuuid
    xaudio2
    comdlg32
)

add_custom_command(TARGET KlanggeistLyricsStudio POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/backend"
            "$<TARGET_FILE_DIR:KlanggeistLyricsStudio>/backend"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/requirements.txt"
            "$<TARGET_FILE_DIR:KlanggeistLyricsStudio>/requirements.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/INSTALL_BACKEND.bat"
            "$<TARGET_FILE_DIR:KlanggeistLyricsStudio>/INSTALL_BACKEND.bat"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/README.md"
            "$<TARGET_FILE_DIR:KlanggeistLyricsStudio>/README.md"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/LICENSE"
            "$<TARGET_FILE_DIR:KlanggeistLyricsStudio>/LICENSE"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
            "$<TARGET_FILE_DIR:KlanggeistLyricsStudio>/THIRD_PARTY_NOTICES.md"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/docs"
            "$<TARGET_FILE_DIR:KlanggeistLyricsStudio>/docs"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:KlanggeistLyricsStudio>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/presets"
            "$<TARGET_FILE_DIR:KlanggeistLyricsStudio>/presets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/tools"
            "$<TARGET_FILE_DIR:KlanggeistLyricsStudio>/tools"
)

add_executable(KlanggeistVideoSmoke EXCLUDE_FROM_ALL
    tests/video_smoke.cpp
    src/lyrics_document.cpp
    src/mini_json.cpp
    src/video_renderer.cpp
    src/cover_image.cpp
    src/cover_motion.cpp
    src/lyrics_compositor.cpp
    src/mp4_encoder.cpp
)
target_compile_features(KlanggeistVideoSmoke PRIVATE cxx_std_20)
target_compile_definitions(KlanggeistVideoSmoke PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_include_directories(KlanggeistVideoSmoke PRIVATE src)
target_link_libraries(KlanggeistVideoSmoke PRIVATE d2d1 dwrite windowscodecs ole32 mf mfplat mfreadwrite mfuuid)
if(MSVC)
    target_compile_options(KlanggeistVideoSmoke PRIVATE /W4 /WX /permissive- /EHsc /utf-8 /Zc:__cplusplus)
endif()

add_executable(KlanggeistExportControllerSmoke EXCLUDE_FROM_ALL
    tests/export_controller_smoke.cpp
    src/video_export_controller.cpp
    src/ffmpeg_video_exporter.cpp
    src/lyrics_document.cpp
    src/mini_json.cpp
    src/video_renderer.cpp
    src/cover_image.cpp
    src/cover_motion.cpp
    src/lyrics_compositor.cpp
    src/mp4_encoder.cpp
)
target_compile_features(KlanggeistExportControllerSmoke PRIVATE cxx_std_20)
target_compile_definitions(KlanggeistExportControllerSmoke PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_include_directories(KlanggeistExportControllerSmoke PRIVATE src)
target_link_libraries(KlanggeistExportControllerSmoke PRIVATE d2d1 dwrite windowscodecs ole32 mf mfplat mfreadwrite mfuuid)
if(MSVC)
    target_compile_options(KlanggeistExportControllerSmoke PRIVATE /W4 /WX /permissive- /EHsc /utf-8 /Zc:__cplusplus)
endif()
add_custom_command(TARGET KlanggeistExportControllerSmoke POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/tools"
            "$<TARGET_FILE_DIR:KlanggeistExportControllerSmoke>/tools"
)
