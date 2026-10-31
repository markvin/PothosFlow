########################################################################
# PothosFlow: the graphical designer of the Pothos framework.
# Builds against an installed Pothos core (find_package(Pothos)) and Qt 6.
# Qt comes from find_package: from the system, or from an install that the
# user gives with CMAKE_PREFIX_PATH (or Qt6_DIR).
########################################################################
cmake_minimum_required(VERSION 3.25)
project(PothosFlow CXX)

option(FLOW_BUILD_TESTS "Register the CTest tests of flow" ${PROJECT_IS_TOP_LEVEL})
option(FLOW_WARNINGS_AS_ERRORS "Treat compiler warnings as errors (GCC and Clang)" OFF)

#the core defines the variable POTHOS_ROOT (its install prefix), and the
#runtime environment often sets POTHOS_ROOT too. find_package must not use
#them as search prefixes (CMP0144): it could find another core than
#CMAKE_PREFIX_PATH gives. NO_PACKAGE_ROOT_PATH keeps them out.
if(POLICY CMP0144)
    cmake_policy(SET CMP0144 NEW)
endif()

find_package(Pothos CONFIG REQUIRED NO_PACKAGE_ROOT_PATH)

include(GNUInstallDirs)

#the core loads modules from its own prefix (POTHOS_ROOT): install there
#when the user does not give CMAKE_INSTALL_PREFIX
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${POTHOS_ROOT}" CACHE PATH "Install prefix (default: the prefix of the Pothos core)" FORCE)
    message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX} (the prefix of the Pothos core)")
endif()

########################################################################
# Qt 6
########################################################################
find_package(Qt6 6.5 CONFIG COMPONENTS Widgets Concurrent)
if(NOT Qt6_FOUND)
    #Qt can be present without one of its own dependencies (for example the
    #OpenGL headers for Qt Gui): show the reason that Qt gives
    set(flow_qt_reason "")
    if(Qt6_NOT_FOUND_MESSAGE)
        set(flow_qt_reason "Reason given by Qt: ${Qt6_NOT_FOUND_MESSAGE}\n")
    endif()
    message(FATAL_ERROR
        "PothosFlow: Qt 6 (>= 6.5) with the Widgets and Concurrent components is missing.\n"
        "${flow_qt_reason}"
        "Install the Qt 6 package of the system (for example qt6-base; on Debian "
        "and Ubuntu also the OpenGL headers, for example libgl-dev), "
        "or give CMAKE_PREFIX_PATH or Qt6_DIR. Refer to README.md.")
endif()
message(STATUS "PothosFlow: Qt ${Qt6_VERSION}")

########################################################################
# Common settings of the targets
########################################################################
#flow_warnings(<target>): the common warnings (GCC and Clang, as the core),
#errors with FLOW_WARNINGS_AS_ERRORS (CI)
function(flow_warnings target)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra)
        if(FLOW_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

#a Qt or a core outside the system dirs (an install of the user or of CI):
#the installed binaries must find them at run time, also when flow is
#installed into another prefix than the core (for example the stage of
#the tests). On Windows the Qt and core DLL dirs are on PATH.
set(FLOW_EXTRA_RPATH "")
set(flow_rpath_dirs "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_LIBS}")
get_target_property(flow_pothos_imported Pothos::Pothos IMPORTED)
if(flow_pothos_imported)
    get_target_property(flow_pothos_configs Pothos::Pothos IMPORTED_CONFIGURATIONS)
    foreach(config ${flow_pothos_configs})
        get_target_property(flow_pothos_location Pothos::Pothos IMPORTED_LOCATION_${config})
        if(flow_pothos_location)
            get_filename_component(flow_pothos_libdir "${flow_pothos_location}" DIRECTORY)
            list(APPEND flow_rpath_dirs "${flow_pothos_libdir}")
        endif()
    endforeach()
endif()
foreach(dir ${flow_rpath_dirs})
    if(NOT dir IN_LIST CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES AND NOT dir IN_LIST FLOW_EXTRA_RPATH)
        list(APPEND FLOW_EXTRA_RPATH "${dir}")
    endif()
endforeach()
if(FLOW_EXTRA_RPATH)
    message(STATUS "PothosFlow: install RPATH ${FLOW_EXTRA_RPATH}")
endif()

#flow_target(<target> [RPATH <relative path to the library dir>]): C++23,
#moc, warnings and the install RPATH. The relative RPATH finds
#PothosQtColorPicker in <libdir> of the same prefix (also a stage prefix).
function(flow_target target)
    cmake_parse_arguments(arg "" "RPATH" "" ${ARGN})
    set_target_properties(${target} PROPERTIES
        AUTOMOC ON
        CXX_EXTENSIONS OFF)
    target_compile_features(${target} PRIVATE cxx_std_23)
    flow_warnings(${target})
    set(rpath ${FLOW_EXTRA_RPATH})
    if(DEFINED arg_RPATH)
        if(APPLE)
            list(APPEND rpath "@loader_path/${arg_RPATH}")
        elseif(UNIX)
            list(APPEND rpath "$ORIGIN/${arg_RPATH}")
        endif()
    endif()
    if(UNIX AND rpath)
        set_property(TARGET ${target} APPEND PROPERTY INSTALL_RPATH "${rpath}")
    endif()
endfunction()

########################################################################
# Color picker library - LGPL separate library
########################################################################
add_library(PothosQtColorPicker SHARED qtcolorpicker/src/qtcolorpicker.cpp)
target_include_directories(PothosQtColorPicker PUBLIC ${PROJECT_SOURCE_DIR}/qtcolorpicker/src)
set_target_properties(PothosQtColorPicker PROPERTIES
    DEFINE_SYMBOL "QT_QTCOLORPICKER_EXPORT"
    VERSION 0)
target_link_libraries(PothosQtColorPicker PRIVATE Qt6::Widgets)
flow_target(PothosQtColorPicker)
install(TARGETS PothosQtColorPicker
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} # .so file
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} # .lib file
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} # .dll file
)

########################################################################
# Icon resources: a generated .qrc, compiled into each executable
# (resources in a static library need Q_INIT_RESOURCE)
########################################################################
file(GLOB ICONS
    ${PROJECT_SOURCE_DIR}/icons/*.png
    ${PROJECT_SOURCE_DIR}/icons/*.gif)
set(FLOW_RESOURCES_QRC ${PROJECT_BINARY_DIR}/resources.qrc)
set(flow_qrc "<!DOCTYPE RCC><RCC version='1.0'>\n<qresource>\n")
foreach(ICON ${ICONS})
    get_filename_component(NAME ${ICON} NAME)
    string(APPEND flow_qrc "<file alias='icons/${NAME}'>${ICON}</file>\n")
endforeach(ICON)
string(APPEND flow_qrc "</qresource>\n</RCC>\n")
file(CONFIGURE OUTPUT ${FLOW_RESOURCES_QRC} CONTENT "${flow_qrc}")

########################################################################
# The application sources: an internal static library that the
# executable and the smoke test link (not installed)
########################################################################
add_library(PothosFlowApp STATIC
    MainWindow/FlowSetup.cpp
    MainWindow/MainWindow.cpp
    MainWindow/MainActions.cpp
    MainWindow/MainMenu.cpp
    MainWindow/MainToolBar.cpp
    MainWindow/MainSettings.cpp
    MainWindow/MainSplash.cpp
    MainWindow/IconUtils.cpp

    ColorUtils/ColorUtils.cpp
    ColorUtils/ColorsDialog.cpp

    PropertiesPanel/PropertiesPanelDock.cpp
    PropertiesPanel/ConnectionPropertiesPanel.cpp
    PropertiesPanel/BlockPropertiesPanel.cpp
    PropertiesPanel/BreakerPropertiesPanel.cpp
    PropertiesPanel/GraphPropertiesPanel.cpp
    PropertiesPanel/PropertyEditWidget.cpp

    MessageWindow/MessageWindowDock.cpp
    MessageWindow/LoggerDisplay.cpp
    MessageWindow/LoggerChannel.cpp

    BlockTree/BlockTreeDock.cpp
    BlockTree/BlockTreeWidget.cpp
    BlockTree/BlockTreeWidgetItem.cpp
    BlockTree/BlockCache.cpp

    AffinitySupport/AffinityZoneEditor.cpp
    AffinitySupport/AffinityZonesMenu.cpp
    AffinitySupport/AffinityZonesComboBox.cpp
    AffinitySupport/AffinityZonesDock.cpp
    AffinitySupport/CpuSelectionWidget.cpp

    HostExplorer/PluginModuleTree.cpp
    HostExplorer/PluginRegistryTree.cpp
    HostExplorer/SystemInfoTree.cpp
    HostExplorer/HostSelectionTable.cpp
    HostExplorer/HostExplorerDock.cpp

    GraphEditor/Constants.cpp
    GraphEditor/GraphState.cpp
    GraphEditor/GraphEditorTabs.cpp
    GraphEditor/GraphEditor.cpp
    GraphEditor/GraphEditorExport.cpp
    GraphEditor/GraphEditorSerialization.cpp
    GraphEditor/GraphEditorDeserialization.cpp
    GraphEditor/GraphEditorRenderedDialog.cpp
    GraphEditor/GraphEditorTopologyStats.cpp
    GraphEditor/GraphDraw.cpp
    GraphEditor/GraphDrawSelection.cpp
    GraphEditor/GraphActionsDock.cpp
    GraphEditor/DockingTabWidget.cpp

    GraphObjects/GraphEndpoint.cpp
    GraphObjects/GraphObject.cpp
    GraphObjects/GraphBlock.cpp
    GraphObjects/GraphBlockUpdate.cpp
    GraphObjects/GraphBreaker.cpp
    GraphObjects/GraphConnection.cpp
    GraphObjects/GraphWidget.cpp
    GraphObjects/GraphWidgetContainer.cpp

    EvalEngine/EvalTracer.cpp
    EvalEngine/EvalEngine.cpp
    EvalEngine/EvalEngineImpl.cpp
    EvalEngine/BlockEval.cpp
    EvalEngine/ThreadPoolEval.cpp
    EvalEngine/EnvironmentEval.cpp
    EvalEngine/TopologyEval.cpp
    EvalEngine/TopologyTraversal.cpp
)
target_include_directories(PothosFlowApp PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(PothosFlowApp PUBLIC
    Pothos::Pothos
    PothosQtColorPicker
    Qt6::Widgets
    Qt6::Concurrent)
flow_target(PothosFlowApp)

########################################################################
# The executable
########################################################################
set(FLOW_EXE_SOURCES PothosFlow.cpp ${FLOW_RESOURCES_QRC})

#an icon in the Windows executable
if(WIN32)
    set(flow_rc_file "${PROJECT_BINARY_DIR}/PothosFlow.rc")
    file(CONFIGURE OUTPUT "${flow_rc_file}"
        CONTENT "id ICON \"${PROJECT_SOURCE_DIR}/icons/PothosFlow.ico\"\n")
    list(APPEND FLOW_EXE_SOURCES "${flow_rc_file}")
endif()

if(APPLE)
    set(flow_icns ${PROJECT_SOURCE_DIR}/icons/PothosFlow.icns)
    set_source_files_properties(${flow_icns} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    list(APPEND FLOW_EXE_SOURCES ${flow_icns})
endif()

set(BUNDLE_DESTINATION ${CMAKE_INSTALL_BINDIR} CACHE STRING "OSX bundle destination")

#under Windows build a GUI application so that a console is not launched with it
if(WIN32)
    add_executable(PothosFlow WIN32 ${FLOW_EXE_SOURCES})
    if(MSVC)
        target_link_options(PothosFlow PRIVATE /entry:mainCRTStartup)
    endif()

#under OSX build a bundle so the menu integration works properly
#this also creates a launcher with an icon when opened in finder
elseif(APPLE)
    add_executable(PothosFlow MACOSX_BUNDLE ${FLOW_EXE_SOURCES})
    string(TIMESTAMP Pothos_BUNDLE_VERSION "%Y.%m.%d")
    set_target_properties(PothosFlow PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_STRING "Graphical designer for the Pothos data-flow framework"
        MACOSX_BUNDLE_BUNDLE_NAME "PothosFlow"
        MACOSX_BUNDLE_BUNDLE_VERSION "${Pothos_BUNDLE_VERSION}"
        MACOSX_BUNDLE_LONG_VERSION_STRING "${Pothos_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${Pothos_VERSION_MAJOR}.${Pothos_VERSION_MINOR}.${Pothos_VERSION_PATCH}"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.pothosware.pothosflow"
        MACOSX_BUNDLE_ICON_FILE "PothosFlow.icns"
        MACOSX_BUNDLE_COPYRIGHT "Copyright (c) 2013-2021 Josh Blum"
        MACOSX_BUNDLE_INFO_PLIST "${PROJECT_SOURCE_DIR}/cmake/Modules/MacOSXBundleInfo.plist.in"
    )
    #provide PothosFlow command line script which calls into the bundle
    set(ABSOLUTE_BUNDLE_DESTINATION ${BUNDLE_DESTINATION})
    if(NOT IS_ABSOLUTE "${BUNDLE_DESTINATION}")
        set(ABSOLUTE_BUNDLE_DESTINATION ${CMAKE_INSTALL_PREFIX}/${BUNDLE_DESTINATION})
    endif()
    configure_file(
        ${PROJECT_SOURCE_DIR}/PothosFlowOSX.sh
        ${PROJECT_BINARY_DIR}/PothosFlow @ONLY)
    install(PROGRAMS ${PROJECT_BINARY_DIR}/PothosFlow DESTINATION ${CMAKE_INSTALL_BINDIR})

#otherwise build a standard UNIX style executable
else()
    add_executable(PothosFlow ${FLOW_EXE_SOURCES})
    add_subdirectory(Desktop) #freedesktop.org
endif()

set_target_properties(PothosFlow PROPERTIES AUTORCC ON)
target_link_libraries(PothosFlow PRIVATE PothosFlowApp)
#the relative path from the executable to the library dir of the prefix
if(APPLE)
    set(flow_exe_dir "${BUNDLE_DESTINATION}/PothosFlow.app/Contents/MacOS")
else()
    set(flow_exe_dir "${CMAKE_INSTALL_BINDIR}")
endif()
file(RELATIVE_PATH flow_rpath "/prefix/${flow_exe_dir}" "/prefix/${CMAKE_INSTALL_LIBDIR}")
flow_target(PothosFlow RPATH "${flow_rpath}")

install(
    TARGETS PothosFlow
    BUNDLE DESTINATION ${BUNDLE_DESTINATION}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT pothos_flow
)

########################################################################
# Edit widgets module
########################################################################
add_subdirectory(EditWidgets)

########################################################################
# Tests: flow is staged into a prefix inside the build tree, and the
# smoke test makes the main window against the installed core with the
# flow module of the stage. Flow needs no display: Qt uses the offscreen
# platform.
########################################################################
if(FLOW_BUILD_TESTS)
    add_subdirectory(Tests)
    enable_testing()
    set(FLOW_STAGE_DIR ${PROJECT_BINARY_DIR}/stage)

    add_test(NAME flow.stage
        COMMAND ${CMAKE_COMMAND} --install ${PROJECT_BINARY_DIR} --prefix ${FLOW_STAGE_DIR} --config $<CONFIG>)
    set_tests_properties(flow.stage PROPERTIES FIXTURES_SETUP flow_stage)

    add_test(NAME flow.smoke
        COMMAND ${CMAKE_COMMAND}
            -DPREFIX=${POTHOS_ROOT}
            -DTEST_HOME_DIR=${PROJECT_BINARY_DIR}/test-home
            -DBINDIR=bin
            -DLIBDIR=${CMAKE_INSTALL_LIBDIR}
            -DPLUGIN_PATH=${FLOW_STAGE_DIR}/${CMAKE_INSTALL_LIBDIR}/Pothos/modules${POTHOS_ABI_VERSION}
            -DSTAGE_BINDIR=${FLOW_STAGE_DIR}/${CMAKE_INSTALL_BINDIR}
            -DSMOKE_TEST=$<TARGET_FILE:PothosFlowSmokeTest>
            -P ${PROJECT_SOURCE_DIR}/cmake/PothosRunFlowSmokeTest.cmake)
    #the main window starts a server on the fixed locator port of Pothos
    set_tests_properties(flow.smoke PROPERTIES
        FIXTURES_REQUIRED flow_stage
        RESOURCE_LOCK pothos_locator_port
        TIMEOUT 240)
endif()
