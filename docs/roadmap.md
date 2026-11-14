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

**C. Balanced fill.** Instead of leaving it to the system, the program decides
how full each layer should be (for example 60–70% of system memory), and
fetches the next block from disk while working on the current one.
*Status: open.* Needs new code. First evidence comes from the memory-pressure
test in Sprint 2.

**D. Three layers on graphics hardware.** Graphics memory holds only the
blocks in use, fed from system memory, fed from disk. On built-in graphics
(shared memory), let the graphics chip work on the mapped file directly so
there is no second copy.
*Status: open.* The graphics-card pieces exist in the underlying program but
are unmeasured; nobody on the project has tested on a graphics card yet.

**E. Load less.** Fold the 4-step add-on into the model file ahead of time,
use a lighter picture decoder, shrink reference pictures, use a smaller
prompt-reading model.
*Status: open.* Each is a separate small experiment.

**F. Go faster.** Fewer tokens per picture, reuse work between steps, save the
prompt-reading result to disk so repeats cost nothing, tune processor threads,
use the graphics chip where it helps.
*Status: open.* Only after memory is settled.

**G. The product.** A simple screen: one subject photo, tagged reference
pictures (scene, outfit, pose), a prompt, one result. A gauge shows how full
each layer is and what the machine is doing.
*Status: open.*

## Sprints

| Sprint | Goal | Who | Status |
|---|---|---|---|
| 1 | Does it run on the low-end laptop at all, and how long does it take? | Owner | Waiting for the laptop test |
| 2 | Bring memory down | Claude | Done, see `docs/sprints/sprint-2.md` |
| 3 | Balanced fill (stop C) and load less (stop E) | Claude | Not started |
| 4 | Speed (stop F) | Both | Not started |
