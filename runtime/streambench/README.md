# streambench

Measures whether a streamed, quantized 20B model can meet the ImageEdit
low-VRAM budget on this machine. It loads no model: it streams random
bit-packed weight blocks from RAM through a two-slot VRAM ring, dequantizes
them inside the compute shader, and projects the denoising time.

## Run (Windows)

Download `streambench.exe` from the repository's Actions tab (workflow
"streambench", artifact `streambench-windows-x64`), then:

```bash
streambench.exe --bits 2 --json streambench-result.json
```

Needs ~1 GB free RAM and a GPU driver with Vulkan. A run takes from under a
minute to roughly ten minutes on slow cards. Exit code 2 means a budget
assertion failed; that is a result, not a crash.

Useful variants:

```bash
streambench.exe --bits 4
streambench.exe --bits 2 --tokens 1600
streambench.exe --bits 2 --params-b 4 --layers 25
```

## Build

```bash
cmake -S runtime/streambench -B build
```

```bash
cmake --build build --config Release
```

No Vulkan SDK needed; headers and loader shim are fetched by CMake.

## Reading the output

- **A. transfer only** — RAM→VRAM throughput. On a discrete card this is the PCIe figure.
- **B. compute only** — effective GFLOPS of the fused dequant+matmul shader.
- **C. async ring** — both overlapped; how much transfer time compute hides.
- **Projected denoising phase** — C x layers x steps. A lower bound: linear
  layers only, with an untuned shader (a tuned kernel may be 2–4x faster).
