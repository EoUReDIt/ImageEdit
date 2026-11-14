# Theory vs stable-diffusion.cpp (upstream)

Checked against leejet/stable-diffusion.cpp at commit `0f8a467` (2026-11-14) by
reading its docs and source and building `sd-cli` (CPU build) in the cloud
session. Nothing below has been run with a real model yet.

## Summary

Most of the five-point theory already exists upstream. The first real
experiment needs no new code: it is a specific set of flags on the upstream
Vulkan build. New code is needed for four gaps listed at the end.

## Point by point

| # | Theory | Upstream today | Evidence | Gap |
|---|---|---|---|---|
| 1 | Encoder and DiT never co-resident | Per-module backends (`--backend te=cpu,diffusion=vulkan0`) and per-module parameter placement; compute copies are released at module-run completion | `docs/backend.md`, `docs/performance.md` | Not yet confirmed by measurement that the encoder's RAM is released before the DiT loads |
| 1b | Cache conditioning by content hash | In-memory LRU cache keyed on prompt + reference images + settings (`--conditioning-cache-size`). Default 0 in `sd-cli`, 4 in `sd-server` | `src/conditioning/conditioning_cache.h`, `docs/caching.md` | Lost when the process exits; exact-match only (a prompt change re-runs the whole encoder) |
| 2 | Weights quantized in RAM, dequantized on GPU | Vulkan matmul shaders consume quantized weights directly, including `Q2_K`, `Q3_K`, `Q4_K`, `IQ2_XS`, `IQ2_XXS`, `IQ3_S` | `ggml/src/ggml-vulkan/vulkan-shaders/mul_mmq*.comp`, `dequant_*.comp` | None found |
| 2b | Weights resident in RAM or on disk | `--params-backend cpu` (RAM) or `disk` (re-read from file); `--mmap` | `docs/performance.md` | None found |
| 3 | Async ring: copy next block while computing current | Graph is cut into segments; current segment pinned, next segment prefetched asynchronously; separate Vulkan transfer queue with semaphores | `src/model_manager_prefetch.cpp`, `ggml-vulkan.cpp` (`async_use_transfer_queue`) | Transfer queue is on by default only for discrete NVIDIA and newer AMD; async is disabled on some older Intel GPUs |
| 3b | Fixed VRAM envelope | `--max-vram <GiB>` managed budget | `docs/backend.md` | It is a managed budget, not a physical cap: a 512 MiB scratch reserve, 128 MiB planning headroom and an idle-buffer cache (up to 1 GiB, off with `GGML_VK_REUSE_BUFFERS=0`) sit outside it. Whether 1.8 GB physical is reachable is unmeasured |
| 4 | 4-step distilled, 512–640², tiled VAE | `--steps 4 --cfg-scale 1`, LoRA support, `--vae-tiling` | `sd-cli --help` | User-supplied model files; no code gap |
| 4b | Fewer DiT passes | Step-caching modes for DiT (`easycache`, `dbcache`, `taylorseer`, `cache-dit`) | `docs/caching.md` | Untested at 4 steps |
| 5 | Phase / VRAM / bus telemetry, asserted budgets | Debug log lines only | `LOG_DEBUG` in prefetch and execution engine | No structured output, no assertions |

## What this changes

- `runtime/streambench` remains useful as an isolated measurement of bus and
  arithmetic throughput, but the real test is now upstream with a real model.
- The prediction ledger rows 1–11 can all be tested on the low-end PC with the
  upstream Windows Vulkan release and `tests/lowend/run-theory.ps1`.
- `sd-server` (HTTP API, conditioning cache on by default) is the natural back
  end for the ImageEdit UI: a web page talking to a local server.

## Gaps to build (the actual extension work)

1. **Persistent conditioning cache.** Save encoder output to disk keyed by a
   hash of encoder file + prompt + reference images + settings, so a seed
   change costs 0 s even across restarts.
2. **Prefix reuse on prompt change.** Keep the encoder's key/value state for
   the ordered image set so only the new text tokens are re-encoded.
3. **Telemetry.** Structured per-phase timing, peak VRAM, bytes transferred and
   transfer rate, with pass/fail against the budget, readable by the UI.
4. **Physical VRAM cap.** Measure the unmanaged overhead on a 2 GB card, then
   tune or patch the reserves so the physical peak stays under 1.8 GB.
5. **UI.** Subject slot, tagged references, prompt, telemetry bar, model picker.

Changes to upstream will live in this repo as patches against the pinned
commit (`patches/`), because the session can only push to this repository.
