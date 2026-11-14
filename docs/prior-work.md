# Prior work check: has someone already proven it?

Rule: before testing a theory ourselves, look for someone who already did.
If they proved it, we borrow the result and test only what is new for us. If
they failed, we ask whether the theory was wrong or only the approach.

Checked 2026-11-14 by web search. Speed-up figures are each paper's own
claims, measured on big graphics cards with many steps (roughly 28–50), not on
a 4-step model on a processor. That gap is what we still have to test.

| Our theory | Already shown by others? | Who / what | What is still ours to test |
|---|---|---|---|
| 1. Reuse the reference picture's work across steps | **Yes** | RegionE: stores the reference's attention data once and reuses it; reports 2.06x on Qwen-Image-Edit. SpotEdit: reference caching alone lifted its gain from 1.24x to 1.95x | Does it still pay at 4 steps? It has to be built into our engine; it is not a setting |
| 2. Only compute the region that changes | **Yes** | SpotEdit (1.59x on Qwen-Image-Edit), RegionE, CacheEdit (also reports fewer unwanted changes) | Same as above. Finding the changed region needs a first step or two, which is half of a 4-step run |
| 1b. Drop unimportant reference tokens | **Yes** | ToPi: over 30% faster on Qwen-Image-Edit | Simpler to build than 1 or 2; good first patch |
| 3. Reuse work between steps | **Yes, for many-step runs** | FreqCa (claims 6–7x), and the cache modes already inside our engine | Likely weak at 4 steps: there are few steps to skip. One quick run settles it |
| 4. Fewer than 4 steps | **No release found** | Only 4-step and 8-step add-ons exist for this model | Cheap to try: run the 4-step add-on at 2 steps and look at the picture |
| 5. Work small, then enlarge | Common practice | Many workflows | One run at smaller size plus an enlarger |
| 6. Fold the add-on into the model file | **Yes, files exist** | lightx2v publishes a ready-made merged file (20.6 GB, 8-bit) | Needs shrinking to 2-bit; cloud disk space is tight |
| 7. Use the idle graphics chip | Engine supports it | Vulkan backend | Only the laptop can test this |
| 8. Save prompt-reading results | Partly | Engine has an in-memory cache; nothing saved to disk | Small patch |
| Processor-only timing for this model | **No one found** | Nearest: 4 min 50 s on a 6 GB graphics card for a related model | Our numbers appear to be new |

## What this changes

- Theories 1 and 2 are not open questions any more: others showed they work,
  with roughly 1.6–2x gains. Our open question is narrower: do they survive
  when there are only 4 steps?
- "The theory was right, only the approach was wrong" applies to theory 3:
  between-step reuse is proven, but it was designed for 30–50 steps. With 4
  steps the same idea has to be applied inside a step (reference reuse,
  region skipping) instead of between steps.
- Order of work now: quick zero-code runs first (6, 4, 5, 3), then build 1b,
  then 1 and 2.

## Sources

- [RegionE: Adaptive Region-Aware Generation for Efficient Image Editing](https://arxiv.org/html/2510.25590v1)
- [SpotEdit: selective region editing in diffusion transformers](https://arxiv.org/pdf/2512.22323v1)
- [CacheEdit (ICML 2026 poster)](https://icml.cc/virtual/2026/poster/66154)
- [Token Pruning for In-Context Generation in Diffusion Transformers (ToPi)](https://arxiv.org/html/2602.01609v1)
- [FreqCa: Accelerating Diffusion Models via Frequency-Aware Caching](https://arxiv.org/html/2510.08669v1)
- [Qwen-Image-Edit-2511-Lightning](https://huggingface.co/lightx2v/Qwen-Image-Edit-2511-Lightning)
- [Can I Run Qwen-Image 2.1 Locally?](https://openclawdc.com/blog/can-i-run-qwen-image-2-1-locally/)

## Update after outside review 1 (2026-11-14)

- **Theory 2 parked.** RegionE and SpotEdit skip unchanged regions. In scene
  and pose swaps (H2–H4) almost the whole picture changes, so there is little
  to skip.
- **Theory 1b (ToPi) is the first patch to build.** It targets reference
  pictures, which is our multi-reference case. The reviewer also named
  "ToDoPro (Oct 2026)"; a search found no such paper, so it is treated as
  unverified. A related June 2026 paper exists:
  [Keep The Essentials: token dropping](https://arxiv.org/pdf/2606.23682).
- **New limit found:** Qwen-Image-Edit-2511 is reported reliable with only 2–3
  reference pictures ([UniRef-Image-Edit](https://arxiv.org/html/2602.14186v1)).
  H4 uses four.
- **Pose input:** since the 2509 release the model advertises built-in
  keypoint-map (pose skeleton) support
  ([model card](https://huggingface.co/Qwen/Qwen-Image-Edit-2509)).
- **Smaller-model route for claim 3b:** FLUX.2 [klein] 4B, Apache 2.0,
  multi-reference editing
  ([BFL](https://bfl.ai/blog/flux2-klein-towards-interactive-visual-intelligence)).
