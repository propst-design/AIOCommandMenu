# All-in-One Command Menu test launcher for Claude.
#
# Leave this window open. Claude drops a request file into .aio-requests\ naming one
# of the presets below; this script starts a hidden Arma Reforger dedicated server
# (no game window, you can keep using the PC), waits for the run to finish, stops the
# server and writes a result file with the [AIO] log lines and script errors.
# It only runs these presets. Your own single-player game doesn't matter; if another
# Reforger dedicated server is running (e.g. Arma Commander Actual's test server, they
# share port 2001), it waits for that to close first.
#
# Close this window (or press Ctrl+C) to stop it.

param([string]$ServerPath)
$ErrorActionPreference = 'Stop'

$Arland = 'worlds/GameMaster/GM_Arland.ent'
$Presets = @{
    # Server boots with only this mod: compiles our scripts and loads our configs.
    'compile'     = @{ Addons = '629405AEFC950C12'; World = $Arland; TimeoutSec = 150; DoneWhen = '\[AIO\] ready' }
    # Same, together with Arma Commander Actual (load-order and input-config check).
    # Game client (opens a game window for ~1 min, separate profile): input self-check.
    'input'       = @{ Client = $true; Addons = '629405AEFC950C12'; World = $Arland; TimeoutSec = 150; DoneWhen = '\[AIO\] INPUT check' }
    'input-aca'   = @{ Client = $true; Addons = 'AC230926A0010001,629405AEFC950C12'; World = 'Worlds/ACR_TestRange.ent'; TimeoutSec = 180; DoneWhen = '\[AIO\] INPUT check' }
    'compile-aca' = @{ Addons = 'AC230926A0010001,629405AEFC950C12'; World = 'Worlds/ACR_TestRange.ent'; TimeoutSec = 180; DoneWhen = '\[AIO\] ready' }
}

$ProjectRoot = Split-Path $PSScriptRoot -Parent
$AddonsDir   = Split-Path $ProjectRoot -Parent
$RequestDir  = Join-Path $ProjectRoot '.aio-requests'
$ServerProfile = Join-Path $ProjectRoot '.local-testserver'
New-Item -ItemType Directory -Force $RequestDir | Out-Null

$created = $false
$mutex = New-Object Threading.Mutex($true, 'Local\AIO_TestWatcher', [ref]$created)
if (!$created) {
    $mutex.Dispose()
    Write-Host 'The AIO test launcher is already running. Keep that window open.' -ForegroundColor Yellow
    exit 0
}
Set-Content -LiteralPath (Join-Path $RequestDir 'watcher-started.txt') -Value (Get-Date -Format o)

function Find-Server {
    if ($ServerPath -and (Test-Path -LiteralPath $ServerPath)) { return $ServerPath }
    $roots = @()
    foreach ($key in 'HKCU:\Software\Valve\Steam', 'HKLM:\SOFTWARE\WOW6432Node\Valve\Steam') {
        $item = Get-ItemProperty $key -ErrorAction SilentlyContinue
        if ($item -and $item.SteamPath) { $roots += $item.SteamPath }
        if ($item -and $item.InstallPath) { $roots += $item.InstallPath }
    }
    $libraries = @()
    foreach ($root in $roots) {
        $libraries += $root
        $vdf = Join-Path $root 'steamapps\libraryfolders.vdf'
        if (Test-Path -LiteralPath $vdf) {
            foreach ($m in [regex]::Matches((Get-Content -LiteralPath $vdf -Raw), '"path"\s+"([^"]+)"')) { $libraries += $m.Groups[1].Value.Replace('\\', '\') }
        }
    }
    foreach ($drive in 'C', 'D', 'E', 'F', 'G') { $libraries += "${drive}:\SteamLibrary"; $libraries += "${drive}:\Program Files (x86)\Steam" }
    foreach ($lib in $libraries) {
        if (!$lib) { continue }
        $candidate = Join-Path $lib 'steamapps\common\Arma Reforger Server\ArmaReforgerServer.exe'
        if (Test-Path -LiteralPath $candidate) { return $candidate }
    }
    return $null
}

function Find-Game {
    $server = Find-Server
    if ($server) {
        $candidate = Join-Path (Split-Path (Split-Path $server -Parent) -Parent) 'Arma Reforger\ArmaReforgerSteam.exe'
        if (Test-Path -LiteralPath $candidate) { return $candidate }
    }
    foreach ($drive in 'C', 'D', 'E', 'F', 'G') {
        $candidate = "${drive}:\SteamLibrary\steamapps\common\Arma Reforger\ArmaReforgerSteam.exe"
        if (Test-Path -LiteralPath $candidate) { return $candidate }
    }
    return $null
}

function Write-Result($id, $lines) {
    $lines | Set-Content -LiteralPath (Join-Path $RequestDir "$id.result.txt") -Encoding UTF8
}

function Other-ServerRunning {
    return [bool](Get-Process -Name 'ArmaReforgerServer' -ErrorAction SilentlyContinue)
}

function Invoke-Request($file) {
    $id = [IO.Path]::GetFileNameWithoutExtension($file.Name)
    $preset = (Get-Content -LiteralPath $file.FullName -Raw).Trim().ToLower()
    Remove-Item -LiteralPath $file.FullName
    $time = Get-Date -Format 'HH:mm:ss'
    if (!$Presets.ContainsKey($preset)) {
        Write-Host "[$time] Refused unknown preset '$preset'" -ForegroundColor Yellow
        Write-Result $id @('status=refused', "reason=unknown preset '$preset'")
        return
    }
    $config = $Presets[$preset]
    if ($config.Client) { Invoke-Client $id $preset $config; return }
    $server = Find-Server
    if (!$server) { Write-Result $id @('status=refused', 'reason=Arma Reforger Server not found (start with -ServerPath)'); return }
    # Wait (up to 10 min) for another dedicated server to close: they share port 2001.
    # (A game you host from the client also uses 2001: the run then fails with
    # 'Unable to start replication', reported below.)
    $waited = 0
    while ((Other-ServerRunning) -and $waited -lt 600) { Start-Sleep -Seconds 5; $waited += 5 }
    if (Other-ServerRunning) { Write-Result $id @('status=refused', 'reason=another Reforger server kept running'); return }

    New-Item -ItemType Directory -Force $ServerProfile | Out-Null
    $serverArgs = '-server "' + $config.World + '" -addonsDir "' + $AddonsDir + '" -addons ' + $config.Addons + ' -profile "' + $ServerProfile + '" -maxFPS 60 -noBackend'
    Write-Host "[$time] Running '$preset' on a hidden test server..." -ForegroundColor Cyan
    $started = Get-Date
    $proc = Start-Process -FilePath $server -ArgumentList $serverArgs -WorkingDirectory (Split-Path $server -Parent) -WindowStyle Hidden -PassThru
    $logs = Join-Path $ServerProfile 'logs'
    $outcome = 'timeout'
    $log = $null
    while ($true) {
        Start-Sleep -Seconds 3
        if (!$log -and (Test-Path -LiteralPath $logs)) {
            $dir = Get-ChildItem -LiteralPath $logs -Directory | Where-Object { $_.CreationTime -ge $started.AddSeconds(-5) } |
                   Sort-Object CreationTime -Descending | Select-Object -First 1
            if ($dir) { $log = Join-Path $dir.FullName 'console.log' }
        }
        if ($proc.HasExited) { $outcome = 'server exited'; break }
        if ($log -and (Test-Path -LiteralPath $log)) {
            $text = Get-Content -LiteralPath $log -Raw -ErrorAction SilentlyContinue
            if ($text -match 'Unable to start replication') { $outcome = 'port busy (a hosted game or server is using 2001)'; break }
            if ($text -match "Can't compile") { Start-Sleep -Seconds 2; $outcome = 'compile error'; break }
            if ($text -match 'SCRIPT\s+\(E\)') { Start-Sleep -Seconds 3; $outcome = 'script error'; break }
            if ($text -match $config.DoneWhen) { Start-Sleep -Seconds 5; $outcome = 'completed'; break }
        }
        if (((Get-Date) - $started).TotalSeconds -gt $config.TimeoutSec) { break }
    }
    if (!$proc.HasExited) { Stop-Process -Id $proc.Id -ErrorAction SilentlyContinue }
    $result = @("status=$outcome", "preset=$preset", "started=$($started.ToString('s'))", "seconds=$([int]((Get-Date) - $started).TotalSeconds)", "log=$log", '---')
    if ($log -and (Test-Path -LiteralPath $log)) {
        $result += Get-Content -LiteralPath $log | Where-Object { $_ -match "\[AIO\]|SCRIPT\s+\(E\)|SCRIPT\s+\(W\)|Can't compile|AIO_|ERROR|chimeraInputCommon|keyBindingMenu" } | Select-Object -First 200
    } else {
        $result += 'No server console.log was found for this run.'
    }
    Write-Result $id $result
    Write-Host "[$(Get-Date -Format 'HH:mm:ss')] '$preset' finished: $outcome" -ForegroundColor Green
}

# Runs the game client (own profile) and reads its console.log; refuses if a game is already open.
function Invoke-Client($id, $preset, $config) {
    if (Get-Process -Name 'ArmaReforgerSteam', 'ArmaReforger_BE' -ErrorAction SilentlyContinue) {
        Write-Result $id @('status=refused', 'reason=a Reforger game is already running (close it for client tests)'); return
    }
    $game = Find-Game
    if (!$game) { Write-Result $id @('status=refused', 'reason=ArmaReforgerSteam.exe not found'); return }
    $gameProfile = Join-Path $ProjectRoot '.local-game'
    New-Item -ItemType Directory -Force $gameProfile | Out-Null
    $gameArgs = '-addonsDir "' + $AddonsDir + '" -addons ' + $config.Addons + ' -profile "' + $gameProfile + '" -noSplash -noBackend -world "' + $config.World + '"'
    Write-Host "[$(Get-Date -Format 'HH:mm:ss')] Running '$preset' (game window)..." -ForegroundColor Cyan
    $started = Get-Date
    $proc = Start-Process -FilePath $game -ArgumentList $gameArgs -WorkingDirectory (Split-Path $game -Parent) -PassThru
    $logs = Join-Path $gameProfile 'logs'
    $outcome = 'timeout'
    $log = $null
    while ($true) {
        Start-Sleep -Seconds 3
        if (!$log -and (Test-Path -LiteralPath $logs)) {
            $dir = Get-ChildItem -LiteralPath $logs -Directory | Where-Object { $_.CreationTime -ge $started.AddSeconds(-5) } |
                   Sort-Object CreationTime -Descending | Select-Object -First 1
            if ($dir) { $log = Join-Path $dir.FullName 'console.log' }
        }
        if ($proc.HasExited) { $outcome = 'game exited'; break }
        if ($log -and (Test-Path -LiteralPath $log)) {
            $text = Get-Content -LiteralPath $log -Raw -ErrorAction SilentlyContinue
            if ($text -match "Can't compile") { Start-Sleep -Seconds 2; $outcome = 'compile error'; break }
            if ($text -match $config.DoneWhen) { Start-Sleep -Seconds 3; $outcome = 'completed'; break }
        }
        if (((Get-Date) - $started).TotalSeconds -gt $config.TimeoutSec) { break }
    }
    if (!$proc.HasExited) { Stop-Process -Id $proc.Id -ErrorAction SilentlyContinue }
    Get-Process -Name 'ArmaReforgerSteam', 'ArmaReforger_BE' -ErrorAction SilentlyContinue | Where-Object { $_.StartTime -ge $started } | Stop-Process -ErrorAction SilentlyContinue
    $result = @("status=$outcome", "preset=$preset", "started=$($started.ToString('s'))", "seconds=$([int]((Get-Date) - $started).TotalSeconds)", "log=$log", '---')
    if ($log -and (Test-Path -LiteralPath $log)) {
        $result += Get-Content -LiteralPath $log | Where-Object { $_ -match "\[AIO\]|SCRIPT\s+\(E\)|Can't compile|AIOCommandMenu|chimeraInputCommon|keyBindingMenu|ResourceDB" } | Select-Object -First 200
    }
    Write-Result $id $result
    Write-Host "[$(Get-Date -Format 'HH:mm:ss')] '$preset' finished: $outcome" -ForegroundColor Green
}

Write-Host 'AIO test launcher is running. Leave this window open; close it to stop.' -ForegroundColor Green
Write-Host "Watching $RequestDir"
try { while ($true) {
    try {
        Get-ChildItem -LiteralPath $RequestDir -Filter '*.request' | Sort-Object CreationTime | ForEach-Object { Invoke-Request $_ }
    } catch {
        Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
    }
    Start-Sleep -Seconds 2
} } finally {
    $mutex.ReleaseMutex()
    $mutex.Dispose()
}
