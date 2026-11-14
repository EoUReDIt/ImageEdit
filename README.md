# ImageEdit

Experimental local image-editing app: one subject photo, tagged reference
images (scene, outfit, pose), a prompt, one output image. The goal is to run a
large editing model on low-end PCs.

- `docs/runtime-design.md` — design, feasibility numbers, model swapping, legal notes
- `runtime/streambench/` — Vulkan benchmark that tests the streaming design on your GPU
- `HANDOFF.md` — current state and next steps
