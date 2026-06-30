cmake_minimum_required(VERSION 3.28)

set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")
set(CMAKE_CXX_MODULE_STD ON)
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS ON CACHE BOOL "Suppress expected CMake developer warnings")

project(boxing_trainer_cpp
    VERSION 0.1.0
    DESCRIPTION "Modern C++ port of boxing-trainer-go"
    LANGUAGES CXX)

option(BOXING_TRAINER_BUILD_GUI "Build the wxWidgets desktop application" ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wno-reserved-module-identifier>)
endif()

function(boxing_trainer_enable_cxx23_modules target_name visibility)
    target_compile_features(${target_name} ${visibility} cxx_std_23)
    set_target_properties(${target_name}
        PROPERTIES
            CXX_SCAN_FOR_MODULES ON
            CXX_MODULE_STD ON)
endfunction()

function(boxing_trainer_enable_project_warnings target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

add_library(boxing_trainer_core
    src/combo/combo.cpp
    src/model/model.cpp
    src/view/video_url.cpp)

target_sources(boxing_trainer_core
    PUBLIC
        FILE_SET CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src
        FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/src/common.cppm
            ${CMAKE_CURRENT_SOURCE_DIR}/src/text.cppm
            ${CMAKE_CURRENT_SOURCE_DIR}/src/combo/combo.cppm
            ${CMAKE_CURRENT_SOURCE_DIR}/src/model/model.cppm
            ${CMAKE_CURRENT_SOURCE_DIR}/src/view/video_url.cppm)

boxing_trainer_enable_cxx23_modules(boxing_trainer_core PUBLIC)
target_compile_definitions(boxing_trainer_core
    PUBLIC
        BOXING_TRAINER_RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources")
boxing_trainer_enable_project_warnings(boxing_trainer_core)

if(BOXING_TRAINER_BUILD_GUI)
    add_executable(boxing_trainer_cpp
        src/main.cpp
        src/view/app.cpp)

    target_sources(boxing_trainer_cpp
        PRIVATE
            FILE_SET CXX_MODULES
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src
            FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/view/app.cppm)

    target_link_libraries(boxing_trainer_cpp PRIVATE boxing_trainer_core)
    boxing_trainer_enable_cxx23_modules(boxing_trainer_cpp PRIVATE)
    boxing_trainer_enable_project_warnings(boxing_trainer_cpp)

    find_package(wxWidgets CONFIG REQUIRED)
    target_link_libraries(boxing_trainer_cpp PRIVATE wx::core wx::base)

    install(TARGETS boxing_trainer_cpp RUNTIME DESTINATION bin)
    install(DIRECTORY resources/ DESTINATION share/boxing-trainer-cpp)
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(Catch2 3 REQUIRED)

    add_executable(boxing_trainer_tests
        tests/test_main.cpp)

    target_link_libraries(boxing_trainer_tests PRIVATE boxing_trainer_core Catch2::Catch2WithMain)
    boxing_trainer_enable_cxx23_modules(boxing_trainer_tests PRIVATE)
    boxing_trainer_enable_project_warnings(boxing_trainer_tests)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(boxing_trainer_tests PRIVATE -Wno-c2y-extensions)
    endif()

    include(Catch)
    catch_discover_tests(boxing_trainer_tests)
endif()
