if(NOT TARGET snow_shot)
    message(FATAL_ERROR "SNOW_APPS_PACKAGE_SNOW_SHOT requires SNOW_APPS_BUILD_SNOW_SHOT=ON")
endif()
if(NOT SNOW_SHOT_RELEASE_STATIC)
    get_filename_component(_snow_shared_qt_prefix "${Qt6_DIR}/../../.." ABSOLUTE)
    set(SNOW_SHOT_QT_LICENSE_DIR
        "${_snow_shared_qt_prefix}/share/snow-apps/qt-licenses" CACHE PATH
        "Qt license metadata collected by snow_install_qt_license_metadata")
    if(NOT IS_DIRECTORY "${SNOW_SHOT_QT_LICENSE_DIR}")
        message(FATAL_ERROR
            "The Qt license bundle is missing: ${SNOW_SHOT_QT_LICENSE_DIR}. "
            "Collect the matching Qt source licenses with snow_install_qt_license_metadata before packaging.")
    endif()
    install(DIRECTORY "${SNOW_SHOT_QT_LICENSE_DIR}/"
        DESTINATION "snow_shot.app/Contents/Resources/snow-shot/licenses/third-party/qt"
        COMPONENT SnowShot)
    install(DIRECTORY "${SNOW_FFMPEG_ROOT}/share/"
        DESTINATION "snow_shot.app/Contents/Resources/snow-shot/licenses/third-party/vcpkg"
        COMPONENT SnowShot FILES_MATCHING PATTERN "copyright")
endif()
set(CPACK_PACKAGE_NAME "xiaomage-screenshot")
set(CPACK_PACKAGE_VENDOR "${SNOW_SHOT_VENDOR}")
set(CPACK_PACKAGE_VERSION "${SNOW_SHOT_VERSION_NUMERIC}")
set(CPACK_PACKAGE_FILE_NAME "xiaomage-screenshot-${SNOW_SHOT_VERSION}-macos-${CMAKE_OSX_ARCHITECTURES}")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CPACK_PACKAGE_CHECKSUM SHA256)
set(CPACK_GENERATOR DragNDrop)
set(CPACK_SNOW_SHOT_BUNDLE_NAME "snow_shot")
set(CPACK_SNOW_SHOT_PRODUCT_NAME "小马哥专用截图工具")
set(CPACK_DMG_VOLUME_NAME "${CPACK_SNOW_SHOT_PRODUCT_NAME}")
include("${CMAKE_CURRENT_LIST_DIR}/SnowShotPackageCompression.cmake")
snow_shot_apply_dmg_compression()
set(CPACK_DMG_DISABLE_APPLICATIONS_SYMLINK OFF)
set(_snow_dmg_assets "${CMAKE_CURRENT_LIST_DIR}/../snow_shot/packaging/macos")
set(_snow_dmg_background_svg "${CMAKE_CURRENT_BINARY_DIR}/macos/dmg-background.svg")
set(CPACK_DMG_BACKGROUND_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/macos/dmg-background.png")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/macos")
configure_file("${_snow_dmg_assets}/dmg-background.svg" "${_snow_dmg_background_svg}" @ONLY)
execute_process(COMMAND /usr/bin/sips -s format png
    "${_snow_dmg_background_svg}" --out "${CPACK_DMG_BACKGROUND_IMAGE}"
    OUTPUT_QUIET COMMAND_ERROR_IS_FATAL ANY)
set(CPACK_DMG_DS_STORE_SETUP_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/macos/dmg-layout.applescript")
configure_file("${_snow_dmg_assets}/dmg-layout.applescript" "${CPACK_DMG_DS_STORE_SETUP_SCRIPT}" @ONLY)
set(CPACK_PRE_BUILD_SCRIPTS "${CMAKE_CURRENT_LIST_DIR}/PrepareSnowShotMacOSDmg.cmake")
set(CPACK_POST_BUILD_SCRIPTS "${CMAKE_CURRENT_LIST_DIR}/SignSnowShotMacOSDmg.cmake")
# Do not ship SDK headers, static archives or other projects' install rules.
set(CPACK_INSTALL_CMAKE_PROJECTS "${CMAKE_BINARY_DIR};${CMAKE_PROJECT_NAME};SnowShot;/")
include(CPack)
