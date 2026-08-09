# Fork-specific version, independent of upstream's project(... VERSION ...).
#
# Releases are cut by moving the `latest` tag (see .claude/skills/release). The
# version is the build timestamp, worked out by the `version` job in
# .github/workflows/build.yml and passed in through the environment, so nothing
# in the working tree and no tag ever has to be bumped.
#
# This script sets:
#
# CHATTERINO_FORK_VERSION_STRING
#   X.Y.Z, from the CHATTERINO_FORK_VERSION cache variable or the environment
#   variable of the same name. Falls back to 0.0.0.
#
# CHATTERINO_FORK_RELEASE
#   1 when a version was handed to us, 0 otherwise. A local build gets no
#   version, so it is a development build, and silent auto-updates stay off -
#   otherwise a build straight from the working tree would overwrite itself.
#
# Both reach C++ through the generated autogen/ForkVersion.hpp.

set(CHATTERINO_FORK_VERSION "" CACHE STRING
    "Fork version X.Y.Z. Empty means: a development build.")

set(_fork_version "")
set(_fork_origin "")

if(NOT CHATTERINO_FORK_VERSION STREQUAL "")
    set(_fork_version "${CHATTERINO_FORK_VERSION}")
    set(_fork_origin "CHATTERINO_FORK_VERSION")
elseif(NOT "$ENV{CHATTERINO_FORK_VERSION}" STREQUAL "")
    set(_fork_version "$ENV{CHATTERINO_FORK_VERSION}")
    set(_fork_origin "the environment")
endif()

if(_fork_version MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    set(CHATTERINO_FORK_RELEASE 1)
    message(STATUS "Fork version: ${_fork_version} (release, from ${_fork_origin})")
else()
    if(NOT _fork_version STREQUAL "")
        message(WARNING
            "Fork version '${_fork_version}' from ${_fork_origin} is not X.Y.Z - "
            "treating this as a development build.")
    endif()
    set(_fork_version "0.0.0")
    set(CHATTERINO_FORK_RELEASE 0)
    message(STATUS "Fork version: 0.0.0 (development build, auto-updates disabled)")
endif()

set(CHATTERINO_FORK_VERSION_STRING "${_fork_version}")

configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/ForkVersion.hpp.in"
    "${CMAKE_BINARY_DIR}/autogen/ForkVersion.hpp"
    @ONLY
)
