# ImageEdit local runtime — design and feasibility

Status: experiment. Nothing here has run on a real GPU yet. The numbers marked
"measured" come from a software Vulkan driver in a cloud container and say
nothing about speed on real hardware.

## Target

Run a ~20B-parameter image-editing model (Qwen-Image-Edit-2511: 20B DiT, 60
blocks, hidden size 3072, plus a 7B Qwen2.5-VL encoder) on a PC with a 2 GB GPU
and 16–32 GB RAM, inside 1.8 GB peak VRAM, in 75–115 s per image.

## The budget, checked

| Quantity | Value | Basis |
|---|---|---|
| DiT weights at ~2.3 bpw | ~5.8 GB in RAM | arithmetic |
| One block at ~2.3 bpw | ~95 MB | 5.8 GB / 60 |
| Two-slot ring | ~190–330 MB (2–4 bpw) | fits the 450 MB allowance |
| Activations, 3200 tokens | ~40 MB per tensor | 3200 x 3072 x 4 B |
| Bus traffic per image | ~23 GB (4 steps) | 5.8 GB x 4 |
| Bus time at 3 GB/s (PCIe 3.0 x4) | ~8 s | arithmetic |
| Linear-layer work, 1 reference at 640² | ~510 TFLOP | 2 x 20e9 x 3200 tokens x 4 steps |
| Linear-layer work, 4 references at 640² | ~1280 TFLOP | 8000 tokens |

**VRAM is feasible. The time target is not, on 2 GB-class cards.** Finishing
510 TFLOP in 85 s needs ~6 TFLOPS sustained; four references need ~15. Cards
that ship with 2 GB (GT 1030, GTX 1050, GTX 960) peak at roughly 1–2.3 TFLOPS
FP32 (from memory, unverified) and sustain less. Expect 5–20 minutes per image,
not 55–85 s.

The plan assumed PCIe was the bottleneck. With quantized transfer it is ~8 s of
a multi-minute run. Arithmetic is the bottleneck, so the async ring (item 3)
is still needed for the VRAM limit but buys little time.

`runtime/streambench` exists to replace these estimates with measurements on
the actual device.

## What actually cuts time

In order of leverage:

1. **Fewer tokens.** Cost is linear in tokens. Target 512² (1024 tokens) and
   downscale each reference to 384² (576 tokens): subject + 3 references is
   ~3300 tokens instead of 8000.
2. **Smaller model.** A 4B model is 5x less work at the same settings. This is
   the realistic low-end tier; 20B is the mid/high tier.
3. **Fewer steps.** 4 is already the distilled floor for this model family.
4. **Step caching** (reuse block outputs between adjacent steps). Unproven at 4
   steps; quality risk.

## Five planned optimizations — status

| # | Item | Verdict |
|---|---|---|
| 1 | Two-phase execution, encoder never co-resident with DiT | Sound. Required for the VRAM limit. |
| 1b | Cache by image hash | Refine: the encoder is causal and images precede the prompt, so cache the **key/value prefix** for the ordered image set. Seed change = 0 s; prompt change = re-encode text tokens only. Per-image embeddings alone are not reusable, because each image's states depend on the images before it. |
| 2 | Quantized transfer, dequantize on GPU | Sound. Prototyped and bit-checked against a CPU reference (4-bit and 2-bit codebooks). Real GGUF K-quant layouts are not implemented yet. |
| 3 | Double-buffered async ring | Sound for memory; prototyped. Small time benefit because transfer is a small share. |
| 4 | 4-step distilled model at 512–640² | Required. The distillation LoRA must be merged before quantizing. Quality at 2-bit is the open risk: published low-bit quants of image DiTs degrade visibly below ~4 bpw. |
| 5 | Telemetry and assertions | Prototyped in streambench (JSON output, exit code 2 on a missed budget). The `--params-backend disk` fallback is not built. |

Phase 1 estimate (12–18 s cold) is also optimistic: a 7B encoder on a low-end
CPU processing ~1500–2000 image and text tokens is more likely 1–3 minutes
cold. The prefix cache matters more than the plan assumed.

## Build order

1. streambench on real hardware (low-end and mid-tier). Decides everything below.
2. Decide: write a runtime from scratch, or extend stable-diffusion.cpp/ggml,
   which already loads Qwen-Image-Edit GGUF files and has a Vulkan backend.
   Extending is far less work; the ring and on-GPU dequant are the parts to add.
3. Real GGUF loader + K-quant dequant shaders, one full DiT block, compared
   against a reference implementation.
4. Encoder phase with prefix cache.
5. UI: subject slot, tagged references (scene / outfit / pose), prompt, telemetry bar.

## Swapping models

The runtime is local and loads weight files from disk. It has no server-side
filter, and the base model has no built-in refusal layer. "Swapping models" is
a settings change:

- **DiT**: any GGUF/safetensors file with the same architecture (community
  fine-tunes and merges of Qwen-Image-Edit load the same way as the original).
- **LoRAs**: merged into the DiT before quantization, or applied at load time.
- **Text encoder**: any Qwen2.5-VL-7B build in the matching format.
- **VAE**: the matching Qwen-Image VAE or a compatible replacement.

Setup is: download the files, put them in the models folder, pick them in a
model dropdown. The app should show each file's licence and hash, and should
not bundle or auto-download third-party fine-tunes.

A hosted version (API key) is different: the provider's content policy applies
and cannot be switched off; violating it gets the key banned.

## Legal risk (summary from memory, not legal advice — verify before launch)

This app's core feature is putting a real person's face into new images. That
is where the exposure is, regardless of which model is loaded.

- **Sexual images of real people without consent.** US: the TAKE IT DOWN Act
  (2025) makes publishing them a federal crime, including AI-generated ones;
  most states have their own criminal or civil laws. UK: creating one is itself
  an offence, not only sharing. EU countries are adding similar offences.
- **Minors.** Sexual images of minors are illegal to create or possess in
  essentially every jurisdiction, including fully synthetic ones. No exception.
- **Likeness and defamation.** Right-of-publicity and defamation claims apply
  to non-sexual fakes of real people too, especially commercial use.
- **Disclosure.** The EU AI Act requires deepfakes to be labelled as
  AI-generated (obligations phase in during 2026).
- **Biometric data.** If face data is processed on a server, GDPR and laws like
  Illinois BIPA apply. Fully local processing avoids most of this.
- **Distributor exposure.** A local tool puts primary responsibility on the
  person using it, but a tool marketed for undressing or sexual face swaps has
  drawn lawsuits and platform bans. How the app is described matters.
- **Licences.** Qwen-Image-Edit is Apache 2.0. Many fine-tunes, LoRAs and
  face-swap models are non-commercial or unclear. Check each file.

Recommended product stance: model-agnostic loader, local-only processing, a
consent confirmation for photos of other people, no marketing around explicit
real-person content, and visible AI-generated labelling on export.
