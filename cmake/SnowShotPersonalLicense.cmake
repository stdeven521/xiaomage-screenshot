# Present the personal product attribution before the unmodified upstream license.
function(snow_shot_personal_license output)
    file(READ "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../snow_shot/packaging/PERSONAL-LICENSE.txt"
        _personal_notice)
    file(READ "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../snow_shot/LICENSE" _upstream_license)
    set(_license "${CMAKE_CURRENT_BINARY_DIR}/personal-installer-license.txt")
    # NSIS detects the UTF-8 BOM when displaying non-ASCII license text.
    string(ASCII 239 187 191 _utf8_bom)
    file(WRITE "${_license}" "${_utf8_bom}${_personal_notice}\n\n${_upstream_license}")
    set(${output} "${_license}" PARENT_SCOPE)
endfunction()
