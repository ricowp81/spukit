# SPDX-License-Identifier: MIT
#
# spukit demo — builds with PSL1GHT (PSL1GHT and PS3DEV must be set).
#
#   make        -> build/spukit-demo.elf, .self, .fake.self
#   make pkg    -> build/spukit-demo.pkg (retail-style package, installable)
#   make clean

ifeq ($(strip $(PSL1GHT)),)
$(error PSL1GHT is not set)
endif

PROGRAM := spukit-demo
TITLE   := spukit demo
APPID   := SPUK00001
CONTENTID ?= UP0001-$(APPID)_00-0000000000000000

HERE := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
LIB  := $(abspath $(HERE)/../..)
OUT  := $(HERE)/build
OBJ  := $(OUT)/obj

ifneq ($(V),1)
  VERB := @
endif

MACHDEP_PPU := -mhard-float -fmodulo-sched -ffunction-sections -fdata-sections
MACHDEP_SPU := -mdual-nops -fmodulo-sched -ffunction-sections -fdata-sections
PPU_CFLAGS  := -std=gnu11 -O2 -Wall -Wextra -mcpu=cell $(MACHDEP_PPU) \
               -I$(PSL1GHT)/ppu/include -I$(LIB)/include -I$(HERE) -I$(OBJ)
SPU_CFLAGS  := -std=gnu11 -O2 -Wall -Wextra $(MACHDEP_SPU) \
               -I$(PSL1GHT)/spu/include -I$(LIB)/include -I$(HERE)

SPU_BINS := $(OBJ)/spu_mix.bin $(OBJ)/spu_lz.bin
EMBEDS   := $(addsuffix .o,$(SPU_BINS))

.PHONY: all pkg clean lib
.SECONDARY:
all: $(OUT)/$(PROGRAM).elf $(OUT)/$(PROGRAM).self $(OUT)/$(PROGRAM).fake.self
pkg: $(OUT)/$(PROGRAM).pkg

lib:
	$(VERB)$(MAKE) --no-print-directory -C $(LIB) OUT=$(OUT)/spukit
$(OUT)/spukit/libspukit.a: lib ;

$(OBJ)/main.o: $(HERE)/main.c $(HERE)/job.h | $(EMBEDS)
	@echo "  PPU CC  main.c"
	@mkdir -p $(@D)
	$(VERB)ppu-gcc $(PPU_CFLAGS) -c $< -o $@

$(OBJ)/spu_mix.bin: $(HERE)/spu/mix.c $(HERE)/job.h
	@echo "  SPU CC  spu/mix.c"
	@mkdir -p $(@D)
	$(VERB)spu-gcc $(SPU_CFLAGS) $< -Wl,--gc-sections -L$(PSL1GHT)/spu/lib -lsputhread -o $@

$(OBJ)/spu_lz.bin: $(LIB)/spu/lz.c
	@echo "  SPU CC  spukit/spu/lz.c"
	@mkdir -p $(@D)
	$(VERB)spu-gcc $(SPU_CFLAGS) $< -Wl,--gc-sections -L$(PSL1GHT)/spu/lib -lsputhread -o $@

# bin2s exports <file>_<ext>; the header declares the three symbols.
$(OBJ)/%.bin.o: $(OBJ)/%.bin
	@echo "  EMBED   $(notdir $<)"
	$(VERB)bin2s -a 64 $< | ppu-as -o $@
	$(VERB)sym=$$(echo $(notdir $<) | tr . _); \
	printf '#include <ppu-types.h>\nextern const u8 %s[];\nextern const u8 %s_end[];\nextern const u32 %s_size;\n' \
		$$sym $$sym $$sym > $(OBJ)/$$sym.h

$(OBJ)/$(PROGRAM).raw.elf: $(OBJ)/main.o $(EMBEDS) $(OUT)/spukit/libspukit.a
	@echo "  PPU LD  $(notdir $@)"
	$(VERB)ppu-gcc $(MACHDEP_PPU) -L$(PSL1GHT)/ppu/lib $^ -lrt -llv2 -lm -o $@

$(OUT)/$(PROGRAM).elf: $(OBJ)/$(PROGRAM).raw.elf
	@echo "  ELF     $(notdir $@)"
	$(VERB)ppu-strip $< -o $@
	$(VERB)sprxlinker $@

$(OUT)/$(PROGRAM).self: $(OUT)/$(PROGRAM).elf
	@echo "  SELF    $(notdir $@)"
	$(VERB)make_self $< $@ >/dev/null

$(OUT)/$(PROGRAM).fake.self: $(OUT)/$(PROGRAM).elf
	@echo "  FSELF   $(notdir $@)"
	$(VERB)fself $< $@

ICON0 ?= $(PS3DEV)/bin/ICON0.PNG

$(OUT)/$(PROGRAM).pkg: $(OUT)/$(PROGRAM).elf $(ICON0)
	@echo "  PKG     $(notdir $@)"
	$(VERB)rm -rf $(OBJ)/pkg && mkdir -p $(OBJ)/pkg/USRDIR
	$(VERB)cp $(ICON0) $(OBJ)/pkg/ICON0.PNG
	$(VERB)make_self_npdrm $< $(OBJ)/pkg/USRDIR/EBOOT.BIN $(CONTENTID) >/dev/null
	$(VERB)sfo --title "$(TITLE)" --appid "$(APPID)" -f $(PS3DEV)/bin/sfo.xml $(OBJ)/pkg/PARAM.SFO
	$(VERB)pkg --contentid $(CONTENTID) $(OBJ)/pkg/ $@ >/dev/null
	$(VERB)cp $@ $(basename $@).gnpdrm.pkg
	$(VERB)package_finalize $(basename $@).gnpdrm.pkg

clean:
	rm -rf $(OUT)
