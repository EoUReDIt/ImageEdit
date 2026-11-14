# Handoff — read this file only; open others when a step needs them

Updated after every experiment. Last update: 2026-11-14, Sprint 3 started.

## Resume in one minute

- **Project:** ImageEdit. Run a 20B image-editing model (Qwen-Image-Edit-2511)
  on ordinary PCs. One subject photo + tagged references + prompt -> one image.
- **Branch:** `claude/compassionate-fermi-1dzo7z` (only branch to push to).
- **Engine:** upstream stable-diffusion.cpp, unmodified so far. No patches yet.
- **Owner:** non-technical explorer. Write reports in plain language. Method:
  start from the wanted conclusion -> theory -> solution; fix a prediction
  before every test; log it in `docs/predictions.md`. If you have a question,
  first explain why, the background, and the theory at stake.
- **Owner's claims being tested:** (1) hardware was never the problem;
  (2) the problem was access to expertise; (3) claim 1 also holds for speed.

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

## Next action

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
