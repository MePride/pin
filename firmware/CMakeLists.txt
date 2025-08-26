# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# Set the target ESP32-C3
set(IDF_TARGET esp32c3)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(pin_firmware)