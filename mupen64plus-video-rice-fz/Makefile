# Experimental RiceFZ sidecar build for Mupen64Plus-ReARMed.
#
# First import the FZ source tree:
#   bash ../tools/import-rice-fz.sh
# Then build:
#   make -C ../mupen64plus-video-rice-fz
#
# This intentionally builds a separate Mupen64Plus video plugin .so. It does
# not replace or link together with the existing gles2rice renderer.

ROOT      := $(abspath ..)
SRC       := $(CURDIR)/upstream/src
TARGET    := mupen64plus-video-rice-fz.so
BUILD_DIR := $(CURDIR)/build

CC  ?= cc
CXX ?= c++
PKG_CONFIG ?= pkg-config

SDL_CFLAGS := $(shell $(PKG_CONFIG) --cflags sdl2 2>/dev/null)
SDL_LIBS   := $(shell $(PKG_CONFIG) --libs sdl2 2>/dev/null)
PNG_CFLAGS := $(shell $(PKG_CONFIG) --cflags libpng 2>/dev/null)
PNG_LIBS   := $(shell $(PKG_CONFIG) --libs libpng 2>/dev/null)

CPPFLAGS += -I$(SRC) \
            -I$(ROOT)/mupen64plus-core/src/api \
            -I$(ROOT)/libretro \
            $(SDL_CFLAGS) $(PNG_CFLAGS)

COMMON_FLAGS := -O2 -fPIC -fsigned-char -DNO_ASM -DUSE_GLES=1
CFLAGS   += $(COMMON_FLAGS) -std=gnu11
CXXFLAGS += $(COMMON_FLAGS) -std=c++11

LDLIBS += $(SDL_LIBS) $(PNG_LIBS) -lGLESv2 -ldl -lm -pthread
LDFLAGS += -shared

# Keep the exported ABI identical to a normal Mupen64Plus video plugin.
ifneq ($(wildcard $(SRC)/video_api_export.ver),)
LDFLAGS += -Wl,-version-script,$(SRC)/video_api_export.ver
endif

CXX_SOURCES := \
    $(SRC)/Blender.cpp \
    $(SRC)/Combiner.cpp \
    $(SRC)/Config.cpp \
    $(SRC)/ConvertImage.cpp \
    $(SRC)/ConvertImage16.cpp \
    $(SRC)/Debugger.cpp \
    $(SRC)/DeviceBuilder.cpp \
    $(SRC)/FrameBuffer.cpp \
    $(SRC)/GraphicsContext.cpp \
    $(SRC)/OGLCombiner.cpp \
    $(SRC)/OGLGraphicsContext.cpp \
    $(SRC)/OGLRender.cpp \
    $(SRC)/OGLRenderExt.cpp \
    $(SRC)/OGLTexture.cpp \
    $(SRC)/Render.cpp \
    $(SRC)/RenderBase.cpp \
    $(SRC)/RenderExt.cpp \
    $(SRC)/RenderTexture.cpp \
    $(SRC)/RSP_Parser.cpp \
    $(SRC)/RSP_S2DEX.cpp \
    $(SRC)/Texture.cpp \
    $(SRC)/TextureFilters.cpp \
    $(SRC)/TextureFilters_2xsai.cpp \
    $(SRC)/TextureFilters_hq2x.cpp \
    $(SRC)/TextureFilters_hq4x.cpp \
    $(SRC)/TextureManager.cpp \
    $(SRC)/VectorMath.cpp \
    $(SRC)/Video.cpp

C_SOURCES := \
    $(CURDIR)/osal_dynamiclib_rearmed.c \
    $(SRC)/osal_files_unix.c \
    $(SRC)/liblinux/BMGImage.c \
    $(SRC)/liblinux/BMGUtils.c \
    $(SRC)/liblinux/bmp.c \
    $(SRC)/liblinux/pngrw.c

CXX_OBJECTS := $(patsubst $(SRC)/%.cpp,$(BUILD_DIR)/cpp/%.o,$(CXX_SOURCES))
C_OBJECTS   := $(patsubst $(SRC)/%.c,$(BUILD_DIR)/c/%.o,$(filter $(SRC)/%,$(C_SOURCES)))
C_OBJECTS   += $(BUILD_DIR)/c/osal_dynamiclib_rearmed.o
OBJECTS     := $(CXX_OBJECTS) $(C_OBJECTS)

.PHONY: all clean check-source info

all: check-source $(TARGET)

check-source:
	@test -f "$(SRC)/Video.cpp" || { \
		echo "RiceFZ source is not imported yet."; \
		echo "Run: bash tools/import-rice-fz.sh from the repository root."; \
		exit 1; \
	}

info:
	@echo "Target:      $(TARGET)"
	@echo "FZ source:   $(SRC)"
	@echo "CXX:         $(CXX)"
	@echo "SDL cflags:  $(SDL_CFLAGS)"
	@echo "PNG cflags:  $(PNG_CFLAGS)"

$(TARGET): $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJECTS) $(LDLIBS)

$(BUILD_DIR)/cpp/%.o: $(SRC)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/c/%.o: $(SRC)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/c/osal_dynamiclib_rearmed.o: $(CURDIR)/osal_dynamiclib_rearmed.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(TARGET)
