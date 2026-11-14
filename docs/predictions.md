# Prediction ledger

Method: build to the theory, then measure. The arithmetic column is the
conventional expectation. A measurement that beats it is a discovery; one that
matches it confirms the conventional limit. Fill in "Measured" with the device
and date. Never edit a prediction after its test has run.

Reference case: 20B DiT, 60 blocks, 4 steps, 640x640 with one reference
(3200 tokens), 2 GB discrete GPU, PCIe 3.0, 16–32 GB RAM.

| # | Claim | Theory target | Arithmetic baseline | Measured |
|---|---|---|---|---|
| 1 | Peak VRAM for ring + activations | <= 1050 MB (1.8 GB with headroom) | ~240–410 MB | — |
| 2 | Quantized transfer cuts bus traffic | ~5.5 GB per step | 5.0 GB (2 bpw) – 10 GB (4 bpw) per step | — |
| 3 | RAM->VRAM throughput on the low-end card | not stated | ~3 GB/s (PCIe 3.0 x4) | — |
| 4 | Async ring hides transfer behind compute | ~100% hidden | up to 100% with a dedicated transfer queue; less on a shared queue | — |
| 5 | Sustained GPU arithmetic | ~6 TFLOPS needed for target | ~0.5–1.5 TFLOPS on 2 GB-class cards | — |
| 6 | Denoising phase (4 steps) | 55–85 s | ~350–1000 s | — |
| 7 | Encoder phase, cold | 12–18 s | ~60–180 s on a low-end CPU | — |
| 8 | Encoder phase, cached (seed change) | 0 s | ~0 s | — |
| 9 | VAE decode + sharpen | 5–10 s | not yet estimated | — |
| 10 | End to end | 75–115 s | ~7–20 min | — |
| 11 | Image quality at ~2 bpw is usable | yes | visible degradation below ~4 bpw | — |

Rows 1–6 are testable today with `runtime/streambench`. Rows 7–11 need the
real runtime.

## Results log

(append: date, device, command, output or JSON)
