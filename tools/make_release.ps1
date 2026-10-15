# Refresh the existing Workshop project while preserving its thumbnail/publisher files.
# Run from Windows: pwsh -File tools/make_release.ps1
$ErrorActionPreference = 'Stop'
$sourceRoot = [IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
$addonsRoot = Split-Path $sourceRoot -Parent
$releaseParent = [IO.Path]::GetFullPath((Join-Path $addonsRoot '_AIO-release'))
$releaseRoot = [IO.Path]::GetFullPath((Join-Path $releaseParent 'AIOCommandMenu'))
$expectedRoot = [IO.Path]::GetFullPath((Join-Path $addonsRoot '_AIO-release\AIOCommandMenu'))
if ($releaseRoot -ne $expectedRoot -or $releaseRoot -eq $sourceRoot) {
    throw 'Release target is outside the expected _AIO-release/AIOCommandMenu directory.'
}
foreach ($path in @($releaseParent, $releaseRoot)) {
    if ((Test-Path -LiteralPath $path) -and ((Get-Item -LiteralPath $path -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw "Refusing a release path containing a junction or symbolic link: $path"
    }
}
$releaseFiles = @('resourceDatabase.rdb', 'COPYING', 'LICENSE.md', 'LICENSE.txt', 'README.md', 'CHANGELOG.md')
foreach ($name in @('Scripts', 'Configs', 'addon.gproj') + $releaseFiles) {
    if (!(Test-Path -LiteralPath (Join-Path $sourceRoot $name))) { throw "Missing release source: $name" }
}
New-Item -ItemType Directory -Path $releaseRoot -Force | Out-Null
foreach ($name in @('Scripts', 'Configs')) {
    $target = [IO.Path]::GetFullPath((Join-Path $releaseRoot $name))
    if ((Split-Path $target -Parent) -ne $expectedRoot -or (Split-Path $target -Leaf) -ne $name) {
        throw "Unsafe managed release directory: $target"
    }
    if (Test-Path -LiteralPath $target) {
        if ((Get-Item -LiteralPath $target -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) {
            throw "Refusing to replace a linked release directory: $target"
        }
        Remove-Item -LiteralPath $target -Recurse -Force
    }
    Copy-Item -LiteralPath (Join-Path $sourceRoot $name) -Destination $target -Recurse
}
foreach ($name in $releaseFiles) {
    Copy-Item -LiteralPath (Join-Path $sourceRoot $name) -Destination (Join-Path $releaseRoot $name) -Force
}
$project = Get-Content -LiteralPath (Join-Path $sourceRoot 'addon.gproj') -Raw
$project = $project -replace '(?m)^ TITLE ".*"', ' TITLE "AiO Command Menu (Reforger)"'
[IO.File]::WriteAllText((Join-Path $releaseRoot 'addon.gproj'), $project, [Text.UTF8Encoding]::new($false))
foreach ($file in Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'workshop') -File) {
    Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $releaseParent $file.Name) -Force
}
Write-Output "Release copy refreshed: $releaseRoot"
Write-Output 'Existing thumbnail and publisher files preserved. Set version 0.3.0 in the Workbench publish dialog.'
