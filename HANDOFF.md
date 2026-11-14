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
- `docs/upstream-gap-analysis.md` — each theory point checked against
  stable-diffusion.cpp; most already exist upstream; five gaps listed.
- `tests/lowend/run-theory.ps1` — runs the theory configuration on upstream and
  records time and peak VRAM. Untested.
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

1. Owner: run `tests/lowend/run-theory.ps1` on the low-end PC with the upstream
   Windows Vulkan build and a real model (see `tests/lowend/README.md`). Share
   `result.json` and `log.txt`. Also still useful: `streambench.exe --bits 2`.
2. Log results in `docs/predictions.md`.
3. Build the gaps from `docs/upstream-gap-analysis.md` as patches against
   upstream commit `0f8a467`, in this order: telemetry, persistent conditioning
   cache, physical VRAM cap, prefix reuse, UI on `sd-server`.
4. The cloud session cannot reach Hugging Face, so it cannot run a real model.
   Patches can be compiled there but not exercised until that is allowed.

## Decisions

- Extend stable-diffusion.cpp (owner's choice), not a from-scratch runtime.
- Upstream changes are kept as patches in this repo against a pinned commit.
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
