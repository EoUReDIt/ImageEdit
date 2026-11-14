# Low-end test of the theory

Two tests. Test A needs no downloads beyond one small program. Test B runs the
real model. `run-theory.ps1` is untested (no Windows machine in the cloud
session); if it errors, send the error text.

See the step-by-step guide in the project chat or `HANDOFF.md`. Folder layout
for Test B is in the header of `run-theory.ps1`.

What you see while it runs: the model's own log scrolls live (loading, encoder,
a progress bar per denoising step with seconds per step, VAE decode). At the
end the script prints a summary and saves `log.txt`, `result.json` and
`result.png` under `C:\imageedit\results\<timestamp>\`.

Peak VRAM is sampled system-wide, so close games and browsers first. For a live
view open Task Manager > Performance > GPU > "Dedicated GPU memory".
A failed run is still a result: keep the log.
