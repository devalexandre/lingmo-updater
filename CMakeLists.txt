cmake_minimum_required(VERSION 3.16)
project(lingmo-updater VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

include(GNUInstallDirs)

# QSystemTrayIcon lives in QtWidgets and needs a QApplication
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Qml Quick DBus Network LinguistTools)
find_package(KF6WindowSystem REQUIRED)

add_executable(lingmo-updater
    src/main.cpp
    src/updatemanager.cpp
    src/updaterapp.cpp
    resources.qrc
)

target_link_libraries(lingmo-updater PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Qml Qt6::Quick Qt6::DBus Qt6::Network
    KF6::WindowSystem
)

set(TRANSLATIONS_DIR ${CMAKE_INSTALL_FULL_DATADIR}/lingmo-updater/translations)
target_compile_definitions(lingmo-updater PRIVATE
    TRANSLATIONS_DIR="${TRANSLATIONS_DIR}"
    LINGMO_UPDATER_VERSION="${PROJECT_VERSION}"
)
# lrelease only: building never rewrites the .ts sources
qt_add_translation(QM_FILES translations/lingmo-updater_pt_BR.ts)
add_custom_target(translations ALL DEPENDS ${QM_FILES})
install(FILES ${QM_FILES} DESTINATION ${TRANSLATIONS_DIR})

install(TARGETS lingmo-updater RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES lingmo-updater.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
# Session daemon: checks for updates periodically and shows the tray icon
install(FILES lingmo-updater-autostart.desktop
        DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/xdg/autostart
        RENAME lingmo-updater.desktop)
