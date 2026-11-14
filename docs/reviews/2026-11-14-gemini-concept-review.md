# Outside review 1: Gemini (AI Studio), 2026-11-14

Reviewer brief: `CONCEPT-REVIEW-BRIEF.md`. The review is copied word for word
in part 2. Part 1 is our check of it against our own measurements and a web
search, done the same day.

## Part 1: our check of the review

### Agreed, and acted on
| Point | Our evidence | Action |
|---|---|---|
| Sign-text test flatters 1–2 steps | E5: 1–2 steps leave ghosted cat ears; only 4 steps clean | 4 steps is the quality setting; H1–H4 run at 4 steps |
| Section 6 GPU predictions contradict themselves (need ~6 TFLOPS, card gives 0.5–1.5) | Our own ledger columns show it | Ledger rows 6 and 10 marked *withdrawn before test* |
| Sprint 2 pressure test is weak (1 step, 256²) | E1 cold start: step 1 took 322 s instead of 69 s | Repeat at 4 steps, 512², cold, on the laptop |
| Stop C can wait | Pressure test barely slowed | Stop C parked until the laptop shows a need |
| RegionE / SpotEdit don't fit scene and pose swaps | Logic: they skip unchanged regions; these edits change almost everything | Theory 2 parked; ToPi (theory 1b) is the first patch |
| Fold the add-on into the model file | E1: add-on costs ~22% and 0.8 GB | E6 stays queued |

### Corrected or unverified
| Gemini said | What we found |
|---|---|
| "ToDoPro (Oct 2026)" paper | Not found by search. Possibly ToPi ([arXiv 2602.01609](https://arxiv.org/html/2602.01609v1)) or a June 2026 token-dropping paper ([arXiv 2606.23682](https://arxiv.org/pdf/2606.23682)). Treated as unverified |
| The model was never trained on pose skeletons, so H3 may fail for that reason | Since the 2509 release, Qwen-Image-Edit advertises built-in keypoint-map (pose skeleton) support ([model card](https://huggingface.co/Qwen/Qwen-Image-Edit-2509)). But a hand-drawn stick figure is not a proper keypoint map, so H3 runs three ways: stick figure, keypoint map, pose photo |
| Use a smaller prompt-reader (Qwen3-VL-2B or 4B) | Will not work: the image model was trained on what Qwen2.5-VL-7B produces. Only a more compressed copy of the same 7B reader is an option. Our own roadmap stop E had the same mistake; corrected |
| Cloud machine has AVX-512 (implying the laptop doesn't) | The i5-1135G7 (Tiger Lake) also has AVX-512. The real gaps: power and heat throttling, Windows memory use, shared memory bandwidth |
| Claim 3 is "false" | A prediction, not a measurement. Our numbers lean the same way for 20B on 4 cores: the clean 4-step run is 392 s, under 120 s needs ~3.3x, and known tricks total ~1.5x. Claim 3 is now split into three (see roadmap) |
| Upstream docs confirm how `--mmap` counts memory | Not checked |

### What the review missed
- Qwen-Image-Edit-2511 is reported to work reliably with only 2–3 reference
  pictures ([UniRef-Image-Edit](https://arxiv.org/html/2602.14186v1);
  [HF discussion](https://huggingface.co/Qwen/Qwen-Image-Edit-2511/discussions/30)).
  H4 gives it four. H4 is therefore also run with three.
- Iris Xe only runs as Iris Xe with two matched memory sticks (dual channel);
  with one stick it falls back to "UHD Graphics" mode. Check the laptop before
  the graphics-chip test. (From general knowledge, not searched.)

### Leads confirmed
- FLUX.2 [klein] 4B exists, is Apache 2.0, and supports multi-reference editing
  ([BFL blog](https://bfl.ai/blog/flux2-klein-towards-interactive-visual-intelligence),
  [README](https://cdn.jsdelivr.net/gh/black-forest-labs/flux2@main/README.md)).
  Candidate for claim 3b (laptop under 2 minutes).
- ToPi: over 30% faster on Qwen-Image-Edit, including multi-image tests.

### Owner decisions taken after this review
- Claim 3 split into 3a (desktop, 20B or larger), 3b (current laptop),
  3c (cloud API).
- Proceed in the review's order, adjusted as above.
- All predictions are estimates. We confirm only by building or by going there.

---

## Part 2: the review, word for word

Here is an outside cartographer’s review of your map, written in **plain language first**, followed by **technical detail and test-by-test answers**.

---

### Plain-Language Summary (The Short Honest Map)

#### What you have already proven (and should be proud of)
Your discipline—writing predictions before running tests and keeping results honest—paid off. You already proved two real wins without changing a single line of engine code:
1. **Taking turns (Stop A)** and **reading weights straight from the model file (Stop B)** slashed the program’s locked memory from **9.0 GB down to 1.9 GB**. That means a 16 GB Windows laptop will likely be able to *run* this 20B model without crashing.
2. **Untiled image conversion (E3)** cut the final decode step in half (`69 s → 31 s`) for a small memory bump (`3.3 GB`) that still fits comfortably on a 16 GB laptop.

#### Where the map is overly optimistic
1. **Your current test picture is flattering you.** Changing text on a sign leaves 95% of the photo untouched. That is why **1–2 steps** at **2-bit compression (`Q2_K`)** looked clean. Your real product—swapping a person’s **outfit, pose, and scene**—is far harder. Do not trust the **151-second** headline until `H1–H4` pass.
2. **Saving memory is not the same as gaining speed.** Reading weights from disk (`--mmap`) stops the app from crashing when RAM is full, but it does **not** make math faster. When the model doesn’t fit in spare RAM, your own test (`E1`) showed a cold disk read blew Step 1 up from **69 s to 322 s**.
3. **A 20-billion-parameter model running in under 2 minutes on 4 laptop CPU cores is fighting physics.** Your three owner claims split cleanly:
   * *Claim 1 (Hardware isn’t the problem for running it at all):* **Mostly true** (with 16 GB RAM and `--mmap`).
   * *Claim 2 (Fixes were hidden in settings):* **True for memory** (`cpu` + `--mmap`).
   * *Claim 3 (Hardware isn’t the problem for speed):* **False.** Trying to push 20 billion weights through 4 thin-laptop CPU cores in under 2 minutes for a real multi-photo edit is like trying to pull a freight trailer at highway speed with a compact car by changing the tires. Software tricks give **1.5×–3×** gains, not **10×**.

#### The shortest route to your actual goal
If the goal is **“a non-technical user gets a great multi-reference edit in under 2 minutes on an ordinary laptop”**, you have two realistic paths on the map:
* **Path 1 (Quality first, ~15 seconds):** Keep 20B `Qwen-Image-Edit-2511` on an optional API key for fast cloud runs, and keep the local CPU mode as an offline “slow/free mode” (**5–12 minutes**).
* **Path 2 (Local under 2 minutes):** Pair your clean UI with a **smaller 4B–6B model** (like `FLUX.2 [klein] 4B`) for ordinary laptops, rather than crushing a 20B model down to 2 bits and 1–2 steps.

---

### Detailed Review (Answers to Questions 1–5)

#### 1. Is the thesis sound? Where is it wrong or naive?

* **What is sound:**
  * *“Take turns” + “Read in place”:* Correct. Because image diffusion processes thousands of image tokens at once per pass, CPU compute is often the bottleneck *when weights stay hot in the OS page cache*—unlike text LLMs, which process one token at a time and stall completely on RAM speed.
  * *Acknowledging the bandwidth floor:* Correctly frames why slow disks hurt cold starts.
* **Where it is naive or wrong:**
  1. **“Scale ambition: 64B on 64 GB” confuses *fitting* with *finishing*.** Even if `--mmap` lets a 64B model fit on a 64 GB machine without crashing, touching 3.2× more weights on the same CPU/iGPU still takes ~3.2× longer.
  2. **The Section 6 GPU predictions (`75–115 s` on a 2 GB discrete GPU) violate their own math.** Look at lines 4–6 of Section 6: your theory target requires **~6 sustained TFLOPS** of compute, while your own table notes that 2 GB-class cards only deliver **0.5–1.5 TFLOPS**. On top of that, PCIe 3.0 ×4 (~3 GB/s) transferring ~5.5 GB per step × 4 steps = ~22 GB of bus traffic (~7.3 s minimum even at 100% overlap, and Q2_K dequantization overhead + low TFLOPS will compute-bound the GPU). The conventional expectation (`7–20 min`) is far closer to reality than `75–115 s`.
  3. **Linux container ≠ Windows ultrabook.** Your cloud VM has 4 fast server cores with AVX-512 and ~14 GB of free RAM after a tiny OS footprint. An **Intel i5-1135G7** laptop runs Windows (which reserves ~4.5–5 GB RAM right away), throttles power to 15–28 W after ~30 seconds of full load, and shares memory bandwidth with the Iris Xe chip.

---

#### 2. Prior work already proven/disproven that was missed

Two important updates change your Section 5 table:
1. **RegionE and SpotEdit (Theory 2) are the wrong fit for your app.** A newer multi-reference acceleration paper (*ToDoPro*, Oct 2026) explicitly notes that **RegionE and SpotEdit are limited to layout-preserving single-image editing** (where most of the original scene stays put, like changing a sign or adding a hat). For your core tasks (`H2` scene swap, `H3` pose swap, `H4` subject + outfit + scene + pose), almost the entire frame changes—so “skip unchanged regions” has nothing to skip.
2. **Look at `ToDoPro` (Oct 2026) alongside `ToPi` (Theory 1b).** Both **ToPi** and **ToDoPro** specifically address **multi-reference / in-context generation** on `Qwen-Image-Edit-2511`. *ToDoPro* prunes low-value reference tokens *early* before the main denoising loop via downsampled self-probing, which stacks directly with fewer steps and smaller reference sizes.
3. **Why `stable-diffusion.cpp` already explains your “disk” surprise:** Upstream `stable-diffusion.cpp` documentation confirms that `--mmap` counts mapped device buffers against component budgets (`diffusion`, `te`, `vae`) and lets the OS drop clean file-backed pages rather than duplicating them into private RAM.

---

#### 3. Assessment of Untested Stops (C, D, E, F, G)

| Stop | Verdict | Why |
|---|---|---|
| **C. Balanced fill + custom prefetch** | **Dead end (for V1)** | OS virtual memory (`mmap` + page cache) already keeps active pages hot and evicts clean file pages first under pressure. Writing custom block-level prefetch inside `stable-diffusion.cpp` adds major complexity for little gain when the model fits in spare RAM. Only revisit if Windows proves eager to evict active weights while RAM is still available. |
| **D. Three layers on GPU / Iris Xe Vulkan** | **Uncertain — likely bottlenecked on 16 GB Iris Xe** | On your i5-1135G7 laptop, **Iris Xe (80 EU)** delivers ~2.8–3.2 TFLOPS FP16 and uses **Unified Memory (UMA)**—the exact same DDR4/LPDDR4x RAM (~51–68 GB/s) as the CPU. Under Windows WDDM, integrated GPUs are capped at **50% of system RAM (8 GB)**. A ~7 GB `Q2_K` DiT + activations + KV/scratch pushes right into that 8 GB ceiling (risking OOM or slow staging copies in `ggml-vulkan`). Worth **one** test on the real laptop, not a large engineering bet. |
| **E. Load less** | **Shortest route** | **Best immediate payoff with zero risk.** Merging the 4-step Lightning LoRA into the GGUF already saves ~22% time and 0.8 GB RAM (proven in E1). Shrinking reference images (smaller token count), caching prompt-reader output across tweaks (Theory 8), and testing a smaller prompt-reader (`Qwen3-VL-2B` or `4B`) are clean, compounding wins. |
| **F. Go faster (Theories 1–8)** | **Split** | **Keep:** Theory 6 (fold add-on), Theory 8 (save prompt-reading), Theory 1b (`ToPi`/`ToDoPro` reference token pruning), Theory 5 (small → enlarge, test quality carefully). **Drop for V1:** Theory 2 (`RegionE`/`SpotEdit`, wrong task), Theory 3 (between-step reuse doesn’t pay at 2–4 steps), Theory 4 (1–2 steps will likely break `H3`/`H4`). |
| **G. Product** | **Essential escape hatch** | A clean UI that supports **Cloud API mode** (for users who want 10–15 s 20B quality) alongside **Local mode** (either a fast 4B–6B model like `FLUX.2 [klein] 4B` for <2 min local runs, or slow offline 20B) solves the speed problem at the product level. |

---

#### 4. Are the tests designed well enough to support your conclusions?

Your **prediction logging** and **memory accounting (“held” vs. “borrowed”)** are textbook-good. However, **three specific flaws in the current tests prevent drawing strong conclusions yet:**

1. **The Sprint 2 “memory pressure” test did NOT prove resilience to real pressure.**
   Look at the timing: `25 s vs. 23 s` is a **1-step, 256×256** run where the model pages were already hot in cache (or barely touched). By contrast, in **E1**, when the OS cache *actually* went cold, Step 1 jumped from **69 s to 322 s**. Until you test a **4-step `512×512` run under real cache eviction on Windows**, you haven’t proven the laptop won’t thrash the SSD between steps 1, 2, 3, and 4.
2. **Sample size `n = 1` on a sign-text edit cannot validate 1–2 steps or `Q2_K`.**
   Sign text leaves the scene, lighting, and subject geometry intact. Flow-matching models can cheat on easy local text edits in 1–2 steps, whereas `H1–H4` require synthesizing new outfits, poses, lighting, and face alignment across multiple references.
3. **Note on `H3` (stick-figure pose):**
   Watch out for `H3` (“subject + stick figure”). Base `Qwen-Image-Edit-2511` was trained on **natural RGB photos**, not ControlNet OpenPose stick figures—so if `H3` paints the stick figure into the scene, that is a **model-conditioning mismatch**, not a step-count failure. Test `H3` with a **real pose photo** alongside or instead of a stick figure.

---

#### 5. Answers to Your 6 Open Questions (Section 8)

1. **1–2 steps with a 4-step LoRA for face/pose?** Almost certainly **no**—expect soft faces, ignored pose/clothing constraints, and color drift on `H3`/`H4`. Keep 4 steps as the quality baseline until proven otherwise.
2. **Will `Q2_K` hold on harder edits?** Likely **borderline to weak** on facial likeness and small clothing details compared to `Q3_K_S` / `Q4_K_M`. The sign test is misleadingly easy.
3. **Is Stop C worth building vs. OS page cache?** **No, not yet.** Trust the OS page cache until a real Windows 16 GB laptop test proves thrashing while free RAM remains unused.
4. **Will Iris Xe Vulkan beat CPU on the laptop?** Possibly by **1.5×–2×** if it fits inside Windows’ **8 GB shared GPU cap** and avoids staging copies—or it may hit OOM/instability. Test it before designing around it.
5. **Best fit among RegionE / SpotEdit / ToPi for 4-step `stable-diffusion.cpp`?** **ToPi** (or **ToDoPro**), because your tasks are multi-reference compositions where the layout changes—not single-image local edits.
6. **Is “< 2 minutes at 512² on 4 CPU cores” realistic for a 20B editor?** **No, not for reliable multi-reference quality.** Expect **~5–12 minutes** for a real 4-step `H4` edit on 4 laptop CPU cores. To hit `< 2 min` locally, you will likely need a **4B–6B model** (`FLUX.2 [klein] 4B`) or **Cloud API mode**.

---

#### 6. Recommended Next Tests (In Order) & Predictions

1. **First: Run `H1–H4` on the cloud machine at 4 steps AND 2 steps (plus `H3` with a real pose photo, not just a stick figure).**
   * *Why first:* Stop optimizing speed for a settings combo (`1–2 steps` + `Q2_K`) until you know whether it even passes your real edits.
   * *Prediction:* At 2 steps + `Q2_K`, `H1`/`H2` look blurry or drop garment/face fidelity and `H4` breaks down; at 4 steps, `H1` and `H2` succeed, stick-figure `H3` struggles (while photo-pose `H3` does much better), and `H4` reveals whether `Q2_K` blurs face identity (if so, test `Q3_K_S` on `H4`).
2. **Second: Run Sprint 1 (actual i5-1135G7 Windows laptop, 16 GB RAM) with `--backend cpu --mmap` at 4 steps (warm AND cold after reboot).**
   * *Why second:* Real Windows RAM overhead (~5 GB) and laptop thermal throttling change everything compared to a Linux cloud container.
   * *Prediction:* Completes without crashing; Task Manager shows `~2.0–3.5 GB` private memory for the app; warm 4-step run takes **8–14 minutes** (cold **12–18 minutes**).
3. **Third: Run one Iris Xe Vulkan test on the laptop (`sd-cli` with Vulkan backend vs. CPU).**
   * *Why third:* Settles Stop D in one afternoon with zero code changes.
   * *Prediction:* Either hits Windows’ **8 GB shared-VRAM ceiling** or runs **~1.5×–2× faster** than CPU (`~4–7 min`), still well above 2 minutes.
4. **Fourth: Stop E quick wins (fold Lightning LoRA into `Q2_K`/`Q3_K_S` GGUF + shrink reference image size to 384–512px).**
   * *Prediction:* Saves **~25–35% total time** and ~0.8 GB held RAM cleanly with zero engine patches.
5. **Fifth: Benchmark a 4B–6B fallback model (`FLUX.2 [klein] 4B`) on `H1–H4` on the laptop.**
   * *Why fifth:* Gives you concrete data on whether a smaller local model hits **under 2 minutes** on the i5-1135G7 while beating a crushed 20B `Q2_K` 1-step output in visual sharpness.
