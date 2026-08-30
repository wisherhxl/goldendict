# ----------------------
# Find the Qt 6 components used by GoldenDict.
set(qt_modules
        Core
        Gui
        Widgets
        Network
        WebChannel
        WebEngineCore
        WebEngineWidgets
        Multimedia
)

find_package(Qt6 6.11.1 REQUIRED COMPONENTS ${qt_modules})
find_package(Qt6 6.11.1 REQUIRED COMPONENTS PrintSupport)
if(UNIX AND NOT APPLE)
    find_package(Qt6 6.11.1 REQUIRED COMPONENTS Help)
    set(qt_help_link "Qt6::Help")
endif()
set(QT_MAJOR 6)
set(QT_NAMESPACE Qt6)
set(QT_DIR ${Qt6_DIR})
set(qt_print_support_link "${QT_NAMESPACE}::PrintSupport")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(qt_link "")
foreach(module ${qt_modules})
    if (NOT TARGET ${QT_NAMESPACE}::${module})
        message(FATAL_ERROR "${QT_NAMESPACE}::${module} NOT FOUND")
    endif()
    list(APPEND qt_link "${QT_NAMESPACE}::${module}")
endforeach()

ti_add_thirdparty_status(qt
    DISPLAY "Qt"
    VERSION "${${QT_NAMESPACE}_VERSION}"
    TARGETS ${qt_link}
    PACKAGE ${QT_NAMESPACE}
    CONFIG
    COMPONENTS ${qt_modules})

# LinguistTools provides translation commands/tools, not a linkable library target.
find_package(${QT_NAMESPACE} COMPONENTS LinguistTools QUIET)
if (${QT_NAMESPACE}LinguistTools_FOUND OR TARGET ${QT_NAMESPACE}::LinguistTools)
    set(QT_LINGUISTTOOLS_FOUND TRUE)
else()
    set(QT_LINGUISTTOOLS_FOUND FALSE)
    message(WARNING "${QT_NAMESPACE} LinguistTools not found; translation update/release targets will be unavailable.")
endif()

# Try to locate Qt binaries (uic, etc.) from the discovered Qt_DIR
if (DEFINED QT_DIR)
    # QtX_DIR points to <prefix>/lib/cmake/QtX; bin is usually ../../../bin
    set(Qt_BIN ${QT_DIR}/../../../bin)
else()
    set(Qt_BIN "")
endif()

find_program(QT_UIC_EXECUTABLE NAMES uic uic.exe
             PATHS ${Qt_BIN}
             HINTS ENV PATH
             NO_DEFAULT_PATH)
if (NOT QT_UIC_EXECUTABLE)
    # fallback to searching PATH
    find_program(QT_UIC_EXECUTABLE NAMES uic uic.exe HINTS ENV PATH)
endif()
if (NOT QT_UIC_EXECUTABLE)
    message(WARNING "Could not find 'uic' executable. UI compilation may fail if uic is unavailable.")
endif()

find_program(QT_LUPDATE_EXECUTABLE NAMES lupdate lupdate.exe
             PATHS ${Qt_BIN}
             HINTS ENV PATH
             NO_DEFAULT_PATH)
if (NOT QT_LUPDATE_EXECUTABLE)
    find_program(QT_LUPDATE_EXECUTABLE NAMES lupdate lupdate.exe HINTS ENV PATH)
endif()

find_program(QT_LRELEASE_EXECUTABLE NAMES lrelease lrelease.exe
             PATHS ${Qt_BIN}
             HINTS ENV PATH
             NO_DEFAULT_PATH)
if (NOT QT_LRELEASE_EXECUTABLE)
    find_program(QT_LRELEASE_EXECUTABLE NAMES lrelease lrelease.exe HINTS ENV PATH)
endif()
