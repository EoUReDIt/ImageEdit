#!/bin/bash
# hrun.sh <name> "<prompt>" <ref1> [ref2 ...]
# Multi-reference run at the quality settings: 512x512, 4 steps, Lightning
# 4-step add-on, read in place, untiled picture conversion. Env: SIZE, STEPS.
here="$(cd "$(dirname "$0")" && pwd)"; name=$1; prompt=$2; shift 2
M=/home/user/models; refs=(); for r in "$@"; do refs+=(-r "$r"); done
cd /home/user/upstream/sdcpp
python3 "$here/memrun.py" /home/user/exp/$name ./build/bin/sd-cli \
 --diffusion-model $M/qwen-image-edit-2511-Q2_K.gguf --vae $M/qwen_image_vae.safetensors \
 --llm $M/Qwen2.5-VL-7B-Instruct.Q3_K_S.gguf --llm_vision $M/Qwen2.5-VL-7B-Instruct.mmproj-Q8_0.gguf \
 --lora-model-dir $M --backend cpu --params-backend te=cpu,diffusion=cpu,vae=cpu --mmap \
 --steps ${STEPS:-4} --cfg-scale 1.0 --sampling-method euler --flow-shift 3 \
 -W ${SIZE:-512} -H ${SIZE:-512} --model-args qwen_image_zero_cond_t=true -s 42 \
 -p "$prompt<lora:Qwen-Image-Edit-2511-Lightning-4steps-V1.0-bf16:1>" "${refs[@]}" \
 -o /home/user/exp/$name/result.png -v
