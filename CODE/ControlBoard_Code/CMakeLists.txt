# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD myboard)
set(PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR}/include/boards)
# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(ControlBoard_Code C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(ControlBoard_Code ControlBoard_Code.c )

pico_set_program_name(ControlBoard_Code "ControlBoard_Code")
pico_set_program_version(ControlBoard_Code "0.1")

# Generate PIO header
pico_generate_pio_header(ControlBoard_Code ${CMAKE_CURRENT_LIST_DIR}/blink.pio)

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(ControlBoard_Code 0)
pico_enable_stdio_usb(ControlBoard_Code 1)



# Add the standard library to the build
target_link_libraries(ControlBoard_Code
        pico_stdlib)

set(TFT_DRIVER st7735 CACHE STRING "TFT Driver")
add_subdirectory(lib/pico-tft)

# Add the standard include files to the build
target_include_directories(ControlBoard_Code PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

# Add any user requested libraries
target_link_libraries(ControlBoard_Code 
        hardware_spi
        hardware_pio
        pico_tft
        )

pico_add_extra_outputs(ControlBoard_Code)

