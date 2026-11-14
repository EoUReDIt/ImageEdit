# Handoff — read this file only; open others when a step needs them

Updated after every experiment. Last update: 2026-11-14, after step 1 (H tests) and E6 attempts.

## Resume in one minute

- **Project:** ImageEdit. Run a 20B image-editing model (Qwen-Image-Edit-2511)
  on ordinary PCs. One subject photo + tagged references + prompt -> one image.
- **Branch:** `claude/sweet-knuth-4ql77j` (work moved here from
  `claude/compassionate-fermi-1dzo7z`, which is merged in and now frozen;
  see `HYGIENE.md` for cleanup).
- **Engine:** upstream stable-diffusion.cpp, unmodified so far. No patches yet.
- **Owner:** non-technical explorer. Write reports in plain language. Method:
  start from the wanted conclusion -> theory -> solution; fix a prediction
  before every test; log it in `docs/predictions.md`. If you have a question,
  first explain why, the background, and the theory at stake.
- **Owner's claims being tested:** (1) hardware was never the problem;
  (2) the problem was access to expertise; (3) claim 1 also holds for speed,
  split into 3a desktop (20B or larger), 3b current laptop, 3c cloud API.
- **Predictions are estimates.** We confirm only by building or travelling.

## Where things stand

| Sprint | Goal | Status |
|---|---|---|
| 1 | Laptop run (i5-1135G7, 16 GB, Iris Xe, Windows) | Owner travelling; not run yet |
| 2 | Memory down | Done: 9.0 GB -> 1.9 GB held. `docs/sprints/sprint-2.md` |
| 3 | Speed: ~9 min -> under 2 min on the same cloud machine | In progress. Journal: `docs/sprints/sprint-3.md` |

Best known settings (CPU):
`--backend cpu --params-backend te=cpu,diffusion=cpu,vae=cpu --mmap --steps 4 --cfg-scale 1 --sampling-method euler --flow-shift 3 --vae-tiling --model-args qwen_image_zero_cond_t=true`
plus the Lightning 4-step LoRA. Baseline at 512x512, one reference:
532 s total (encoder 18, denoising 354, VAE decode 69), 1.9 GB held.
Fastest so far: 1 step, no `--vae-tiling`: 151 s, 3.3 GB held, correct on an
easy edit only. E5: on a harder edit 1–2 steps leave ghosting; 4 steps
(392 s) is the quality setting.

## Next action

Order after outside review 1 (`docs/reviews/2026-11-14-gemini-concept-review.md`):
1. H1–H4 at 4 steps on the cloud machine (H3 three ways, H4 also with three pictures).
2. Laptop CPU run, warm and cold (owner).
3. One Iris Xe Vulkan test (owner; check the chip says "Iris Xe").
4. E6: fold the add-on into the model file.
5. FLUX.2 [klein] 4B on the same edits.

Predictions for all five are in `docs/predictions.md`.

Status 2026-11-14 evening: step 1 done (two pictures per pass works; three
or four pictures fail; one change per pass does the full outfit + scene +
pose edit in ~36 min; pose must be a skeleton). Step 4 (E6) blocked on this
machine: folding needs ~38 GB memory and load-time folding crashes the
engine. Steps 2, 3, 5 need the owner's laptop.

See the last entry of `docs/sprints/sprint-3.md`: it always names the
experiment in flight and the next one queued.

## Cloud session facts

- Environment "ImageEdit": Hugging Face allowed. 4 CPU cores, 15 GB RAM, no
  GPU, no PowerShell. Container is disposable.
- If the container was reclaimed, rebuild in this order:
  1. `git clone --depth 1 --recurse-submodules --shallow-submodules https://github.com/leejet/stable-diffusion.cpp /home/user/upstream/sdcpp`
     then `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j4 --target sd-cli`
     (results so far are from commit `0f8a467`).
  2. Models into `/home/user/models` (exact names and repos: results log in
     `docs/predictions.md`; download host is `us.aws.cdn.hf.co`).
  3. Measure with `tools/measure/exp.sh <name> <params-backend> [flags]`
     (wraps `memrun.py`; env vars SIZE, STEPS, PROMPT, LLM). Output lands in
     `/home/user/exp/<name>/` as `log.txt`, `mem.json`, `result.png`.
- Test input image: `assets/flux/flux1-dev-q8_0.png` in the upstream repo.

## Files

- `CONCEPT-REVIEW-BRIEF.md` — self-contained brief for outside reviewers.
- `docs/reviews/` — outside reviews, with our check of each.
- `HYGIENE.md` — branch cleanup guide and questions for the owner.
- `docs/prior-work.md` — which theories others already proved; check and
  extend it before testing any new theory.
- `docs/roadmap.md` — thesis, three layers, stops A–G, speed theories.
- `docs/predictions.md` — every prediction and result.
- `docs/sprints/` — one plain-language report per sprint.
- `docs/upstream-gap-analysis.md` — what upstream already does vs the theory.
- `tests/lowend/run-theory.ps1` — owner's laptop test (untested on Windows).
- `runtime/streambench/` — standalone GPU bus/arithmetic benchmark (parked).

## Gotchas

- `--params-backend disk` keeps a full private copy in memory; only
  `cpu` + `--mmap` reads in place.
- A background task's exit code 0 does not mean its commands worked; check outputs.
- Reading in place is CPU-only; graphics chips still get a copy (stop D).
- Parked, not dropped: hosted API-key front end; model-swap and legal notes
  are in `docs/runtime-design.md`.
