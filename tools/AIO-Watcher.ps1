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
# A request 'pak:<preset>' runs a server preset on port 2303 instead of 2001, so it can run
# while ACA's test server or a hosted game holds 2001. Port options only work with a server
# config (-config), which only loads packed mods: the mods are packed first (see Get-Pack).
#
# A 'push' request pushes main to the public GitHub repo propst-design/AIOCommandMenu
# with your GitHub login (creating it with gh the first time). Nothing else.
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

function Find-Workbench {
    $server = Find-Server
    $libs = @()
    if ($server) { $libs += Split-Path (Split-Path $server -Parent) -Parent }
    foreach ($drive in 'C', 'D', 'E', 'F', 'G') { $libs += "${drive}:\SteamLibrary\steamapps\common"; $libs += "${drive}:\Program Files (x86)\Steam\steamapps\common" }
    foreach ($lib in $libs) {
        $candidate = Join-Path $lib 'Arma Reforger Tools\Workbench\ArmaReforgerWorkbenchSteamDiag.exe'
        if (Test-Path -LiteralPath $candidate) { return $candidate }
    }
    return $null
}

# pak: runs. Packed copies live outside the Workbench addons folder (no duplicate GUIDs there).
$PakRoot = Join-Path $env:LOCALAPPDATA 'AIO-TestPack'
$PakPort = 2303
$PakFolders = @{ '629405AEFC950C12' = 'AIOCommandMenu'; 'AC230926A0010001' = 'ArmaCommanderReforger'; 'BBB0261015000001' = 'BetterBattleBuddies' }
$PakScenarios = @{ 'worlds/gamemaster/gm_arland.ent' = '{2BBBE828037C6F4B}Missions/22_GM_Arland.conf'; 'worlds/acr_testrange.ent' = '{AC230926A0010011}Missions/ACR_TestRange.conf' }

# Packs one mod folder with Workbench unless its sources are unchanged since the last pack.
# Returns an error message, or $null when the pack is ready.
function Get-Pack($guid) {
    $source = Join-Path $AddonsDir $PakFolders[$guid]
    $target = Join-Path $PakRoot "addons\$guid"
    # Signature: every source file's path, size and time. Workbench rewrites
    # resourceDatabase.rdb in the source on each pack, so that file doesn't count.
    $sig = New-Object Text.StringBuilder
    Get-ChildItem -LiteralPath $source -Recurse -File | Sort-Object FullName | ForEach-Object {
        $rel = $_.FullName.Substring($source.Length + 1)
        if ($rel -match '^(\.git|\.local-|\.aio-requests|\.acr-requests|docs\\|tools\\|dist\\)' -or $rel -eq 'resourceDatabase.rdb') { return }
        [void]$sig.AppendLine("$rel $($_.Length) $($_.LastWriteTimeUtc.Ticks)")
    }
    $sigFile = "$target.sig"
    $sigText = $sig.ToString()
    # Download manifests in the folder: the server's backend swapped our pack for a Workshop release.
    # A pack folder inside the old one: the last swap couldn't remove it, so it is stale too.
    $swapped = @(Get-ChildItem -LiteralPath $target -Filter '*_manifest.json' -ErrorAction SilentlyContinue).Count -gt 0 -or (Test-Path -LiteralPath (Join-Path $target "$guid.packing"))
    if (!$swapped -and (Test-Path -LiteralPath (Join-Path $target 'data.pak')) -and (Test-Path -LiteralPath $sigFile) -and ([IO.File]::ReadAllText($sigFile) -eq $sigText)) { return $null }
    $wb = Find-Workbench
    if (!$wb) { return 'Arma Reforger Tools (Workbench) not found' }
    $gameAddons = Join-Path (Split-Path (Find-Server) -Parent) '..\Arma Reforger\addons'
    $fresh = "$target.packing"
    $wbProfile = Join-Path $PakRoot 'workbench'
    if (Test-Path -LiteralPath $fresh) { Remove-Item -LiteralPath $fresh -Recurse -Force }
    New-Item -ItemType Directory -Force $fresh, $wbProfile | Out-Null
    Write-Host "[$(Get-Date -Format 'HH:mm:ss')] Packing $($PakFolders[$guid])..." -ForegroundColor Cyan
    $wbArgs = '-gproj "' + (Join-Path $source 'addon.gproj') + '" -addonsDir "' + [IO.Path]::GetFullPath($gameAddons) + ',' + $AddonsDir + '" -profile "' + $wbProfile + '" -noThrow -wbModule=ResourceManager -packAddon -packAddonDir "' + $fresh + '"'
    $proc = Start-Process -FilePath $wb -ArgumentList $wbArgs -WindowStyle Hidden -PassThru
    if (!$proc.WaitForExit(900000)) { Stop-Process -Id $proc.Id -ErrorAction SilentlyContinue; return "packing $($PakFolders[$guid]) timed out" }
    if (!(Test-Path -LiteralPath (Join-Path $fresh 'data.pak'))) { return "packing $($PakFolders[$guid]) failed (Workbench logs in $wbProfile\logs)" }
    # A server that just stopped can hold the old data.pak for a moment: the removal failed
    # silently and Move-Item put the new pack inside the old folder (30 Oct). Retry, then give up.
    for ($try = 0; $try -lt 10 -and (Test-Path -LiteralPath $target); $try++) {
        Remove-Item -LiteralPath $target -Recurse -Force -ErrorAction SilentlyContinue
        if (Test-Path -LiteralPath $target) { Start-Sleep -Seconds 2 }
    }
    if (Test-Path -LiteralPath $target) { return "couldn't replace the old $($PakFolders[$guid]) pack in $target (still in use?)" }
    Move-Item -LiteralPath $fresh -Destination $target
    [IO.File]::WriteAllText($sigFile, $sigText)
    return $null
}

# Packs the preset's mods and writes the server config; returns the -config start line or an error.
function Get-PakArgs($config, $serverProfile) {
    $scenario = $PakScenarios[$config.World.ToLower()]
    if (!$scenario) { return @{ Error = "no scenario known for $($config.World)" } }
    $guids = @($config.Addons.Split(','))
    # ACA needs BBB, its dependency.
    if ($guids -contains 'AC230926A0010001' -and $guids -notcontains 'BBB0261015000001') { $guids = @('BBB0261015000001') + $guids }
    foreach ($guid in $guids) { $err = Get-Pack $guid; if ($err) { return @{ Error = $err } } }
    # Ask for the version the profile already holds (its first run downloaded it): asking for the
    # latest made the backend "update" our packs to a newer Workshop release, and pak: runs then
    # tested the published build instead of the source (30 Oct).
    $mods = @($guids | ForEach-Object {
        $mod = @{ modId = $_; name = $PakFolders[$_] }
        $held = Get-ChildItem -LiteralPath (Join-Path $serverProfile 'addons') -Directory -Filter "*_$_" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($held -and (Test-Path -LiteralPath (Join-Path $held.FullName 'ServerData.json'))) {
            try { $mod.version = [string]((Get-Content -LiteralPath (Join-Path $held.FullName 'ServerData.json') -Raw | ConvertFrom-Json).revision.version) } catch {}
        }
        $mod
    })
    $serverConfig = @{
        bindAddress = '0.0.0.0'; bindPort = $PakPort; publicAddress = '127.0.0.1'; publicPort = $PakPort
        a2s = @{ address = '127.0.0.1'; port = 17779 }
        game = @{ name = 'AIO hidden test server'; password = ''; passwordAdmin = ''; scenarioId = $scenario; maxPlayers = 4
                  visible = $false; crossPlatform = $false; gameProperties = @{ battlEye = $false }; mods = $mods }
    }
    $cfgPath = Join-Path $serverProfile 'server.json'
    Set-Content -LiteralPath $cfgPath -Value ($serverConfig | ConvertTo-Json -Depth 5) -Encoding ASCII
    return @{ Args = '-config "' + $cfgPath + '" -addonsDir "' + (Join-Path $PakRoot 'addons') + '" -profile "' + $serverProfile + '"' }
}

function Test-PortListening($port) {
    return [bool]([Net.NetworkInformation.IPGlobalProperties]::GetIPGlobalProperties().GetActiveUdpListeners() | Where-Object Port -eq $port)
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

function Invoke-Push($id) {
    # 'push': pushes main to the public repo propst-design/AIOCommandMenu with your GitHub
    # login (creates the repo with gh first if it doesn't exist). Nothing else.
    $ErrorActionPreference = 'Continue'
    $repo = 'propst-design/AIOCommandMenu'
    $started = Get-Date
    $log = @()
    Write-Host "[$(Get-Date -Format 'HH:mm:ss')] Pushing main to GitHub ($repo)..." -ForegroundColor Cyan
    $gh = (Get-Command gh -ErrorAction SilentlyContinue).Source
    if (-not $gh) {
        foreach ($c in @("$env:ProgramFiles\GitHub CLI\gh.exe", "${env:ProgramFiles(x86)}\GitHub CLI\gh.exe", "$env:LOCALAPPDATA\Programs\GitHub CLI\gh.exe", "$env:LOCALAPPDATA\Microsoft\WinGet\Links\gh.exe")) {
            if ($c -and (Test-Path $c)) { $gh = $c; break }
        }
    }
    if (-not $gh) { Write-Result $id @('status=failed', 'preset=push', 'reason=GitHub CLI (gh.exe) not found'); return }
    $null = & $gh repo view $repo 2>&1
    if ($LASTEXITCODE -ne 0) {
        $log += (& $gh repo create $repo --public --description "All-in-One Command Menu for Arma Reforger (after Leopard20's Arma 3 mod). Bug reports under Issues." 2>&1 | Out-String)
        if ($LASTEXITCODE -ne 0) { Write-Result $id (@('status=failed', 'preset=push', 'reason=gh repo create failed', '---') + $log); return }
    }
    $log += (& $gh repo edit $repo --enable-issues 2>&1 | Out-String)
    $null = & git -C $ProjectRoot remote get-url origin 2>&1
    if ($LASTEXITCODE -ne 0) { $log += (& git -C $ProjectRoot remote add origin "https://github.com/$repo.git" 2>&1 | Out-String) }
    $log += (& git -C $ProjectRoot push -u origin main 2>&1 | Out-String)
    $outcome = 'completed'
    if ($LASTEXITCODE -ne 0) { $outcome = 'failed' }
    Write-Result $id (@("status=$outcome", 'preset=push', "seconds=$([int]((Get-Date) - $started).TotalSeconds)", '---') + $log)
    Write-Host "[$(Get-Date -Format 'HH:mm:ss')] Push finished: $outcome" -ForegroundColor Green
}

function Invoke-Request($file) {
    $id = [IO.Path]::GetFileNameWithoutExtension($file.Name)
    $preset = (Get-Content -LiteralPath $file.FullName -Raw).Trim().ToLower()
    Remove-Item -LiteralPath $file.FullName
    $pak = $preset.StartsWith('pak:')
    if ($pak) { $preset = $preset.Substring(4) }
    # Extra server test presets from tools\test-presets.txt, re-read on every request so new tests
    # need no launcher restart: "name|-aioSomethingTest 1|timeoutSec[|aca]" (only -aio...Test flags).
    $extraFile = Join-Path $PSScriptRoot 'test-presets.txt'
    if (Test-Path -LiteralPath $extraFile) {
        foreach ($line in Get-Content -LiteralPath $extraFile) {
            $parts = $line.Split('|')
            if ($parts.Count -lt 3 -or $parts[1] -notmatch '^-aio[A-Za-z]+Test 1$') { continue }
            $entry = @{ Addons = '629405AEFC950C12'; World = $Arland; Flags = $parts[1]; TimeoutSec = [int]$parts[2]; DoneWhen = '\[AIO\] TEST COMPLETE' }
            if ($parts.Count -ge 4 -and $parts[3].Trim() -eq 'aca') { $entry.Addons = 'AC230926A0010001,629405AEFC950C12'; $entry.World = 'Worlds/ACR_TestRange.ent' }
            $Presets[$parts[0].Trim().ToLower()] = $entry
        }
    }
    $time = Get-Date -Format 'HH:mm:ss'
    if ($preset -eq 'push') { Invoke-Push $id; return }
    if (!$Presets.ContainsKey($preset)) {
        Write-Host "[$time] Refused unknown preset '$preset'" -ForegroundColor Yellow
        Write-Result $id @('status=refused', "reason=unknown preset '$preset'")
        return
    }
    $config = $Presets[$preset]
    if ($config.Client) {
        if ($pak) { Write-Result $id @('status=refused', "reason=$preset needs the game client; pak: is for server presets"); return }
        Invoke-Client $id $preset $config; return
    }
    $server = Find-Server
    if (!$server) { Write-Result $id @('status=refused', 'reason=Arma Reforger Server not found (start with -ServerPath)'); return }
    $label = $preset
    if ($pak) {
        $label = "pak:$preset"
        if (Test-PortListening $PakPort) { Write-Result $id @('status=refused', "reason=port $PakPort is in use"); return }
        $profileDir = Join-Path $PakRoot 'server'
        New-Item -ItemType Directory -Force $profileDir | Out-Null
        $packStart = Get-Date
        $pakArgs = Get-PakArgs $config $profileDir
        if ($pakArgs.Error) { Write-Result $id @('status=pack failed', "preset=$label", "reason=$($pakArgs.Error)"); return }
        $packSeconds = [int]((Get-Date) - $packStart).TotalSeconds
        # Backend stays on: without it the server config's mods aren't mounted. It fetches the
        # Workshop copies into the profile once but loads the packs from -addonsDir.
        $serverArgs = $pakArgs.Args + ' -maxFPS 60 -aioCheats all ' + $config.Flags
    } else {
        # Wait (up to 10 min) for another dedicated server to close: they share port 2001.
        # (A game you host from the client also uses 2001: the run then fails with
        # 'Unable to start replication', reported below.)
        $waited = 0
        while ((Other-ServerRunning) -and $waited -lt 600) { Start-Sleep -Seconds 5; $waited += 5 }
        if (Other-ServerRunning) { Write-Result $id @('status=refused', 'reason=another Reforger server kept running'); return }
        $profileDir = $ServerProfile
        New-Item -ItemType Directory -Force $profileDir | Out-Null
        # -aioCheats all: hidden dev servers have no host, so everyone gets the Admin page.
        $serverArgs = '-server "' + $config.World + '" -addonsDir "' + $AddonsDir + '" -addons ' + $config.Addons + ' -profile "' + $profileDir + '" -maxFPS 60 -noBackend -aioCheats all ' + $config.Flags
    }
    Write-Host "[$time] Running '$label' on a hidden test server..." -ForegroundColor Cyan
    $started = Get-Date
    $proc = Start-Process -FilePath $server -ArgumentList $serverArgs -WorkingDirectory (Split-Path $server -Parent) -WindowStyle Hidden -PassThru
    $logs = Join-Path $profileDir 'logs'
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
            if ($text -match 'Unable to start replication') { $outcome = 'port busy (a hosted game or server is using the port)'; break }
            if ($text -match "Can't compile") { Start-Sleep -Seconds 2; $outcome = 'compile error'; break }
            # Vanilla noise (crash damage, AI movement, suppression) is not a script error of ours.
            $errors = ($text -split "`n") | Where-Object { $_ -match 'SCRIPT\s+\(E\)' -and $_ -notmatch 'SCR_InstigatorContextData|SCR_AIProcessFailedMovementResult|No suppression volume provided' }
            if ($errors) { Start-Sleep -Seconds 3; $outcome = 'script error'; break }
            if ($text -match $config.DoneWhen) { Start-Sleep -Seconds 5; $outcome = 'completed'; break }
        }
        if (((Get-Date) - $started).TotalSeconds -gt $config.TimeoutSec) { break }
    }
    if (!$proc.HasExited) { Stop-Process -Id $proc.Id -ErrorAction SilentlyContinue }
    $result = @("status=$outcome", "preset=$label", "started=$($started.ToString('s'))", "seconds=$([int]((Get-Date) - $started).TotalSeconds)", "log=$log")
    if ($pak) { $result += "packSeconds=$packSeconds" }
    $result += '---'
    if ($log -and (Test-Path -LiteralPath $log)) {
        $result += Get-Content -LiteralPath $log | Where-Object { $_ -match "\[AIO\]|SCRIPT\s+\(E\)|SCRIPT\s+\(W\)|Can't compile|AIO_|ERROR|chimeraInputCommon|keyBindingMenu" } | Select-Object -First 200
    } else {
        $result += 'No server console.log was found for this run.'
    }
    Write-Result $id $result
    Write-Host "[$(Get-Date -Format 'HH:mm:ss')] '$label' finished: $outcome" -ForegroundColor Green
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
