# Invoke through build-queue. Build on the development host; desktop timings
# only on the designated physical benchmark host (the runner enforces this).
SHADER_BENCH_DEVICE ?= desktop
SHADER_BENCH_MODE ?= interpreter
SHADER_BENCH_FRAMES ?= 60
SHADER_BENCH_WARMUP ?= 8
SHADER_BENCH_ARGS ?=
.PHONY: shader-bench shader-bench-build shader-bench-test
shader-bench-build: project-toolchain
	@python3 tests/shader_benchmark.py --device $(SHADER_BENCH_DEVICE) --build-only $(SHADER_BENCH_ARGS)
shader-bench:
	@python3 tests/shader_benchmark.py --device $(SHADER_BENCH_DEVICE) --mode $(SHADER_BENCH_MODE) --run-only --frames $(SHADER_BENCH_FRAMES) --warmup $(SHADER_BENCH_WARMUP) $(SHADER_BENCH_ARGS)
shader-bench-test: project-toolchain
	@python3 tests/shader_benchmark_test.py
	@python3 tests/shader_benchmark.py --self-test $(SHADER_BENCH_ARGS)

.PHONY: shader-prepared-test
shader-prepared-test: project-toolchain
	@sh tests/shader_prepared_test.sh

.PHONY: shader-gpu-accuracy-test
shader-gpu-accuracy-test: project-toolchain
	@SHADER_BENCH_DEVICE=$(SHADER_BENCH_DEVICE) sh tests/shader_gpu_accuracy_test.sh
