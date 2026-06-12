# Pico 2W Multi-Peripheral Platform
# LED Matrix (WS2812B) + GPS (NEO-6M) + WiFi Web Server

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
set(PICO_BOARD pico2_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(pico2w-dev C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# ─── Source files ────────────────────────────────────────────────────────────

add_executable(pico2w-dev
    pico2w-dev.c
    ws2812.c
    matrix.c
    gps.c
    webserver.c
)

pico_set_program_name(pico2w-dev "pico2w-dev")
pico_set_program_version(pico2w-dev "0.2")

# ─── PIO program generation ─────────────────────────────────────────────────
# Generate ws2812_pio.pio.h from the PIO assembly file
pico_generate_pio_header(pico2w-dev ${CMAKE_CURRENT_LIST_DIR}/ws2812_pio.pio)

# ─── Web Server Content ─────────────────────────────────────────────────────
pico_add_library(pico_httpd_content NOFLAG)

pico_set_lwip_httpd_content(pico_httpd_content INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/www/index.shtml
    ${CMAKE_CURRENT_LIST_DIR}/www/gpsdata.shtml
    ${CMAKE_CURRENT_LIST_DIR}/www/style.css
    ${CMAKE_CURRENT_LIST_DIR}/www/app.js
)

# ─── stdio configuration ────────────────────────────────────────────────────
# USB serial for debug output (disable UART stdio since UART0 is used by GPS)
pico_enable_stdio_uart(pico2w-dev 0)
pico_enable_stdio_usb(pico2w-dev 1)

# ─── Include directories ────────────────────────────────────────────────────

target_include_directories(pico2w-dev PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

# ─── Link libraries ─────────────────────────────────────────────────────────

target_link_libraries(pico2w-dev
    pico_stdlib
    pico_cyw43_arch_lwip_poll   # WiFi + lwIP in poll mode (replaces _none)
    pico_lwip_http              # lwIP HTTP server (httpd with SSI/CGI)
    pico_httpd_content          # Link our custom web assets
    hardware_uart               # UART for GPS
    hardware_pio                # PIO for WS2812
)

# ─── Extra outputs (.uf2, .hex, etc.) ───────────────────────────────────────

pico_add_extra_outputs(pico2w-dev)
