# The PC VR build on this PC's SteamVR with its null driver (a headset that is not there, the owner's OK for the
# Halo port's PC VR tests, the same rig): the real game, the mod's OpenXR backend (no QUEST_SIM_SCRIPT), the loader
# beside the mod. The null driver is turned on in steamvr.vrsettings for the run and the file put back after it,
# SteamVR stopped first (it writes the file as it closes); the run refuses to start if SteamVR is running or the file
# still holds a run's settings. Its log: C:\RACQuest\pc\out\<Name>\stdout.txt.
#
#   & vr\tests\pcvr_null.ps1 [-Name pcvr] [-Seconds 60] [-Level 3]
#
# -Launcher <the PC release's folder> starts it as a player's PLAY IN VR does instead (RatchetClankVR.exe --test-play,
# which writes the run's report), its set-up and logs in -LauncherHome and -LauncherLogs (a test's own, never the
# player's %LOCALAPPDATA%\RatchetQuestVR); the report's path goes into the run's stdout.txt.
#
# -Play gets it into the level: the null driver has no controllers, so without it the run stays on the title's movies
# (Y is tapped until play, QUEST_PC_TAP_TO_PLAY), and saves what goes to the runtime every 15 seconds or so, the left
# eye and the panel, as .ppm files in the run's folder (QUEST_PC_SHOTS). Give it some minutes; delete them after.
param([string]$Name = "pcvr", [int]$Seconds = 60, [int]$Level = 3, [string]$Launcher = "", [switch]$Play,
    [string]$Game = "C:\RACQuest\RatchetQuestVR\third_party\OpenRAC\port\build\release\openrac-rac1-pal.exe",
    [string]$Package = "C:\RACQuest\b\vrpc\package\quest_vr",
      [string]$LauncherHome = "C:\RACQuest\pc\launcher-home", [string]$LauncherLogs = "C:\RACQuest\pc\launcher-logs")
$ErrorActionPreference = "Stop"
$settings = "C:\Program Files (x86)\Steam\config\steamvr.vrsettings"
$original = "C:\RACQuest\pc\steamvr.vrsettings.original"  # the first run's copy, kept
$prerun = "C:\RACQuest\pc\steamvr.vrsettings.prerun"
if ((Get-Content $settings -Raw) -match '"forcedDriver"') {
    "steamvr.vrsettings still holds a null-driver run's settings: put $original back first"; exit 1
}
if (Get-Process -Name vrserver, vrmonitor, vrcompositor -ErrorAction SilentlyContinue) { "SteamVR is running: not started"; exit 1 }
if (-not (Test-Path $original)) { Copy-Item $settings $original }
Copy-Item $settings $prerun -Force

$game = $Game        # -Game and -Package: another build's, to compare
$package = $Package
$data = "C:\RACQuest\pc\data"
$out = "C:\RACQuest\pc\out\$Name"
$cards = "C:\RACQuest\pc\cards\pcvr"
New-Item -ItemType Directory -Force $out, "$data\plugins\quest-vr\lib\windows-x86_64" | Out-Null
if (Test-Path $cards) { Remove-Item -Recurse -Force $cards }
New-Item -ItemType Directory -Force $cards | Out-Null
Copy-Item "$package\lib\windows-x86_64\quest_vr.dll", "$package\lib\windows-x86_64\openxr_loader.dll" "$data\plugins\quest-vr\lib\windows-x86_64\" -Force
Copy-Item "$package\openrac-mod.json" "$data\plugins\quest-vr\" -Force
# Beside the mods' folders, where the game looks for them; not pc_run.sh's plugins.json.
Set-Content -Path "$data\plugins\plugins-pcvr.json" -Encoding ascii -Value '{"format": 1, "mods": [{"id": "quest-vr", "enabled": true, "settings": {}}]}'
# As the launcher writes it: the headset paces the frames.
Set-Content -Path "$out\quest-settings.ini" -Encoding ascii -Value @"
vsync = off
frame_rate = unlimited
aspect = 4:3
hud = centre
menu_bar = off
show_fps = off
render_scale = 100
msaa = 2
texture_filter = normal
anisotropy = 4
original_lod = false
max_draw_distance = true
matching_shadow = true
fades_in_view = true
smooth_plant_edges = true
skip_any_movie = true
volume = 0
"@
Remove-Item -Force -ErrorAction SilentlyContinue "$out\vr-options.txt", "$out\stdout.txt", "$out\stderr.txt"
try {
    $json = Get-Content $settings -Raw | ConvertFrom-Json
    if (-not $json.steamvr) { $json | Add-Member -NotePropertyName steamvr -NotePropertyValue ([pscustomobject]@{}) }
    $json.steamvr | Add-Member -Force -NotePropertyName requireHmd -NotePropertyValue $false
    $json.steamvr | Add-Member -Force -NotePropertyName forcedDriver -NotePropertyValue "null"
    $json.steamvr | Add-Member -Force -NotePropertyName activateMultipleDrivers -NotePropertyValue $true
    $json.steamvr | Add-Member -Force -NotePropertyName startMonitorFromAppLaunch -NotePropertyValue $false
    $json.steamvr | Add-Member -Force -NotePropertyName enableHomeApp -NotePropertyValue $false
    $json | Add-Member -Force -NotePropertyName power -NotePropertyValue ([pscustomobject]@{
        pauseCompositorOnStandby = $false; turnOffScreensTimeout = 86400; turnOffControllersTimeout = 0 })
    $json | Add-Member -Force -NotePropertyName driver_null -NotePropertyValue ([pscustomobject]@{
        enable = $true; serialNumber = "Null Serial Number"; modelNumber = "Null Model Number";
        windowX = 0; windowY = 0; windowWidth = 1280; windowHeight = 720; renderWidth = 1440; renderHeight = 1600;
        secondsFromVsyncToPhotons = 0.1; displayFrequency = 90 })
    $json | ConvertTo-Json -Depth 20 | Set-Content -Path $settings -Encoding utf8

    Remove-Item Env:QUEST_SIM_SCRIPT -ErrorAction SilentlyContinue
    if ($Play) {
        $env:QUEST_PC_TAP_TO_PLAY = "1"
        $env:QUEST_PC_SHOTS = $out
    }
    if ($Launcher) {
        # The launcher writes the game's settings, mods and plugins itself, from its own set-up.
        $env:RATCHETQUESTVR_HOME = $LauncherHome
        $env:RATCHETQUESTVR_LOGS = $LauncherLogs
        Remove-Item Env:RATCHETQUESTVR_INSTALL -ErrorAction SilentlyContinue
        Remove-Item Env:QUEST_VR_STATUS -ErrorAction SilentlyContinue
        $p = Start-Process -FilePath "$Launcher\RatchetClankVR.exe" -ArgumentList @("--test-play", "vr", "$Seconds", "$Level") `
            -PassThru -Wait -RedirectStandardOutput "$out\stdout.txt" -RedirectStandardError "$out\stderr.txt"
        "run ${Name}: the launcher's play test ended with exit code $($p.ExitCode)"
        return
    }
    $env:QUEST_VR_STATUS = "$out\status.txt"
    $arguments = @("--data", $data, "--window", "--levels", "$data\port\pal", "--cards", $cards,
                   "--plugins", "$data\plugins\plugins-pcvr.json", "--settings", "$out\quest-settings.ini")
    if ($Level -ge 0) { $arguments += @("--level", "$Level") }
    $p = Start-Process -FilePath $game -ArgumentList $arguments -PassThru -NoNewWindow `
        -RedirectStandardOutput "$out\stdout.txt" -RedirectStandardError "$out\stderr.txt"
    Start-Sleep -Seconds $Seconds
    $alive = -not $p.HasExited
    if ($alive) { Stop-Process -Id $p.Id -Force }
    "run ${Name}: the game was still running at the end: $alive (exit code $(if ($alive) { 'none' } else { $p.ExitCode }))"
} finally {
    foreach ($process in @("vrmonitor", "vrcompositor", "vrserver", "vrdashboard", "vrwebhelper", "vrstartup")) {
        Get-Process -Name $process -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    }
    Start-Sleep -Seconds 3
    Copy-Item $prerun $settings -Force
    "steamvr.vrsettings put back as it was: " + ((Get-FileHash $settings).Hash -eq (Get-FileHash $prerun).Hash)
}
