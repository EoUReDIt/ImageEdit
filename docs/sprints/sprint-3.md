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

Prior work is checked first: see `docs/prior-work.md`. Theories 1 and 2 are
already proven by others at many steps; ours to test is whether they survive
at 4 steps.

## Entries

### E1 — How much does the on-the-fly add-on cost? (theory 6)

Run the same 4 steps without the add-on. The picture will be poor; only the
timing matters.
**Prediction:** denoising drops from 354 s to 250–290 s (the add-on costs 20–30%).
**Result:** steps 2–4 took 69 s each without the add-on, against about 88 s
each with it. Four warm steps = 276 s. Held memory peaked at 1.1 GB instead of
1.9 GB.
**Verdict:** confirmed. The on-the-fly add-on costs about 22% of image-making
time (roughly 75 s per picture) and 0.8 GB. Folding it into the model file is
worth doing.
**Side finding:** this run's first step took 322 s and prompt-reading took
128 s, because an earlier test had pushed the model files out of the system's
memory and they had to be re-read from this machine's slow disk (about
30 MB per second). A cold start costs minutes here. On a fast laptop SSD it
should be seconds, but this is the price of the disk layer and needs measuring
on the laptop.

### E2 — Two steps instead of four (theory 4)

No ready-made 2-step add-on exists, so run the 4-step add-on at 2 steps.
**Prediction:** image-making takes about 176 s (half). Picture quality:
unknown; my guess is visibly softer but recognisable.
**Result:** in flight.

### E3 — Turn the result into a picture in one piece instead of 9 tiles

**Prediction:** this stage drops from 69 s to 25–40 s; held memory rises to
3–4 GB during it.
**Result:** queued after E2.

**Queued next:** E4 work at 384x384 (theory 5); E5 the engine's built-in step reuse (theory 3).
