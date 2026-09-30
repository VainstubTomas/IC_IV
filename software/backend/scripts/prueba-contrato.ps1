param(
  [string]$DeviceId = "650e8400-e29b-41d4-a716-446655440001",
  [string]$BridgeId = "650e8400-e29b-41d4-a716-446655440002",
  [string]$Api = "http://localhost:8080",
  [switch]$ProbarComandos
)
$ErrorActionPreference = 'Stop'
if (!([uri]$Api).IsLoopback) { throw 'Este script solo prueba API local; no usar contra AURA real' }
$projectRoot = Split-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) -Parent
function Publish-Local($Topic, $Payload, $Retain = $false) {
  $publishArgs = @('compose', '--project-directory', $projectRoot, 'exec', '-T', 'mqtt-broker', 'mosquitto_pub', '-q', '1', '-h', 'localhost', '-t', $Topic, '-s')
  if ($Retain) { $publishArgs += '-r' }
  $Payload | docker @publishArgs
  if ($LASTEXITCODE -ne 0) { throw "Fallo al publicar en broker LOCAL" }
}
$status = Invoke-RestMethod "$Api/api/v1/dispositivo/config"
if ($status.deviceId -ne $DeviceId) { throw "AURA_DEVICE_ID debe coincidir con DeviceId antes de probar" }
$ingestId = [guid]::NewGuid().ToString()
$payload = @{values=@{temp_c=4.25}; ingest_id=$ingestId} | ConvertTo-Json -Compress
Publish-Local "devices/$DeviceId/data" $payload
Publish-Local "devices/$DeviceId/data" $payload
$count = 0
for ($attempt=0; $attempt -lt 20; $attempt++) {
  Start-Sleep -Milliseconds 250
  $history = Invoke-RestMethod "$Api/api/v1/telemetria/history?limit=500"
  $count = @($history.data | Where-Object { $_.ingest_id -eq $ingestId }).Count
  if ($count -ge 1) { break }
}
Start-Sleep -Milliseconds 500
$history = Invoke-RestMethod "$Api/api/v1/telemetria/history?limit=500"
$count = @($history.data | Where-Object { $_.ingest_id -eq $ingestId }).Count
if ($count -ne 1) { throw "Deduplicacion: esperado 1 registro, obtenido $count" }
Write-Host 'OK: una lectura para dos publicaciones del mismo ingest_id.'
Publish-Local "devices/$DeviceId/status" '{"status":"online","details":{"evento":"up","rssi":-87,"snr":6.5,"fcnt":1,"dr":2,"f_port":1}}' $true
Publish-Local "devices/$BridgeId/status" '{"status":"offline"}' $true
Write-Host 'Publicado status de prueba. Configurar AURA_BRIDGE_DEVICE_ID con BridgeId para ver bridge offline.'
if ($ProbarComandos) {
  if (!$status.experimental) { throw 'Habilitar modo experimental solo en el banco LOCAL para esta prueba' }
  $body = @{interval_s=60;confirmed=$true;adr=$false;dr=2;offset_c=0} | ConvertTo-Json
  $command = Invoke-RestMethod "$Api/api/v1/dispositivo/config" -Method Post -ContentType 'application/json' -Body $body
  foreach ($state in @('encolado','transmitido','recibido')) {
    $reply = @{status=$state;details=@{command_id=$command.command_id}} | ConvertTo-Json -Compress
    Publish-Local "devices/$DeviceId/response" $reply
  }
  Write-Host 'Respuestas de prueba publicadas; recibido debe mantener ejecucion no confirmada.'
}
Write-Host 'Prueba LOCAL: no acredita uplinks/downlinks fisicos ni integracion AURA.'
