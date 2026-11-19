# Laptop Stage C runs: notes

- Machine and toolchain: see `machine_info.txt`. Runs in WSL2 Ubuntu 24.04 on Windows 11,
  pinned with `taskset -c 2`, same commands as `stageC/BENCHMARK_INSTRUCTIONS.md` step 4.
- **Interrupted run removed:** the first `v0` 1x run (started 2026-11-19, about 14:31)
  overlapped the laptop going to sleep, about 1 hour missing between runs. It was deleted
  and re-run after the other 29 runs finished, under the same name `replay_v0_1x_run1.txt`.
  The 1000x and 100x runs before it finished on schedule and were kept.
