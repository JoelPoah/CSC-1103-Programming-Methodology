# Makefile - builds Stage 0 and every version on the efficiency ladder
#            (run from the project root)
#
#   make            build everything into bin/ (stage0 + every ladder version)
#   make ladder     build the ladder versions only
#   make stage0     build stage0 only (question a + question b)
#   make test       build, then check every version against test20_expected.txt
#   make report     build, then run the full automated report (run_report.sh)
#   make plot       build, then draw results/signals.png with Version 0
#   make clean      delete all built executables
#
# Works with GNU make on Windows (MinGW gcc adds .exe automatically), Linux and macOS.
# The assembly version (v7) is only built on 64-bit ARM Linux, e.g. the Raspberry Pi 5.

CC = gcc
WARN = -Wall

# Windows adds .exe to every executable; Linux / macOS do not
ifeq ($(OS),Windows_NT)
EXE = .exe
RM  = del /Q
else
EXE =
RM  = rm -f
endif

# What machine does the compiler build for?  e.g. aarch64-linux-gnu on the Pi
MACHINE := $(shell $(CC) -dumpmachine)
ifneq (,$(findstring aarch64,$(MACHINE)))
ifneq (,$(findstring linux,$(MACHINE)))
ON_ARM64_LINUX = yes
endif
endif

# typing just "make" runs the "all" target (defined near the bottom)
.DEFAULT_GOAL := all

# ======================= the parts =======================
# Every version is a main program plus a choice of parts.

HELPER      = helper_function/trade_io.c

# double engine (Stage A style: no structs, no pointers). SMA file added per version.
ENGINE      = src/engine/vwap.c src/engine/ohlc.c src/engine/signal.c src/engine/report.c
SMA_NAIVE   = src/engine/sma_naive.c
SMA_RUNNING = src/engine/sma_running.c

# our own readers (Stage A+)
READ_SSCANF = src/reader/read_trade_sscanf.c
READ_FAST   = src/reader/read_trade_fast.c
READ_FX     = src/reader/read_trade_fx.c

# trades held in memory (Stage B / C)
STORE        = src/store/trades.c
STORE_ARRAY  = src/store/trades_fx_array.c
STORE_MALLOC = src/store/trades_fx_malloc.c

# fixed-point engine: the same functions written in C and in assembly
ENGINE_FX   = src/engine_fx/vwap_fx.c src/engine_fx/ohlc_fx.c src/engine_fx/sma_fx.c src/engine_fx/signal_fx.c
ENGINE_ASM  = src/engine_asm/vwap_fx.S src/engine_asm/ohlc_fx.S src/engine_asm/sma_fx.S src/engine_asm/signal_fx.S

# ======================= the ladder =======================
# Each version = the one before it + ONE change (marked <--).

# V0  Stage A : helper library (sscanf), double prices, naive SMA
V0 = src/main_stageA.c         $(HELPER)      $(ENGINE) $(SMA_NAIVE)
# V1  Stage A+: our own read_trade() with pointers            <-- reader
V1 = src/main_stageAplus.c     $(READ_SSCANF) $(ENGINE) $(SMA_NAIVE)
# V2  Stage A+: fast parser instead of sscanf                 <-- parser
V2 = src/main_stageAplus.c     $(READ_FAST)   $(ENGINE) $(SMA_NAIVE)
# V3  Stage B : load into an array of structs, then process   <-- in memory
V3 = src/main_stageB_inmem.c   $(READ_FAST) $(STORE) $(ENGINE) $(SMA_NAIVE)
# V4  Stage B : running-sum SMA instead of naive              <-- SMA
V4 = src/main_stageB_inmem.c   $(READ_FAST) $(STORE) $(ENGINE) $(SMA_RUNNING)
# V5  Stage B : fixed-point (long long) instead of double     <-- numbers
V5 = src/main_stageB_fixed.c   $(READ_FX) $(STORE_ARRAY)  $(ENGINE_FX)  src/engine/report.c
# V6  Stage C : malloc array that fits the file exactly       <-- memory
V6 = src/main_stageB_fixed.c   $(READ_FX) $(STORE_MALLOC) $(ENGINE_FX)  src/engine/report.c
# V7  Stretch : engine in hand-written AArch64 assembly       <-- engine
V7 = src/main_stageB_fixed.c   $(READ_FX) $(STORE_MALLOC) $(ENGINE_ASM) src/engine/report.c

# every header: if any of them changes, rebuild
HEADERS = $(wildcard src/*.h src/*/*.h helper_function/*.h)

# BUILD: one rule for one executable
#   $(1) = name in bin/    $(2) = list of source files    $(3) = compiler flags
define BUILD
LADDER += bin/$(1)$(EXE)
bin/$(1)$(EXE): $(2) $(HEADERS)
	$(CC) $(3) $(WARN) $(2) -o $$@
endef

# ---- the ladder at -O2 ----
$(eval $(call BUILD,v0_stageA_naive_sma,$(V0),-O2))
$(eval $(call BUILD,v1_stageAplus_own_reader,$(V1),-O2))
$(eval $(call BUILD,v2_stageAplus_fast_parser,$(V2),-O2))
$(eval $(call BUILD,v3_stageB_in_memory,$(V3),-O2))
$(eval $(call BUILD,v4_stageB_running_sma,$(V4),-O2))
$(eval $(call BUILD,v5_stageB_fixed_point,$(V5),-O2))
$(eval $(call BUILD,v6_stageC_malloc,$(V6),-O2))

# ---- compiler flag rungs: the slowest and the fastest C version at -O0 and -O3 ----
$(eval $(call BUILD,v0_stageA_naive_sma_O0,$(V0),-O0))
$(eval $(call BUILD,v0_stageA_naive_sma_O3,$(V0),-O3))
$(eval $(call BUILD,v6_stageC_malloc_O0,$(V6),-O0))
$(eval $(call BUILD,v6_stageC_malloc_O3,$(V6),-O3))

# ---- only on the Pi (64-bit ARM Linux): -mcpu=cortex-a76 and the assembly engine ----
ifdef ON_ARM64_LINUX
$(eval $(call BUILD,v6_stageC_malloc_O3_a76,$(V6),-O3 -mcpu=cortex-a76))
$(eval $(call BUILD,v7_asm_engine,$(V7),-O2))
$(eval $(call BUILD,v7_asm_engine_O3,$(V7),-O3))
endif

# ======================= stage0 =======================
# question a: default gcc build (stage0/question_a/output.txt)
STAGE0_A = stage0/question_a/a_loop_sum$(EXE)

# question b: loop_sum and recursion_sum at 4 optimisation levels (stage0/question_b/output.txt)
OPT_LEVELS = O0 O1 O2 O3
STAGE0_B = $(foreach p,loop_sum recursion_sum,$(foreach o,$(OPT_LEVELS),stage0/question_b/$(p)_$(o)$(EXE)))

# helper headers used by the stage0 programs
HELPERS = helper_function/timing.h helper_function/led.h

# ======================= targets =======================
.PHONY: all stage0 ladder test report plot clean

all: stage0 ladder

stage0: $(STAGE0_A) $(STAGE0_B)

ladder: $(LADDER)
ifndef ON_ARM64_LINUX
	@echo "note: v7_asm_engine not built - the assembly is for 64-bit ARM Linux (the Pi); this compiler targets $(MACHINE)"
endif

test: ladder
	bash run_report.sh test

report: ladder
	bash run_report.sh

plot: bin/v0_stageA_naive_sma$(EXE)
	bin/v0_stageA_naive_sma$(EXE) plot

# question a: gcc stage0/question_a/a_loop_sum.c -o a_loop_sum
$(STAGE0_A): stage0/question_a/a_loop_sum.c $(HELPERS)
	$(CC) stage0/question_a/a_loop_sum.c -o $@

# question b: gcc -O0 loop_sum.c -o loop_sum_O0   (same pattern for O1, O2, O3 and recursion_sum)
define QB_RULE
stage0/question_b/%_$(1)$(EXE): stage0/question_b/%.c $(HELPERS)
	$(CC) -$(1) $$< -o $$@
endef
$(foreach lvl,$(OPT_LEVELS),$(eval $(call QB_RULE,$(lvl))))

clean:
ifeq ($(OS),Windows_NT)
	-$(RM) $(subst /,\,$(STAGE0_A) $(STAGE0_B) $(LADDER)) 2>nul
else
	-$(RM) $(STAGE0_A) $(STAGE0_B) $(LADDER) bin/v6_stageC_malloc_O3_a76 bin/v7_asm_engine bin/v7_asm_engine_O3
endif
