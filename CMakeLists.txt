cmake_minimum_required(VERSION 3.19)
project(Palantir LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Sql HttpServer)

qt_standard_project_setup()

qt_add_executable(Palantir
    main.cpp
    config/config.ini
    config.h config.cpp
    Server/server.h Server/server.cpp
    Docs/api.md
)

target_link_libraries(Palantir
    PRIVATE
        Qt::Core
        Qt::Sql  # 🔹 Додано підтримку SQL
        Qt::HttpServer  # 🔹 Підключаємо HttpServer
)

include(GNUInstallDirs)

install(TARGETS Palantir
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET Palantir
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
