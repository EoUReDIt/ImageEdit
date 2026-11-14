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

- Nothing has run on a real GPU. The only real speed numbers are CPU-only.
- No real model is loaded: no GGUF parser, no K-quant layouts, no attention,
  no VAE, no encoder phase, no UI.
- The Windows build has not been run on a real machine.

## Key findings so far

- Sprint 2 (memory): reading weights in place from the model file
  (`--backend cpu --params-backend ...=cpu --mmap`) cut peak held memory from
  9.0 GB to 1.9 GB at 512x512 / 4 steps, 628 s -> 532 s, identical output, and
  it still ran at normal speed with 9.9 GB of RAM occupied by another process.
  `--params-backend disk` does NOT do this. See `docs/sprints/sprint-2.md`.
- Owner's guiding rule: fill graphics memory and system memory only to a
  balanced point, leave room for the OS, disk as overflow. Thesis and stops A–G
  are in `docs/roadmap.md`. Reports must be written for a non-technical reader.

- The full pipeline works end to end on upstream stable-diffusion.cpp with the
  2-bit model and the 4-step LoRA. Measured in the cloud container on 4 CPU
  cores, no GPU: 512x512, 4 steps, one reference = 628 s total (encoder 30 s,
  denoising 449 s, VAE decode 92 s), 9.0 GB peak memory, correct edit.
  Details in `docs/predictions.md`; images in `docs/results/`.
- The earlier arithmetic baseline was too pessimistic (it counted all 20B
  parameters per token; each token uses about half).
- The owner's test PC is an i5-1135G7 laptop, 16 GB RAM, Intel Iris Xe
  integrated graphics. No PCIe bus, no dedicated VRAM: the binding limit is
  16 GB RAM, and theory points 2–3 (bus compression, async ring) cannot be
  tested on it. They still need a discrete-GPU machine.
- Model files are verified and listed in `docs/predictions.md` (results log).
  They live in `/home/user/models` in the cloud container (lost when the
  container is reclaimed; re-download from Hugging Face).

## Next steps

1. Owner (Sprint 1): on the laptop, run
   `run-theory.ps1 -Device cpu -Lora Qwen-Image-Edit-2511-Lightning-4steps-V1.0-bf16`
   then the same without `-Device cpu` for Iris Xe graphics. Share
   `result.json` and `log.txt`. Prediction: completes, ~2 GB held, 15–25 min.
2. Sprint 3 (Claude): stop E first, because it is cheap: merge the LoRA into
   the GGUF before quantizing; larger VAE tiles or a lighter decoder. Then
   stop C: a chosen fill level with read-ahead, as a patch to upstream.
3. Stop D needs a machine with a graphics card. Nobody has one in the loop yet.
4. The cloud session runs in the "ImageEdit" environment (Hugging Face
   allowed). No GPU, no PowerShell. Models are in `/home/user/models` and are
   lost when the container is reclaimed; upstream is cloned and built at
   `/home/user/upstream/sdcpp`. Measurement helper scripts (`memrun.py`,
   `exp.sh`) were in the scratchpad and are saved under `tools/measure/`.

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
