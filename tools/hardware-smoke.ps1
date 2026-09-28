param(
  [string]$ReturnProfile = '',
  [string]$Address = '192.168.4.1',
  [string]$StaffSSID = 'POVSTAFFFA34',
  [string]$Interface = 'Wi-Fi',
  [string]$PythonExe = '',
  [string]$HostAddress = '',
  [switch]$BrowserOTA,
  [switch]$ArduinoOTA,
  [switch]$Fallback
)
# Explicit opt-in bench tool. AP tests temporarily switch PC Wi-Fi, then restore it.
# Never formats storage or removes preexisting images. -Fallback temporarily
# changes staff Wi-Fi settings and requires an original AP-only configuration.
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
Set-Location -LiteralPath $repo
$profileName = 'POV-test-' + [guid]::NewGuid().ToString('N').Substring(0,8)
$profilePath = Join-Path $repo '.build/ap-test.xml'
$reportPath = Join-Path $repo '.build/hardware-smoke.json'
$testName = 'qa-' + [guid]::NewGuid().ToString('N').Substring(0,8) + '.bmp'
$headers = @{'X-POV-Request'='1'}
$base = 'http://' + $Address
$switchWifi = $Address -eq '192.168.4.1'
if ($switchWifi -and !$ReturnProfile) { throw 'Specify -ReturnProfile before running an AP test' }
if ($Fallback -and !$switchWifi) { throw 'Fallback testing requires a direct AP connection' }
$report = [ordered]@{started=[DateTime]::UtcNow.ToString('o'); testFile=$testName; checks=@(); initial=$null; initialFiles=$null; final=$null; error=$null}
function Record([string]$message) { $report.checks += $message; Write-Host $message }
function Api([string]$path, [string]$method='GET', $body=$null) {
  $args = @{Uri=($base+$path); Method=$method; Headers=$headers; TimeoutSec=3; NoProxy=$true}
  if ($null -ne $body) { $args.Body=$body; $args.ContentType='application/x-www-form-urlencoded' }
  Invoke-RestMethod @args
}
function RawUpload([string]$path,[string]$file,[string]$name) {
  $uploadHeaders=@{'X-POV-Request'='1';'X-POV-Filename'=[Uri]::EscapeDataString($name);'X-POV-Size'=(Get-Item -LiteralPath $file).Length.ToString()}
  Invoke-RestMethod -Uri ($base+$path) -Method Post -Headers $uploadHeaders -ContentType 'application/octet-stream' -InFile $file -TimeoutSec 35 -NoProxy
}
function WaitStaff([int]$seconds=12) {
  $watch=[Diagnostics.Stopwatch]::StartNew()
  do {
    try { return Api '/api/status' } catch { Start-Sleep -Milliseconds 250 }
  } while ($watch.Elapsed.TotalSeconds -lt $seconds)
  throw 'Staff did not answer within the connection window'
}
function ConnectStaffAP {
  # Windows can retain the previous WPA/open scan entry across a reboot.
  # Refresh the visible network list before requesting this profile.
  & netsh wlan show networks mode=bssid | Out-Null
  & netsh wlan connect "name=$profileName" "interface=$Interface" | Out-Null
  if ($LASTEXITCODE) { throw 'Cannot connect to staff AP' }
}
$escapedSSID=[Security.SecurityElement]::Escape($StaffSSID)
$xml=@"
<?xml version="1.0"?><WLANProfile xmlns="http://www.microsoft.com/networking/WLAN/profile/v1"><name>$profileName</name><SSIDConfig><SSID><name>$escapedSSID</name></SSID></SSIDConfig><connectionType>ESS</connectionType><connectionMode>manual</connectionMode><MSM><security><authEncryption><authentication>open</authentication><encryption>none</encryption><useOneX>false</useOneX></authEncryption></security></MSM></WLANProfile>
"@
if ($switchWifi) { $xml | Set-Content -LiteralPath $profilePath -Encoding utf8 }
$uploaded=$false
$wifiTestChanged=$false
try {
  if ($switchWifi) {
    & netsh wlan add profile "filename=$profilePath" user=current | Out-Null
    if ($LASTEXITCODE) { throw 'Cannot add temporary AP connection profile' }
    ConnectStaffAP
  }
  $report.initial=WaitStaff
  Record ('Dashboard answered at '+$Address+', firmware '+$report.initial.version)
  $unauth=Invoke-WebRequest -Uri ($base+'/api/status') -TimeoutSec 3 -NoProxy -SkipHttpErrorCheck
  if ($unauth.StatusCode -ne 200 -or $unauth.Headers.ContainsKey('WWW-Authenticate')) { throw 'Dashboard still requires authentication' }
  Record 'Status available without login'
  $csrf=Invoke-WebRequest -Uri ($base+'/api/mode') -Method Post -Body @{name='off'} -TimeoutSec 3 -NoProxy -SkipHttpErrorCheck
  if ($csrf.StatusCode -ne 403) { throw 'Missing mutation header was not rejected' }
  Record 'Mutation without request header rejected'
  if ($BrowserOTA) {
    RawUpload '/api/ota' (Join-Path $repo '.build/release/POV4.1.ino.bin') 'POV4.1.ino.bin' | Out-Null
    Record 'Browser OTA accepted and validated application image'
    Start-Sleep -Seconds 3
    if ($switchWifi) { ConnectStaffAP }
    $report.final=WaitStaff 15
    Record ('Firmware returned after browser OTA: '+$report.final.version)
  }
  Api '/api/mode' 'POST' @{name='off'} | Out-Null
  Api '/api/brightness' 'POST' @{value='1'} | Out-Null
  foreach ($mode in @('solid','sword','flashlight','walking_stick','rainbow','breathing','chase','led_test','pov','idle','off')) {
    Api '/api/mode' 'POST' @{name=$mode} | Out-Null
    $status=Api '/api/status'
    if ($status.engine.requestedMode -ne $mode -or !$status.engine.locked) { throw "Mode/lock failed: $mode" }
  }
  Record 'All manual modes accepted, locked, at brightness 1; ended Off'
  if ($report.initial.storage.mounted) {
    $before=Api '/api/files'
    $report.initialFiles=$before.files
    RawUpload '/api/files/upload' (Join-Path $repo 'images/rg-lines.bmp') $testName | Out-Null
    $uploaded=$true
    $after=Api '/api/files'
    $entry=$after.files | Where-Object name -EQ $testName
    if (!$entry -or $after.count -ne $before.count+1) { throw 'Uploaded image missing from catalogue' }
    Record 'BMP uploaded, validated, and discovered without reboot'
    $replacement=Join-Path $repo 'images/by-triangles.bmp'
    RawUpload '/api/files/upload' $replacement $testName | Out-Null
    $entry=(Api '/api/files').files | Where-Object name -EQ $testName
    if ($entry.bytes -ne (Get-Item -LiteralPath $replacement).Length) { throw 'Valid replacement did not publish' }
    Record 'Existing BMP replaced successfully through atomic rename'
    $badPath=Join-Path $repo '.build/invalid.bmp'
    [IO.File]::WriteAllBytes($badPath,[byte[]]@(0,1,2,3,4,5))
    $rejected=$false
    try { RawUpload '/api/files/upload' $badPath $testName | Out-Null } catch { $rejected=$true }
    if (!$rejected) { throw 'Invalid replacement was accepted' }
    $remaining=(Api '/api/files').files | Where-Object name -EQ $testName
    if (!$remaining -or $remaining.bytes -ne $entry.bytes) { throw 'Invalid replacement lost original file' }
    Record 'Malformed replacement rejected; original test image retained'
    # Advertise a complete replacement but close after a short prefix.
    $body=[IO.File]::ReadAllBytes($replacement)
    $wireHeaders="POST /api/files/upload HTTP/1.1`r`nHost: $Address`r`nX-POV-Request: 1`r`nX-POV-Filename: $testName`r`nX-POV-Size: $($body.Length)`r`nContent-Type: application/octet-stream`r`nContent-Length: $($body.Length)`r`nConnection: close`r`n`r`n"
    $tcp=[Net.Sockets.TcpClient]::new($Address,80)
    try {
      $stream=$tcp.GetStream(); $request=[Text.Encoding]::ASCII.GetBytes($wireHeaders)
      $stream.Write($request,0,$request.Length); $stream.Write($body,0,100); $stream.Flush()
    } finally { $tcp.Close() }
    $afterAbort=WaitStaff 8
    $remaining=(Api '/api/files').files | Where-Object name -EQ $testName
    if ($afterAbort.engine.maintenance -or !$remaining -or $remaining.bytes -ne $entry.bytes) { throw 'Aborted replacement did not recover safely' }
    Record 'Disconnected replacement recovered; original image retained and maintenance ended'
    Api ('/api/files?name='+$testName) 'DELETE' | Out-Null
    $uploaded=$false
    if ((Api '/api/files').count -ne $before.count) { throw 'Test file deletion failed' }
    Record 'Test file deleted; original library count preserved'
  } else { Record 'Storage unavailable: skipped file tests; no automatic format' }
  Api '/api/brightness' 'POST' @{value=$report.initial.engine.brightness.ToString()} | Out-Null
  $invalidFirmware=Join-Path $repo '.build/invalid-firmware.bin'
  [IO.File]::WriteAllBytes($invalidFirmware,[byte[]]::new(64))
  $rejected=$false
  try { RawUpload '/api/ota' $invalidFirmware 'invalid.bin' | Out-Null } catch { $rejected=$true }
  if (!$rejected -or (Api '/api/status').engine.maintenance) { throw 'Invalid OTA failed to recover' }
  Record 'Invalid firmware rejected without reboot or stuck maintenance state'
  # A valid image prefix must not finalize the inactive slot on disconnect.
  $firmwarePath=Join-Path $repo '.build/release/POV4.1.ino.bin'
  $firmwareBytes=[IO.File]::ReadAllBytes($firmwarePath)
  $beforeAbort=Api '/api/status'
  $wireHeaders="POST /api/ota HTTP/1.1`r`nHost: $Address`r`nX-POV-Request: 1`r`nX-POV-Filename: POV4.1.ino.bin`r`nX-POV-Size: $($firmwareBytes.Length)`r`nContent-Type: application/octet-stream`r`nContent-Length: $($firmwareBytes.Length)`r`nConnection: close`r`n`r`n"
  $tcp=[Net.Sockets.TcpClient]::new($Address,80)
  try {
    $stream=$tcp.GetStream(); $request=[Text.Encoding]::ASCII.GetBytes($wireHeaders)
    $stream.Write($request,0,$request.Length); $stream.Write($firmwareBytes,0,8192); $stream.Flush()
  } finally { $tcp.Close() }
  $afterAbort=WaitStaff 8
  if ($afterAbort.engine.maintenance -or $afterAbort.version -ne $beforeAbort.version -or $afterAbort.uptime -lt $beforeAbort.uptime) { throw 'Interrupted firmware upload failed to recover without reboot' }
  Record 'Interrupted firmware upload recovered without finalizing or rebooting'
  if ($Fallback) {
    if ($report.initial.network.mode -ne 'ap' -or $report.initial.network.savedSSID) { throw 'Fallback test requires original AP mode with no saved home credentials' }
    Api '/api/wifi' 'POST' @{mode='sta';ssid='POV-QA-NETWORK-NOT-PRESENT';password='temporary-test-password'} | Out-Null
    $wifiTestChanged=$true
    $pending=(Api '/api/status').network
    if ($pending.mode -ne 'ap' -or $pending.savedMode -ne 'sta' -or !$pending.pendingReboot) { throw 'Wi-Fi save changed the live mode or failed to persist' }
    Record 'Client selection saved without changing active AP'
    Api '/api/reboot' 'POST' @{} | Out-Null
    Start-Sleep -Seconds 12
    ConnectStaffAP
    $fallbackState=WaitStaff 12
    if (!$fallbackState.network.fallback -or $fallbackState.network.mode -ne 'sta') { throw 'Missing home network did not enable AP fallback' }
    Record 'Reboot applied client mode; missing network enabled AP fallback'
    Api '/api/wifi' 'POST' @{mode='ap';forget='1'} | Out-Null
    Api '/api/reboot' 'POST' @{} | Out-Null
    Start-Sleep -Seconds 3
    ConnectStaffAP
    $restored=WaitStaff 12
    if ($restored.network.mode -ne 'ap' -or $restored.network.savedSSID -or $restored.network.pendingReboot) { throw 'Factory AP settings were not restored' }
    $wifiTestChanged=$false
    Record 'Restored AP default and removed test client credentials'
  }
  if ($ArduinoOTA) {
    $otaTools=Join-Path $env:LOCALAPPDATA 'Arduino15/packages/esp32/hardware/esp32/3.3.11/tools'
    $otaArgs=@('-i',$Address,'-p','3232','-f','.build/release/POV4.1.ino.bin')
    if ($HostAddress) { $otaArgs+=@('-I',$HostAddress,'-P','32320') }
    if ($PythonExe) { & $PythonExe (Join-Path $otaTools 'espota.py') @otaArgs *> .build/arduino-ota.log }
    else { & (Join-Path $otaTools 'espota.exe') @otaArgs *> .build/arduino-ota.log }
    if ($LASTEXITCODE) { throw 'Arduino OTA failed; see .build/arduino-ota.log' }
    Record 'Arduino OTA transport completed successfully'
    Start-Sleep -Seconds 3
    if ($switchWifi) { ConnectStaffAP }
    $report.final=WaitStaff 15
    Record ('Firmware returned after Arduino OTA: '+$report.final.version)
  }
  Api '/api/mode' 'POST' @{name='off'} | Out-Null
  $report.final=Api '/api/status'
} catch {
  $report.error=$_.Exception.Message
  Write-Warning $report.error
} finally {
  if ($uploaded) { try { Api ('/api/files?name='+$testName) 'DELETE' | Out-Null } catch {} }
  if ($wifiTestChanged) { try { Api '/api/wifi' 'POST' @{mode='ap';forget='1'} | Out-Null } catch {} }
  if ($switchWifi) {
    & netsh wlan connect "name=$ReturnProfile" "interface=$Interface" | Out-Null
    Start-Sleep -Seconds 3
    & netsh wlan delete profile "name=$profileName" "interface=$Interface" | Out-Null
    Remove-Item -LiteralPath $profilePath -ErrorAction SilentlyContinue
  }
  $report | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $reportPath
}
if ($report.error) { throw $report.error }
