set(_boxing_trainer_vcpkg_root "$ENV{VCPKG_ROOT}")

if(_boxing_trainer_vcpkg_root STREQUAL "" AND DEFINED ENV{HOME})
    set(_boxing_trainer_home_vcpkg "$ENV{HOME}/vcpkg")
    if(EXISTS "${_boxing_trainer_home_vcpkg}/scripts/buildsystems/vcpkg.cmake")
        set(_boxing_trainer_vcpkg_root "${_boxing_trainer_home_vcpkg}")
        message(STATUS "VCPKG_ROOT is not set; using ${_boxing_trainer_vcpkg_root}")
    endif()
endif()

if(_boxing_trainer_vcpkg_root STREQUAL "")
    message(FATAL_ERROR
        "VCPKG_ROOT is not set and $HOME/vcpkg was not found. Set VCPKG_ROOT "
        "to a vcpkg checkout before using the desktop-vcpkg preset, for "
        "example: export VCPKG_ROOT=\"$HOME/vcpkg\"")
endif()

set(_boxing_trainer_vcpkg_toolchain "${_boxing_trainer_vcpkg_root}/scripts/buildsystems/vcpkg.cmake")

if(NOT EXISTS "${_boxing_trainer_vcpkg_toolchain}")
    message(FATAL_ERROR
        "VCPKG_ROOT does not point at a usable vcpkg checkout. Expected to find: "
        "${_boxing_trainer_vcpkg_toolchain}")
endif()

include("${_boxing_trainer_vcpkg_toolchain}")
