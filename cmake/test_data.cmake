# Fetches the test-data repositories pinned in `test/data.cmake` into
# `test/data/`. They were git submodules until this file existed.
#
# Why they stopped being submodules: SwiftPM runs `git submodule update --init
# --recursive` on a package repository at *every consumer's* checkout, so the
# root `Package.swift` that ships the Apple framework would have made everyone
# clone 4.5 GB of test data — and then fail outright on the two private
# remotes. Fetching here keeps that cost on the people who run the tests.
#
# The checkouts live in the source tree, not the build tree, on purpose: the
# path is baked into `test_info.cpp`, the three usual build directories would
# otherwise hold three copies, and `reference-output/*` has to stay a working
# copy the regeneration workflow can commit from.
#
# Two modes:
#
#   include()   configure time. Clones what is missing and reports what has
#               drifted. Never moves an existing checkout — during a
#               reference-output regeneration the local revision is ahead of
#               the pin on purpose, and silently resetting it would throw the
#               work away.
#   cmake -P    the `update_test_data` target. Moves clean checkouts onto their
#               pinned revisions.
#
# This shells out to plain `git`, so authentication is git's business and not
# CMake's: the two private repositories need whatever your git is already set
# up with. A credential helper (`gh auth setup-git` is the one-liner, and what
# CI does), or ssh, in which case:
#
#   git config --global url."git@github.com:".insteadOf "https://github.com/"

# Only in script mode: calling this from an include() would reset the policy
# defaults of the including directory.
if (CMAKE_SCRIPT_MODE_FILE)
    cmake_minimum_required(VERSION 3.15)
endif ()

find_package(Git REQUIRED)

# Both overridable, so a downstream can keep the data elsewhere or pin a
# different set.
if (NOT DEFINED ODR_TEST_DATA_ROOT)
    set(ODR_TEST_DATA_ROOT "${CMAKE_CURRENT_LIST_DIR}/../test/data")
endif ()
if (NOT DEFINED ODR_TEST_DATA_PINS)
    set(ODR_TEST_DATA_PINS "${CMAKE_CURRENT_LIST_DIR}/../test/data.cmake")
endif ()

# Clone at a single revision. `--depth 1` of a bare sha needs the server to
# allow it; GitHub does, but a mirror may not, so fall back to a full fetch
# rather than failing.
function(odr_test_data_clone directory url revision)
    message(STATUS "test data: cloning ${url} at ${revision}")

    file(MAKE_DIRECTORY "${directory}")
    execute_process(
            COMMAND "${GIT_EXECUTABLE}" init --quiet
            COMMAND_ERROR_IS_FATAL ANY
            WORKING_DIRECTORY "${directory}")
    execute_process(
            COMMAND "${GIT_EXECUTABLE}" remote add origin "${url}"
            COMMAND_ERROR_IS_FATAL ANY
            WORKING_DIRECTORY "${directory}")

    execute_process(
            COMMAND "${GIT_EXECUTABLE}" fetch --depth 1 origin "${revision}"
            RESULT_VARIABLE shallow
            WORKING_DIRECTORY "${directory}")
    if (NOT shallow EQUAL 0)
        execute_process(
                COMMAND "${GIT_EXECUTABLE}" fetch origin
                COMMAND_ERROR_IS_FATAL ANY
                WORKING_DIRECTORY "${directory}")
    endif ()

    execute_process(
            COMMAND "${GIT_EXECUTABLE}" checkout --quiet --detach "${revision}"
            COMMAND_ERROR_IS_FATAL ANY
            WORKING_DIRECTORY "${directory}")
endfunction()

# Move an existing clean checkout onto `revision`. Refuses on a dirty tree.
function(odr_test_data_update directory revision)
    execute_process(
            COMMAND "${GIT_EXECUTABLE}" status --porcelain
            OUTPUT_VARIABLE dirty
            OUTPUT_STRIP_TRAILING_WHITESPACE
            COMMAND_ERROR_IS_FATAL ANY
            WORKING_DIRECTORY "${directory}")
    if (NOT dirty STREQUAL "")
        message(FATAL_ERROR
                "${directory} has uncommitted changes. Commit or stash them "
                "before updating the test data.")
    endif ()

    execute_process(
            COMMAND "${GIT_EXECUTABLE}" fetch --depth 1 origin "${revision}"
            RESULT_VARIABLE shallow
            WORKING_DIRECTORY "${directory}")
    if (NOT shallow EQUAL 0)
        execute_process(
                COMMAND "${GIT_EXECUTABLE}" fetch origin
                COMMAND_ERROR_IS_FATAL ANY
                WORKING_DIRECTORY "${directory}")
    endif ()

    message(STATUS "test data: ${directory} -> ${revision}")
    execute_process(
            COMMAND "${GIT_EXECUTABLE}" checkout --quiet --detach "${revision}"
            COMMAND_ERROR_IS_FATAL ANY
            WORKING_DIRECTORY "${directory}")
endfunction()

# One test-data repository. Called from `test/data.cmake`.
#
#   PATH      below `test/data`, also the name the tests refer to it by
#   URL       clone url
#   REVISION  the pinned commit; advance it here after regenerating output
function(odr_test_data)
    cmake_parse_arguments(PARSE_ARGV 0 ARG "" "PATH;URL;REVISION" "")
    set(directory "${ODR_TEST_DATA_ROOT}/${ARG_PATH}")

    # a submodule checkout carries `.git` as a file, a plain clone as a
    # directory, so test for either
    if (NOT EXISTS "${directory}/.git")
        odr_test_data_clone("${directory}" "${ARG_URL}" "${ARG_REVISION}")
        return()
    endif ()

    execute_process(
            COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
            OUTPUT_VARIABLE head
            OUTPUT_STRIP_TRAILING_WHITESPACE
            COMMAND_ERROR_IS_FATAL ANY
            WORKING_DIRECTORY "${directory}")
    if (head STREQUAL ARG_REVISION)
        return()
    endif ()

    if (ODR_TEST_DATA_UPDATE)
        odr_test_data_update("${directory}" "${ARG_REVISION}")
        return()
    endif ()

    message(WARNING
            "test/data/${ARG_PATH} is at ${head}, but test/data.cmake pins "
            "${ARG_REVISION}. Left untouched — if this is not a reference-output "
            "regeneration in progress, run the `update_test_data` target.")
endfunction()

include("${ODR_TEST_DATA_PINS}")
