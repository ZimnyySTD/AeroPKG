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
    __ORBIS__
    SQLITE_THREADSAFE=0
    SQLITE_DEFAULT_MEMSTATUS=0
)

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

set(PKG_STAGE ${CMAKE_CURRENT_BINARY_DIR}/pkg_stage)
set(PKGTOOL ${OO_TOOLCHAIN}/bin/linux/PkgTool.Core)
set(OOTOOL ${OO_TOOLCHAIN}/bin/linux)

add_custom_command(TARGET AeroPKG.elf POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${PKG_STAGE}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PKG_STAGE}/sce_sys/about
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PKG_STAGE}/sce_module
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PKG_STAGE}/assets/fonts
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:AeroPKG.elf> ${PKG_STAGE}/AeroPKG.elf
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/sce_sys/about/right.sprx ${PKG_STAGE}/sce_sys/about/right.sprx
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/sce_sys/icon0.png ${PKG_STAGE}/sce_sys/icon0.png
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/sce_module/libc.prx ${PKG_STAGE}/sce_module/libc.prx
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/sce_module/libSceFios2.prx ${PKG_STAGE}/sce_module/libSceFios2.prx
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/font.ttf ${PKG_STAGE}/assets/fonts/font.ttf
    COMMAND ${CMAKE_COMMAND} -E env DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1 ${PKGTOOL} sfo_new ${PKG_STAGE}/sce_sys/param.sfo
    COMMAND ${CMAKE_COMMAND} -E env DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1 ${PKGTOOL} sfo_setentry ${PKG_STAGE}/sce_sys/param.sfo APP_TYPE --type Integer --maxsize 4 --value 1
    COMMAND ${CMAKE_COMMAND} -E env DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1 ${PKGTOOL} sfo_setentry ${PKG_STAGE}/sce_sys/param.sfo APP_VER --type Utf8 --maxsize 8 --value ${APP_VERSION}
    COMMAND ${CMAKE_COMMAND} -E env DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1 ${PKGTOOL} sfo_setentry ${PKG_STAGE}/sce_sys/param.sfo ATTRIBUTE --type Integer --maxsize 4 --value 0
    COMMAND ${CMAKE_COMMAND} -E env DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1 ${PKGTOOL} sfo_setentry ${PKG_STAGE}/sce_sys/param.sfo CATEGORY --type Utf8 --maxsize 4 --value gd
    COMMAND ${CMAKE_COMMAND} -E env DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1 ${PKGTOOL} sfo_setentry ${PKG_STAGE}/sce_sys/param.sfo CONTENT_ID --type Utf8 --maxsize 48 --value ${CONTENT_ID}
    COMMAND ${CMAKE_COMMAND} -E env DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1 ${PKGTOOL} sfo_setentry ${PKG_STAGE}/sce_sys/param.sfo DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
    COMMAND ${CMAKE_COMMAND} -E env DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1 ${PKGTOOL} sfo_setentry ${PKG_STAGE}/sce_sys/param.sfo SYSTEM_VER --type Integer --maxsize 4 --value 0
    COMMAND ${CMAKE_COMMAND} -E env DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1 ${PKGTOOL} sfo_setentry ${PKG_STAGE}/sce_sys/param.sfo TITLE --type Utf8 --maxsize 128 --value ${APP_TITLE}
    COMMAND ${CMAKE_COMMAND} -E env DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1 ${PKGTOOL} sfo_setentry ${PKG_STAGE}/sce_sys/param.sfo TITLE_ID --type Utf8 --maxsize 12 --value ${APP_TITLE_ID}
    COMMAND ${CMAKE_COMMAND} -E env DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1 ${PKGTOOL} sfo_setentry ${PKG_STAGE}/sce_sys/param.sfo VERSION --type Utf8 --maxsize 8 --value ${APP_VERSION}
    COMMAND ${OOTOOL}/create-fself -in=${PKG_STAGE}/AeroPKG.elf -out=${PKG_STAGE}/AeroPKG.oelf --eboot ${PKG_STAGE}/eboot.bin --paid 0x3800000000000011
    COMMAND ${OOTOOL}/create-gp4 -out ${PKG_STAGE}/pkg.gp4 --content-id=${CONTENT_ID} --files "${PKG_STAGE}/eboot.bin ${PKG_STAGE}/sce_sys/about/right.sprx ${PKG_STAGE}/sce_sys/param.sfo ${PKG_STAGE}/sce_sys/icon0.png ${PKG_STAGE}/sce_module/libc.prx ${PKG_STAGE}/sce_module/libSceFios2.prx ${PKG_STAGE}/assets/fonts/font.ttf"
    COMMAND ${CMAKE_COMMAND} -E env DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1 ${PKGTOOL} pkg_build ${PKG_STAGE}/pkg.gp4 ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} -E rename ${CMAKE_CURRENT_BINARY_DIR}/${CONTENT_ID}.pkg ${CMAKE_CURRENT_BINARY_DIR}/AeroPKG.pkg
    COMMENT "Building PS4 EBOOT.BIN and AeroPKG.pkg"
    VERBATIM
)
