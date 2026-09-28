# Downloads the pinned pluginval Windows release binary and verifies its checksum.
# pluginval has no published checksums, so PLUGINVAL_SHA256 below was computed once,
# by build-engineer, from the exact release asset and is treated as the source of truth
# for every subsequent configure. See docs/dependencies.md for the update procedure.

set(PLUGINVAL_VERSION "1.0.4")
set(PLUGINVAL_URL "https://github.com/Tracktion/pluginval/releases/download/v${PLUGINVAL_VERSION}/pluginval_Windows.zip")
set(PLUGINVAL_SHA256 "c08e61ce3b96db41636f8ec7e76f4c7e2c13ebdac7fa1b5a1f52b4f32ec715ab")

set(PLUGINVAL_ROOT "${CMAKE_SOURCE_DIR}/third_party/pluginval/${PLUGINVAL_VERSION}")
set(PLUGINVAL_EXECUTABLE "${PLUGINVAL_ROOT}/pluginval.exe")

if (NOT EXISTS "${PLUGINVAL_EXECUTABLE}")
    file(MAKE_DIRECTORY "${PLUGINVAL_ROOT}")
    set(_pluginval_zip "${PLUGINVAL_ROOT}/pluginval_Windows.zip")

    message(STATUS "Downloading pluginval ${PLUGINVAL_VERSION}...")
    file(DOWNLOAD
        "${PLUGINVAL_URL}"
        "${_pluginval_zip}"
        EXPECTED_HASH SHA256=${PLUGINVAL_SHA256}
        STATUS _pluginval_download_status
        TLS_VERIFY ON
    )

    list(GET _pluginval_download_status 0 _pluginval_download_code)
    if (NOT _pluginval_download_code EQUAL 0)
        list(GET _pluginval_download_status 1 _pluginval_download_message)
        message(FATAL_ERROR "Failed to download pluginval ${PLUGINVAL_VERSION}: ${_pluginval_download_message}")
    endif ()

    message(STATUS "Extracting pluginval ${PLUGINVAL_VERSION}...")
    file(ARCHIVE_EXTRACT INPUT "${_pluginval_zip}" DESTINATION "${PLUGINVAL_ROOT}")

    if (NOT EXISTS "${PLUGINVAL_EXECUTABLE}")
        message(FATAL_ERROR "pluginval.exe not found after extracting ${_pluginval_zip}")
    endif ()
endif ()
