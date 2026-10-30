# AeroPKG OpenOrbis Makefile
TITLE       := AeroPKG
VERSION     := 1.00
TITLE_ID    := APKG00001
CONTENT_ID  := IV0000-APKG00001_00-AEROPKG000000000

# Libraries linked into the ELF.
LIBS        := -lc -lkernel -lc++ -lc++abi -lpthread -lSceUserService -lSceVideoOut -lSceAudioOut -lScePad -lSceSysmodule -lSceNet -lSceNetCtl -lSceHttp -lSceSsl -lSceFreeType -lSDL2 -lSDL2_image

# Root vars
TOOLCHAIN   := $(OO_PS4_TOOLCHAIN)
PROJDIR     := src
INTDIR      := build
SQLITE_DIR  := third_party/sqlite
SQLITE_SRC  := $(SQLITE_DIR)/sqlite3.c

# Define objects to build
CFILES      := $(wildcard $(PROJDIR)/*.c)
CPPFILES    := $(wildcard $(PROJDIR)/*.cpp)
C_OBJS      := $(patsubst $(PROJDIR)/%.c, $(INTDIR)/%.o, $(CFILES))
CPP_OBJS    := $(patsubst $(PROJDIR)/%.cpp, $(INTDIR)/%.o, $(CPPFILES))
OBJS        := $(C_OBJS) $(CPP_OBJS) $(INTDIR)/sqlite3.o

# Define final C/C++ flags
TARGET_FLAGS := --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -isysroot $(TOOLCHAIN) -isystem $(TOOLCHAIN)/include -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L
CFLAGS      := $(TARGET_FLAGS) -c -Iinclude -I$(SQLITE_DIR)
CXXFLAGS    := $(TARGET_FLAGS) -c -Iinclude -I$(SQLITE_DIR) -isystem $(TOOLCHAIN)/include/c++/v1 -std=c++17
SQLITEFLAGS := $(TARGET_FLAGS) -c -I$(SQLITE_DIR) -DSQLITE_THREADSAFE=0 -DSQLITE_DEFAULT_MEMSTATUS=0
LDFLAGS     := --target=x86_64-pc-freebsd12-elf -fuse-ld=lld -pie --sysroot=$(TOOLCHAIN) --script $(TOOLCHAIN)/link.x --eh-frame-hdr -L$(TOOLCHAIN)/lib $(LIBS) $(TOOLCHAIN)/lib/crt1.o

CC          := clang
CCX         := clang++
LD          := ld.lld
CDIR        := linux

all: AeroPKG.pkg

AeroPKG.pkg: pkg.gp4
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core pkg_build $< .
	@if [ -f "$(CONTENT_ID).pkg" ]; then mv $(CONTENT_ID).pkg AeroPKG.pkg; fi

pkg.gp4: eboot.bin sce_sys/param.sfo
	$(TOOLCHAIN)/bin/$(CDIR)/create-gp4 -out $@ --content-id=$(CONTENT_ID) --files "$^"

sce_sys/param.sfo:
	mkdir -p sce_sys
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_new $@
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ APP_TYPE --type Integer --maxsize 4 --value 1
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ APP_VER --type Utf8 --maxsize 8 --value '$(VERSION)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ ATTRIBUTE --type Integer --maxsize 4 --value 0
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ CATEGORY --type Utf8 --maxsize 4 --value 'gd'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ CONTENT_ID --type Utf8 --maxsize 48 --value '$(CONTENT_ID)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ SYSTEM_VER --type Integer --maxsize 4 --value 0
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ TITLE --type Utf8 --maxsize 128 --value '$(TITLE)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ TITLE_ID --type Utf8 --maxsize 12 --value '$(TITLE_ID)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ VERSION --type Utf8 --maxsize 8 --value '$(VERSION)'

eboot.bin: $(OBJS)
	$(LD) $(INTDIR)/*.o -o $(INTDIR)/AeroPKG.elf $(LDFLAGS)
	$(TOOLCHAIN)/bin/$(CDIR)/create-fself -in=$(INTDIR)/AeroPKG.elf -out=$(INTDIR)/AeroPKG.oelf --eboot "eboot.bin" --paid 0x3800000000000011

$(INTDIR)/%.o: $(PROJDIR)/%.c
	mkdir -p $(INTDIR)
	$(CC) $(CFLAGS) -o $@ $<

$(INTDIR)/%.o: $(PROJDIR)/%.cpp
	mkdir -p $(INTDIR)
	$(CCX) $(CXXFLAGS) -o $@ $<

$(INTDIR)/sqlite3.o: $(SQLITE_SRC)
	mkdir -p $(INTDIR)
	$(CC) $(SQLITEFLAGS) -o $@ $<

clean:
	rm -rf build AeroPKG.pkg $(CONTENT_ID).pkg pkg.gp4 eboot.bin sce_sys
