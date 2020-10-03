#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules

#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# DATA is a list of directories containing data files
# INCLUDES is a list of directories containing header files
# GRAPHICS is a list of directories containing graphics files
# GFXBUILD is the directory where converted graphics files will be placed
#   If set to $(BUILD), it will statically link in the converted
#   files as if they were data files.
#
# NO_SMDH: if set to anything, no SMDH file is generated.
# ROMFS is the directory which contains the RomFS, relative to the Makefile (Optional)
# APP_TITLE is the name of the app stored in the SMDH file (Optional)
# APP_DESCRIPTION is the description of the app stored in the SMDH file (Optional)
# APP_AUTHOR is the author of the app stored in the SMDH file (Optional)
# ICON is the filename of the icon (.png), relative to the project folder.
#   If not set, it attempts to use one of the following (in this order):
#     - <Project name>.png
#     - icon.png
#     - <libctru folder>/default_icon.png
#---------------------------------------------------------------------------------
TARGET		:=	TinyVNC
BUILD		:=	build
SOURCES		:=	$(shell find -L src -type d 2> /dev/null)
DATA		:=	data
META		:=	meta
INCLUDES	:=	$(SOURCES)
GRAPHICS	:=	gfx
GFXBUILD	:=	$(BUILD)
ROMFS		:=	romfs
APP_TITLE	:=	TinyVNC
APP_DESCRIPTION	:=	VNC Viewer for Nintendo 3DS
APP_AUTHOR	:=	badda71 <me@badda.de>
ICON		:=	icon.png

VERSION_MAJOR :=	1
VERSION_MINOR :=	0
VERSION_MICRO :=	0

ifeq ($(VERSION_MICRO),0)
	VERSION	:=	$(VERSION_MAJOR).$(VERSION_MINOR)
else
	VERSION	:=	$(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_MICRO)
endif

# CIA Build options
PRODUCT_CODE	:=	CTR-P-TVNC
UNIQUE_ID		:=	0x0C2433
CPU_SPEED		:=	804MHz
ENABLE_L2_CACHE :=	true
SYSTEM_MODE_EXT :=	124MB
BANNER_AUDIO	:=	$(TOPDIR)/$(META)/audio.wav
BANNER_IMAGE	:=	$(TOPDIR)/$(META)/banner.png
LOGO			:=	$(TOPDIR)/$(META)/logo.lz11

ifeq (, $(shell which gm))
	MKICOPNG := cp -f
else
	MKICOPNG := gm convert -fill white -font "Picopixel-Standard"\
		-draw "font-size 8;text 1,47 'v$(VERSION)';"
endif


#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
ARCH	:=	-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft

CFLAGS	:=	-g -Wall -O3 -mword-relocations -ffast-math \
			-fomit-frame-pointer -ffunction-sections \
			$(ARCH)

CFLAGS	+=	$(INCLUDE) -DARM11 -D_3DS -DVERSION=\"$(VERSION)\"

CXXFLAGS	:= $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++11

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS	:= -lcurl -lmbedtls -lmbedx509 -lmbedcrypto -lmpg123 -lSDL_image -lSDL -lpng -ljpeg -lz -lcitro3d -lctru -lm

#---------------------------------------------------------------------------------
# makerom options (cia/3ds build)
#---------------------------------------------------------------------------------

CATEGORY ?= Application
USE_ON_SD ?= true
MEMORY_TYPE ?= Application
SYSTEM_MODE ?= 64MB
SYSTEM_MODE_EXT ?= Legacy
CPU_SPEED ?= 268MHz
ENABLE_L2_CACHE ?= false

COMMON_MAKEROM_FLAGS	:=	-rsf $(TOPDIR)/$(META)/template.rsf -target t -exefslogo -icon icon.icn -banner banner.bnr -major $(VERSION_MAJOR) -minor $(VERSION_MINOR) -micro $(VERSION_MICRO) -DAPP_TITLE="$(APP_TITLE)" -DAPP_PRODUCT_CODE="$(PRODUCT_CODE)" -DAPP_UNIQUE_ID="$(UNIQUE_ID)" -DAPP_SYSTEM_MODE="$(SYSTEM_MODE)" -DAPP_SYSTEM_MODE_EXT="$(SYSTEM_MODE_EXT)" -DAPP_CATEGORY="$(CATEGORY)" -DAPP_USE_ON_SD="$(USE_ON_SD)" -DAPP_MEMORY_TYPE="$(MEMORY_TYPE)" -DAPP_CPU_SPEED="$(CPU_SPEED)" -DAPP_ENABLE_L2_CACHE="$(ENABLE_L2_CACHE)" -DAPP_VERSION_MAJOR="$(VERSION_MAJOR)" -logo "$(LOGO)"

ifneq ("$(wildcard $(TOPDIR)/$(ROMFS))","")
	_3DSXTOOL_FLAGS += --romfs=$(TOPDIR)/$(ROMFS)
	COMMON_MAKEROM_FLAGS += -DAPP_ROMFS="$(TOPDIR)/$(ROMFS)"
endif

ifeq ($(suffix $(BANNER_IMAGE)),.cgfx)
	BANNER_IMAGE_ARG := -ci
else
	BANNER_IMAGE_ARG := -i
endif

ifeq ($(suffix $(BANNER_AUDIO)),.cwav)
	BANNER_AUDIO_ARG := -ca
else
	BANNER_AUDIO_ARG := -a
endif

#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
LIBDIRS	:= $(CTRULIB) $(PORTLIBS)

#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------

LIBS	:=	$(addprefix -l,$(SUBLIBS)) $(LIBS)
LIBDIRS	:=	$(addprefix $(TOPDIR)/,$(SUBLIBS)) $(LIBDIRS)

##################################################################################
ifneq ($(BUILD),$(notdir $(CURDIR)))
##################################################################################

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export TOPDIR	:=	$(CURDIR)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(GRAPHICS),$(CURDIR)/$(dir)) \
			$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
PICAFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.v.pica)))
SHLISTFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.shlist)))
GFXFILES	:=	$(foreach dir,$(GRAPHICS),$(notdir $(wildcard $(dir)/*.t3s)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
#---------------------------------------------------------------------------------
	export LD	:=	$(CC)
#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------
	export LD	:=	$(CXX)
#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------

#---------------------------------------------------------------------------------
ifeq ($(GFXBUILD),$(BUILD))
#---------------------------------------------------------------------------------
export T3XFILES :=  $(GFXFILES:.t3s=.t3x)
#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------
export ROMFS_T3XFILES	:=	$(patsubst %.t3s, $(GFXBUILD)/%.t3x, $(GFXFILES))
export T3XHFILES		:=	$(patsubst %.t3s, $(BUILD)/%.h, $(GFXFILES))
#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------

export OFILES_SOURCES 	:=	$(CPPFILES:.cpp=.opp) $(CFILES:.c=.o) $(SFILES:.s=.o)

export OFILES_BIN	:=	$(addsuffix .o,$(BINFILES)) \
			$(PICAFILES:.v.pica=.shbin.o) $(SHLISTFILES:.shlist=.shbin.o) \
			$(addsuffix .o,$(T3XFILES))

export OFILES := $(OFILES_BIN) $(OFILES_SOURCES)

export HFILES	:=	$(PICAFILES:.v.pica=_shbin.h) $(SHLISTFILES:.shlist=_shbin.h) \
			$(addsuffix .h,$(subst .,_,$(BINFILES))) \
			$(GFXFILES:.t3s=.h)

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

export _3DSXDEPS	:=	$(if $(NO_SMDH),,$(OUTPUT).smdh)

ifeq ($(strip $(ICON)),)
	icons := $(wildcard *.png)
	ifneq (,$(findstring $(TARGET).png,$(icons)))
		export APP_ICON := $(TOPDIR)/$(TARGET).png
	else
		ifneq (,$(findstring icon.png,$(icons)))
			export APP_ICON := $(TOPDIR)/icon.png
		endif
	endif
else
	export RAW_ICON := $(TOPDIR)/$(META)/$(ICON)
	export APP_ICON := $(CURDIR)/$(BUILD)/$(ICON)
endif

ifeq ($(strip $(NO_SMDH)),)
	export _3DSXFLAGS += --smdh=$(CURDIR)/$(TARGET).smdh
endif

ifneq ($(ROMFS),)
	export _3DSXFLAGS += --romfs=$(CURDIR)/$(ROMFS)
endif

SUBLIBS_CLEAN := $(SUBLIBS:%=clean-%)

.PHONY: all clean
.PHONY: $(SUBLIBS)
.PHONY: $(SUBLIBS_CLEAN)

#---------------------------------------------------------------------------------
all: $(BUILD) $(GFXBUILD) $(DEPSDIR) $(ROMFS_T3XFILES) $(T3XHFILES)
	@echo building $(APP_TITLE)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile $(MAKECMDGOALS)

$(BUILD):
	@mkdir -p $@

ifneq ($(GFXBUILD),$(BUILD))
$(GFXBUILD):
	@mkdir -p $@
endif

ifneq ($(DEPSDIR),$(BUILD))
$(DEPSDIR):
	@mkdir -p $@
endif

#---------------------------------------------------------------------------------
clean: $(SUBLIBS_CLEAN)
	@echo cleaning $(TARGET) 
	@rm -fr $(BUILD) $(TARGET).3dsx $(TARGET).cia $(TARGET).3ds $(OUTPUT).smdh $(TARGET).elf $(GFXBUILD)

$(SUBLIBS_CLEAN):
	@echo cleaning $(@:clean-%=%)
	@$(MAKE) --no-print-directory -C $(@:clean-%=%) clean

#---------------------------------------------------------------------------------
$(GFXBUILD)/%.t3x	$(BUILD)/%.h	:	%.t3s
#---------------------------------------------------------------------------------
	@echo $<
	@tex3ds -i $< -H $(BUILD)/$*.h -d $(DEPSDIR)/$*.d -o $(GFXBUILD)/$*.t3x

##################################################################################
else 
##################################################################################

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
all	:	$(OUTPUT).3dsx $(OUTPUT).cia $(OUTPUT).3ds

$(SUBLIBS):
	@$(MAKE) -C ../$@

$(OUTPUT).3dsx	:	$(OUTPUT).elf $(_3DSXDEPS) $(TOPDIR)/$(ROMFS)

$(OFILES_SOURCES) : $(HFILES)

$(OUTPUT).elf	:	$(OFILES) $(SUBLIBS)

%.opp: %.cpp
	@echo $(notdir $<)
	@$(CXX) -MMD -MP -MF $(DEPSDIR)/$*.d $(CXXFLAGS) -c $< -o $@ $(ERROR_FILTER)

#---------------------------------------------------------------------------------
# cia/3ds generation targets
#---------------------------------------------------------------------------------
$(APP_ICON): $(RAW_ICON) $(TOPDIR)/Makefile
	@echo -n generating $@ ...
	@$(MKICOPNG) $< $@
	@echo OK

icon.icn: $(APP_ICON)
	@echo -n generating $(notdir $@) ...
	@bannertool makesmdh -s "$(APP_TITLE)" -l "$(APP_TITLE) - $(APP_DESCRIPTION)" -p "$(APP_AUTHOR)" -i $(APP_ICON) -o $@
	@echo OK

banner.bnr: $(BANNER_IMAGE) $(BANNER_AUDIO)
	@echo -n generating $(notdir $@) ...
	@bannertool makebanner $(BANNER_IMAGE_ARG) $(BANNER_IMAGE) $(BANNER_AUDIO_ARG) $(BANNER_AUDIO) -o $@
	@echo OK

$(OUTPUT).3ds: $(OUTPUT).elf banner.bnr icon.icn
	@echo -n building $(notdir $(OUTPUT).3ds) ... 
	@makerom -f cci -o $@ -elf $< -DAPP_ENCRYPTED=true $(COMMON_MAKEROM_FLAGS) 2> /dev/null
	@echo OK

$(OUTPUT).cia: $(OUTPUT).elf banner.bnr icon.icn
	@echo -n building $(notdir $(OUTPUT).cia) ... 
	@makerom -f cia -o $@ -elf $< -DAPP_ENCRYPTED=false $(COMMON_MAKEROM_FLAGS)
	@echo OK

#---------------------------------------------------------------------------------
# you need a rule like this for each extension you use as binary data
#---------------------------------------------------------------------------------
%.bin.o	%_bin.h :	%.bin
#---------------------------------------------------------------------------------
	@echo $<
	@$(bin2o)

#---------------------------------------------------------------------------------
.PRECIOUS	:	%.t3x
#---------------------------------------------------------------------------------
%.t3x.o	%_t3x.h :	%.t3x
#---------------------------------------------------------------------------------
	@echo $<
	@$(bin2o)

#---------------------------------------------------------------------------------
# rules for assembling GPU shaders
#---------------------------------------------------------------------------------
define shader-as
	$(eval CURBIN := $*.shbin)
	$(eval DEPSFILE := $(DEPSDIR)/$*.shbin.d)
	echo "$(CURBIN).o: $< $1" > $(DEPSFILE)
	echo "extern const u8" `(echo $(CURBIN) | sed -e 's/^\([0-9]\)/_\1/' | tr . _)`"_end[];" > `(echo $(CURBIN) | tr . _)`.h
	echo "extern const u8" `(echo $(CURBIN) | sed -e 's/^\([0-9]\)/_\1/' | tr . _)`"[];" >> `(echo $(CURBIN) | tr . _)`.h
	echo "extern const u32" `(echo $(CURBIN) | sed -e 's/^\([0-9]\)/_\1/' | tr . _)`_size";" >> `(echo $(CURBIN) | tr . _)`.h
	picasso -o $(CURBIN) $1
	bin2s $(CURBIN) | $(AS) -o $*.shbin.o
endef

%.shbin.o %_shbin.h : %.v.pica %.g.pica
	@echo $<
	@$(call shader-as,$^)

%.shbin.o %_shbin.h : %.v.pica
	@echo $<
	@$(call shader-as,$<)

%.shbin.o %_shbin.h : %.shlist
	@echo $<
	@$(call shader-as,$(foreach file,$(shell cat $<),$(dir $<)$(file)))

#---------------------------------------------------------------------------------
%.t3x	%.h	:	%.t3s
#---------------------------------------------------------------------------------
	@echo $<
	@tex3ds -i $< -H $*.h -d $*.d -o $*.t3x

-include $(DEPSDIR)/*.d

##################################################################################
endif
##################################################################################
