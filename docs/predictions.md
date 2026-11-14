# Prediction ledger

Method: build to the theory, then measure. The arithmetic column is the
conventional expectation. A measurement that beats it is a discovery; one that
matches it confirms the conventional limit. Fill in "Measured" with the device
and date. Never edit a prediction after its test has run.

> **Every prediction in this ledger is an estimate, not a finding.** Like
> pioneers working from a sketched map, we only know a route once we have
> built it or travelled it. Arithmetic, papers and outside reviews tell us
> where to look; only a measured run on the real machine confirms anything.
> A prediction marked "withdrawn" was dropped before testing because its own
> arithmetic did not hold; it stays visible so the record is honest.

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

**Withdrawn before testing (2026-11-14, after outside review 1):** rows 6 and
10. Their targets need ~6 TFLOPS sustained; the same table says a 2 GB-class
card gives 0.5–1.5. The conventional column (7–20 min) is the better estimate.

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

### 2026-11-14 — Sprint 2, memory (same cloud container, CPU only)

Full write-up: `docs/sprints/sprint-2.md`. "Held" = anonymous memory (RssAnon);
"borrowed" = file-backed pages (RssFile). Sampled 4x per second.

| Test | Prediction (fixed first) | Measured | Verdict |
|---|---|---|---|
| Read in place, 256x256, 1 step: held during denoising | under 2 GB (was 7.8 GB) | 0.56 GB | confirmed |
| Read in place, 512x512, 4 steps + LoRA: peak held | 1.5–2.5 GB (was 9.0 GB) | 1.87 GB | confirmed |
| Same: total time | no worse than 628 s | 532 s | confirmed |
| Same: output | — | byte-identical to Run 2 | — |
| Under pressure (9.9 GB occupied): completes | yes | yes | confirmed |
| Under pressure: denoising time | 100–250 s (was 23 s) | 25 s | wrong (too pessimistic) |

Flags for read-in-place: `--backend cpu --params-backend te=cpu,diffusion=cpu,vae=cpu --mmap`.
`--params-backend disk` does not map in place (peak held 7.8 GB, with or without `--mmap`).

## Claim 3 split into three (owner decision, 2026-11-14)

Claim 3 was "the hardware was never the problem, for speed too". After outside
review 1 (`docs/reviews/2026-11-14-gemini-concept-review.md`) it is tested as
three separate claims, because "the hardware" means three different machines.

| # | Claim | Where it is tested | Our estimate now | Status |
|---|---|---|---|---|
| 3a | A desktop runs the 20B model (or larger) at good quality in a usable time | A desktop with a graphics card, not yet available | Unknown: no measurement on a graphics card yet | open |
| 3b | The current laptop (i5-1135G7, 16 GB, Iris Xe, Windows) gives a good multi-reference edit in under 2 minutes | The owner's laptop | Unlikely with 20B at 4 steps (8–25 min estimated); possible with a smaller model such as FLUX.2 [klein] 4B | open |
| 3c | A cloud API gives 20B quality in seconds, behind a simple local screen | A hosted Qwen-Image-Edit API with the owner's key | Likely (~10–15 s per the review); cost and privacy to check | open, previously parked |

Evidence so far for 3b on the cloud machine (4 cores, Linux): clean 4-step
512x512 edit = 392 s (E5). Under 120 s needs ~3.3x more speed; known 20B
tricks (fold add-on ~1.2x, reference pruning ~1.3x) total ~1.5x.

## Open predictions after outside review 1 (fixed before any of these runs)

Two predictions per test where the reviewer gave one: ours and Gemini's. Both
are estimates.

### Step 1 — H1–H4 on the cloud machine, 4 steps, 512x512, Q2_K, untiled conversion

Changes from the earlier H plan: 4 steps instead of 2 (E5 showed 2 steps
ghosts); H3 run three ways; H4 also run with three pictures (the model is
reported to be reliable with 2–3 references only).

| Test | Pictures | Our prediction | Gemini's prediction | Measured |
|---|---|---|---|---|
| H1 outfit | subject + outfit | Works; garment approximate; face kept. ~10 min total | Succeeds at 4 steps | **Works.** Face, hair, pose, background kept; jacket detail carried over; one sleeve came out black. 722 s (12 min). Both predictions right on quality; our time 20% low |
| H2 scene | subject + scene | Works; face kept. ~10 min | Succeeds at 4 steps | **Works.** Face and clothes kept, beach correct; looks slightly pasted in; still holds the marker. 782 s (13 min). Both right on quality |
| H3a pose, stick figure | subject + hand-drawn stick figure | Pose partly followed or stick figure painted in. ~10 min | Struggles | **Both happened:** arms raised, face kept, but the stick figure is drawn over her and the board text vanished. 744 s. Both right |
| H3b pose, keypoint map | subject + coloured skeleton | Pose followed better than H3a | (not tested in review) | **Works, cleanly.** Full-body arms-up pose, face kept, background kept, nothing drawn in; trousers invented for the full body. 701 s. Confirmed. Disproves the review's "not trained on skeletons" |
| H3c pose, photo | subject + generated photo of a person in the pose | Pose followed; risk of face mixing with the pose person | Much better than stick figure | **Fails:** the man is added beside her; her pose unchanged; faces not mixed. 707 s. Both predictions wrong |
| H4 all four | subject + outfit + scene + stick figure | 1–2 of 3 changes right; face drifts. ~15 min; held < 4 GB | Reveals whether Q2_K blurs the face | **Fails, 0 of 3.** She is unchanged; two extra figures in red jackets appear (one is the mannequin given a face); no beach, no pose. Her face is sharp, so Q2_K is not the cause. 1408 s (23.5 min); held 3.45 GB. Ours wrong (worse, slower); memory right |
| H4' three | subject + outfit + scene | 2 of 2 changes right; face kept. ~12 min | (not tested in review) | **Fails, 0 of 2.** She is unchanged; the mannequin is added beside her with a face; no beach. 1026 s (17 min). Ours wrong. With these settings the working limit is 2 pictures, not 2–3 |

Step 1 timing pattern (image-making, 4 steps): 1 picture 323 s (E5), 2
pictures 590–654 s, 4 pictures 1224 s. Each extra picture adds roughly 290–330 s
per edit on this machine, so the number of pictures, not their size, is the
main cost. Held memory stayed 3.4–3.5 GB throughout. Pictures:
`docs/results/2026-11-14-H*.jpg` (each shows inputs, then result).

**Step 1 conclusion:** two pictures per pass works (H1, H2, H3b); three or
four pictures in one pass fails every time (H4, H4', H4k), with the extra
pictures turned into extra people. One change per pass (H4chain) does the full
outfit + scene + pose edit. Pose must be given as a skeleton (H3b), not a
stick figure (H3a) or a photo (H3c). Cost on this machine: ~12 min per change.

Added during the run (after H3a–H3c, before H4k ran): H3b showed the pose
skeleton is the pose input that works, so H4 is also run with it.

| Test | Pictures | Our prediction | Measured |
|---|---|---|---|
| H4k four, skeleton | subject + outfit + scene + skeleton | Better than H4: no figure painted in; 2 of 3 changes right (four pictures is above the reported 2–3 limit). ~15 min | **Fails, 0 of 3 for her.** Outfit and pose are applied correctly, but to the mannequin, which becomes a man; she is unchanged; no beach. 1459 s (24 min). Ours wrong. Finding: the model treats the mannequin in the outfit picture as a second person |

Added after H4p, before the run: **H4chain**, one change per pass, each pass
with two pictures (the case that works): pass 1 = H1 result is reused (outfit);
pass 2 = that result + beach; pass 3 = that result + pose skeleton.

| Test | Our prediction | Measured |
|---|---|---|
| H4chain | All 3 changes right; face drifts slightly with each pass; passes 2 and 3 ~12 min each, so ~36 min for all three changes | **Works: 3 of 3.** Red jacket, beach, both arms raised; face recognisable at every pass (smaller in the final full-body frame); trousers invented. Passes 722 + 718 + 690 s = 2130 s (35.5 min). Confirmed, time included. Picture: `docs/results/2026-11-14-H4chain.jpg` (subject, pass 1, pass 2, pass 3) |

### Step 2 — laptop, CPU, 4 steps, 512x512 (owner runs)

| Measure | Our prediction (Sprint 2) | Gemini's prediction | Measured |
|---|---|---|---|
| Completes | yes | yes | — |
| Program's own memory (Task Manager) | ~2 GB | 2.0–3.5 GB | — |
| Warm run | 15–25 min | 8–14 min | — |
| Cold run (after reboot) | not predicted | 12–18 min | — |

### Step 3 — laptop, Iris Xe graphics chip (Vulkan), same edit

Check first: Task Manager > Performance > GPU must say "Iris Xe". "UHD
Graphics" means one memory stick, and the test result would not represent
Iris Xe.

| Our prediction | Gemini's prediction | Measured |
|---|---|---|
| Runs, but no faster than CPU, or fails for lack of shared memory | Hits the 8 GB shared limit, or 1.5–2x faster than CPU (~4–7 min) | — |

### Step 4 — E6, fold the add-on into the model file

Method (cloud machine, no extra download): the engine's convert mode merges
the add-on while rewriting the Q2_K file. This unpacks already-compressed
weights and compresses them again, so a small quality loss is possible; the
clean route is merging into the full-precision file first (needs ~40 GB disk).
Added prediction: picture visibly the same as the add-on-at-run-time version
on H1 and E5; if not, double compression is the suspect.
**Result (2026-11-14): could not run on this machine.** Folding the add-on into
a compressed file makes the engine unpack the whole model to full precision in
memory first (38 GB); this machine has 15 GB. Folding has to be done on a
machine with ~48 GB of memory, or offline from the full-precision file.

**Substitute test E6' (prediction fixed before the run):** fold the add-on in
at load time (`--lora-apply-mode immediately`) instead of at every step. It
measures the speed gain of a folded model, but not its memory, because the
folded weights become a private copy. Prediction: E5's hat edit image-making
~260 s (from 323 s, about 20% faster); held memory rises to 8–9 GB; loading
takes 1–3 min longer.

Planned runs (not done): E6a = E5's hat-and-beach edit (compare image-making with E5's 323 s);
E6b = H1 (compare with H1's 602 s). Prediction: both ~20% faster image-making,
held memory ~0.8 GB lower during image-making.

| Our prediction | Gemini's prediction | Measured |
|---|---|---|
| Image-making ~22% faster, 0.8 GB less held (from E1) | 25–35% total time saved (with smaller references), ~0.8 GB less | — |

### Step 5 — FLUX.2 [klein] 4B on H1–H4 (laptop)

| Our prediction | Gemini's prediction | Measured |
|---|---|---|
| Under 2 min per edit on the laptop at 512x512; face likeness weaker than 20B at 4 steps | Under 2 min; sharper than 20B at 1 step | — |
