# Sprint 3 journal: speed

Goal: same cloud machine, same picture quality, from about 9 minutes to under
2 minutes at 512x512. Working backward from the claim "hardware was never the
problem, for speed too": the software must be doing more work than the picture
needs. Each entry: theory, prediction (fixed before the run), result, verdict.

Baseline: 532 s total = encoder 18 + denoising 354 (4 steps) + VAE decode 69 +
loading and reference encoding. 1.9 GB held.

Speed theories (full list in `docs/roadmap.md`): 1 reuse reference work across
steps; 2 only compute the changed region; 3 reuse work between steps; 4 fewer
steps; 5 work small then enlarge; 6 remove add-on overhead; 7 use the idle
graphics chip; 8 save prompt-reading results to disk.

## Entries

### E1 — How much does the on-the-fly add-on cost? (theory 6)

Run the same 4 steps without the add-on. The picture will be poor; only the
timing matters.
**Prediction:** denoising drops from 354 s to 250–290 s (the add-on costs 20–30%).
**Result:** in flight.

**Queued next:** E2 two steps instead of four (theory 4); E3 VAE decode without tiling.
