# Reads the firmware version from firmware/VERSION, checks it and generates version.h in the build directory
# (references/internal/menu_subsystem_spec.md section 17.3). Uses no command that needs project().
#
# Results: FIRMWARE_VERSION, FIRMWARE_VERSION_FILE, FIRMWARE_VERSION_INCLUDE_DIR.

# The startup screen and the About screen have room for 17 characters (ui-ux.md UX-63).
set(FIRMWARE_VERSION_MAX_LENGTH 17)

get_filename_component(FIRMWARE_VERSION_FILE "${CMAKE_CURRENT_LIST_DIR}/../VERSION" ABSOLUTE)
file(STRINGS "${FIRMWARE_VERSION_FILE}" FIRMWARE_VERSION LIMIT_COUNT 1)

# file(STRINGS) drops a BOM and a CR, but CI reads the file with cat. So the raw bytes must be the version and at
# most one LF.
file(READ "${FIRMWARE_VERSION_FILE}" firmware_version_raw_hex HEX)
string(HEX "${FIRMWARE_VERSION}" firmware_version_hex)
if(NOT ((firmware_version_raw_hex STREQUAL firmware_version_hex) OR
        (firmware_version_raw_hex STREQUAL "${firmware_version_hex}0a")))
  message(FATAL_ERROR
    "${FIRMWARE_VERSION_FILE}: the file must hold only the version and at most one LF "
    "(no BOM, CR, spaces or second line); its bytes are ${firmware_version_raw_hex}")
endif()

if(NOT FIRMWARE_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
  message(FATAL_ERROR
    "${FIRMWARE_VERSION_FILE}: version \"${FIRMWARE_VERSION}\" does not match the format MAJOR.MINOR.PATCH")
endif()
string(LENGTH "${FIRMWARE_VERSION}" firmware_version_length)
if(firmware_version_length GREATER FIRMWARE_VERSION_MAX_LENGTH)
  message(FATAL_ERROR
    "${FIRMWARE_VERSION_FILE}: version \"${FIRMWARE_VERSION}\" has ${firmware_version_length} characters, "
    "the limit is ${FIRMWARE_VERSION_MAX_LENGTH}")
endif()

# An edit of the file reconfigures the build at the next cmake --build.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${FIRMWARE_VERSION_FILE}")

set(FIRMWARE_VERSION_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file("${CMAKE_CURRENT_LIST_DIR}/version.h.in" "${FIRMWARE_VERSION_INCLUDE_DIR}/version.h" @ONLY)
