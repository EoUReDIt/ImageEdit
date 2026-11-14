# Roadmap: the theory map, A to G

Written in plain language. Each stop has a claim, how we test it, and where it
stands. Status words: **confirmed** (measured), **partly** (some evidence),
**open** (not tested yet).

## Guiding thesis

The whole model never needs to *stay* loaded in graphics memory or system
memory. Each layer of the machine should be filled only to the point where
speed and efficiency are balanced, leaving room for Windows and other programs.
The disk sits behind them as overflow.

One hard limit to design around: every pass through the model still has to
*touch* every weight once. So speed depends on (1) how much work each pass
gets done and (2) how fast the slowest layer can feed weights. Image models
are the favourable case: one pass handles the whole picture, thousands of
tokens at once. A chat model produces one token per pass, so it would need to
skip weights or batch tokens to benefit the same way.

## The layers

| Machine type | Layer 1 (fastest) | Layer 2 | Layer 3 (overflow) |
|---|---|---|---|
| Has a graphics card | Graphics memory | System memory | Fast disk (NVMe) |
| Laptop with built-in graphics | Processor (and built-in graphics) | System memory | Fast disk (NVMe) |

Scale goal: model size should grow in step with the machine, for example an
8-billion-parameter model on 16 GB and a 64-billion-parameter model on 64 GB.

## The stops

**A. Take turns.** The stage that reads the prompt and pictures, and the stage
that makes the image, are never loaded together.
*Status: confirmed.* The first stage peaks, lets go, then the second starts.

**B. Read in place, don't copy.** The program works directly from the model
file on disk. The system keeps in memory whatever fits and drops the rest.
*Status: confirmed on the processor path.* Memory truly held fell from 9.0 GB
to 1.9 GB at full size, with an identical image and a faster run.

**C. Balanced fill.** *(Parked 2026-11-14: the system's own caching held up
in Sprint 2; revisit only if the laptop shows thrashing.)* Instead of leaving it to the system, the program decides
how full each layer should be (for example 60–70% of system memory), and
fetches the next block from disk while working on the current one.
*Status: open.* Needs new code. First evidence comes from the memory-pressure
test in Sprint 2.

**D. Three layers on graphics hardware.** *(One Iris Xe test on the laptop
first; Windows limits a built-in graphics chip to half of system memory.)* Graphics memory holds only the
blocks in use, fed from system memory, fed from disk. On built-in graphics
(shared memory), let the graphics chip work on the mapped file directly so
there is no second copy.
*Status: open.* The graphics-card pieces exist in the underlying program but
are unmeasured; nobody on the project has tested on a graphics card yet.

**E. Load less.** Fold the 4-step add-on into the model file ahead of time,
use a lighter picture decoder, shrink reference pictures, use a more
compressed copy of the same prompt-reading model. (Correction, 2026-11-14: a
*different, smaller* prompt-reader will not work, because the image model was
trained on what Qwen2.5-VL-7B produces.)
*Status: open.* Each is a separate small experiment.

**F. Go faster.** Fewer tokens per picture, reuse work between steps, save the
prompt-reading result to disk so repeats cost nothing, tune processor threads,
use the graphics chip where it helps.
*Status: open.* Only after memory is settled.

**G. The product.** A simple screen: one subject photo, tagged reference
pictures (scene, outfit, pose), a prompt, one result. A gauge shows how full
each layer is and what the machine is doing.
*Status: open.*

## Owner's claims under test

1. The hardware was never the problem. *Memory: supported (Sprint 2). Speed: under test (Sprint 3).*
2. The problem was access to expertise: the fixes exist but are locked behind
   settings and source code. *Supported so far: the Sprint 2 fix was an
   existing, undocumented combination of settings.*
3. Claim 1 holds for speed too. Split into three on 2026-11-14 after outside
   review 1, because "the hardware" means three different machines:
   - **3a Desktop:** the 20B model, or larger, at good quality in a usable
     time on a desktop with a graphics card. *Open: no graphics card tested.*
   - **3b Current laptop:** a good multi-reference edit in under 2 minutes on
     the i5-1135G7. *Open. Unlikely with 20B at 4 steps; a smaller model
     (FLUX.2 [klein] 4B) is the candidate route.*
   - **3c Cloud API:** 20B quality in seconds through a hosted API behind the
     same simple screen. *Open; previously parked, now back on the map.*

All predictions are estimates. We confirm a route only by building it or
travelling it. Ledger: `docs/predictions.md`.

## Speed theories (stop F, expanded)

1. The reference picture is recomputed at every step though it never changes.
2. The whole picture is recomputed when only a region changes.
3. Every step runs in full even when little changes between steps.
4. Four steps may be more than needed.
5. Work is done at full size that could be done small, then enlarged.
6. The 4-step add-on is applied on the fly at every step.
7. The built-in graphics chip sits idle during a processor run.
8. Reading the prompt and pictures is repeated on every run.

## Sprints

| Sprint | Goal | Who | Status |
|---|---|---|---|
| 1 | Does it run on the low-end laptop at all, and how long does it take? | Owner | Waiting for the laptop test |
| 2 | Bring memory down | Claude | Done, see `docs/sprints/sprint-2.md` |
| 3 | Speed (stop F): about 9 min to under 2 min | Claude | In progress, see `docs/sprints/sprint-3.md` |
| 4 | Balanced fill (stop C) and load less (stop E) | Claude | Not started |
