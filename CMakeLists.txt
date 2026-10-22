cmake_minimum_required(VERSION 3.16)
project(vk-cpp-bot LANGUAGES CXX)

# CMake >=3.30: модуль FindBoost удалён, ищем Boost только в config-режиме
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

# фаза 3 переведёт на C++17
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(VKBOT_WITH_PYTHON "Встраивать интерпретатор Python (команда /py)" ON)

# --- сведения о репозитории (раньше делал Makefile: -DGIT_URL=... -DGIT_VER=...)
find_package(Git QUIET)
set(GIT_REMOTE_URL "unknown")
set(GIT_REV "unknown")
if(GIT_FOUND)
    execute_process(COMMAND ${GIT_EXECUTABLE} config --get remote.origin.url
                    OUTPUT_VARIABLE GIT_REMOTE_URL OUTPUT_STRIP_TRAILING_WHITESPACE
                    ERROR_QUIET)
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short=6 HEAD
                    OUTPUT_VARIABLE GIT_REV OUTPUT_STRIP_TRAILING_WHITESPACE
                    ERROR_QUIET)
endif()

# --- обязательные зависимости
find_package(CURL REQUIRED)
find_package(OpenCV REQUIRED COMPONENTS core objdetect photo imgproc imgcodecs)

add_executable(vkbot
    src/net.cpp
    src/bd.cpp
    src/vk.cpp
    src/lp.cpp
    src/fs.cpp
    src/doc.cpp
    src/events.cpp
    src/worker.cpp
    src/str.cpp
    src/cmd.cpp
    src/cmds.cpp
    src/users.cpp
    src/img.cpp
    src/timer.cpp
    src/game.cpp
    src/main.cpp)

target_include_directories(vkbot PRIVATE include thr ${OpenCV_INCLUDE_DIRS})
target_compile_definitions(vkbot PRIVATE GIT_URL="${GIT_REMOTE_URL}" GIT_VER="${GIT_REV}")
target_link_libraries(vkbot PRIVATE CURL::libcurl ${OpenCV_LIBS})

if(MSVC)
    target_compile_options(vkbot PRIVATE /W3)
else()
    target_compile_options(vkbot PRIVATE -Wall)
endif()

# --- Python-слой (Boost.Python до миграции на pybind11, см. docs/TASKS.md фаза 2)
if(VKBOT_WITH_PYTHON)
    find_package(Python3 REQUIRED COMPONENTS Development)
    find_package(Boost QUIET COMPONENTS python3)
    if(TARGET Boost::python3)
        set(VKBOT_BOOST_PY Boost::python3)
    else()
        find_package(Boost QUIET COMPONENTS python)
        if(TARGET Boost::python)
            set(VKBOT_BOOST_PY Boost::python)
        else()
            # ручная подстраховка: библиотека с именем версии python, как в старом Makefile
            find_library(BOOST_PYTHON_LIB NAMES
                boost_python${Python3_VERSION_MAJOR}${Python3_VERSION_MINOR}
                boost_python${Python3_VERSION_MAJOR})
            if(NOT BOOST_PYTHON_LIB)
                message(FATAL_ERROR
                    "Boost.Python не найден. Установите libboost-python-dev "
                    "(или дождитесь фазы 2 — pybind11 без этой зависимости)")
            endif()
            set(VKBOT_BOOST_PY ${BOOST_PYTHON_LIB})
        endif()
    endif()
    target_sources(vkbot PRIVATE src/py.cpp)
    target_link_libraries(vkbot PRIVATE ${VKBOT_BOOST_PY} Python3::Python)
else()
    target_compile_definitions(vkbot PRIVATE NO_PYTHON)
endif()
