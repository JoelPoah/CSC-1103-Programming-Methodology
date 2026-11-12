# Makefile - builds stage0 and stageA (run from the project root)
#
#   make            build everything (stage0 + stageA)
#   make stage0     build stage0 only (question a + question b)
#   make stageA     build stageA only (stageA_manual = Version 0, stageA_running_sma, stageA_stream)
#   make stageC     build stageC only (stageC_malloc, stageC_replay, stageC_rival)
#   make asm        write the -O2 assembly of the fixed-point kernels to stageC/asm/
#   make clean      delete all built executables
#
# Works with GNU make on Windows (MinGW gcc adds .exe automatically), Linux and macOS.

CC = gcc

# Windows adds .exe to every executable; Linux / macOS do not
ifeq ($(OS),Windows_NT)
EXE = .exe
RM  = del /Q
else
EXE =
RM  = rm -f
endif

# ---- stage0 ----
# question a: default gcc build (stage0/question_a/output.txt)
STAGE0_A = stage0/question_a/a_loop_sum$(EXE)

# question b: loop_sum and recursion_sum at 4 optimisation levels (stage0/question_b/output.txt)
OPT_LEVELS = O0 O1 O2 O3
STAGE0_B = $(foreach p,loop_sum recursion_sum,$(foreach o,$(OPT_LEVELS),stage0/question_b/$(p)_$(o)$(EXE)))

# ---- stageA ----
# stageA_manual      = Version 0 (engine modules + naive sma.c)
# stageA_running_sma = same program with running_sma.c (ladder rung)
# stageA_stream      = original single-file version (reference)
STAGEA_V0      = stageA/stageA_manual$(EXE)
STAGEA_RUNNING = stageA/stageA_running_sma$(EXE)
STAGEA_STREAM  = stageA/stageA_stream$(EXE)
STAGEA = $(STAGEA_V0) $(STAGEA_RUNNING) $(STAGEA_STREAM)

# engine modules shared by stageA_manual and stageA_running_sma (SMA file added per target)
ENGINE_A   = stageA/engine/vwap.c stageA/engine/ohlc.c stageA/engine/signal.c stageA/engine/report.c
ENGINE_A_H = stageA/engine/vwap.h stageA/engine/ohlc.h stageA/engine/signal.h stageA/engine/report.h
TRADE_IO   = helper_function/trade_io.c helper_function/trade_io.h

# helper headers used by the stage0 programs
HELPERS = helper_function/timing.h helper_function/led.h

# ---- stageC ----
# stageC_malloc = Stage B in-memory engine with a malloc'd array (Leroi)
# stageC_replay = real-time replay, LED / coloured signals, latency, lag, live P&L
# stageC_rival  = rival bot P&L for v0 / mem and the R sweep (rival_sweep.dat)
STAGEC_MALLOC = stageC/stageC_malloc$(EXE)
STAGEC_REPLAY = stageC/stageC_replay$(EXE)
STAGEC_RIVAL  = stageC/stageC_rival$(EXE)
STAGEC = $(STAGEC_MALLOC) $(STAGEC_REPLAY) $(STAGEC_RIVAL)

# The replay and rival bot only call code from earlier stages:
#   Version 0 = helper library (next_trade) + Stage A modules (naive SMA, double)
#   mem       = Leroi's malloc loader + Leroi's Stage B fixed-point modules
ENGINE_A_V0 = stageA/engine/vwap.c stageA/engine/ohlc.c stageA/engine/sma.c stageA/engine/signal.c
ENGINE_B_FX = stageB/engine/vwap_fx.c stageB/engine/ohlc_fx.c stageB/engine/sma_fx.c stageB/engine/signal_fx.c
ENGINE_C    = stageC/engine/trades_malloc.c stageC/engine/v0_engine.c stageC/engine/mem_engine.c stageC/engine/rival.c helper_function/trade_io.c
ENGINE_C_H  = $(wildcard stageC/engine/*.h) $(wildcard stageB/engine/*.h) $(wildcard stageA/engine/*.h) $(HELPERS) helper_function/trade_io.h

# extra flags for Stage C, e.g. make stageC CFLAGS_C="-O2 -mcpu=cortex-a76" on the Pi
CFLAGS_C = -O2 -Wall

# assembly listings are named after the machine: x86-64 on the laptop, aarch64 on the Pi
ifeq ($(OS),Windows_NT)
ASM_TAG = x86-64
else
ASM_TAG = $(shell uname -m)
endif
ASM_SRC = stageC/engine/mem_engine.c stageB/engine/sma_fx.c stageB/engine/signal_fx.c stageB/engine/vwap_fx.c stageB/engine/ohlc_fx.c

ALL_EXES = $(STAGE0_A) $(STAGE0_B) $(STAGEA) $(STAGEC)

.PHONY: all stage0 stageA stageC asm clean

all: stage0 stageA stageC

stage0: $(STAGE0_A) $(STAGE0_B)

stageA: $(STAGEA)

# question a: gcc stage0/question_a/a_loop_sum.c -o a_loop_sum
$(STAGE0_A): stage0/question_a/a_loop_sum.c $(HELPERS)
	$(CC) stage0/question_a/a_loop_sum.c -o $@

# question b: gcc -O0 loop_sum.c -o loop_sum_O0   (same pattern for O1, O2, O3 and recursion_sum)
define QB_RULE
stage0/question_b/%_$(1)$(EXE): stage0/question_b/%.c $(HELPERS)
	$(CC) -$(1) $$< -o $$@
endef
$(foreach lvl,$(OPT_LEVELS),$(eval $(call QB_RULE,$(lvl))))

# stageA Version 0: stageA_manual.c + engine modules + naive sma.c
$(STAGEA_V0): stageA/stageA_manual.c stageA/engine/sma.c stageA/engine/sma.h $(ENGINE_A) $(ENGINE_A_H) $(TRADE_IO)
	$(CC) -O2 -Wall stageA/stageA_manual.c $(ENGINE_A) stageA/engine/sma.c helper_function/trade_io.c -o $@

# stageA running-sum rung: same modules but running_sma.c instead of sma.c
$(STAGEA_RUNNING): stageA/stageA_running_sma.c stageA/engine/running_sma.c stageA/engine/running_sma.h $(ENGINE_A) $(ENGINE_A_H) $(TRADE_IO)
	$(CC) -O2 -Wall stageA/stageA_running_sma.c $(ENGINE_A) stageA/engine/running_sma.c helper_function/trade_io.c -o $@

# stageA reference: gcc -O2 -Wall stageA/stageA_stream.c helper_function/trade_io.c -o stageA/stageA_stream
$(STAGEA_STREAM): stageA/stageA_stream.c $(TRADE_IO)
	$(CC) -O2 -Wall stageA/stageA_stream.c helper_function/trade_io.c -o $@

stageC: $(STAGEC)

# stageC malloc (Leroi): same build line as in stageC/stageC_malloc.c
$(STAGEC_MALLOC): stageC/stageC_malloc.c stageC/engine/trades_malloc.c stageC/engine/trades_malloc.h $(TRADE_IO)
	$(CC) $(CFLAGS_C) stageC/stageC_malloc.c stageC/engine/trades_malloc.c stageB/engine/vwap.c stageB/engine/ohlc.c stageB/engine/sma.c stageB/engine/signal.c stageB/engine/report.c helper_function/trade_io.c -o $@

$(STAGEC_REPLAY): stageC/stageC_replay.c $(ENGINE_C) $(ENGINE_A_V0) $(ENGINE_B_FX) $(ENGINE_C_H)
	$(CC) $(CFLAGS_C) stageC/stageC_replay.c $(ENGINE_C) $(ENGINE_A_V0) $(ENGINE_B_FX) -o $@

$(STAGEC_RIVAL): stageC/stageC_rival.c $(ENGINE_C) $(ENGINE_A_V0) $(ENGINE_B_FX) $(ENGINE_C_H)
	$(CC) $(CFLAGS_C) stageC/stageC_rival.c $(ENGINE_C) $(ENGINE_A_V0) $(ENGINE_B_FX) -lm -o $@

# gcc -S -O2 for each fixed-point kernel -> stageC/asm/<name>_<machine>_O2.s
asm:
	$(foreach f,$(ASM_SRC),$(CC) -S -O2 -fno-asynchronous-unwind-tables -fno-ident $(f) -o stageC/asm/$(basename $(notdir $(f)))_$(ASM_TAG)_O2.s &&) echo assembly written to stageC/asm/

clean:
ifeq ($(OS),Windows_NT)
	-$(RM) $(subst /,\,$(ALL_EXES)) 2>nul
else
	-$(RM) $(ALL_EXES)
endif
