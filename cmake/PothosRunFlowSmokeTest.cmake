########################################################################
# Run the smoke test of PothosFlow (cmake -P script).
# The test makes the main window of flow without a display
# (QT_QPA_PLATFORM=offscreen) against an installed core.
#
# Required: -DPREFIX=<prefix of the core, with PothosBlocks>
#           -DTEST_HOME_DIR=<isolated user dirs>
#           -DBINDIR=<relative bin dir> -DLIBDIR=<relative lib dir>
#           -DSMOKE_TEST=<path of PothosFlowSmokeTest>
# Optional: -DPLUGIN_PATH=<module directory> for POTHOS_PLUGIN_PATH (the
#           flow module when it is not installed in PREFIX)
#           -DSTAGE_BINDIR=<bin dir of the stage prefix>: on Windows it goes
#           on PATH (PothosQtColorPicker.dll)
########################################################################
cmake_minimum_required(VERSION 3.25) #policies for cmake -P scripts
foreach(var PREFIX TEST_HOME_DIR BINDIR LIBDIR SMOKE_TEST)
    if (NOT DEFINED ${var})
        message(FATAL_ERROR "PothosRunFlowSmokeTest: ${var} is not set")
    endif ()
endforeach()

file(GLOB blocks_dirs "${PREFIX}/*/Pothos/modules*/blocks")
if (NOT blocks_dirs)
    message(FATAL_ERROR "PothosRunFlowSmokeTest: the blocks toolkit is not installed in ${PREFIX}. "
        "The smoke test needs PothosBlocks (/blocks/copier in the block cache). "
        "Install it into the prefix of the core.")
endif ()

include(${CMAKE_CURRENT_LIST_DIR}/PothosTestEnv.cmake)
pothos_test_env("${PREFIX}" "${TEST_HOME_DIR}" "${BINDIR}" "${LIBDIR}")
if (PLUGIN_PATH)
    set(ENV{POTHOS_PLUGIN_PATH} "${PLUGIN_PATH}")
endif ()
if (WIN32 AND STAGE_BINDIR)
    file(TO_NATIVE_PATH "${STAGE_BINDIR}" stage_bin_path)
    set(ENV{PATH} "${stage_bin_path};$ENV{PATH}")
endif ()
set(ENV{QT_QPA_PLATFORM} "offscreen")

execute_process(COMMAND "${SMOKE_TEST}" RESULT_VARIABLE rc TIMEOUT 180)
if (NOT rc EQUAL 0)
    message(FATAL_ERROR "PothosFlowSmokeTest failed: ${rc}")
endif ()
