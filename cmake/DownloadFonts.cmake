# Downloads the approved fonts (Inter v4.1, JetBrains Mono v2.304 - see
# docs/research/T-022-font-licenses.md and the roadmap decision dated
# 2026-09-28) with SHA-256 checksum verification, extracts only the specific
# files the ui-spec calls for, renames the two OFL license files so they do
# not collide, and copies pffft's license text alongside them. Everything
# lands in third_party/fonts/, flat, ready to be picked up by the
# InvestigateAssets binary-data glob in the root CMakeLists.txt.
#
# third_party/ is gitignored as a whole (see docs/dependencies.md), so none
# of this is committed; every configure on a clean checkout re-downloads it,
# exactly like cmake/DownloadPluginval.cmake.

set(INVESTIGATE_INTER_VERSION "4.1")
set(INVESTIGATE_INTER_URL "https://github.com/rsms/inter/releases/download/v${INVESTIGATE_INTER_VERSION}/Inter-${INVESTIGATE_INTER_VERSION}.zip")
set(INVESTIGATE_INTER_SHA256 "9883fdd4a49d4fb66bd8177ba6625ef9a64aa45899767dde3d36aa425756b11e")

set(INVESTIGATE_JETBRAINS_MONO_VERSION "2.304")
set(INVESTIGATE_JETBRAINS_MONO_URL "https://github.com/JetBrains/JetBrainsMono/releases/download/v${INVESTIGATE_JETBRAINS_MONO_VERSION}/JetBrainsMono-${INVESTIGATE_JETBRAINS_MONO_VERSION}.zip")
set(INVESTIGATE_JETBRAINS_MONO_SHA256 "6f6376c6ed2960ea8a963cd7387ec9d76e3f629125bc33d1fdcd7eb7012f7bbf")

set(INVESTIGATE_FONTS_DIR "${CMAKE_SOURCE_DIR}/third_party/fonts")

set(INVESTIGATE_FONT_ASSET_FILES
    "${INVESTIGATE_FONTS_DIR}/Inter-Medium.ttf"
    "${INVESTIGATE_FONTS_DIR}/Inter-SemiBold.ttf"
    "${INVESTIGATE_FONTS_DIR}/OFL-Inter.txt"
    "${INVESTIGATE_FONTS_DIR}/JetBrainsMono-Regular.ttf"
    "${INVESTIGATE_FONTS_DIR}/JetBrainsMono-Medium.ttf"
    "${INVESTIGATE_FONTS_DIR}/OFL-JetBrainsMono.txt"
)

set(_investigate_fonts_missing FALSE)
foreach (_investigate_font_file ${INVESTIGATE_FONT_ASSET_FILES})
    if (NOT EXISTS "${_investigate_font_file}")
        set(_investigate_fonts_missing TRUE)
    endif ()
endforeach ()

if (_investigate_fonts_missing)
    file(MAKE_DIRECTORY "${INVESTIGATE_FONTS_DIR}")
    set(_investigate_fonts_download_dir "${INVESTIGATE_FONTS_DIR}/_download")
    file(MAKE_DIRECTORY "${_investigate_fonts_download_dir}")

    # --- Inter --------------------------------------------------------------
    set(_inter_zip "${_investigate_fonts_download_dir}/Inter-${INVESTIGATE_INTER_VERSION}.zip")
    message(STATUS "Downloading Inter ${INVESTIGATE_INTER_VERSION}...")
    file(DOWNLOAD
        "${INVESTIGATE_INTER_URL}"
        "${_inter_zip}"
        EXPECTED_HASH SHA256=${INVESTIGATE_INTER_SHA256}
        STATUS _inter_download_status
        TLS_VERIFY ON
    )
    list(GET _inter_download_status 0 _inter_download_code)
    if (NOT _inter_download_code EQUAL 0)
        list(GET _inter_download_status 1 _inter_download_message)
        message(FATAL_ERROR "Failed to download Inter ${INVESTIGATE_INTER_VERSION}: ${_inter_download_message}")
    endif ()

    set(_inter_extract_dir "${_investigate_fonts_download_dir}/inter-extract")
    file(ARCHIVE_EXTRACT INPUT "${_inter_zip}" DESTINATION "${_inter_extract_dir}")

    file(COPY_FILE "${_inter_extract_dir}/extras/ttf/Inter-Medium.ttf" "${INVESTIGATE_FONTS_DIR}/Inter-Medium.ttf")
    file(COPY_FILE "${_inter_extract_dir}/extras/ttf/Inter-SemiBold.ttf" "${INVESTIGATE_FONTS_DIR}/Inter-SemiBold.ttf")
    file(COPY_FILE "${_inter_extract_dir}/LICENSE.txt" "${INVESTIGATE_FONTS_DIR}/OFL-Inter.txt")

    # --- JetBrains Mono ------------------------------------------------------
    set(_jbm_zip "${_investigate_fonts_download_dir}/JetBrainsMono-${INVESTIGATE_JETBRAINS_MONO_VERSION}.zip")
    message(STATUS "Downloading JetBrains Mono ${INVESTIGATE_JETBRAINS_MONO_VERSION}...")
    file(DOWNLOAD
        "${INVESTIGATE_JETBRAINS_MONO_URL}"
        "${_jbm_zip}"
        EXPECTED_HASH SHA256=${INVESTIGATE_JETBRAINS_MONO_SHA256}
        STATUS _jbm_download_status
        TLS_VERIFY ON
    )
    list(GET _jbm_download_status 0 _jbm_download_code)
    if (NOT _jbm_download_code EQUAL 0)
        list(GET _jbm_download_status 1 _jbm_download_message)
        message(FATAL_ERROR "Failed to download JetBrains Mono ${INVESTIGATE_JETBRAINS_MONO_VERSION}: ${_jbm_download_message}")
    endif ()

    set(_jbm_extract_dir "${_investigate_fonts_download_dir}/jetbrains-mono-extract")
    file(ARCHIVE_EXTRACT INPUT "${_jbm_zip}" DESTINATION "${_jbm_extract_dir}")

    file(COPY_FILE "${_jbm_extract_dir}/fonts/ttf/JetBrainsMono-Regular.ttf" "${INVESTIGATE_FONTS_DIR}/JetBrainsMono-Regular.ttf")
    file(COPY_FILE "${_jbm_extract_dir}/fonts/ttf/JetBrainsMono-Medium.ttf" "${INVESTIGATE_FONTS_DIR}/JetBrainsMono-Medium.ttf")
    file(COPY_FILE "${_jbm_extract_dir}/OFL.txt" "${INVESTIGATE_FONTS_DIR}/OFL-JetBrainsMono.txt")

    foreach (_investigate_font_file ${INVESTIGATE_FONT_ASSET_FILES})
        if (NOT EXISTS "${_investigate_font_file}")
            message(FATAL_ERROR "Expected font asset ${_investigate_font_file} is missing after extraction.")
        endif ()
    endforeach ()

    file(REMOVE_RECURSE "${_investigate_fonts_download_dir}")
endif ()

# --- pffft's license text ---------------------------------------------------
# pffft is already fetched as an exact, GIT_TAG-pinned commit by the FetchContent
# block above (pffft_SOURCE_DIR), so its license text is copied straight from
# that already-verified checkout rather than downloaded and checksummed a
# second time.
if (DEFINED pffft_SOURCE_DIR)
    set(_investigate_pffft_license_dest "${INVESTIGATE_FONTS_DIR}/LICENSE-pffft.txt")
    if (NOT EXISTS "${_investigate_pffft_license_dest}")
        file(MAKE_DIRECTORY "${INVESTIGATE_FONTS_DIR}")
        file(COPY_FILE "${pffft_SOURCE_DIR}/LICENSE.txt" "${_investigate_pffft_license_dest}")
    endif ()
else ()
    message(FATAL_ERROR "cmake/DownloadFonts.cmake requires pffft_SOURCE_DIR to be set; include it after the pffft FetchContent block.")
endif ()
