# ----------------------
# Find Qt

set(Qt6_LIBRARIES)

set(qt_modules
    Core
    Gui
    Widgets
    Network
    OpenGL
    OpenGLWidgets
    Sql
    Xml
    SerialPort
    # SerialBus
    Test
    LinguistTools)

find_package(
  Qt6
  COMPONENTS ${qt_modules}
  REQUIRED QUIET)

if(NOT Qt6_FOUND OR NOT Qt6_DIR)
  message(FATAL_ERROR "Qt6 NOT FOUND")
endif()

foreach(module ${qt_modules})
  if(NOT TARGET Qt6::${module})
    message(FATAL_ERROR "Qt6::${module} NOT FOUND")
  endif()
  list(APPEND Qt6_LIBRARIES "Qt6::${module}")
endforeach()

ti_list_components(Qt6)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(Qt6_BIN ${Qt6_DIR}/../../../bin)
set(QT_UIC_EXECUTABLE ${Qt6_BIN}/uic.exe)
