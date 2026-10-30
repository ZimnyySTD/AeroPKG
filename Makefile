# AeroPKG OpenOrbis Makefile
OO_TOOLCHAIN ?= $(OO_PS4_TOOLCHAIN)
CC      := clang
CXX     := clang++
OBJCOPY := objcopy
ODIR    := build
SDIR    := src
IDIRS   := -Iinclude -I$(OO_TOOLCHAIN)/include

TITLE_ID := APKG00001
APP_NAME := AeroPKG

CFLAGS   := $(IDIRS) -O3 -std=c11 -fPIE -D__PS4__
CXXFLAGS := $(IDIRS) -O3 -std=c++17 -fPIE -D__PS4__
LDFLAGS  := -L$(OO_TOOLCHAIN)/lib -lkernel -lSceBgftService -lSceAppInstUtil \
            -lSceUserService -lScePad -lSceSysmodule -lSceNet -lSceNetCtl \
            -lSceHttp -lSceSsl -lsqlite3 -lSDL2 -lSDL2_image -lSDL2_ttf -lpthread -lc

CPPS := $(wildcard $(SDIR)/*.cpp)
CS   := $(wildcard $(SDIR)/*.c)
OBJS := $(patsubst $(SDIR)/%.cpp, $(ODIR)/%.o, $(CPPS)) $(patsubst $(SDIR)/%.c, $(ODIR)/%.o, $(CS))

TARGET := $(ODIR)/$(APP_NAME).elf

all: $(TARGET) pkg

$(ODIR)/%.o: $(SDIR)/%.cpp
	@mkdir -p $(ODIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(ODIR)/%.o: $(SDIR)/%.c
	@mkdir -p $(ODIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(TARGET): $(OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

pkg: $(TARGET)
	create-eboot -in=$(TARGET) -out=eboot.bin
	create-gp4 -proj=$(APP_NAME).gp4 -titleid=$(TITLE_ID) -appname="$(APP_NAME)"
	create-pkg -in=$(APP_NAME).gp4 -out=$(APP_NAME).pkg

clean:
	rm -rf $(ODIR) eboot.bin $(APP_NAME).gp4 $(APP_NAME).pkg
