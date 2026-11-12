# Reuse the completed production hosts, excluding their original test entry points.
# Include this after all feature modules have supplied their sources and libraries.
if(SNOW_SHOT_BUILD_TESTS)
    snow_shot_offscreen_qpa_environment(_qq_mode_qpa_environment)
    add_test(NAME snow-shot-qq-capture-mode-tests
        COMMAND snow_shot --qq-capture-mode-tests -platform offscreen)
    set_tests_properties(snow-shot-qq-capture-mode-tests PROPERTIES LABELS unit TIMEOUT 60
        SNOW_TEST_CROSS_PLATFORM TRUE RUN_SERIAL TRUE
        ENVIRONMENT "${_qq_mode_qpa_environment}"
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")
    function(snow_shot_add_qq_test target host source)
        snow_shot_offscreen_qpa_environment(_qq_qpa_environment)
        get_target_property(_sources ${host} SOURCES)
        list(FILTER _sources EXCLUDE REGEX "(^|[/\\\\])tests[/\\\\]")
        add_executable(${target} ${source} ${_sources})
        foreach(_property IN ITEMS INCLUDE_DIRECTORIES LINK_LIBRARIES COMPILE_DEFINITIONS
                COMPILE_OPTIONS RUNTIME_OUTPUT_DIRECTORY RUNTIME_OUTPUT_DIRECTORY_DEBUG
                RUNTIME_OUTPUT_DIRECTORY_RELEASE RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO)
            get_target_property(_values ${host} ${_property})
            if(_values)
                set_property(TARGET ${target} PROPERTY ${_property} "${_values}")
            endif()
        endforeach()
        if(WIN32 AND NOT SNOW_SHOT_IMAGE_CODEC_BACKEND_STATIC)
            _snow_shot_target_uses_image_codec(${target} _qq_needs_image_runtime "")
            if(_qq_needs_image_runtime)
                _snow_shot_stage_image_runtime(${target})
            endif()
        endif()
        snow_shot_import_offscreen_platform(${target})
        add_test(NAME ${target} COMMAND ${target} -platform offscreen)
        set_tests_properties(${target} PROPERTIES LABELS unit TIMEOUT 60
            SNOW_TEST_CROSS_PLATFORM TRUE
            ENVIRONMENT "${_qq_qpa_environment}"
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")
    endfunction()

    snow_shot_add_qq_test(snow-shot-qq-screenshot-toolbar-tests
        snow-shot-toolbar-main-panel-tests tests/qq_screenshot_toolbar_tests.cpp)
    snow_shot_add_qq_test(snow-shot-qq-watermark-controls-tests
        snow-shot-toolbar-main-panel-tests tests/qq_watermark_controls_tests.cpp)
    snow_shot_add_qq_test(snow-shot-qq-selection-toolbar-tests
        snow-shot-screenshot-selection-toolbar-hover-tests tests/qq_selection_toolbar_tests.cpp)
endif()
