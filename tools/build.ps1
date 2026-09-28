param(
  [switch]$Setup,
  [string]$Port = "",
  [string]$ArduinoCli = ""
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
Set-Location -LiteralPath $repo
if (-not $ArduinoCli) {
  $command = Get-Command arduino-cli -ErrorAction SilentlyContinue
  if ($command) { $ArduinoCli = $command.Source }
  else { $ArduinoCli = 'C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe' }
}
if (-not (Test-Path -LiteralPath $ArduinoCli)) { throw 'Install Arduino CLI 1.5.1 or provide -ArduinoCli path.' }
function Invoke-Arduino {
  & $ArduinoCli @args
  if ($LASTEXITCODE -ne 0) { throw "Arduino CLI failed with exit code $LASTEXITCODE" }
}
if ($Setup) {
  Invoke-Arduino core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
  Invoke-Arduino core install esp32:esp32@3.3.11 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
  Invoke-Arduino lib install 'Adafruit DotStar@1.2.5' 'Adafruit NeoPixel@1.15.5' 'Adafruit BusIO@1.17.4'
}
$fqbn = 'esp32:esp32:adafruit_qtpy_esp32s3_nopsram:PartitionScheme=default_8MB,USBMode=hwcdc,CDCOnBoot=cdc,EraseFlash=none'
New-Item -ItemType Directory -Force .build/release | Out-Null
Invoke-Arduino compile --jobs 4 --fqbn $fqbn --warnings all --build-path .build/firmware --output-dir .build/release code/POV4.1
if ($Port) {
  Invoke-Arduino upload --fqbn $fqbn --port $Port --input-dir .build/release code/POV4.1
}
Write-Host 'OTA application image: .build/release/POV4.1.ino.bin (NOT merged.bin or bootloader.bin)'
