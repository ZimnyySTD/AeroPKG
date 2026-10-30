cmake_minimum_required(VERSION 3.16)
project(AeroPKG C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# OpenOrbis Toolchain target configuration
if(NOT DEFINED ENV{OO_TOOLCHAIN})
    message(WARNING "OO_TOOLCHAIN environment variable is not defined. Ensure OpenOrbis is set up for local builds.")
endif()

# Title ID and Package Properties
set(APP_TITLE_ID "APKG00001")
set(APP_NAME "AeroPKG")
set(APP_VERSION "01.00")

include_directories(
    include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Core sources
file(GLOB_RECURSE SOURCES
    "src/*.cpp"
    "src/*.c"
)

add_executable(AeroPKG.elf ${SOURCES})

# Required PS4 / OpenOrbis system libraries
target_link_libraries(AeroPKG.elf
    -lkernel
    -lSceBgftService
    -lSceAppInstUtil
    -lSceUserService
    -lScePad
    -lSceSysmodule
    -lSceNet
    -lSceNetCtl
    -lSceHttp
    -lSceSsl
    -lsqlite3
    -lSDL2
    -lSDL2_image
    -lSDL2_ttf
    -lpthread
    -lc
)

# Custom target for package generation
add_custom_command(TARGET AeroPKG.elf POST_BUILD
    COMMAND create-eboot -in=AeroPKG.elf -out=eboot.bin
    COMMAND create-gp4 -proj=AeroPKG.gp4 -titleid=${APP_TITLE_ID} -appname=${APP_NAME}
    COMMAND create-pkg -in=AeroPKG.gp4 -out=AeroPKG.pkg
    COMMENT "Building PS4 EBOOT.BIN and AeroPKG.pkg"
)
