# Runs the theory configuration on upstream stable-diffusion.cpp and prints a
# readable summary: time per phase, total time, peak VRAM, pass/fail.
# UNTESTED: written without access to a Windows machine.
#
# Expected layout (everything under one folder, default C:\imageedit):
#   sd\        unzipped stable-diffusion.cpp Vulkan build (contains sd-cli.exe)
#   models\    the DiT .gguf, the VAE .safetensors, the Qwen2.5-VL .gguf and its mmproj .gguf
#   images\    subject.png, plus optional ref1.png, ref2.png, ref3.png
#
# Run:  powershell -ExecutionPolicy Bypass -File run-theory.ps1
param(
  [string]$Root = "C:\imageedit",
  [string]$Prompt = "the person from image 1, photorealistic portrait, studio lighting",
  [string]$Device = "",            # blank = first Vulkan device from --list-devices
  [double]$MaxVramGiB = 1.0,
  # Where the model weights live:
  #   mapped = read in place from the model file (lowest memory; CPU runs only)
  #   disk   = re-read from the file on demand, private copy kept in memory
  #   cpu    = whole model copied into RAM
  # Blank = "mapped" for -Device cpu, "disk" for a graphics device.
  [ValidateSet("", "mapped", "disk", "cpu")] [string]$Weights = "",
  [string]$Lora = "",              # e.g. Qwen-Image-Edit-2511-Lightning-4steps-V1.0-bf16 (file in models\)
  [int]$Size = 512,
  [int]$Steps = 4,
  [long]$Seed = 42
)
$ErrorActionPreference = "Stop"
function Find-One($dir, $include, $exclude, $what) {
  $f = Get-ChildItem -Path $dir -Recurse -File -Include $include |
       Where-Object { -not $exclude -or $_.Name -notmatch $exclude } | Select-Object -First 1
  if (-not $f) { Write-Host "MISSING: $what (looked in $dir)" -ForegroundColor Red; exit 1 }
  Write-Host ("found {0,-16} {1}  ({2:N1} GB)" -f $what, $f.Name, ($f.Length / 1GB))
  $f.FullName
}
$SdCli  = Find-One "$Root\sd"     @("sd-cli.exe") $null "sd-cli.exe"
$Dit    = Find-One "$Root\models" @("*image*edit*.gguf") "Lightning" "edit model"
$Vae    = Find-One "$Root\models" @("*vae*.safetensors") $null "VAE"
$Vision = Find-One "$Root\models" @("*mmproj*.gguf") $null "vision projector"
$Llm    = Find-One "$Root\models" @("*VL*.gguf") "mmproj" "text encoder"
$Subject = Find-One "$Root\images" @("subject.*") $null "subject image"
$Refs = @(Get-ChildItem -Path "$Root\images" -File -Filter "ref*.*" | Sort-Object Name | ForEach-Object FullName)
Write-Host "reference images: $($Refs.Count)"

if (-not $Device) {
  $Device = (& $SdCli --list-devices 2>$null | Where-Object { $_ -match '^(?i)vulkan' } |
             Select-Object -First 1) -replace "\s.*$", ""
  if (-not $Device) { Write-Host "No Vulkan device found. Update your GPU driver." -ForegroundColor Red; exit 1 }
}
Write-Host "device: $Device   VRAM budget: $MaxVramGiB GiB   size: ${Size}x$Size   steps: $Steps`n"

$Out = Join-Path $Root ("results\" + (Get-Date -Format "yyyyMMdd-HHmmss"))
New-Item -ItemType Directory -Force -Path $Out | Out-Null
$Log = Join-Path $Out "log.txt"
$env:GGML_VK_REUSE_BUFFERS = "0"   # idle-buffer cache is outside --max-vram

if (-not $Weights) { $Weights = if ($Device -eq "cpu") { "mapped" } else { "disk" } }
if ($Weights -eq "mapped" -and $Device -ne "cpu") { Write-Host "mapped weights need -Device cpu" -ForegroundColor Red; exit 1 }
$pb = if ($Weights -eq "mapped") { "te=cpu,diffusion=cpu,vae=cpu" } else { "te=$Weights,diffusion=$Weights,vae=cpu" }
Write-Host "weights: $Weights"

$a = @(
  "--diffusion-model", $Dit, "--vae", $Vae, "--llm", $Llm, "--llm_vision", $Vision,
  "--backend", "te=cpu,diffusion=$Device,vae=$Device",
  "--params-backend", $pb,
  "--max-vram", "$MaxVramGiB",
  "--steps", "$Steps", "--cfg-scale", "1.0", "--sampling-method", "euler", "--flow-shift", "3",
  "-W", "$Size", "-H", "$Size", "--vae-tiling",
  "--model-args", "qwen_image_zero_cond_t=true",
  "-s", "$Seed", "-p", $Prompt, "-r", $Subject,
  "-o", (Join-Path $Out "result.png"), "-v"
)
foreach ($r in $Refs) { $a += @("-r", $r) }
if ($Weights -eq "mapped") { $a += "--mmap" }
if ($Lora) {
  $a += @("--lora-model-dir", "$Root\models")
  $i = [array]::IndexOf($a, "-p"); $a[$i + 1] = "$Prompt<lora:${Lora}:1>"
}

# Peak dedicated VRAM (all processes), sampled twice a second in the background.
$sampler = Start-Job {
  while ($true) {
    try {
      ((Get-Counter '\GPU Adapter Memory(*)\Dedicated Usage' -ErrorAction Stop).CounterSamples |
        Measure-Object -Property CookedValue -Maximum).Maximum
    } catch {}
    Start-Sleep -Milliseconds 500
  }
}
$sw = [Diagnostics.Stopwatch]::StartNew()
$ErrorActionPreference = "Continue"
& $SdCli @a 2>&1 | ForEach-Object { "$_" } | Tee-Object -FilePath $Log
$code = $LASTEXITCODE
$sw.Stop()
$samples = @(Receive-Job $sampler); Stop-Job $sampler; Remove-Job $sampler
$peakMB = if ($samples.Count) { [math]::Round((($samples | Measure-Object -Maximum).Maximum) / 1MB) } else { $null }

function Phase($pattern) {
  $m = Select-String -Path $Log -Pattern $pattern | Select-Object -Last 1
  if ($m) { [double]$m.Matches[0].Groups[1].Value } else { $null }
}
$result = [ordered]@{
  exit_code         = $code
  device            = $Device
  size              = $Size
  steps             = $Steps
  images            = $Refs.Count + 1
  max_vram_gib      = $MaxVramGiB
  weights           = $Weights
  lora              = $Lora
  encoder_seconds   = Phase 'get_learned_condition completed, taking ([\d.]+)s'
  denoise_seconds   = Phase 'sampling completed, taking ([\d.]+)s'
  vae_seconds       = Phase 'decode_first_stage completed, taking ([\d.]+)s'
  generate_seconds  = Phase 'generate_image completed in ([\d.]+)s'
  wall_seconds      = [math]::Round($sw.Elapsed.TotalSeconds, 1)
  peak_vram_mb      = $peakMB
  pass_time_115s    = ($code -eq 0 -and $sw.Elapsed.TotalSeconds -le 115)
  pass_vram_1800mb  = if ($peakMB) { $peakMB -le 1800 } else { $null }
}
$result | ConvertTo-Json | Set-Content (Join-Path $Out "result.json")
Write-Host "`n================ SUMMARY ================" -ForegroundColor Cyan
$result.GetEnumerator() | ForEach-Object { "{0,-18} {1}" -f $_.Key, $_.Value }
Write-Host "saved to $Out" -ForegroundColor Cyan
