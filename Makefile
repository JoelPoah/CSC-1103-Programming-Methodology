# Makefile - builds stage0 and stageA (run from the project root)
#
#   make            build everything (stage0 + stageA)
#   make stage0     build stage0 only (question a + question b)
#   make stageA     build stageA only
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
STAGEA = stageA/stageA_stream$(EXE)

# helper headers used by the stage0 programs
HELPERS = helper_function/timing.h helper_function/led.h

ALL_EXES = $(STAGE0_A) $(STAGE0_B) $(STAGEA)

.PHONY: all stage0 stageA clean

all: stage0 stageA

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

# stageA: gcc -O2 -Wall stageA/stageA_stream.c helper_function/trade_io.c -o stageA/stageA_stream
$(STAGEA): stageA/stageA_stream.c helper_function/trade_io.c helper_function/trade_io.h $(HELPERS)
	$(CC) -O2 -Wall stageA/stageA_stream.c helper_function/trade_io.c -o $@

clean:
ifeq ($(OS),Windows_NT)
	-$(RM) $(subst /,\,$(ALL_EXES)) 2>nul
else
	-$(RM) $(ALL_EXES)
endif
