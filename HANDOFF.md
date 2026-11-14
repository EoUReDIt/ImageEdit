# Handoff

Updated: 2026-11-14. Update this file at every pause.

Project: ImageEdit — local image-editing app for low-end PCs
Repo: github.com/EoUReDIt/ImageEdit
Branch: claude/compassionate-fermi-1dzo7z (develop here only)

## Goal

One subject photo (identity only), several reference images each tagged
scene / outfit / pose, an optional prompt, one output image. Simple UI. Run a
~20B editing model (Qwen-Image-Edit-2511 class) locally in <= 1.8 GB VRAM via a
custom C++/Vulkan streaming runtime. The owner tests on a low-end Windows PC.

## Method

Discovery-based: build to the theory first, then test, then compare against the
arithmetic baseline in `docs/predictions.md`. The arithmetic validates results;
it does not veto building. Predictions are fixed before each test.

## Done

- `runtime/streambench/` — Vulkan benchmark of the core idea: bit-packed
  weights in RAM, two-slot VRAM ring fed by an async transfer queue, fused
  on-GPU dequantize + matmul, projection and budget assertions, JSON output.
  Verified on software Vulkan (llvmpipe): GPU result matches a CPU reference
  for 2-bit and 4-bit, at hidden size 3072.
- `docs/runtime-design.md` — budget arithmetic, verdict on each of the five
  planned optimizations, build order, model swapping, legal summary.
- `docs/predictions.md` — theory target vs arithmetic baseline vs measured, per claim.
- `.github/workflows/streambench.yml` — builds `streambench.exe` for Windows
  and runs a correctness check on Linux.

## Not done / not verified

- Nothing has run on a real GPU. No speed number is real yet.
- No real model is loaded: no GGUF parser, no K-quant layouts, no attention,
  no VAE, no encoder phase, no UI.
- The Windows build has not been run on a real machine.

## Key finding so far

VRAM fits. Time probably does not: arithmetic, not PCIe, is the bottleneck.
A 20B model at 640² with one reference is ~510 TFLOP per image; 2 GB-class
cards sustain roughly 1 TFLOPS, so expect minutes, not 55–85 s. See the design
doc. This is an estimate until streambench runs on real hardware.

## Next steps

1. Owner: run `streambench.exe --bits 2 --json streambench-result.json` on the
   low-end PC and share the output (see `runtime/streambench/README.md`).
2. Get a mid-tier GPU result too. The cloud session has no GPU.
3. Log both results in `docs/predictions.md`.
4. Build on, per the theory, without waiting for the results: GGUF loader +
   K-quant dequant shaders streaming real tensors through the ring; then one
   real DiT block checked against a reference; then the encoder phase with
   key/value prefix cache; then the UI.
5. Open choice: extend stable-diffusion.cpp/ggml or write from scratch.

## Decisions

- Benchmark before runtime: the time target is unproven, so measure first.
- volk + fetched Vulkan headers, SPIR-V committed: builds without the Vulkan SDK.
- Dispatches are chunked (256 tokens) to stay under the Windows 2 s GPU watchdog.
- Runtime will be model-agnostic (loads user-supplied files); no bundled
  third-party fine-tunes; consent prompt for photos of other people.

## Gotchas

- streambench's projection is a lower bound (linear layers only, untuned shader).
- On integrated GPUs and llvmpipe, "VRAM" is system RAM and the bus figure is not PCIe.
- Peak-TFLOPS figures for specific cards in the design doc are from memory.
- Earlier direction (hosted API key front end on Perchance) is parked, not dropped.
