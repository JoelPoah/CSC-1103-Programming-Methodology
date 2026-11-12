# Makefile - builds stage0, stageA, stageC and the src/ ladder (run from the project root)
#
#   make            build everything (stage0 + stageA + stageC + ladder)
#   make stage0     build stage0 only (question a + question b)
#   make stageA     build stageA only (stageA_manual = Version 0, stageA_plot)
#   make plotA      build stageA_plot and run it (writes stageA/signals.dat + signals.png)
#   make stageC     build stageC only (stageC_malloc, stageC_replay, stageC_rival)
#   make asm        write the -O2 assembly of the fixed-point kernels to stageC/asm/
#   make ladder     build every version V0-V7 from src/ into bin/ (+ V8 on the Pi)
#   make benchmark  build the -O0..-O3 benchmark tool (benchmark/benchmark.c)
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
# stageA_manual      = Version 0 (engine modules + naive sma.c), timed, no plot files
# stageA_plot        = writes signals.dat (dev_1M slice, Version 0 SMA) and runs gnuplot
# Run the timed programs with one dataset: test20 | dev | stress
STAGEA_V0      = stageA/stageA_manual$(EXE)
STAGEA_PLOT    = stageA/stageA_plot$(EXE)
STAGEA = $(STAGEA_V0) $(STAGEA_PLOT)

# engine modules used by stageA_manual (naive sma.c added in its rule)
ENGINE_A   = stageA/engine/vwap.c stageA/engine/ohlc.c stageA/engine/signal.c stageA/engine/report.c
ENGINE_A_H = stageA/engine/vwap.h stageA/engine/ohlc.h stageA/engine/signal.h stageA/engine/report.h
TRADE_IO   = helper_function/trade_io.c helper_function/trade_io.h

# ---- benchmark ----
BENCHMARK = benchmark/benchmark$(EXE)

# helper headers used by the stage0 programs
HELPERS = helper_function/timing.h helper_function/led.h

# ---- stageC ----
# stageC_malloc = Stage B in-memory engine with a malloc'd array (Leroi)
# stageC_replay = real-time replay, LED / coloured signals, latency, lag, live P&L
# stageC_rival  = rival bot P&L for v0 / fast / mem and the R sweep (rival_sweep.dat)
STAGEC_MALLOC = stageC/stageC_malloc$(EXE)
STAGEC_REPLAY = stageC/stageC_replay$(EXE)
STAGEC_RIVAL  = stageC/stageC_rival$(EXE)
STAGEC_TEST   = stageC/test_parser$(EXE)
STAGEC = $(STAGEC_MALLOC) $(STAGEC_REPLAY) $(STAGEC_RIVAL) $(STAGEC_TEST)

# Stage B engine modules: double (Version 0 maths) and fixed-point (fastest)
ENGINE_B_DBL = stageB/engine/vwap.c stageB/engine/ohlc.c stageB/engine/sma.c stageB/engine/sma_naive.c stageB/engine/signal.c
ENGINE_B_FX  = stageB/engine/vwap_fx.c stageB/engine/ohlc_fx.c stageB/engine/sma_fx.c stageB/engine/signal_fx.c
ENGINE_C     = stageC/engine/feed.c stageC/engine/fast_parse.c stageC/engine/engine.c stageC/engine/rival.c stageC/engine/latency.c stageC/engine/display.c
ENGINE_C_H   = $(wildcard stageC/engine/*.h) $(wildcard stageB/engine/*.h) $(HELPERS)

# extra flags for Stage C, e.g. make stageC CFLAGS_C="-O2 -mcpu=cortex-a76" on the Pi
CFLAGS_C = -O2 -Wall

# assembly listings are named after the machine: x86-64 on the laptop, aarch64 on the Pi
ifeq ($(OS),Windows_NT)
ASM_TAG = x86-64
else
ASM_TAG = $(shell uname -m)
endif
ASM_SRC = stageC/engine/engine.c stageB/engine/sma_fx.c stageB/engine/signal_fx.c stageB/engine/vwap_fx.c stageB/engine/ohlc_fx.c

# ---- ladder (src/) ----
# Every version is built from src/ with the same flags; only the files linked change.
# V0  main_stageA        helper library + naive SMA
# V1  main_stageAplus    own read_trade(), fgets + sscanf
# V2  main_stageAplus    own read_trade(), digit-by-digit parser
# V3  main_stageB_inmem  all trades in a global array, naive SMA
# V4  main_stageB_inmem  same, running-sum SMA
# V5  main_stageB_fixed  fixed-point prices, fixed-size global array
# V6  main_stageB_fixed  fixed-point prices, malloc'd array (Stage C)
# V7  main_stageB_fixed  same array, whole file read with one fread (Lucas's feed)
# V8  main_stageB_fixed  V7 + hand-written AArch64 OHLC (Yong Sheng), Raspberry Pi only
LADDER_V0 = bin/v0_stageA_naive_sma$(EXE)
LADDER_V1 = bin/v1_stageAplus_own_reader$(EXE)
LADDER_V2 = bin/v2_stageAplus_fast_parser$(EXE)
LADDER_V3 = bin/v3_stageB_in_memory$(EXE)
LADDER_V4 = bin/v4_stageB_running_sma$(EXE)
LADDER_V5 = bin/v5_stageB_fixed_point$(EXE)
LADDER_V6 = bin/v6_stageC_malloc$(EXE)
LADDER_V7 = bin/v7_stageC_fread$(EXE)
LADDER_V8 = bin/v8_stageC_asm_ohlc$(EXE)
LADDER = $(LADDER_V0) $(LADDER_V1) $(LADDER_V2) $(LADDER_V3) $(LADDER_V4) $(LADDER_V5) $(LADDER_V6) $(LADDER_V7)

CFLAGS_L   = -O2 -Wall
SRC_H      = $(wildcard src/*.h src/*/*.h) $(HELPERS)
SRC_ENGINE = src/engine/vwap.c src/engine/ohlc.c src/engine/signal.c src/engine/report.c
SRC_FX_CORE = src/engine_fx/vwap_fx.c src/engine_fx/sma_fx.c src/engine_fx/signal_fx.c src/engine/report.c
SRC_FX     = $(SRC_FX_CORE) src/engine_fx/v5-v7_ohlc_fx.c

# V8 is AArch64 assembly, so it is only part of the ladder on the Pi (uname -m = aarch64)
ifeq ($(ASM_TAG),aarch64)
LADDER += $(LADDER_V8)
endif

ALL_EXES = $(STAGE0_A) $(STAGE0_B) $(STAGEA) $(STAGEC) $(LADDER) $(BENCHMARK)

.PHONY: all stage0 stageA plotA stageC test_parser asm ladder benchmark clean

all: stage0 stageA stageC ladder

stage0: $(STAGE0_A) $(STAGE0_B)

stageA: $(STAGEA)

# build the plot program and run it (from the project root)
plotA: $(STAGEA_PLOT)
	./$(STAGEA_PLOT)

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

# stageA plot: only SMA + signal + report (for the .dat file), naive sma.c = Version 0
$(STAGEA_PLOT): stageA/stageA_plot.c stageA/engine/sma.c stageA/engine/sma.h stageA/engine/signal.c stageA/engine/signal.h stageA/engine/report.c stageA/engine/report.h $(TRADE_IO)
	$(CC) -O2 -Wall stageA/stageA_plot.c stageA/engine/sma.c stageA/engine/signal.c stageA/engine/report.c helper_function/trade_io.c -o $@

stageC: $(STAGEC)

# stageC malloc (Leroi): same build line as in stageC/stageC_malloc.c
$(STAGEC_MALLOC): stageC/stageC_malloc.c stageC/engine/trades_malloc.c stageC/engine/trades_malloc.h $(TRADE_IO)
	$(CC) $(CFLAGS_C) stageC/stageC_malloc.c stageC/engine/trades_malloc.c stageB/engine/vwap.c stageB/engine/ohlc.c stageB/engine/sma.c stageB/engine/signal.c stageB/engine/report.c helper_function/trade_io.c -o $@

$(STAGEC_REPLAY): stageC/stageC_replay.c $(ENGINE_C) $(ENGINE_B_DBL) $(ENGINE_B_FX) $(ENGINE_C_H)
	$(CC) $(CFLAGS_C) stageC/stageC_replay.c $(ENGINE_C) $(ENGINE_B_DBL) $(ENGINE_B_FX) -o $@

$(STAGEC_RIVAL): stageC/stageC_rival.c $(ENGINE_C) $(ENGINE_B_DBL) $(ENGINE_B_FX) $(ENGINE_C_H)
	$(CC) $(CFLAGS_C) stageC/stageC_rival.c $(ENGINE_C) $(ENGINE_B_DBL) $(ENGINE_B_FX) -lm -o $@

# parser check: ./stageC/test_parser data/stress_full.csv
test_parser: $(STAGEC_TEST)

$(STAGEC_TEST): stageC/test_parser.c stageC/engine/feed.c stageC/engine/fast_parse.c $(ENGINE_C_H)
	$(CC) $(CFLAGS_C) stageC/test_parser.c stageC/engine/feed.c stageC/engine/fast_parse.c -o $@

# gcc -S -O2 for each fixed-point kernel -> stageC/asm/<name>_<machine>_O2.s
asm:
	$(foreach f,$(ASM_SRC),$(CC) -S -O2 -fno-asynchronous-unwind-tables -fno-ident $(f) -o stageC/asm/$(basename $(notdir $(f)))_$(ASM_TAG)_O2.s &&) echo assembly written to stageC/asm/

ladder: $(LADDER)

$(LADDER_V0): src/v0_main_stageA.c $(SRC_ENGINE) src/engine/v0-v3_sma_naive.c $(SRC_H) $(TRADE_IO)
	$(CC) $(CFLAGS_L) src/v0_main_stageA.c $(SRC_ENGINE) src/engine/v0-v3_sma_naive.c helper_function/trade_io.c -o $@

$(LADDER_V1): src/v1-v2_main_stageAplus.c src/reader/v1_read_trade_sscanf.c $(SRC_ENGINE) src/engine/v0-v3_sma_naive.c $(SRC_H)
	$(CC) $(CFLAGS_L) src/v1-v2_main_stageAplus.c src/reader/v1_read_trade_sscanf.c $(SRC_ENGINE) src/engine/v0-v3_sma_naive.c -o $@

$(LADDER_V2): src/v1-v2_main_stageAplus.c src/reader/v2-v4_read_trade_fast.c $(SRC_ENGINE) src/engine/v0-v3_sma_naive.c $(SRC_H)
	$(CC) $(CFLAGS_L) src/v1-v2_main_stageAplus.c src/reader/v2-v4_read_trade_fast.c $(SRC_ENGINE) src/engine/v0-v3_sma_naive.c -o $@

$(LADDER_V3): src/v3-v4_main_stageB_inmem.c src/store/v3-v4_trades.c src/reader/v2-v4_read_trade_fast.c $(SRC_ENGINE) src/engine/v0-v3_sma_naive.c $(SRC_H)
	$(CC) $(CFLAGS_L) src/v3-v4_main_stageB_inmem.c src/store/v3-v4_trades.c src/reader/v2-v4_read_trade_fast.c $(SRC_ENGINE) src/engine/v0-v3_sma_naive.c -o $@

$(LADDER_V4): src/v3-v4_main_stageB_inmem.c src/store/v3-v4_trades.c src/reader/v2-v4_read_trade_fast.c $(SRC_ENGINE) src/engine/v4_sma_running.c $(SRC_H)
	$(CC) $(CFLAGS_L) src/v3-v4_main_stageB_inmem.c src/store/v3-v4_trades.c src/reader/v2-v4_read_trade_fast.c $(SRC_ENGINE) src/engine/v4_sma_running.c -o $@

$(LADDER_V5): src/v5-v8_main_stageB_fixed.c src/store/v5_trades_fx_array.c src/reader/v5-v8_read_trade_fx.c $(SRC_FX) $(SRC_H)
	$(CC) $(CFLAGS_L) src/v5-v8_main_stageB_fixed.c src/store/v5_trades_fx_array.c src/reader/v5-v8_read_trade_fx.c $(SRC_FX) -o $@

$(LADDER_V6): src/v5-v8_main_stageB_fixed.c src/store/v6_trades_fx_malloc.c src/reader/v5-v8_read_trade_fx.c $(SRC_FX) $(SRC_H)
	$(CC) $(CFLAGS_L) src/v5-v8_main_stageB_fixed.c src/store/v6_trades_fx_malloc.c src/reader/v5-v8_read_trade_fx.c $(SRC_FX) -o $@

$(LADDER_V7): src/v5-v8_main_stageB_fixed.c src/store/v7-v8_trades_fx_fread.c src/reader/v5-v8_read_trade_fx.c $(SRC_FX) $(SRC_H)
	$(CC) $(CFLAGS_L) src/v5-v8_main_stageB_fixed.c src/store/v7-v8_trades_fx_fread.c src/reader/v5-v8_read_trade_fx.c $(SRC_FX) -o $@

$(LADDER_V8): src/v5-v8_main_stageB_fixed.c src/store/v7-v8_trades_fx_fread.c src/reader/v5-v8_read_trade_fx.c src/engine_asm/v8_ohlc_fx.s $(SRC_FX_CORE) $(SRC_H)
	$(CC) $(CFLAGS_L) src/v5-v8_main_stageB_fixed.c src/store/v7-v8_trades_fx_fread.c src/reader/v5-v8_read_trade_fx.c $(SRC_FX_CORE) src/engine_asm/v8_ohlc_fx.s -o $@

benchmark: $(BENCHMARK)

# benchmark: gcc -O2 -Wall benchmark/benchmark.c -o benchmark/benchmark
$(BENCHMARK): benchmark/benchmark.c helper_function/timing.h
	$(CC) -O2 -Wall benchmark/benchmark.c -o $@

clean:
ifeq ($(OS),Windows_NT)
	-$(RM) $(subst /,\,$(ALL_EXES)) 2>nul
else
	-$(RM) $(ALL_EXES)
endif
