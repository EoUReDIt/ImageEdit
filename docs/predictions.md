# Prediction ledger

Method: build to the theory, then measure. The arithmetic column is the
conventional expectation. A measurement that beats it is a discovery; one that
matches it confirms the conventional limit. Fill in "Measured" with the device
and date. Never edit a prediction after its test has run.

Reference case: 20B DiT, 60 blocks, 4 steps, 640x640 with one reference
(3200 tokens), 2 GB discrete GPU, PCIe 3.0, 16–32 GB RAM.

| # | Claim | Theory target | Arithmetic baseline | Measured |
|---|---|---|---|---|
| 1 | Peak VRAM for ring + activations | <= 1050 MB (1.8 GB with headroom) | ~240–410 MB | — |
| 2 | Quantized transfer cuts bus traffic | ~5.5 GB per step | 5.0 GB (2 bpw) – 10 GB (4 bpw) per step | — |
| 3 | RAM->VRAM throughput on the low-end card | not stated | ~3 GB/s (PCIe 3.0 x4) | — |
| 4 | Async ring hides transfer behind compute | ~100% hidden | up to 100% with a dedicated transfer queue; less on a shared queue | — |
| 5 | Sustained GPU arithmetic | ~6 TFLOPS needed for target | ~0.5–1.5 TFLOPS on 2 GB-class cards | — |
| 6 | Denoising phase (4 steps) | 55–85 s | ~350–1000 s | — |
| 7 | Encoder phase, cold | 12–18 s | ~60–180 s on a low-end CPU | — |
| 8 | Encoder phase, cached (seed change) | 0 s | ~0 s | — |
| 9 | VAE decode + sharpen | 5–10 s | not yet estimated | — |
| 10 | End to end | 75–115 s | ~7–20 min | — |
| 11 | Image quality at ~2 bpw is usable | yes | visible degradation below ~4 bpw | — |

Rows 1–6 are testable with `runtime/streambench`. All rows are testable with a
real model through `tests/lowend/run-theory.ps1` (upstream stable-diffusion.cpp).

## Results log

(append: date, device, command, output or JSON)

### 2026-11-14 — cloud container, CPU only (4 cores, AVX-512, 15 GB RAM, no GPU)

Upstream stable-diffusion.cpp `0f8a467`, CPU build. Models:
`qwen-image-edit-2511-Q2_K.gguf`, `Qwen2.5-VL-7B-Instruct.Q3_K_S.gguf` +
`mmproj-Q8_0`, `qwen_image_vae.safetensors`. Flags: `--backend cpu
--params-backend te=disk,diffusion=disk,vae=cpu --cfg-scale 1 --vae-tiling
--model-args qwen_image_zero_cond_t=true`, one reference image.

**Run 1: 256x256, 1 step, no LoRA.** No prediction was fixed beforehand.

| Phase | Measured |
|---|---|
| VAE encode of reference | 6.4 s |
| Encoder (VLM, cold) | 24.0 s |
| Denoising, 1 step | 29.2 s |
| VAE decode | 11.0 s |
| Total | 70.8 s |
| Peak process memory | 7.8 GB |

Finding: the arithmetic baseline in this ledger was too pessimistic. It
counted all 20B parameters per token; the model has separate image-stream and
text-stream weights, so each token passes through about half of them.

**Run 2: 512x512, 4 steps, 4-step Lightning LoRA.** Prediction fixed before
the run, scaled from Run 1: denoising ~320 s (4 x ~80 s), encoder ~24 s,
peak memory 8–9 GB.

| Phase | Predicted | Measured |
|---|---|---|
| Encoder (VLM, cold) | ~24 s | 30.3 s |
| Denoising, 4 steps | ~320 s | 449.0 s (122, 110, 110, 107 s per step) |
| VAE decode (9 tiles) | not predicted | 91.7 s |
| Total | not predicted | 628.2 s |
| Peak process memory | 8–9 GB | 8.96 GB |

Prompt: "change the text on the sign to 'hello'". Input and output:
`docs/results/2026-11-14-input.png`, `docs/results/2026-11-14-cpu-512-4step.png`.

Verdicts:
- Denoising was 40% slower than predicted. The LoRA was applied at run time
  (upstream's automatic choice for quantized weights), which adds work per
  step that Run 1 did not have. Merging it into the model file before
  quantizing should remove this.
- Memory prediction held.
- Ledger row 11 (quality at ~2 bpw): the edit is correct and clean in this one
  sample. One image is not a quality study.
- VAE decode with tiling took longer than one denoising step would suggest;
  larger tiles or less overlap are the obvious lever.
- Rows 1–6 (VRAM, bus, async ring) are untested: this was CPU only.
