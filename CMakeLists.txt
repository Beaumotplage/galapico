# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.0.0)
set(toolchainVersion 13_2_Rel1)
set(picotoolVersion 2.0.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NEVER EDIT THE NEXT LINES for Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.0.0)
set(toolchainVersion 13_2_Rel1)
set(picotoolVersion 2.0.0)
include(${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
# ====================================================================================
set(PICO_BOARD pico2 CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(GALAPICO C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1


add_executable(Galapico 
    Main_galapico.cpp  
    hal_pwm_audio.cpp
    hal_Video.cpp 
    galagino/i8048.cpp
    galagino/leds.cpp
    galagino/z80.cpp
    galagino/emulation.cpp)


# must match with pio filename and executable name from above
pico_generate_pio_header(Galapico ${CMAKE_CURRENT_LIST_DIR}/rgb_notransparency.pio)
pico_generate_pio_header(Galapico ${CMAKE_CURRENT_LIST_DIR}/csync.pio)


pico_set_program_name(Galapico "Galapico")
pico_set_program_version(Galapico "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(Galapico 0)
pico_enable_stdio_usb(Galapico 0)

# Add the standard library to the build
target_link_libraries(Galapico
    pico_stdlib
	hardware_pio 
	hardware_dma 
	hardware_irq 
	hardware_pwm 
	hardware_spi 
    pico_multicore
	)

# Add the standard include files to the build
target_include_directories(Galapico PRIVATE /app
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/.. # for our common lwipopts or any other standard includes, if required
)

pico_add_extra_outputs(Galapico)

