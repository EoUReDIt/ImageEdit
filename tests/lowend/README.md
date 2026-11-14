# Low-end test of the theory on upstream stable-diffusion.cpp

Status: the script is untested (no Windows machine in the cloud session).

1. Download the Windows Vulkan build from the stable-diffusion.cpp releases page.
2. Download model files (links in upstream `docs/qwen_image_edit.md`): a
   Qwen-Image-Edit-2511 GGUF at the lowest quantization offered, the Qwen-Image
   VAE, a Qwen2.5-VL-7B GGUF and its vision projector (mmproj). A 4-step
   distilled variant or Lightning LoRA is needed for `--steps 4` to look right.
3. Find your device name:

```bash
sd-cli.exe --list-devices
```

4. Run `run-theory.ps1` (usage is in the file header).
5. Send back `theory-out/result.json` and `theory-out/log.txt`.

Peak VRAM is sampled system-wide, so close other GPU-heavy programs first.
A failed run is still a result: keep the log.
