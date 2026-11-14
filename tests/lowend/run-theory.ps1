# Runs the theory configuration on upstream stable-diffusion.cpp and records
# time and peak VRAM. UNTESTED: written without access to a Windows machine.
#
# Usage (PowerShell):
#   .\run-theory.ps1 -SdCli C:\sd\sd-cli.exe -Dit C:\models\qwen-image-edit-2511-Q2_K.gguf `
#     -Vae C:\models\qwen_image_vae.safetensors -Llm C:\models\Qwen2.5-VL-7B-Instruct-Q3_K_S.gguf `
#     -LlmVision C:\models\Qwen2.5-VL-7B-Instruct.mmproj-Q8_0.gguf `
#     -Subject subject.png -Refs outfit.png,scene.png -Prompt "the person from image 1 wearing the outfit from image 2 in the scene from image 3"
param(
  [Parameter(Mandatory)] [string]$SdCli,
  [Parameter(Mandatory)] [string]$Dit,
  [Parameter(Mandatory)] [string]$Vae,
  [Parameter(Mandatory)] [string]$Llm,
  [string]$LlmVision = "",
  [Parameter(Mandatory)] [string]$Subject,
  [string[]]$Refs = @(),
  [Parameter(Mandatory)] [string]$Prompt,
  [string]$Device = "vulkan0",     # see: sd-cli.exe --list-devices
  [double]$MaxVramGiB = 1.0,
  [int]$Size = 512,
  [int]$Steps = 4,
  [long]$Seed = 42,
  [string]$Out = "theory-out"
)
$ErrorActionPreference = "Stop"
New-Item -ItemType Directory -Force -Path $Out | Out-Null
$env:GGML_VK_REUSE_BUFFERS = "0"   # idle-buffer cache is outside --max-vram

$a = @(
  "--diffusion-model", $Dit, "--vae", $Vae, "--llm", $Llm,
  "--backend", "te=cpu,diffusion=$Device,vae=$Device",
  "--params-backend", "te=cpu,diffusion=cpu,vae=cpu",
  "--max-vram", "$MaxVramGiB",
  "--steps", "$Steps", "--cfg-scale", "1.0", "--sampling-method", "euler", "--flow-shift", "3",
  "-W", "$Size", "-H", "$Size", "--vae-tiling",
  "--model-args", "qwen_image_zero_cond_t=true",
  "-s", "$Seed", "-p", $Prompt, "-r", $Subject,
  "-o", (Join-Path $Out "result.png"), "-v"
)
if ($LlmVision) { $a += @("--llm_vision", $LlmVision) }
foreach ($r in $Refs) { $a += @("-r", $r) }

# Peak dedicated VRAM across all processes, sampled twice a second.
$sampler = Start-Job {
  $peak = 0
  while ($true) {
    try {
      $v = (Get-Counter '\GPU Adapter Memory(*)\Dedicated Usage' -ErrorAction Stop).CounterSamples |
           Measure-Object -Property CookedValue -Maximum
      if ($v.Maximum -gt $peak) { $peak = $v.Maximum; Write-Output $peak }
    } catch {}
    Start-Sleep -Milliseconds 500
  }
}
$sw = [Diagnostics.Stopwatch]::StartNew()
& $SdCli @a 2>&1 | Tee-Object -FilePath (Join-Path $Out "log.txt")
$code = $LASTEXITCODE
$sw.Stop()
$peak = (Receive-Job $sampler | Measure-Object -Maximum).Maximum
Stop-Job $sampler; Remove-Job $sampler

$result = [ordered]@{
  exit_code       = $code
  total_seconds   = [math]::Round($sw.Elapsed.TotalSeconds, 1)
  peak_vram_mb    = if ($peak) { [math]::Round($peak / 1MB) } else { $null }
  max_vram_gib    = $MaxVramGiB
  size            = $Size
  steps           = $Steps
  references      = $Refs.Count + 1
  pass_time_115s  = ($sw.Elapsed.TotalSeconds -le 115)
  pass_vram_1800  = if ($peak) { ($peak / 1MB) -le 1800 } else { $null }
}
$result | ConvertTo-Json | Set-Content (Join-Path $Out "result.json")
$result | Format-List
