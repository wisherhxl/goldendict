# ----------------------
# Find Qt (supports Qt6 and Qt5)
set(qt_modules
        Core
        Gui
        Widgets
        Network
        OpenGL
        # Sql
        # Xml
        # SerialPort
        # SerialBus
        Test
)

# Try Qt6 first, then fall back to Qt5
find_package(Qt6 COMPONENTS ${qt_modules} LinguistTools QUIET)
if (Qt6_FOUND)
    set(QT_MAJOR 6)
    set(QT_NAMESPACE Qt6)
    set(QT_DIR ${Qt6_DIR})
else()
    find_package(Qt5 COMPONENTS ${qt_modules} LinguistTools QUIET)
    if (Qt5_FOUND)
        set(QT_MAJOR 5)
        set(QT_NAMESPACE Qt5)
        set(QT_DIR ${Qt5_DIR})
    else()
        message(FATAL_ERROR "Neither Qt6 nor Qt5 was found. Please install Qt or adjust CMAKE_PREFIX_PATH.")
    endif()
endif()

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

# LinguistTools may be provided as a target as well
if (TARGET ${QT_NAMESPACE}::LinguistTools)
    list(APPEND qt_link "${QT_NAMESPACE}::LinguistTools")
else()
    message(WARNING "${QT_NAMESPACE}::LinguistTools target not found; translation tools may be unavailable.")
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