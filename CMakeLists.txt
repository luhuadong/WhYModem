cmake_minimum_required(VERSION 3.16)

project(WhYModem VERSION 0.2.0 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets SerialPort)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets SerialPort)

set(PROJECT_SOURCES
    src/main.cpp
    src/ui/widget.cpp
    src/ui/widget.h
    src/ui/widget.ui
    src/protocol/ITransferProtocol.h
    src/protocol/xmodem/XmodemProtocol.cpp
    src/protocol/xmodem/XmodemProtocol.h
    src/protocol/ymodem/Ymodem.cpp
    src/protocol/ymodem/Ymodem.h
    src/protocol/ymodem/YmodemProtocol.cpp
    src/protocol/ymodem/YmodemProtocol.h
    src/protocol/zmodem/ZmodemProtocol.cpp
    src/protocol/zmodem/ZmodemProtocol.h
    src/transfer/FileReceiver.cpp
    src/transfer/FileReceiver.h
    src/transfer/FileTransmitter.cpp
    src/transfer/FileTransmitter.h
    src/transfer/ProtocolFactory.cpp
    src/transfer/ProtocolFactory.h
    resources/resources.qrc
)

add_executable(WhYModem WIN32 ${PROJECT_SOURCES})

target_include_directories(WhYModem
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(WhYModem
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::SerialPort
)

install(TARGETS WhYModem
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES resources/logo.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps
    RENAME io.geekat.WhYModem.png
)

install(FILES packaging/io.geekat.WhYModem.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(WhYModem)
endif()

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "WhYModem")
set(CPACK_PACKAGE_VENDOR "GeekAt")
set(CPACK_PACKAGE_CONTACT "GeekAt <geekat@example.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Qt serial YMODEM transfer tool")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/dist")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "GeekAt <geekat@example.com>")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

include(CPack)
