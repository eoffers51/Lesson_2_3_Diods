# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.22)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# Each build folder (build, build_rtos, build_trace) has its own sdkconfig, and
# only build_trace pulls in esp_sysview. Keeping the component manager's lock
# file per build folder stops the builds from rewriting each other's lock.
idf_build_set_property(DEPENDENCIES_LOCK "${CMAKE_BINARY_DIR}/dependencies.lock")
project(Lesson_2_3_Diods)
