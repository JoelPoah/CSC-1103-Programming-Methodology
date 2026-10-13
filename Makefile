CC     = gcc
CFLAGS = -Wall -Wextra -O0 -I./helper_function

# Shared helper sources
HELPERS = helper_function/trade_io.c

# Stage 0 programs (each one is a standalone file)
STAGE0_BINS = stage0/question_a/a_loop_sum \
              stage0/question_b/loop_sum \
              stage0/question_b/recursion_sum \
              helper_function/test_helpers

# Stage A program
STAGEA_BIN = stageA/stageA_stream

# Targets
all: stage0 stageA opt-levels

# ---- Stage 0 ----
stage0: $(STAGE0_BINS)

stage0/question_a/a_loop_sum: stage0/question_a/a_loop_sum.c
	$(CC) $(CFLAGS) $< -o $@

stage0/question_b/loop_sum: stage0/question_b/loop_sum.c
	$(CC) $(CFLAGS) $< -o $@

stage0/question_b/recursion_sum: stage0/question_b/recursion_sum.c
	$(CC) $(CFLAGS) $< -o $@

helper_function/test_helpers: helper_function/test_helpers.c $(HELPERS)
	$(CC) $(CFLAGS) $^ -o $@

# ---- Optimization levels for loop_sum and recursion_sum ----
# Builds loop_sum_O0..O3 and recursion_sum_O0..O3
OPT_LEVELS = 0 1 2 3
LOOP_SUM_OPT = $(foreach L,$(OPT_LEVELS),stage0/question_b/loop_sum_O$(L))
RECURSION_SUM_OPT = $(foreach L,$(OPT_LEVELS),stage0/question_b/recursion_sum_O$(L))
OPT_BINS = $(LOOP_SUM_OPT) $(RECURSION_SUM_OPT)

opt-levels: $(OPT_BINS)

# The -O$* comes after $(CFLAGS), so it overrides the -O0 in CFLAGS
stage0/question_b/loop_sum_O%: stage0/question_b/loop_sum.c
	$(CC) $(CFLAGS) -O$* $< -o $@

stage0/question_b/recursion_sum_O%: stage0/question_b/recursion_sum.c
	$(CC) $(CFLAGS) -O$* $< -o $@

# ---- Stage A ----
stageA: $(STAGEA_BIN)

$(STAGEA_BIN): stageA/stageA_stream.c $(HELPERS)
	$(CC) $(CFLAGS) $^ -o $@ -lm

# ---- Run ----
run-stage0: stage0
	./stage0/question_a/a_loop_sum
	./stage0/question_b/loop_sum
	./stage0/question_b/recursion_sum
	./helper_function/test_helpers

run-stageA: stageA
	./$(STAGEA_BIN)

clean:
	rm -f $(STAGE0_BINS) $(STAGEA_BIN) $(OPT_BINS)
	rm -f stageA/signals.dat stageA/signals.png

.PHONY: all stage0 stageA run-stage0 run-stageA opt-levels clean
