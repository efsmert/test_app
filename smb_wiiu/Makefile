#-------------------------------------------------------------------------------
.SUFFIXES:
#-------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITPRO)/wut/share/wut_rules

TARGET      := smb_wiiu
BUILD       := build
SOURCES     := src
INCLUDES    := src
ROMFS       := content

APP_NAME    := "Super Mario Bros."
APP_SHORTNAME := "SMB WiiU"
APP_AUTHOR  := "SMB1R Port"

CFLAGS   := -g -Wall -O2 -ffunction-sections $(MACHDEP)
CFLAGS   += $(INCLUDE) -D__WIIU__ -D__WUT__
CFLAGS   += $(shell $(DEVKITPRO)/portlibs/wiiu/bin/powerpc-eabi-pkg-config --cflags sdl2 SDL2_image SDL2_mixer)

CXXFLAGS := $(CFLAGS) -std=c++17
ASFLAGS  := -g $(MACHDEP)
LDFLAGS   = -g $(MACHDEP) $(RPXSPECS) -Wl,-Map,$(notdir $*.map)

LIBS     := $(shell $(DEVKITPRO)/portlibs/wiiu/bin/powerpc-eabi-pkg-config --libs sdl2 SDL2_image SDL2_mixer) -lwut
LIBDIRS  := $(PORTLIBS) $(WUT_ROOT)

# Include romfs-wiiu if available
-include $(DEVKITPRO)/portlibs/wiiu/share/romfs-wiiu.mk

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT   := $(CURDIR)/$(TARGET)
export TOPDIR   := $(CURDIR)
export VPATH    := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR  := $(CURDIR)/$(BUILD)

CFILES      := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES    := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES      := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))

ifeq ($(strip $(CPPFILES)),)
    export LD := $(CC)
else
    export LD := $(CXX)
endif

export OFILES_BIN :=
export OFILES_SRC := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES     := $(OFILES_BIN) $(OFILES_SRC)
export HFILES_BIN :=

export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                  $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                  -I$(CURDIR)/$(BUILD)

export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean all wuhb
.PHONY: cemu-sync

all: $(BUILD)
	@echo "Build complete: $(TARGET).rpx"

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

wuhb: $(BUILD)
	@echo "Creating WUHB with romfs content..."
	@wuhbtool $(TARGET).rpx $(TARGET).wuhb \
		--name=$(APP_NAME) \
		--short-name=$(APP_SHORTNAME) \
		--author=$(APP_AUTHOR) \
		--content=$(ROMFS)
	@echo ""
	@echo "=== BUILD COMPLETE ==="
	@echo "WUHB: $(TARGET).wuhb"
	@ls -lh $(TARGET).wuhb

cemu-sync: $(BUILD)
	@mkdir -p smb_wiiu_cemu/code smb_wiiu_cemu/content
	@cp -f $(TARGET).rpx smb_wiiu_cemu/code/
	@# Merge+overwrite generated content into the Cemu folder layout used by ../content/%s.
	@rm -rf smb_wiiu_cemu/content/tilesets 2>/dev/null || true
	@cp -R content/* smb_wiiu_cemu/content/ 2>/dev/null || true
	@echo "Synced to smb_wiiu_cemu/ (code + content)"

clean:
	@rm -fr $(BUILD) $(TARGET).rpx $(TARGET).elf $(TARGET).wuhb $(TARGET).map

else
.PHONY: all

DEPENDS := $(OFILES:.o=.d)

all: $(OUTPUT).rpx

$(OUTPUT).rpx: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)
$(OFILES_SRC): $(HFILES_BIN)

-include $(DEPENDS)

endif
