cmake_minimum_required(VERSION 3.16)
project(vk-cpp-bot LANGUAGES CXX)

# pybind11 3.x требует C++14. C++17 перенесён в фазу 3 — там же заменён <codecvt>,
# который в C++17 уже deprecated (держим -Wall чистым).
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(VKBOT_WITH_PYTHON "Встраивать интерпретатор Python (команда /py)" ON)
option(VKBOT_BUILD_TESTS "Строить интеграционный тест python-слоя (ctest)" ON)

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

# общие исходники (vkbot и тест оба используют их без main.cpp)
set(VKBOT_CORE_SOURCES
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
    src/game.cpp)

add_executable(vkbot ${VKBOT_CORE_SOURCES} src/main.cpp)

target_include_directories(vkbot PRIVATE include thr ${OpenCV_INCLUDE_DIRS})
target_compile_definitions(vkbot PRIVATE GIT_URL="${GIT_REMOTE_URL}" GIT_VER="${GIT_REV}")
target_link_libraries(vkbot PRIVATE CURL::libcurl ${OpenCV_LIBS})

if(MSVC)
    target_compile_options(vkbot PRIVATE /W3)
else()
    target_compile_options(vkbot PRIVATE -Wall)
endif()

# pybind11 использует hidden visibility для своих типов; в статическом исполняемом
# файле это безобидно, но гадает -Wattributes в местах, где py-типы лежат в полях.
set_source_files_properties(src/py.cpp PROPERTIES COMPILE_OPTIONS "-Wno-attributes")

# --- Python-слой на pybind11 (header-only)
if(VKBOT_WITH_PYTHON)
    find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
    find_package(pybind11 CONFIG QUIET)
    if(TARGET pybind11::embed)
        set(VKBOT_PY_TARGETS pybind11::embed)
    else()
        # pybind11 без cmake-конфига (pip-установка): берём инклюды из python-модуля
        execute_process(COMMAND ${Python3_EXECUTABLE} -m pybind11 --includes
                        OUTPUT_VARIABLE PYBIND11_INCLUDES OUTPUT_STRIP_TRAILING_WHITESPACE
                        ERROR_QUIET RESULT_VARIABLE _pybind11_rc)
        if(NOT _pybind11_rc STREQUAL "0")
            message(FATAL_ERROR
                "pybind11 не найден: установите pybind11-dev (apt) или pip install pybind11")
        endif()
        string(REGEX REPLACE "[-]I" ";" PYBIND11_DIRS "${PYBIND11_INCLUDES}")
        target_include_directories(vkbot PRIVATE ${PYBIND11_DIRS})
        set(VKBOT_PY_TARGETS Python3::Python)
    endif()
    target_sources(vkbot PRIVATE src/py.cpp)
    target_link_libraries(vkbot PRIVATE ${VKBOT_PY_TARGETS})
else()
    target_compile_definitions(vkbot PRIVATE NO_PYTHON)
endif()

# --- автоматическое тестирование (ctest): python-слой без VK-сети
if(VKBOT_WITH_PYTHON AND VKBOT_BUILD_TESTS)
    enable_testing()
    add_executable(vkbot_pytest ${VKBOT_CORE_SOURCES} src/py.cpp tests/py_integration.cpp)
    target_include_directories(vkbot_pytest PRIVATE include thr tests ${OpenCV_INCLUDE_DIRS})
    target_compile_definitions(vkbot_pytest PRIVATE GIT_URL="${GIT_REMOTE_URL}" GIT_VER="test")
    target_link_libraries(vkbot_pytest PRIVATE CURL::libcurl ${OpenCV_LIBS})
    if(TARGET pybind11::embed)
        target_link_libraries(vkbot_pytest PRIVATE pybind11::embed)
    else()
        target_include_directories(vkbot_pytest PRIVATE ${PYBIND11_DIRS})
        target_link_libraries(vkbot_pytest PRIVATE Python3::Python)
    endif()
    add_test(NAME py_integration COMMAND vkbot_pytest)
endif()
