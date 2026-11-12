if(VCPKG_TARGET_IS_WINDOWS)
    vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
    # Keep the archive static, but inherit the consumer's CRT from the triplet.
endif()

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO zlib-ng/minizip-ng
    REF "${VERSION}"
    SHA512 9ea5dde14acd2f7d1efd0e38b11017b679d3aaabac61552f9c5f4c7f45f2563543e0fbb2d74429c6b1b9c37d8728ebc4f1cf0efad5f71807c11bb8a2a681a556
    HEAD_REF master
)

vcpkg_check_features(
    OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES zlib MZ_ZLIB
)

set(_snow_minizip_platform_options)
if(VCPKG_TARGET_IS_OSX)
    # Snow Shot writes DEFLATE ZIPs. Keep the declared vcpkg zlib dependency and
    # Apple's CommonCrypto encryption backend instead of auto-detecting optional
    # Homebrew dylibs whose deployment target may exceed the app's macOS floor.
    list(APPEND _snow_minizip_platform_options
        -DMZ_ZLIB_FLAVOR=zlib
        -DMZ_LIBCOMP=OFF
        -DMZ_BZIP2=OFF
        -DMZ_LZMA=OFF
        -DMZ_ZSTD=OFF
        -DMZ_PKCRYPT=ON
        -DMZ_WZAES=ON
        -DMZ_OPENSSL=OFF
        -DMZ_LIBBSD=OFF)
endif()

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        ${FEATURE_OPTIONS}
        ${_snow_minizip_platform_options}
        -DMZ_FETCH_LIBS=OFF
        -DMZ_LIB_SUFFIX=-ng
        -DMZ_ICONV=OFF
        -DMZ_COMPAT=OFF
)
vcpkg_cmake_install()
vcpkg_fixup_pkgconfig()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/minizip-ng)
if(VCPKG_TARGET_IS_OSX)
    # Binary caches must not export the builder's SDK into consumers. An SDK
    # framework path adds a non-system -F search path, exposing Apple's headers
    # to project warnings when the consumer selects a different Xcode/CLT SDK.
    set(_minizip_targets "${CURRENT_PACKAGES_DIR}/share/minizip-ng/minizip-ng.cmake")
    file(READ "${_minizip_targets}" _minizip_export)
    string(REGEX REPLACE
        [[/[^;"]+[.]sdk/System/Library/Frameworks/([A-Za-z0-9_]+)[.]framework]]
        [[-framework \1]] _minizip_export "${_minizip_export}")
    # The config already calls find_dependency(BZip2); retain its consumer-side
    # resolution instead of the absolute libbz2.tbd captured during packaging.
    string(REGEX REPLACE [[/[^;"]+[.]sdk/usr/lib/libbz2[.]tbd]]
        [[BZip2::BZip2]] _minizip_export "${_minizip_export}")
    file(WRITE "${_minizip_targets}" "${_minizip_export}")
    string(REGEX MATCH [[INTERFACE_LINK_LIBRARIES[ \t]+"[^"]*"]]
        _minizip_link_dependencies "${_minizip_export}")
    message(STATUS "Snow Shot minizip exported links: ${_minizip_link_dependencies}")
    set(_minizip_config "${CURRENT_PACKAGES_DIR}/share/minizip-ng/minizip-ng-config.cmake")
    if(EXISTS "${_minizip_config}")
        file(READ "${_minizip_config}" _minizip_package_config)
        string(REGEX MATCHALL [[find_dependency\([^)]*\)]]
            _minizip_package_dependencies "${_minizip_package_config}")
        message(STATUS "Snow Shot minizip package dependencies: ${_minizip_package_dependencies}")
    endif()
endif()
vcpkg_copy_pdbs()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share" "${CURRENT_PACKAGES_DIR}/debug/include")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
