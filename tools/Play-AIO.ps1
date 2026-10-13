# Starts Arma Reforger (your normal profile) with the All-in-One Command Menu loaded.
#   -WithACA : also load Arma Commander Actual on its test range (take over a squad leader to lead AI)
#   default  : vanilla Game Master Arland
param([switch]$WithACA, [string]$GamePath)
$ErrorActionPreference = 'Stop'
try {
    if (!$GamePath) {
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
        foreach ($drive in 'C', 'D', 'E', 'F') { $libraries += "${drive}:\SteamLibrary" }
        foreach ($lib in $libraries) {
            if (!$lib) { continue }
            $candidate = Join-Path $lib 'steamapps\common\Arma Reforger\ArmaReforgerSteam.exe'
            if (Test-Path -LiteralPath $candidate) { $GamePath = $candidate; break }
        }
    }
    if (!$GamePath) { throw 'Arma Reforger was not found. Run with -GamePath pointing to ArmaReforgerSteam.exe.' }
    $addonsDir = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
    $addons = '629405AEFC950C12'
    $world = 'worlds/GameMaster/GM_Arland.ent'
    if ($WithACA) { $addons = 'AC230926A0010001,629405AEFC950C12'; $world = 'Worlds/ACR_TestRange.ent' }
    $arguments = '-addonsDir "' + $addonsDir + '" -addons ' + $addons + ' -noSplash -noBackend -world "' + $world + '"'
    Start-Process -FilePath $GamePath -ArgumentList $arguments -WorkingDirectory (Split-Path $GamePath -Parent)
} catch {
    Write-Host $_.Exception.Message -ForegroundColor Red
    Read-Host 'Press Enter to close'
    exit 1
}
