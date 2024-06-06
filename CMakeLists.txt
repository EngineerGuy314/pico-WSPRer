# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_BUILD_TYPE "Release") 

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
set(PICO_BOARD pico CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.4.0")
  message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.4.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

project(pico-WSPRer C CXX ASM)

pico_sdk_init()
add_executable(pico-WSPRer)
pico_generate_pio_header(pico-WSPRer ${CMAKE_CURRENT_LIST_DIR}/hf-oscillator/piodco/dco2.pio)

target_sources(pico-WSPRer PUBLIC
	           ${CMAKE_CURRENT_LIST_DIR}/hf-oscillator/lib/assert.c
               ${CMAKE_CURRENT_LIST_DIR}/hf-oscillator/piodco/piodco.c
               ${CMAKE_CURRENT_LIST_DIR}/hf-oscillator/gpstime/GPStime.c
               ${CMAKE_CURRENT_LIST_DIR}/TxChannel/TxChannel.c
               ${CMAKE_CURRENT_LIST_DIR}/WSPRbeacon/thirdparty/WSPRutility.c
               ${CMAKE_CURRENT_LIST_DIR}/WSPRbeacon/thirdparty/nhash.c
               ${CMAKE_CURRENT_LIST_DIR}/WSPRbeacon/thirdparty/maidenhead.c
               ${CMAKE_CURRENT_LIST_DIR}/WSPRbeacon/WSPRbeacon.c
               ${CMAKE_CURRENT_LIST_DIR}/debug/logutils.c
               ${CMAKE_CURRENT_LIST_DIR}/utilities.c
               ${CMAKE_CURRENT_LIST_DIR}/init.c
               ${CMAKE_CURRENT_LIST_DIR}/core1.c
               ${CMAKE_CURRENT_LIST_DIR}/main.c
              )

pico_set_program_name(pico-WSPRer "pico-WSPRer")
pico_set_program_version(pico-WSPRer "0.5")

pico_enable_stdio_uart(pico-WSPRer 0) # disable stdio to UART
pico_enable_stdio_usb(pico-WSPRer 1)

# Add the standard include files to the build
target_include_directories(pico-WSPRer PRIVATE
                           ${CMAKE_CURRENT_LIST_DIR}
                           ${CMAKE_CURRENT_LIST_DIR}/hf-oscillator/piodco
                           ${CMAKE_CURRENT_LIST_DIR}/hf-oscillator/debug
                           ${CMAKE_CURRENT_LIST_DIR}/TxChannel
                           ${CMAKE_CURRENT_LIST_DIR}/WSPRbeacon
                           ${CMAKE_CURRENT_LIST_DIR}/WSPRbeacon/thirdparty
                           ${CMAKE_CURRENT_LIST_DIR}/..
                          )

target_link_libraries(pico-WSPRer
                      pico_stdlib
                      pico_sync
                      pico_multicore
                      hardware_timer
                      hardware_clocks
                      hardware_pio
					  hardware_adc
					  hardware_flash
					  hardware_sync
                     )

pico_add_extra_outputs(pico-WSPRer)
