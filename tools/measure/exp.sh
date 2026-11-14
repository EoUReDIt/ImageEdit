#!/bin/bash
# exp.sh <name> <params-backend> [extra sd-cli args...]   (256x256, 1 step, 1 reference)
name=$1; pb=$2; shift 2
M=/home/user/models; cd /home/user/upstream/sdcpp
python3 "$(dirname "$0")/memrun.py" /home/user/exp/$name ./build/bin/sd-cli \
 --diffusion-model $M/qwen-image-edit-2511-Q2_K.gguf --vae $M/qwen_image_vae.safetensors \
 --llm ${LLM:-$M/Qwen2.5-VL-7B-Instruct.Q3_K_S.gguf} --llm_vision $M/Qwen2.5-VL-7B-Instruct.mmproj-Q8_0.gguf \
 --backend cpu --params-backend "$pb" --steps ${STEPS:-1} --cfg-scale 1.0 --sampling-method euler --flow-shift 3 \
 -W ${SIZE:-256} -H ${SIZE:-256} --model-args qwen_image_zero_cond_t=true -s 42 \
 -p "${PROMPT:-change the text on the sign to 'hello'}" -r assets/flux/flux1-dev-q8_0.png -o /home/user/exp/$name/result.png -v "$@"
