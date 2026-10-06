# Fetch pinned test data. Configure leaves existing checkouts untouched,
# because during a reference-output regeneration the local revision is ahead
# of the pin on purpose; ODR_TEST_DATA_UPDATE moves clean checkouts to their
# pins.
#
# This runs plain `git`, so the two private repositories need whatever git is
# set up with: a credential helper (`gh auth setup-git`, as CI does), or ssh:
#
#   git config --global url."git@github.com:".insteadOf "https://github.com/"

# Only in script mode: from an include() this would reset the policy defaults
# of the including directory.
if (CMAKE_SCRIPT_MODE_FILE)
    cmake_minimum_required(VERSION 3.15)
endif ()

find_package(Git REQUIRED)

if (NOT DEFINED ODR_TEST_DATA_ROOT)
    set(ODR_TEST_DATA_ROOT "${CMAKE_CURRENT_LIST_DIR}/../test/data")
endif ()
if (NOT DEFINED ODR_TEST_DATA_PINS)
    set(ODR_TEST_DATA_PINS "${CMAKE_CURRENT_LIST_DIR}/../test/data.cmake")
endif ()

function(odr_test_data_git directory)
    execute_process(
            COMMAND "${GIT_EXECUTABLE}" ${ARGN}
            RESULT_VARIABLE result
            OUTPUT_VARIABLE output
            OUTPUT_STRIP_TRAILING_WHITESPACE
            WORKING_DIRECTORY "${directory}")
    if (NOT "${result}" STREQUAL "0")
        message(FATAL_ERROR "Git failed in ${directory}: ${ARGN} (${result})")
    endif ()
    set(ODR_TEST_DATA_GIT_OUTPUT "${output}" PARENT_SCOPE)
endfunction()

function(odr_test_data_update directory revision)
    odr_test_data_git("${directory}" status --porcelain)
    if (NOT ODR_TEST_DATA_GIT_OUTPUT STREQUAL "")
        message(FATAL_ERROR
                "${directory} has uncommitted changes. Commit or stash them "
                "before updating the test data.")
    endif ()

    execute_process(
            COMMAND "${GIT_EXECUTABLE}" fetch --depth 1 origin "${revision}"
            RESULT_VARIABLE shallow
            WORKING_DIRECTORY "${directory}")
    if (NOT "${shallow}" STREQUAL "0")
        # Some mirrors do not allow shallow fetches of a bare SHA.
        odr_test_data_git("${directory}" fetch origin)
    endif ()

    message(STATUS "test data: ${directory} -> ${revision}")
    odr_test_data_git("${directory}" checkout --quiet --detach "${revision}")
endfunction()

function(odr_test_data_clone directory url revision)
    message(STATUS "test data: initializing ${url} at ${revision}")
    file(MAKE_DIRECTORY "${directory}")
    if (NOT EXISTS "${directory}/.git")
        odr_test_data_git("${directory}" init --quiet)
    endif ()
    execute_process(
            COMMAND "${GIT_EXECUTABLE}" remote get-url origin
            RESULT_VARIABLE origin
            OUTPUT_QUIET ERROR_QUIET
            WORKING_DIRECTORY "${directory}")
    if (NOT "${origin}" STREQUAL "0")
        odr_test_data_git("${directory}" remote add origin "${url}")
    endif ()
    odr_test_data_update("${directory}" "${revision}")
endfunction()

# PATH is relative to the data root; URL and REVISION identify its pinned source.
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
            COMMAND "${GIT_EXECUTABLE}" rev-parse --verify HEAD
            RESULT_VARIABLE resolved
            OUTPUT_VARIABLE head
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            WORKING_DIRECTORY "${directory}")
    if (NOT "${resolved}" STREQUAL "0")
        odr_test_data_clone("${directory}" "${ARG_URL}" "${ARG_REVISION}")
        return()
    endif ()
    if ("${head}" STREQUAL "${ARG_REVISION}")
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
