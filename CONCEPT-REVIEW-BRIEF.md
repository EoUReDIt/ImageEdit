# ImageEdit — Concept Review Brief (for an outside reviewer)

Date: 2026-11-14. Compiled from the project handoff, sprint journals, prediction
ledger, roadmap and prior-work check. Self-contained: you do not need the repo.

## What we want from you

You are the cartographer, not the builder. Please review the **concept and the
map**, not the code:

1. Is the thesis sound? Where is it wrong or naive?
2. Are any theories below already disproven or proven elsewhere that we missed?
3. Which untested stops (C, D, E, F, G) are dead ends, and which are the shortest
   route to the goal?
4. Are our tests designed well enough to support the conclusions we drew?
5. What would you test next, in what order, and what would you predict?

The owner is non-technical. Please answer in plain language first, detail after.

---

## 1. The project

- **Goal:** run a 20-billion-parameter image-editing model (Qwen-Image-Edit-2511)
  on ordinary PCs. Input: one subject photo + tagged reference pictures (scene,
  outfit, pose) + a text prompt. Output: one edited image.
- **Engine:** upstream stable-diffusion.cpp (commit `0f8a467`), **unmodified**.
  Every result so far comes from settings alone, no code patches.
- **Target laptop:** Intel i5-1135G7, 16 GB RAM, Iris Xe built-in graphics, Windows.
- **Test machine so far:** cloud container, 4 CPU cores (AVX-512), 15 GB RAM,
  no GPU, Linux, slow disk (~30 MB/s cold reads).
- **Models used:** `qwen-image-edit-2511-Q2_K.gguf` (≈2-bit), prompt-reader
  `Qwen2.5-VL-7B-Instruct.Q3_K_S.gguf` + `mmproj-Q8_0`,
  `qwen_image_vae.safetensors`, plus the Lightning 4-step LoRA ("add-on").

### Method
Start from the wanted conclusion → form a theory → design a solution. Write down
a prediction **before** every test; never edit it afterward; log the result.

### Owner's claims under test
1. The hardware was never the problem.
2. The problem was access to expertise (fixes exist but are hidden behind
   settings and source code).
3. Claim 1 also holds for **speed**, not only memory.

---

## 2. The thesis (roadmap)

The whole model never needs to *stay* loaded in graphics memory or system
memory. Each memory layer should be filled only to the point where speed and
efficiency balance, leaving room for Windows; the disk is overflow.

| Machine type | Layer 1 (fastest) | Layer 2 | Layer 3 (overflow) |
|---|---|---|---|
| Has a graphics card | Graphics memory | System memory | Fast disk (NVMe) |
| Laptop, built-in graphics | CPU (+ built-in graphics) | System memory | Fast disk (NVMe) |

Hard limit acknowledged: every pass must touch every weight once. Speed =
work done per pass × how fast the slowest layer feeds weights. Image models are
the favourable case (one pass handles thousands of tokens at once).

Scale ambition: model size grows with the machine (e.g. 8B on 16 GB, 64B on 64 GB).

### The stops, A to G

| Stop | Idea | Status |
|---|---|---|
| A. Take turns | Prompt-reader and image-maker never loaded together | **Confirmed** |
| B. Read in place | Work directly from the model file; OS keeps what fits | **Confirmed (CPU path)**: 9.0 GB → 1.9 GB held |
| C. Balanced fill | Program chooses fill level per layer (e.g. 60–70% RAM) and prefetches next block | Open, needs code |
| D. Three layers on GPU | VRAM holds only blocks in use, fed from RAM, fed from disk; built-in GPU reads the mapped file directly | Open, untested (no GPU yet) |
| E. Load less | Fold add-on into model file; lighter decoder; smaller references; smaller prompt-reader | Open |
| F. Go faster | See speed theories below | In progress (Sprint 3) |
| G. Product | Simple screen: subject + tagged references + prompt → result; gauge showing layer fill | Open |

### Speed theories (stop F)
1. Reference picture is recomputed every step though it never changes.
2. Whole picture is recomputed when only a region changes.
3. Every step runs in full even when little changes between steps.
4. Four steps may be more than needed.
5. Work at full size that could be done small, then enlarged.
6. The 4-step add-on is applied on the fly every step.
7. Built-in graphics chip sits idle during a CPU run.
8. Prompt-reading is repeated every run.

---

## 3. Where things stand

| Sprint | Goal | Status |
|---|---|---|
| 1 | Laptop run (does it work at all, how long?) | **Not run yet** (owner travelling) |
| 2 | Bring memory down | **Done**: 9.0 GB → 1.9 GB held |
| 3 | Speed: ~9 min → under 2 min, same cloud machine, 512×512 | **In progress**: best so far 151 s |

**Best known CPU settings:**
```
--backend cpu --params-backend te=cpu,diffusion=cpu,vae=cpu --mmap --steps 4
--cfg-scale 1 --sampling-method euler --flow-shift 3 --vae-tiling
--model-args qwen_image_zero_cond_t=true
```
plus the Lightning 4-step LoRA.

---

## 4. Tests and results

### 4a. First runs (before any optimisation)

**Run 1 — 256×256, 1 step, no add-on** (no prediction fixed)

| Phase | Measured |
|---|---|
| Reference picture encode | 6.4 s |
| Prompt-reader, cold | 24.0 s |
| Image-making, 1 step | 29.2 s |
| Picture conversion (VAE decode) | 11.0 s |
| Total | 70.8 s |
| Peak memory | 7.8 GB |

Finding: our arithmetic was too pessimistic. It counted all 20B parameters per
token; the model has separate image-stream and text-stream weights, so each
token passes through about half.

**Run 2 — 512×512, 4 steps, with add-on.** Edit: "change the text on the sign to 'hello'".

| Phase | Predicted | Measured |
|---|---|---|
| Prompt-reader, cold | ~24 s | 30.3 s |
| Image-making, 4 steps | ~320 s | 449.0 s (122/110/110/107 s per step) |
| Picture conversion (9 tiles) | — | 91.7 s |
| Total | — | 628.2 s |
| Peak memory | 8–9 GB | 8.96 GB |

Verdicts: image-making 40% slower than predicted (the add-on is applied at run
time); memory prediction held; edit correct and clean (one sample only).

### 4b. Sprint 2 — memory

"Held" = the program's own memory the OS can't reclaim. "Borrowed" = cached
model-file pages the OS drops when anything else needs room.

**Memory by stage** (small picture, 1 step):

| Stage | All copied | "disk" setting | Read in place |
|---|---|---|---|
| Prompt + picture reading | 4.4 GB | 4.1 GB | 0.4 GB |
| Image-making | 11.6 GB | 7.8 GB | 0.6 GB |
| Picture conversion | 12.0 GB | 1.0 GB | 1.0 GB |
| **Peak held** | **12.0 GB** | **7.8 GB** | **1.0 GB** |

| Test | Prediction (fixed first) | Measured | Verdict |
|---|---|---|---|
| Read in place, 256², 1 step: held during image-making | < 2 GB | 0.56 GB | confirmed |
| Read in place, 512², 4 steps + add-on: peak held | 1.5–2.5 GB | 1.87 GB | confirmed |
| Same: total time | ≤ 628 s | 532 s | confirmed (faster) |
| Same: output | — | byte-identical to Run 2 | — |
| Under pressure (9.9 of 15 GB taken by another program): completes | yes | yes | confirmed |
| Under pressure: image-making time | 100–250 s (normal 23 s) | 25 s | **wrong** (too pessimistic) |

Key lesson: the "disk" setting sounds right but still makes a private copy.
Reading in place only happens with weights on CPU **and** `--mmap` on.
Why the pressure test was barely slower is not certain (likely the disk is
faster on repeat reads). So this does **not** prove behaviour on a truly slow disk.

Not proven: Windows; graphics chips (GPU still gets its own copy); deliberate
fill control (stop C).

### 4c. Sprint 3 — speed (512×512, one reference)

Baseline: 532 s = prompt-reader 18 + image-making 354 (4 steps) + picture
conversion 69 + loading/reference encoding. 1.9 GB held.

**E1 — cost of the on-the-fly add-on (theory 6)**
Prediction: image-making 354 s → 250–290 s.
Result: 69 s/step without vs ~88 s/step with → 4 warm steps = 276 s; held peak 1.1 GB vs 1.9 GB.
Verdict: **confirmed.** Add-on costs ~22% of image-making (~75 s) and 0.8 GB.
Side finding: a cold start (model pushed out of OS cache) made step 1 take
322 s and prompt-reading 128 s, re-reading from a ~30 MB/s disk. Needs
measuring on a laptop SSD.

**E2 — 2 steps instead of 4 (theory 4)**
Prediction: image-making ~176 s; quality "visibly softer but recognisable".
Result: 179 s; total 350 s; no visible difference from 4 steps.
Verdict: time **confirmed**; quality better than predicted.

**E3 — picture conversion in one piece instead of 9 tiles**
Prediction: 69 s → 25–40 s; held memory 3–4 GB during it.
Result: 31 s; 3.3 GB. Verdict: **confirmed.**
Unplanned: the 1-step picture was also a correct, clean edit. Whole run 151 s
(prompt-reader 18, image-making 80, conversion 31, rest loading/reference).

**Scoreboard**

| Settings | Total | Held memory |
|---|---|---|
| Sprint 1 settings, 4 steps | 628 s | 9.0 GB |
| Read in place, 4 steps | 532 s | 1.9 GB |
| Read in place, 2 steps | 350 s | 1.9 GB |
| Read in place, 1 step, untiled conversion | **151 s** | 3.3 GB |

**Caution:** the test edit (sign text) is easy, and it is one picture. E5
below shows 1–2 steps leave ghosting on a harder edit.

**E5 — do 1 and 2 steps survive a harder edit? (theory 4)**
Prompt: "put a red knitted hat on the cat and change the background to a sunny
beach". Prediction: 1 step flawed, 2 acceptable, 4 clean.

| Steps | Total | Image-making | Picture |
|---|---|---|---|
| 1 | 158 s | 82 s | Hat and beach present; faint ghost of cat's ears through the hat; soft background |
| 2 | 242 s | 170 s | Same ghost ears, slightly fainter; soft background |
| 4 | 392 s | 323 s | Clean: no ghosting, sharp background, natural hat |

Verdict: **partly confirmed.** 1 step flawed (milder than expected); 2 steps
also not clean (prediction wrong). For edits that add or remove things, 4 steps
remains the quality setting; 1–2 steps is a fast preview. Fewer steps is not the
route to "same quality, faster"; reference-reuse methods (section 5) are.

**Queued (no results yet):**
- **H1–H4** — the edits the project is for, 2 steps, 512². Subject is an
  AI-generated sample person (not a real individual).

  | Test | Inputs | Prediction |
  |---|---|---|
  | H1 outfit | subject + outfit | Works; small garment differences |
  | H2 scene | subject + scene | Works |
  | H3 pose | subject + stick figure | Most likely to fail (ignore pose or paint the stick figure in) |
  | H4 all | subject + outfit + scene + pose | 2–3 of 3 changes right; ~9 min; < 4 GB held |

  Face identity must hold; H3/H4 are most at risk. Failures re-run at 4 steps
  to separate "too few steps" from "wrong approach".
- **E4** — work at 384×384 then enlarge (theory 5).
- **E6** — fold the add-on into the model file (theory 6).

---

## 5. Prior work already found (checked 2026-11-14)

Others' speed-ups were measured on big GPUs at ~28–50 steps, not 4 steps on a CPU.

| Our theory | Already shown? | Who | Still ours to test |
|---|---|---|---|
| 1. Reuse reference work across steps | Yes | RegionE (2.06× on Qwen-Image-Edit); SpotEdit (ref caching 1.24× → 1.95×) | Does it pay at 4 steps? Needs engine code |
| 2. Compute only the changed region | Yes | SpotEdit (1.59×), RegionE, CacheEdit | Finding the region costs 1–2 steps = half a 4-step run |
| 1b. Drop unimportant reference tokens | Yes | ToPi (>30% faster on Qwen-Image-Edit) | Simplest first patch |
| 3. Reuse work between steps | Yes, many-step runs | FreqCa (6–7×), engine's cache modes | Likely weak at 4 steps |
| 4. Fewer than 4 steps | No release found | Only 4- and 8-step add-ons exist | Being tested (E2, E3, E5) |
| 5. Small then enlarge | Common practice | — | One run |
| 6. Fold add-on into model | Files exist | lightx2v merged file (20.6 GB, 8-bit) | Must re-shrink to 2-bit; disk space tight |
| 7. Use idle graphics chip | Engine supports (Vulkan) | — | Laptop only |
| 8. Save prompt-reading to disk | Partly (in-memory cache only) | — | Small patch |
| CPU-only timing for this model | None found | Nearest: 4 min 50 s on a 6 GB GPU, related model | Our numbers appear new |

Planned order: zero-code runs first (6, 4, 5, 3), then build 1b, then 1 and 2.
Reframe: between-step reuse (theory 3) is proven but built for 30–50 steps;
at 4 steps the same idea must apply *inside* a step (reference reuse, region skipping).

Sources: RegionE arxiv.org/html/2510.25590v1 · SpotEdit arxiv.org/pdf/2512.22323v1 ·
CacheEdit icml.cc/virtual/2026/poster/66154 · ToPi arxiv.org/html/2602.01609v1 ·
FreqCa arxiv.org/html/2510.08669v1 · huggingface.co/lightx2v/Qwen-Image-Edit-2511-Lightning

---

## 6. Original GPU-era predictions (all untested; CPU only so far)

Reference case: 20B DiT, 60 blocks, 4 steps, 640×640 + one reference (3200
tokens), 2 GB discrete GPU, PCIe 3.0, 16–32 GB RAM.

| # | Claim | Theory target | Conventional expectation |
|---|---|---|---|
| 1 | Peak VRAM, ring + activations | ≤ 1050 MB | ~240–410 MB |
| 2 | Quantized transfer per step | ~5.5 GB | 5–10 GB |
| 3 | RAM→VRAM throughput | — | ~3 GB/s (PCIe 3.0 x4) |
| 4 | Async ring hides transfer | ~100% | up to 100% with dedicated transfer queue |
| 5 | Sustained GPU arithmetic needed | ~6 TFLOPS | 0.5–1.5 TFLOPS on 2 GB-class cards |
| 6 | Image-making, 4 steps | 55–85 s | ~350–1000 s |
| 7 | Prompt-reader, cold | 12–18 s | 60–180 s on low-end CPU |
| 8 | Prompt-reader, cached | 0 s | ~0 s |
| 9 | Decode + sharpen | 5–10 s | — |
| 10 | End to end | 75–115 s | ~7–20 min |
| 11 | Quality at ~2 bits/weight usable | yes | visible degradation below ~4 bpw |

Laptop prediction (fixed): CPU run completes, Task Manager shows ~2 GB for the
program, total 15–25 minutes.

---

## 7. Known gotchas
- `--params-backend disk` keeps a full private copy; only `cpu` + `--mmap` reads in place.
- Reading in place is CPU-only; graphics chips still get a copy (stop D).
- A background task exiting 0 doesn't mean it worked; check outputs.
- Cold start on slow disk costs minutes.
- Parked, not dropped: hosted API-key front end; model-swap and legal notes.

## 8. Open questions we'd most like a second opinion on
1. Is 1–2 steps with a 4-step distilled LoRA a reliable shortcut, or will it
   collapse on identity-preserving edits (face, pose)?
2. Is ~2-bit (Q2_K) quality going to hold across harder edits, or is the sign
   test misleading?
3. Is stop C (program-controlled fill + prefetch) worth building vs. trusting
   the OS page cache, given the pressure test barely slowed down?
4. For the Iris Xe laptop: is Vulkan on shared memory likely to beat the CPU
   path, or will the copy cost (stop D) erase the gain?
5. Which of RegionE / SpotEdit / ToPi is the best fit to port into
   stable-diffusion.cpp for a 4-step model?
6. Is "under 2 minutes at 512² on 4 CPU cores" realistic for a 20B editor, or
   is the real ceiling elsewhere?
