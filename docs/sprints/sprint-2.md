# Sprint 2 report: bring memory down

Date: 2026-11-14. Plain-language summary first, numbers after.

## In one paragraph

The program was holding the entire 7.4 GB edit model in memory the whole time
it was making an image. By switching one setting so that it reads the model
directly from the file on disk instead of making its own copy, the memory it
truly holds dropped from 9.0 GB to 1.9 GB. The picture came out identical, and
the run got faster, not slower. It kept working at normal speed even when most
of the machine's memory was deliberately taken by another program.

## What "memory truly held" means

There are two kinds of memory use:

- **Held**: the program's own working memory. The system cannot take it back.
  This is what crowds out Windows and your other programs.
- **Borrowed**: pieces of the model file the system is keeping handy because
  there is spare room. The system takes these back the moment anything else
  needs the space, and re-reads them from disk later if needed.

The goal is to make "held" small. "Borrowed" can be as large as the spare room
allows; that is the balanced fill described in the roadmap.

## Results

All tests ran in the cloud machine: 4 processor cores, 15 GB memory, no
graphics card. Same model files as the laptop test.

**Where the memory goes, stage by stage** (small test picture, 1 step):

| Stage | All copied into memory | "Disk" setting | Read in place |
|---|---|---|---|
| Reading the prompt and picture | 4.4 GB | 4.1 GB | 0.4 GB |
| Making the image | 11.6 GB | 7.8 GB | 0.6 GB |
| Turning the result into a picture | 12.0 GB | 1.0 GB | 1.0 GB |
| **Peak held** | **12.0 GB** | **7.8 GB** | **1.0 GB** |

**Full-size test** (512x512, 4 steps, with the 4-step add-on):

| | Before (Sprint 1 settings) | After (read in place) | Predicted |
|---|---|---|---|
| Peak memory held | 9.0 GB | 1.9 GB | 1.5–2.5 GB |
| Total time | 628 s | 532 s | no worse than 628 s |
| Output picture | — | identical, byte for byte | — |

**Under memory pressure** (another program holding 9.9 GB of the 15 GB):

| | Normal | Under pressure | Predicted |
|---|---|---|---|
| Finished? | yes | yes | yes |
| Peak held | 1.0 GB | 1.0 GB | about 1 GB |
| Borrowed | 11.0 GB | 3.9 GB | — |
| Image-making time | 23 s | 25 s | 100–250 s |
| Output picture | — | identical | — |

## What we learned

1. **The stages already take turns** (roadmap stop A). Confirmed.
2. **Reading in place works** (stop B). The earlier "disk" setting sounded
   right but still made a private copy of the model. Reading in place only
   happens when the weights are assigned to the processor *and* file mapping
   is switched on. That combination is the new default for processor runs in
   the test script.
3. **My slowdown prediction was wrong, in the good direction.** I expected the
   pressure test to be 4–10 times slower because this machine's disk looked
   slow. It was barely slower. I do not know why for certain; the likeliest
   reason is that the disk here is faster on repeat reads than on first reads.
   So this test proves the system can run in a small memory footprint, but it
   does not tell us how a genuinely slow disk behaves.

## What is not proven yet

- **Windows.** All of this was measured on Linux. Windows has the same
  mechanism, but it is untested until the laptop run.
- **Graphics chips.** Reading in place currently works only for the processor.
  A graphics chip still gets its own copy. Roadmap stop D covers this.
- **Deliberate control.** Right now the system decides what to keep and drop.
  Roadmap stop C is about the program choosing a fill level on purpose.
- **The 4-step add-on** is applied on the fly and adds roughly 0.9 GB held and
  some time per step. Folding it into the model file ahead of time should
  remove both (stop E).

## What this means for the laptop test

Use the processor run first. The script now picks the low-memory mode by
itself:

```bash
powershell -ExecutionPolicy Bypass -File run-theory.ps1 -Device cpu -Lora Qwen-Image-Edit-2511-Lightning-4steps-V1.0-bf16
```

Prediction for the 16 GB laptop, fixed now: it completes, Task Manager shows
the program itself using about 2 GB, and total time is 15–25 minutes.
