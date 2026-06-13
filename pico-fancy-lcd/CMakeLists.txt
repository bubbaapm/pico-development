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
cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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

set(PICO_BOARD pico2_w CACHE STRING "Board type")

include(pico_sdk_import.cmake)

project(pico-fancy-lcd C CXX ASM)

pico_sdk_init()

add_executable(pico-fancy-lcd
    pico-fancy-lcd.c
    fancy_lcd_app.c
    st7735.c
    lcd_animations.c
    buttons.c
    ld2410.c
    webserver.c
    web_capture.c
    animations/anim_basic.c
    animations/anim_plasma.c
    animations/anim_geom.c
    animations/anim_tunnels.c
    animations/anim_grid.c
    animations/anim_particles.c
    animations/anim_retro.c
    animations/anim_games.c
    animations/anim_extra.c
    animations/anim_demo_all.c
)

pico_set_program_name(pico-fancy-lcd "pico-fancy-lcd")
pico_set_program_version(pico-fancy-lcd "0.1")

pico_add_library(pico_httpd_content NOFLAG)
pico_set_lwip_httpd_content(pico_httpd_content INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/www/index.shtml
    ${CMAKE_CURRENT_LIST_DIR}/www/style.css
    ${CMAKE_CURRENT_LIST_DIR}/www/app.js
)

pico_enable_stdio_uart(pico-fancy-lcd 0)
pico_enable_stdio_usb(pico-fancy-lcd 1)

target_include_directories(pico-fancy-lcd PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(pico-fancy-lcd
    pico_stdlib
    pico_cyw43_arch_lwip_poll
    pico_lwip_http
    pico_httpd_content
    hardware_spi
    hardware_gpio
    hardware_uart
    hardware_timer
    hardware_dma
)

pico_add_extra_outputs(pico-fancy-lcd)
