cmake_minimum_required(VERSION 3.16)
project(AeroPKG C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# OpenOrbis Toolchain target configuration
if(DEFINED ENV{OO_PS4_TOOLCHAIN})
    set(OO_TOOLCHAIN $ENV{OO_PS4_TOOLCHAIN})
elseif(DEFINED ENV{OO_TOOLCHAIN})
    set(OO_TOOLCHAIN $ENV{OO_TOOLCHAIN})
else()
    message(WARNING "Neither OO_PS4_TOOLCHAIN nor OO_TOOLCHAIN is defined.")
endif()

# Title ID and Package Properties
set(APP_TITLE "AeroPKG")
set(APP_TITLE_ID "APKG00001")
set(APP_VERSION "01.00")
set(CONTENT_ID "IV0000-APKG00001_00-AEROPKG000000000")

include_directories(
    include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/SDL_ttf
)

# Core sources
file(GLOB_RECURSE SOURCES
    "src/*.cpp"
    "src/*.c"
)

list(APPEND SOURCES
    "third_party/sqlite/sqlite3.c"
    "third_party/SDL_ttf/SDL_ttf.c"
)

add_executable(AeroPKG.elf ${SOURCES})

target_compile_definitions(AeroPKG.elf PRIVATE
    SQLITE_THREADSAFE=0
    SQLITE_DEFAULT_MEMSTATUS=0
)

# Required PS4 / OpenOrbis system libraries
target_link_libraries(AeroPKG.elf
    -lc
    -lkernel
    -lc++
    -lSceUserService
    -lSceVideoOut
    -lSceAudioOut
    -lScePad
    -lSceSysmodule
    -lSceNet
    -lSceNetCtl
    -lSceHttp
    -lSceSsl
    -lSceFreeType
    -lSDL2
    -lSDL2_image
)

# Custom target for package generation
add_custom_command(TARGET AeroPKG.elf POST_BUILD
    COMMAND env DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1 ${OO_TOOLCHAIN}/bin/linux/PkgTool.Core sfo_new sce_sys/param.sfo
    COMMAND env DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1 ${OO_TOOLCHAIN}/bin/linux/PkgTool.Core sfo_setentry sce_sys/param.sfo APP_TYPE --type Integer --maxsize 4 --value 1
    COMMAND env DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1 ${OO_TOOLCHAIN}/bin/linux/PkgTool.Core sfo_setentry sce_sys/param.sfo APP_VER --type Utf8 --maxsize 8 --value ${APP_VERSION}
    COMMAND env DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1 ${OO_TOOLCHAIN}/bin/linux/PkgTool.Core sfo_setentry sce_sys/param.sfo ATTRIBUTE --type Integer --maxsize 4 --value 0
    COMMAND env DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1 ${OO_TOOLCHAIN}/bin/linux/PkgTool.Core sfo_setentry sce_sys/param.sfo CATEGORY --type Utf8 --maxsize 4 --value gd
    COMMAND env DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1 ${OO_TOOLCHAIN}/bin/linux/PkgTool.Core sfo_setentry sce_sys/param.sfo CONTENT_ID --type Utf8 --maxsize 48 --value ${CONTENT_ID}
    COMMAND env DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1 ${OO_TOOLCHAIN}/bin/linux/PkgTool.Core sfo_setentry sce_sys/param.sfo DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
    COMMAND env DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1 ${OO_TOOLCHAIN}/bin/linux/PkgTool.Core sfo_setentry sce_sys/param.sfo SYSTEM_VER --type Integer --maxsize 4 --value 0
    COMMAND env DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1 ${OO_TOOLCHAIN}/bin/linux/PkgTool.Core sfo_setentry sce_sys/param.sfo TITLE --type Utf8 --maxsize 128 --value ${APP_TITLE}
    COMMAND env DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1 ${OO_TOOLCHAIN}/bin/linux/PkgTool.Core sfo_setentry sce_sys/param.sfo TITLE_ID --type Utf8 --maxsize 12 --value ${APP_TITLE_ID}
    COMMAND env DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1 ${OO_TOOLCHAIN}/bin/linux/PkgTool.Core sfo_setentry sce_sys/param.sfo VERSION --type Utf8 --maxsize 8 --value ${APP_VERSION}
    COMMAND ${OO_TOOLCHAIN}/bin/linux/create-fself -in=AeroPKG.elf -out=AeroPKG.oelf --eboot "eboot.bin" --paid 0x3800000000000011
    COMMAND ${OO_TOOLCHAIN}/bin/linux/create-gp4 -out pkg.gp4 --content-id=${CONTENT_ID} --files "eboot.bin sce_sys/about/right.sprx sce_sys/param.sfo sce_sys/icon0.png sce_module/libc.prx sce_module/libSceFios2.prx assets/fonts/font.ttf"
    COMMAND env DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1 ${OO_TOOLCHAIN}/bin/linux/PkgTool.Core pkg_build pkg.gp4 .
    COMMAND cmake -E rename ${CONTENT_ID}.pkg AeroPKG.pkg
    COMMENT "Building PS4 EBOOT.BIN and AeroPKG.pkg"
)
