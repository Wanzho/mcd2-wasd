# wasdmod for Windows, without the app: installs the Minecraft Dungeons II keyboard
# controller mod next to the game and opens its key layout editor.
#
# A plain script for the PowerShell every Windows has, beside the mod's files:
# xinput1_4.dll, default.txt, author.txt, Key Layout Editor.html and lang.json.
# wasdmod-windows.cmd starts it. It does what the Windows app does: it finds the game
# through Steam's library folders (or XboxGames, for the Minecraft Launcher and Xbox
# app), serves the editor on 127.0.0.1 (a free port, with a random token in every path
# so no other page can use it) and opens it as an Edge or Chrome app window, otherwise
# in the default browser. The page has the mod's own buttons (install, disable,
# uninstall), the game folder and Troubleshooting; its requests are listed in the
# page's first script (configurator.html in the source). The script ends when the
# editor's window is closed, when this window is closed, or with Ctrl+C.
#
# What it writes: the mod's files in the game folder; when the editor is asked to
# change the game's keys, the game's own keyboard settings file; its settings in
# %APPDATA%\wasdmod; the report of Record logs on the Desktop; and a layout exported
# from the editor, where the Save dialog says. Nothing needs administrator rights, no
# registry setting is changed (Windows loads xinput1_4.dll from the game folder), and
# nothing is downloaded.
#
# Commands, without the editor (a game folder or exe may follow each one):
#   status      what is installed
#   install     install or update the mod
#   uninstall   uninstall it (saved layouts stay)
#   off, on     turn it off (the game starts without it) or back on
#   find        where the game is
#   --no-open [folder]   the editor's server only: prints its address, shows no messages
# For tests: with WASDMOD_TEST_GAME the game counts as running while that file exists.
$ErrorActionPreference = 'Stop'
$Version = '@VERSION@'
#if !NEXUS
$Updates = 'github'
$ReleasesApi = 'https://api.github.com/repos/Wanzho/mcd2-wasd/releases/latest'
$ReleasesPage = 'https://github.com/Wanzho/mcd2-wasd/releases/latest'
#else
$Updates = 'nexus'
$NexusPage = 'https://www.nexusmods.com/minecraftdungeons2/mods/104'
#endif
$IssuesPage = 'https://github.com/Wanzho/mcd2-wasd/issues/new'
$Here = Split-Path -Parent $MyInvocation.MyCommand.Path
$Exes = 'Dungeons-Win64-Shipping.exe', 'Dungeons-WinGDK-Shipping.exe'
$Utf8 = New-Object Text.UTF8Encoding($false)
$Latin1 = [Text.Encoding]::GetEncoding(28591) # every byte as it is
$Ordinal = [StringComparison]::Ordinal
$Headless = $false # --no-open and the commands: no messages or file dialogs

# ---------------------------------------------------------------- files
# Paths are put together and looked at with .NET's own functions: they take every
# name as it is (no wildcards), and a drive that isn't there is just "no such file".
function Join-Parts([string]$a, [string]$b) { [IO.Path]::Combine($a, $b) }
function Test-File([string]$path) { [IO.File]::Exists($path) }
function Test-Folder([string]$path) { [IO.Directory]::Exists($path) }
# The last part of a path or of a name sent by the page (nothing before a \, / or :).
function Get-Leaf([string]$path) { $path.Substring($path.LastIndexOfAny([char[]]'\/:') + 1) }
function Read-Bytes([string]$path) {
    try { $b = [IO.File]::ReadAllBytes($path) } catch { return $null }
    return , $b
}
function Write-Bytes([string]$path, [byte[]]$data) {
    try { [IO.File]::WriteAllBytes($path, $data); $true } catch { $false }
}
function Copy-File([string]$from, [string]$to) {
    try { [IO.File]::Copy($from, $to, $true); $true } catch { $false }
}
function Remove-File([string]$path) {
    try { [IO.File]::Delete($path); $true } catch { $false }
}
# The folder's files with such a name ("wasdmod*.txt"), oldest first.
function Get-Files([string]$dir, [string]$pattern) {
    try { $found = @((New-Object IO.DirectoryInfo($dir)).GetFiles($pattern)) } catch { $found = @() }
    @($found | Where-Object { $_.Name -like $pattern } | Sort-Object LastWriteTimeUtc)
}

# ---------------------------------------------------------------- language
# The text in every language the game has (lang.json, made by lang.py from
# lang/*.json). English is the key: (T 'Yes') is that text in the current
# language, or the English if it has none. \n in a key is a new line. The editor's
# status line and the messages are translated; what the commands print is English.
Add-Type -AssemblyName System.Web.Extensions
$Json = New-Object System.Web.Script.Serialization.JavaScriptSerializer
$Lang = 'en'
$Langs = $null
try { $Langs = $Json.DeserializeObject([IO.File]::ReadAllText((Join-Parts $Here 'lang.json'), $Utf8)) } catch {}
function N_([string]$en) { $en } # marks text that T translates where it's shown
function T([string]$en, [hashtable]$fill) {
    $key = $en.Replace('\n', "`n"); $text = $key
    if ($Langs -and $Langs['text'].ContainsKey($script:Lang) -and $Langs['text'][$script:Lang].ContainsKey($key) -and $Langs['text'][$script:Lang][$key]) { $text = $Langs['text'][$script:Lang][$key] }
    if ($fill) { foreach ($k in $fill.Keys) { $text = $text.Replace('{' + $k + '}', [string]$fill[$k]) } }
    $text
}
# "de", "de-DE", "pt_BR", "zh-Hant", "zh-TW"... -> one of ours, or $null.
function Get-LangOf([string]$tag) {
    $t = $tag.Replace('_', '-').ToLowerInvariant()
    if ($t -eq 'zh' -or $t.StartsWith('zh-', $Ordinal)) { $t = $(if ($t -match '^zh-(hant|tw|hk|mo)') { 'zh-hant' } else { 'zh-hans' }) }
    if ($Langs) { foreach ($code in $Langs['order']) { $c = $code.ToLowerInvariant(); if ($t -eq $c -or $t.StartsWith($c + '-', $Ordinal)) { return $code } } }
    $null
}
# Where the editor's layouts are kept (the Windows app's folder: the same layouts in
# both), with the game folder picked by hand and the day of the last update check.
function Get-StoreFile([string]$name) { Join-Parts (Join-Parts $env:APPDATA 'wasdmod') $name }
function Set-StoreFile([string]$name, [byte[]]$data) {
    try { [void][IO.Directory]::CreateDirectory((Join-Parts $env:APPDATA 'wasdmod')) } catch { return $false } # (made when first needed)
    Write-Bytes (Get-StoreFile $name) $data
}
# The language picked in the key layout editor (kept with its layouts), else Windows'.
function Set-Language {
    $code = $null
    $saved = Read-Bytes (Get-StoreFile 'editor.json')
    if ($saved -and $Latin1.GetString($saved) -match '"d2kb-lang":"([^"]*)"') { $code = Get-LangOf $Matches[1] }
    if (-not $code) { $code = Get-LangOf ([Globalization.CultureInfo]::CurrentUICulture.Name) }
    $script:Lang = $(if ($code) { $code } else { 'en' })
}

# ---------------------------------------------------------------- finding the game
$Subs = '', 'Dungeons\Binaries\Win64', 'Dungeons\Binaries\WinGDK', 'Content\Dungeons\Binaries\Win64', 'Content\Dungeons\Binaries\WinGDK',
        'Binaries\Win64', 'Binaries\WinGDK', 'Win64', 'WinGDK'
function Test-GameDir([string]$d) {
    foreach ($e in $Exes) { if (Test-File (Join-Parts $d $e)) { return $true } }
    $false
}
function Get-GameAt([string]$root) {
    foreach ($s in $Subs) { $d = $(if ($s) { Join-Parts $root $s } else { $root }); if (Test-GameDir $d) { return $d } }
    $null
}
# The game folder for a picked folder or exe: itself, or a nearby parent's (no search of the disk).
function Get-DirFromPick([string]$pick) {
    try {
        $root = [IO.Path]::GetFullPath($pick)
        if (Test-File $root) {
            if (-not $root.EndsWith('.exe', [StringComparison]::OrdinalIgnoreCase)) { return $null }
            $root = [IO.Path]::GetDirectoryName($root)
        } elseif (-not (Test-Folder $root)) { return $null }
        for ($up = 0; $up -le 4 -and $root; $up++) {
            $found = Get-GameAt $root
            if ($found) { return $found }
            $root = [IO.Path]::GetDirectoryName($root)
        }
    } catch {} # (not a path at all)
    $null
}
function Get-RegString([string]$key, [string]$name) {
    try { $v = (Get-ItemProperty -LiteralPath $key -Name $name -ErrorAction Stop).$name } catch { return $null }
    if ($v) { ([string]$v).Replace('/', '\').TrimEnd('\') } else { $null }
}
# A Steam library: the game under steamapps\common.
function Get-InLibrary([string]$lib) {
    try {
        foreach ($name in 'Minecraft Dungeons II', 'Minecraft Dungeons 2') {
            $found = Get-GameAt (Join-Parts $lib "steamapps\common\$name")
            if ($found) { return $found }
        }
    } catch {} # (a library whose path isn't one)
    $null
}
# Steam's library list: every "path" "D:\\SteamLibrary" entry in libraryfolders.vdf.
function Search-Libraries([string]$steam) {
    $found = Get-InLibrary $steam
    if ($found) { return $found }
    $vdf = Read-Bytes (Join-Parts $steam 'steamapps\libraryfolders.vdf')
    if (-not $vdf) { return $null }
    foreach ($m in [regex]::Matches($Utf8.GetString($vdf), '(?i)"path"\s+"((?:[^"\\]|\\.)*)"')) {
        $found = Get-InLibrary $m.Groups[1].Value.Replace('\\', '\').Replace('/', '\')
        if ($found) { return $found }
    }
    $null
}
function Find-Game {
    $steams = @((Get-RegString 'HKCU:\Software\Valve\Steam' 'SteamPath'), (Get-RegString 'HKLM:\SOFTWARE\WOW6432Node\Valve\Steam' 'InstallPath'),
                (Get-RegString 'HKLM:\SOFTWARE\Valve\Steam' 'InstallPath')) | Where-Object { $_ }
    foreach ($steam in $steams) { $found = Search-Libraries $steam; if ($found) { return $found } }
    foreach ($guess in 'C:\Program Files (x86)\Steam', 'C:\Program Files\Steam', 'D:\SteamLibrary', 'D:\Steam', 'E:\SteamLibrary') {
        $found = Get-InLibrary $guess
        if ($found) { return $found }
    }
    # Bought from Minecraft.net (Minecraft Launcher) or the Xbox app: X:\XboxGames on any
    # local drive (removable ones are skipped: an empty card reader asks for a disk).
    foreach ($drive in [IO.DriveInfo]::GetDrives()) {
        if ($drive.DriveType -ne 'Fixed') { continue }
        foreach ($place in 'XboxGames\Minecraft Dungeons II', 'XboxGames\Minecraft Dungeons 2', 'Games\Minecraft Dungeons II', 'Games\Minecraft Dungeons 2') {
            $found = Get-GameAt (Join-Parts $drive.Name $place)
            if ($found) { return $found }
        }
    }
    $null
}
function Test-GameRunning {
    if ($env:WASDMOD_TEST_GAME) { return (Test-File $env:WASDMOD_TEST_GAME) } # tests: "running" while that file exists
    [bool](Get-Process -Name ($Exes | ForEach-Object { $_ -replace '\.exe$', '' }) -ErrorAction SilentlyContinue)
}

# ---------------------------------------------------------------- install / uninstall
$OK = 0; $ERR_RUNNING = 1; $ERR_OTHER_DLL = 2; $ERR_WRITE = 3
$NOT_INSTALLED = 0; $INSTALLED_LATEST = 1; $INSTALLED_OLDER = 2; $OTHER_DLL = 3; $TURNED_OFF = 4
$StateNames = 'not installed', 'installed, up to date', 'installed, older version', 'a different xinput1_4.dll is there', 'turned off'
function Get-ErrorText([int]$code) {
    switch ($code) {
        1 { T 'Minecraft Dungeons II is running. Close it, then try again.' }
        2 { T 'The game folder already has a different xinput1_4.dll (another mod?).' }
        3 { T 'Couldn''t write to the game folder.' }
    }
}
# One of the files beside this script.
function Get-Payload([string]$name) {
    $b = Read-Bytes (Join-Parts $Here $name)
    if ($null -eq $b) { $b = New-Object byte[] 0 }
    return , $b
}
function Test-SameBytes($a, $b) { $null -ne $a -and $null -ne $b -and $a.Length -eq $b.Length -and [Linq.Enumerable]::SequenceEqual([byte[]]$a, [byte[]]$b) }
function Test-OurDll([string]$path) {
    $b = Read-Bytes $path
    if (-not $b) { return $false }
    $text = $Latin1.GetString($b)
    $text.IndexOf('Controller mod loaded', $Ordinal) -ge 0 -or $text.IndexOf('WASD mod loaded', $Ordinal) -ge 0
}
function Get-InstallState([string]$game) {
    $dll = Join-Parts $game 'xinput1_4.dll'
    if (-not (Test-File $dll)) { return $(if (Test-OurDll "$dll.off") { $TURNED_OFF } else { $NOT_INSTALLED }) }
    if (-not (Test-OurDll $dll)) { return $OTHER_DLL }
    $(if (Test-SameBytes (Read-Bytes $dll) (Get-Payload 'xinput1_4.dll')) { $INSTALLED_LATEST } else { $INSTALLED_OLDER })
}
# The version in the game folder's mod (" wasdmod 1.3.1, " in its log line), or "" for versions before that line.
function Get-ModVersion([string]$game) {
    $b = Read-Bytes (Join-Parts $game 'xinput1_4.dll')
    if (-not $b) { $b = Read-Bytes (Join-Parts $game 'xinput1_4.dll.off') }
    if ($b -and $Latin1.GetString($b) -match ' wasdmod ([0-9][0-9.]*),') { $Matches[1] } else { '' }
}
# Saved key layouts: author.txt and the editor's wasdmod-MMDDYY.txt files, oldest first.
function Get-Layouts([string]$game) {
    @(@(Get-Files $game 'author.txt') + @(Get-Files $game 'wasdmod*.txt') | Sort-Object LastWriteTimeUtc)
}
# The settings file the mod uses: the newest saved layout when it's newer than default.txt.
function Get-ActiveSettings([string]$game) {
    $ini = Join-Parts $game 'default.txt'
    $saved = Get-Layouts $game | Select-Object -Last 1
    if ($saved -and (-not (Test-File $ini) -or $saved.LastWriteTimeUtc -gt [IO.File]::GetLastWriteTimeUtc($ini))) { $saved.FullName } else { $ini }
}
# Writes a layout, which makes it the newest file and so the one the mod uses; an edited copy is kept as NAME.bak.
function Set-Layout([string]$game, [string]$name, [byte[]]$data) {
    $path = Join-Parts $game $name
    if ((Test-File $path) -and -not (Test-SameBytes (Read-Bytes $path) $data)) { [void](Copy-File $path "$path.bak") }
    Write-Bytes $path $data
}
function Install-Mod([string]$game, [bool]$replaceOther) {
    if (Test-GameRunning) { return $ERR_RUNNING }
    $dll = Join-Parts $game 'xinput1_4.dll'
    $mod = Get-Payload 'xinput1_4.dll'
    if ((Test-File $dll) -and -not (Test-OurDll $dll)) {
        if (-not $replaceOther) { return $ERR_OTHER_DLL }
        if (-not (Copy-File $dll "$dll.other")) { return $ERR_WRITE }
    }
    # (written as new files, not copied: nothing of the zip's "downloaded from the internet" mark goes with them)
    if (-not $mod.Length -or -not (Write-Bytes $dll $mod)) { return $ERR_WRITE }
    if (Test-OurDll "$dll.off") { [void](Remove-File "$dll.off") } # installing turns it back on
    # Older versions named everything wasd-mod...: a saved layout keeps its date and its time stamp.
    foreach ($old in @(Get-Files $game 'wasd-mod*.txt')) {
        $to = Join-Parts $game ('wasdmod' + $old.Name.Substring(8))
        try { [void](Remove-File $to); [IO.File]::Move($old.FullName, $to) } catch {}
    }
    # The settings file used to be wasd-mod.ini, then wasdmod.ini; edited ones are kept as default.txt.bak.
    foreach ($name in 'wasd-mod.ini', 'wasdmod.ini') {
        $p = Join-Parts $game $name
        if (Test-File $p) {
            if (-not (Test-SameBytes (Read-Bytes $p) (Get-Payload 'default.txt'))) { [void](Copy-File $p (Join-Parts $game 'default.txt.bak')) }
            [void](Remove-File $p)
        }
    }
    [void](Remove-File (Join-Parts $game 'wasd-mod.log'))
    $was = Get-ActiveSettings $game # the layout in use before the update
    if (-not (Set-Layout $game 'default.txt' (Get-Payload 'default.txt'))) { return $ERR_WRITE }
    [void](Write-Bytes (Join-Parts $game 'Key Layout Editor.html') (Get-Payload 'Key Layout Editor.html'))
    # It stays in use: a saved layout is marked newer than the fresh default.txt (two seconds
    # newer, if "now" isn't later than that file's time).
    if ((Get-Leaf $was) -ne 'default.txt' -and (Test-File $was)) {
        try {
            $stamp = [DateTime]::UtcNow; $fresh = [IO.File]::GetLastWriteTimeUtc((Join-Parts $game 'default.txt'))
            if ($stamp -le $fresh) { $stamp = $fresh.AddSeconds(2) }
            [IO.File]::SetLastWriteTimeUtc($was, $stamp)
        } catch {}
    }
    $OK
}
function Uninstall-Mod([string]$game, [bool]$removeLayouts) {
    if (Test-GameRunning) { return $ERR_RUNNING }
    $dll = Join-Parts $game 'xinput1_4.dll'
    if ((Test-OurDll $dll) -and -not (Remove-File $dll)) { return $ERR_WRITE }
    if (Test-OurDll "$dll.off") { [void](Remove-File "$dll.off") }
    $gone = @('default.txt', 'default.txt.bak', 'wasdmod.log', 'wasdmod.old.log', 'wasdmod-record.flag', 'Key Layout Editor.html', 'wasdmod.ini', 'wasdmod.ini.bak', 'wasd-mod.ini', 'wasd-mod.ini.bak', 'wasd-mod.log')
    if ($removeLayouts) { $gone += @(@(Get-Layouts $game) + @(Get-Files $game 'wasd-mod*.txt') | ForEach-Object { $_.Name }) + 'author.txt.bak' }
    foreach ($name in $gone) { [void](Remove-File (Join-Parts $game $name)) }
    $OK
}
# Turning the mod off renames it to xinput1_4.dll.off: the game starts without it, and the
# layouts stay. Works with the game running too (it applies the next time the game starts).
function Set-ModOn([string]$game, [bool]$on) {
    $dll = Join-Parts $game 'xinput1_4.dll'
    try {
        if ($on) {
            if (-not (Test-File $dll)) {
                if (-not (Test-OurDll "$dll.off")) { return $ERR_WRITE }
                [IO.File]::Move("$dll.off", $dll)
            }
        } elseif (Test-OurDll $dll) {
            [IO.File]::Delete("$dll.off")
            [IO.File]::Move($dll, "$dll.off")
        }
    } catch { return $ERR_WRITE }
    $OK
}

# ---------------------------------------------------------------- Record Logs
#
# While wasdmod-record.flag is in the game folder, the mod logs each key and mouse
# button it handles. Stop and save logs writes one text file to the Desktop for a
# bug report: the system, the mod's state, the layout in use and the log.

# The end of a file: at most $keep bytes, from the start of a line; with -SettingsOnly, without comments.
function Get-Tail([string]$path, [int]$keep, [switch]$SettingsOnly) {
    $b = Read-Bytes $path
    if (-not $b) { return '' }
    $from = [Math]::Max(0, $b.Length - $keep)
    if ($from) { while ($from -lt $b.Length -and $b[$from - 1] -ne 10) { $from++ } }
    $lines = @($Utf8.GetString($b, $from, $b.Length - $from).Replace("`r", '').Split("`n"))
    if ($SettingsOnly) { $lines = @($lines | Where-Object { $_.Trim() -and -not $_.StartsWith(';', $Ordinal) }) }
    (($lines -join "`n").TrimEnd("`n")) + "`n"
}
# The report's file on the Desktop, or $null.
function Write-Report([string]$game) {
    $now = Get-Date
    $out = Join-Parts ([Environment]::GetFolderPath('DesktopDirectory')) ('wasdmod-logs-' + $now.ToString('yyyyMMdd-HHmm') + '.txt')
    $active = Get-ActiveSettings $game
    $shown = $game
    if ($env:USERPROFILE -and $game.StartsWith($env:USERPROFILE, [StringComparison]::OrdinalIgnoreCase)) { $shown = '%USERPROFILE%' + $game.Substring($env:USERPROFILE.Length) } # no user name in the report
    $text = 'wasdmod logs, ' + $now.ToString('yyyy-MM-dd HH:mm') + "`n"
    $text += "App: wasdmod for Windows (script) $Version`nSystem: Windows " + [Environment]::OSVersion.Version.ToString(3) + "`n"
    $text += "Game folder: $shown`nMod: " + ($StateNames[(Get-InstallState $game)] -replace ' is there$', '') + "`n"
    $text += 'Layout in use: ' + $(if (Test-File $active) { Get-Leaf $active } else { 'none' }) + "`n"
    if (Test-File $active) { $text += "`n===== " + (Get-Leaf $active) + " =====`n" + (Get-Tail $active (64 * 1024) -SettingsOnly) }
    $old = Join-Parts $game 'wasdmod.old.log'
    if (Test-File $old) { $text += "`n===== wasdmod.old.log (end) =====`n" + (Get-Tail $old (32 * 1024)) }
    $text += "`n===== wasdmod.log =====`n"
    $log = Join-Parts $game 'wasdmod.log'
    if (Test-File $log) { $text += Get-Tail $log (1200 * 1024) } else { $text += "(no log yet: start the game once with wasdmod installed)`n" }
    if (Write-Bytes $out $Utf8.GetBytes($text)) { $out } else { $null }
}

# ---------------------------------------------------------------- messages and file dialogs
# Windows' own, in front of the browser (their owner is a window nobody sees, kept on top).
Add-Type -AssemblyName System.Windows.Forms
[Windows.Forms.Application]::EnableVisualStyles()
function New-Owner {
    $f = New-Object Windows.Forms.Form
    $f.TopMost = $true; $f.ShowInTaskbar = $false; $f.FormBorderStyle = 'None'; $f.Opacity = 0; $f.StartPosition = 'CenterScreen'; $f.Width = 1; $f.Height = 1
    $f.Show(); $f.Activate()
    $f
}
function Show-Message([string]$text, [switch]$Warning) {
    if ($Headless) { Write-Host $text; return }
    $o = New-Owner
    [void][Windows.Forms.MessageBox]::Show($o, $text, 'wasdmod', 'OK', $(if ($Warning) { 'Warning' } else { 'Information' }))
    $o.Close()
}
# A question: "yes", "no", or (with -Cancel) "cancel". Without messages: "no".
function Show-Question([string]$text, [switch]$Cancel) {
    if ($Headless) { return 'no' }
    $o = New-Owner
    $r = [Windows.Forms.MessageBox]::Show($o, $text, 'wasdmod', $(if ($Cancel) { 'YesNoCancel' } else { 'YesNo' }), 'Question')
    $o.Close()
    $r.ToString().ToLowerInvariant()
}
function Get-OpenFile([string]$title, [string]$filter) {
    if ($Headless) { return $null }
    $d = New-Object Windows.Forms.OpenFileDialog
    $d.Title = $title; $d.Filter = $filter; $d.CheckFileExists = $true
    $o = New-Owner
    $r = $d.ShowDialog($o)
    $o.Close()
    if ($r -eq 'OK') { $d.FileName } else { $null }
}
function Get-SaveFile([string]$name) {
    if ($Headless) { return $null }
    $d = New-Object Windows.Forms.SaveFileDialog
    $d.FileName = $name; $d.DefaultExt = 'txt'; $d.Filter = (T 'Key layout (*.ini;*.txt)') + '|*.ini;*.txt|' + (T 'All files') + '|*.*'
    $o = New-Owner
    $r = $d.ShowDialog($o)
    $o.Close()
    if ($r -eq 'OK') { $d.FileName } else { $null }
}

# ---------------------------------------------------------------- the mod's state, for the editor page
#
# status when the page loads, then on every change, as in the app: text is the status
# line; state is none (also another mod's file), older, current or off, for the page's
# mod buttons; found is false without a game folder; busy while one of the page's
# buttons is at work.
$Game = $null # the game folder
$Busy = $false
$StatusNow = ''; $StatusSeq = 0
$NotFound = N_ 'Game not found in Steam or XboxGames. Under Game folder, click Choose game folder… and pick Dungeons*.exe (in Dungeons\Binaries) or Dungeons.exe.'
# Text as a JSON string; < as \u003c so it can't end a script.
function ConvertTo-JsonText([string]$s) {
    $b = New-Object Text.StringBuilder
    [void]$b.Append('"')
    foreach ($ch in $s.ToCharArray()) {
        $c = [int]$ch
        if ($c -eq 34 -or $c -eq 92) { [void]$b.Append('\').Append($ch) }
        elseif ($c -lt 32 -or $c -eq 60) { [void]$b.Append('\u').Append($c.ToString('x4')) }
        else { [void]$b.Append($ch) }
    }
    [void]$b.Append('"')
    $b.ToString()
}
function Get-OlderText {
    $v = Get-ModVersion $Game
    $running = Test-GameRunning
    if (-not $v) { return $(if ($running) { T 'The mod in the game is an older version. Quit the game, then click Update Mod.' } else { T 'The mod in the game is an older version. Click Update Mod to update it.' }) }
    $(if ($running) { T 'The mod in the game is still {version}; this app is {app}. Quit the game, then click Update Mod.' @{ version = $v; app = $Version } }
      else { T 'The mod in the game is still {version}; this app is {app}. Click Update Mod to update it.' @{ version = $v; app = $Version } })
}
function Get-StatusJson {
    $state = $(if ($Game) { Get-InstallState $Game } else { $NOT_INSTALLED })
    $text = $(if (-not $Game) { T $NotFound }
        elseif ($state -eq $INSTALLED_LATEST) { T 'Installed and up to date. Start (or restart) the game to use it.' }
        elseif ($state -eq $INSTALLED_OLDER) { Get-OlderText }
        elseif ($state -eq $OTHER_DLL) { T 'The game folder has a different xinput1_4.dll (another mod?). Install Mod replaces it and keeps a copy.' }
        elseif ($state -eq $TURNED_OFF) { T 'Disabled: the game starts without wasdmod. Your layouts are kept; click Enable to use it again.' }
        else { T 'Not installed yet. Quit the game, then click Install Mod.' })
    $recording = $Game -and (Test-File (Join-Parts $Game 'wasdmod-record.flag'))
    $name = $(if ($state -eq $INSTALLED_OLDER) { 'older' } elseif ($state -eq $INSTALLED_LATEST) { 'current' } elseif ($state -eq $TURNED_OFF) { 'off' } else { 'none' })
    '{"text":' + (ConvertTo-JsonText $text) + ',"recording":' + ([bool]$recording).ToString().ToLower() + ',"found":' + ([bool]$Game).ToString().ToLower() +
        ',"state":"' + $name + '","busy":' + ([bool]$Busy).ToString().ToLower() + '}'
}
# Looked at again after anything that may have changed it: a new state gets the next
# number, and the pages that wait for one are answered (Step-Waiting, below).
function Update-Status {
    $now = Get-StatusJson
    if ($now -ne $script:StatusNow -or -not $script:StatusSeq) { $script:StatusNow = $now; $script:StatusSeq++ }
}

# ---------------------------------------------------------------- the editor page's own buttons
function Invoke-Install {
    $r = Install-Mod $Game $false
    if ($r -eq $ERR_OTHER_DLL -and -not $Headless) {
        if ((Show-Question (T 'The game folder already has a different xinput1_4.dll (probably another mod). Replace it? A copy is kept as xinput1_4.dll.other.')) -ne 'yes') { return $null }
        $r = Install-Mod $Game $true
    }
    Update-Status
    if ($r -ne $OK) { Show-Message (Get-ErrorText $r) -Warning; return (Get-ErrorText $r) }
    if (-not $Headless) { Show-Message (T 'Installed. Start (or restart) Minecraft Dungeons II.\n\nIn game: WASD moves, Tab opens the menu wheel (the game''s own key, S, moves you now), F9 shows the key list, and the backtick key (`) turns the mod off and on.') }
    $null
}
function Invoke-Toggle {
    $on = (Get-InstallState $Game) -eq $TURNED_OFF
    $r = Set-ModOn $Game $on
    Update-Status
    if ($r -ne $OK) { $failed = T 'Couldn''t rename xinput1_4.dll in the game folder. Close the game and try again.'; Show-Message $failed -Warning; return $failed }
    if (-not $Headless) {
        Show-Message $(if ($on) { T 'Enabled. Start (or restart) the game to use wasdmod again.' }
                       else { T 'Disabled. From the next game start, the game runs without wasdmod; your layouts are kept.\n\n(In a running game, the backtick key (`) turns it off right away.)' })
    }
    $null
}
# (Asked about the saved layouts first, when there are any: Cancel leaves the mod installed.)
function Invoke-Uninstall {
    if (Test-GameRunning) { Show-Message (Get-ErrorText $ERR_RUNNING) -Warning; return (Get-ErrorText $ERR_RUNNING) } # (said before the question: nothing would be removed anyway)
    $answer = $(if (@(Get-Layouts $Game).Count) { Show-Question (T 'Also delete your saved key layouts (author.txt, wasdmod*.txt)?') -Cancel } else { 'no' })
    if ($answer -eq 'cancel') { return $null }
    $r = Uninstall-Mod $Game ($answer -eq 'yes')
    Update-Status
    if ($r -ne $OK) { Show-Message (Get-ErrorText $r) -Warning; return (Get-ErrorText $r) }
    if (-not $Headless) { Show-Message (T 'Uninstalled. The game is back to how it was.') }
    $null
}
function Invoke-Record {
    $flag = Join-Parts $Game 'wasdmod-record.flag'
    if (-not (Test-File $flag)) {
        $ok = Write-Bytes $flag (New-Object byte[] 0)
        Update-Status
        if (-not $ok) { Show-Message (Get-ErrorText $ERR_WRITE) -Warning }
        elseif (-not $Headless) { Show-Message (T 'Recording logs. Play until the problem happens, then come back here and click Stop and save logs.\n\nIf the game is running, recording starts within a second; otherwise it starts with the game. Nothing you type is recorded.') }
        return
    }
    [void](Remove-File $flag)
    Update-Status
    $out = Write-Report $Game
    if (-not $out) { Show-Message (T 'Couldn''t save the logs on the Desktop.') -Warning; return }
    if ($Headless) { Write-Host $out; return }
    Start-Process -FilePath 'explorer.exe' -ArgumentList ('/select,"' + $out + '"') # the file, shown in its folder
    if ((Show-Question (T 'Saved {file} on your Desktop. It has the mod''s log, your key layout and your system''s version; nothing you type is recorded.\n\nOpen GitHub to report the problem (attach the file)?' @{ file = (Get-Leaf $out) })) -eq 'yes') { Start-Process -FilePath $IssuesPage }
}
function Invoke-ChooseFolder {
    $pick = Get-OpenFile (T 'Find Minecraft Dungeons II') ((T 'Minecraft Dungeons II (*.exe)') + '|*.exe|' + (T 'All files') + '|*.*')
    if (-not $pick) { return 'cancelled' }
    $found = Get-DirFromPick $pick
    if (-not $found) { Show-Message (T 'That isn''t the Minecraft Dungeons II folder. Pick Dungeons*.exe (in Dungeons\Binaries) or Dungeons.exe.') -Warning; return 'cancelled' }
    $script:Game = $found
    [void](Set-StoreFile 'game-folder' $Utf8.GetBytes($found)) # for the next start
    'done'
}
# Export... in the page: a Save As dialog for the layout's file.
function Invoke-Export([string]$name, [byte[]]$body) {
    $name = [regex]::Replace((Get-Leaf $name), '[\\/:*?"<>|\x00-\x1f]', '-')
    $out = Get-SaveFile $name
    if (-not $out) { return 'cancelled' }
    [IO.File]::WriteAllBytes($out, $body) # (an error here is the request's answer, in Windows' own words)
    'saved'
}
#if !NEXUS
# The newest version on GitHub ("1.4.1"), or $null when GitHub can't be asked. One
# request, with nothing about this computer in it; nothing is downloaded but its
# answer, which is only read.
function Get-NewestRelease {
    [void](Set-StoreFile 'last-update-check' $Utf8.GetBytes($Version)) # asked today
    try {
        [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
        $r = Invoke-RestMethod -Uri $ReleasesApi -UseBasicParsing -TimeoutSec 10 -UserAgent "wasdmod-script/$Version" -Headers @{ Accept = 'application/vnd.github+json' }
        $tag = [string]$r.tag_name
    } catch { return $null }
    if (-not $tag) { return $null }
    $tag.TrimStart('v', 'V')
}
# "1.10.0" > "1.9.2"; a version that isn't numbers is never newer.
function Test-Newer([string]$a, [string]$b) {
    if ($a -notmatch '^[0-9]+(\.[0-9]+)*$' -or $b -notmatch '^[0-9]') { return $false }
    $x = @($a.Split('.') | ForEach-Object { [int]$_ }); $y = @([regex]::Matches($b, '\d+') | ForEach-Object { [int]$_.Value })
    for ($i = 0; $i -lt [Math]::Max($x.Count, $y.Count); $i++) {
        $p = $(if ($i -lt $x.Count) { $x[$i] } else { 0 }); $q = $(if ($i -lt $y.Count) { $y[$i] } else { 0 })
        if ($p -ne $q) { return $p -gt $q }
    }
    $false
}
# A newer version: said, with the way to its download page (nothing is downloaded here).
function Show-Update([string]$version) {
    Write-Host ((T 'wasdmod {version} is available' @{ version = $version }) + ": $ReleasesPage")
    if ((Show-Question (T 'wasdmod {version} is available. Open its download page?' @{ version = $version })) -eq 'yes') { Start-Process -FilePath $ReleasesPage }
}
# At most once a day, once the editor has opened: only tells, and says nothing when GitHub can't be reached.
function Invoke-DailyCheck {
    $last = Get-StoreFile 'last-update-check'
    if ((Test-File $last) -and [Math]::Abs(([DateTime]::UtcNow - [IO.File]::GetLastWriteTimeUtc($last)).TotalHours) -lt 24) { return }
    $version = Get-NewestRelease
    if (-not $version -or -not (Test-Newer $version $Version) -or $Busy) { return }
    $script:Busy = $true; Update-Status
    try { Show-Update $version } finally { $script:Busy = $false; Update-Status }
}
#endif
# Check for updates in the page. Without messages, the answer is the line to show.
function Invoke-Updates {
#if !NEXUS
    $version = Get-NewestRelease
    if ($null -eq $version) { $said = T 'Couldn''t reach GitHub. Check the internet connection and try again.' }
    elseif (Test-Newer $version $Version) { Show-Update $version; return $(if ($Headless) { T 'wasdmod {version} is available' @{ version = $version } } else { '' }) }
    else { $said = T 'wasdmod {version} is the newest version.' @{ version = $Version } }
    if ($Headless) { return $said }
    Show-Message $said -Warning:($null -eq $version)
#else
    if (-not $Headless) { Start-Process -FilePath $NexusPage } # the browser; nothing is downloaded here
#endif
    ''
}
# The answer to one of the page's buttons that came while another one was at work.
function Get-BusyAnswer([string]$what) {
    if ($what -eq 'mod') { throw 'busy' }
    if ($what -eq 'record') { return $(if ($Game -and (Test-File (Join-Parts $Game 'wasdmod-record.flag'))) { 'on' } else { 'off' }) }
    $(if ($what -eq 'find-game' -or $what -eq 'updates') { '' } else { 'cancelled' })
}
# One of the page's buttons, one at a time: the answer's text, or an error with the reason.
# (While its message or file dialog is up, the page's other requests are still answered:
# see $Pump below. Another button clicked meanwhile gets the answer for "busy".)
function Invoke-PageCall([string]$what, [string]$arg, [byte[]]$body) {
    if ($what -eq 'show-folder') { if ($Game -and -not $Headless) { Start-Process -FilePath 'explorer.exe' -ArgumentList ('"' + $Game + '"') }; return '' }
    if ($Busy) { return (Get-BusyAnswer $what) }
    $script:Busy = $true; Update-Status # (the page greys out the mod's buttons)
    try {
        if ($what -eq 'updates') { return (Invoke-Updates) }
        if ($what -eq 'choose-folder') { return (Invoke-ChooseFolder) }
        if ($what -eq 'find-game') { [void](Remove-File (Get-StoreFile 'game-folder')); $script:Game = Find-Game; return '' }
        if ($what -eq 'export') { return (Invoke-Export $arg $body) }
        if (-not $Game) { throw (T $NotFound) }
        if ($what -eq 'record') { Invoke-Record; return $(if (Test-File (Join-Parts $Game 'wasdmod-record.flag')) { 'on' } else { 'off' }) }
        $failed = $(switch ($arg) { 'install' { Invoke-Install } 'toggle' { Invoke-Toggle } 'uninstall' { Invoke-Uninstall } default { 'Unknown request.' } })
        if ($failed) { throw $failed }
        ''
    } finally { $script:Busy = $false; Update-Status }
}

# ---------------------------------------------------------------- the editor's server
#
# GET /<token>/ is the page; everything else is POST /<token>/<name>[?<argument>], as
# listed in the page's first script. Anything without the token is answered 404. A
# request's body is at most 2 MB.
$MaxReq = 2MB
$raw = New-Object byte[] 16
(New-Object Security.Cryptography.RNGCryptoServiceProvider).GetBytes($raw)
$Token = -join ($raw | ForEach-Object { $_.ToString('x2') }) # 32 hex digits, new at every start
# The game's own keyboard settings (Settings > Controls > Keyboard).
$Controls = Join-Parts $env:LOCALAPPDATA 'Dungeons2\Saved\SaveGames\EnhancedInputUserSettings.sav'
# The file's stamp: its size and the time it was written ("" when there's no file).
function Get-ControlsStamp {
    try { $f = New-Object IO.FileInfo($Controls); if ($f.Exists) { '{0:x}-{1:x}' -f $f.Length, $f.LastWriteTimeUtc.Ticks } else { '' } } catch { '' }
}
# "name=..." in a request's address, as text.
function Get-QueryName([string]$query) {
    if ($query -match '^name=([^&]*)') { try { [Uri]::UnescapeDataString($Matches[1].Replace('+', ' ')) } catch { '' } } else { '' }
}
# A layout from the editor, into the game folder: the answer's code and text, as the app's.
function Save-Layout([string]$name, [byte[]]$body) {
    $keep = $name -eq 'author.txt' -or $name -eq 'default.txt' -or ($name.Length -gt 11 -and $name -like 'wasdmod*.txt')
    if (-not $keep -or $name -match '[\\/:\x00-\x1f]') { $name = 'wasdmod.txt' } # only the editor's names; anything else is wasdmod.txt
    if (-not $Game -or -not (Test-GameDir $Game)) { return 409, (T $NotFound) }
    $text = $Latin1.GetString($body)
    if ($text.IndexOf('[Buttons]', $Ordinal) -lt 0 -and $text.IndexOf('[Move]', $Ordinal) -lt 0) { return 400, (T 'That isn''t a key layout.') }
    if (-not (Set-Layout $Game $name $body)) { return 500, (Get-ErrorText $ERR_WRITE) }
    $state = Get-InstallState $Game
    Update-Status
    # Saved but not in use yet: "saved:" tells the editor it isn't an error.
    if ($state -eq $TURNED_OFF) { return 409, ('saved:' + (T 'Saved, but wasdmod is disabled: click Enable at the top.')) }
    if ($state -ne $INSTALLED_LATEST -and $state -ne $INSTALLED_OLDER) { return 409, ('saved:' + (T 'Saved, but the mod isn''t installed yet: click Install Mod at the top.')) }
    200, (Get-Leaf (Get-ActiveSettings $Game))
}
# The game's keyboard settings from the editor (base64). The first write keeps the original.
function Write-Controls([byte[]]$body) {
    if (-not (Test-File $Controls)) { return 500, (T 'The in-game controls file wasn''t found. Change any key in the game''s settings once, then try again.') }
    if (Test-GameRunning) { return 500, (Get-ErrorText $ERR_RUNNING) }
    try { $data = [Convert]::FromBase64String($Latin1.GetString($body)) } catch { $data = New-Object byte[] 0 }
    if ($data.Length -lt 8 -or $Latin1.GetString($data, 0, 4) -cne 'GVAS') { return 500, 'Not a settings file.' }
    if (-not (Test-File "$Controls.wasdmod-backup") -and -not (Copy-File $Controls "$Controls.wasdmod-backup")) { return 500, (T 'Couldn''t write the in-game controls file.') }
    if (-not (Write-Bytes $Controls $data)) { return 500, (T 'Couldn''t write the in-game controls file.') }
    200, 'ok'
}
# The editor with what makes its window.wasdmodHost, after the page's first four lines:
# the saved layouts and this script's settings as two JSON script elements (see the
# page's first script).
function Get-Page {
    $html = Get-Payload 'Key Layout Editor.html'
    $at = 0
    for ($i = 0; $i -lt 4; $i++) { $at = [Array]::IndexOf($html, [byte]10, $at) + 1 }
    $saved = Read-Bytes (Get-StoreFile 'editor.json')
    if (-not $saved -or $saved[0] -ne 123 -or $Latin1.GetString($saved).IndexOf('</', $Ordinal) -ge 0) { $saved = $Utf8.GetBytes('{}') }
    $inUse = 'null' # the layout the game uses, so the editor can show it
    if ($Game -and (Get-InstallState $Game) -ne $NOT_INSTALLED) {
        $path = Get-ActiveSettings $Game
        $text = Read-Bytes $path
        if ($null -ne $text) { $inUse = '{"file":' + (ConvertTo-JsonText (Get-Leaf $path)) + ',"text":' + (ConvertTo-JsonText $Utf8.GetString($text)) + '}' }
    }
    $hostJson = '{"base":"/' + $Token + '/","app":"win","version":' + (ConvertTo-JsonText $Version) + ',"updates":"' + $Updates + '","lang":"' + $Lang + '","game":' + $inUse +
        ',"status":' + $StatusNow + ',"seq":' + $StatusSeq + '}'
    $out = New-Object IO.MemoryStream
    $out.Write($html, 0, $at)
    foreach ($piece in $Utf8.GetBytes('<script type="application/json" id="wasdmod-store">'), $saved, $Utf8.GetBytes("</script>`n<script type=`"application/json`" id=`"wasdmod-host`">" + $hostJson + "</script>`n")) { $out.Write($piece, 0, $piece.Length) }
    $out.Write($html, $at, $html.Length - $at)
    return , $out.ToArray()
}

# One connection at a time is read, answered and closed here; a request that waits
# (status-wait, controls-wait) stays in $Conns until it has its answer. For ending when
# the editor is closed: when the page's last request ended, whether there has been a
# page, and whether the page dropped a request that waited (which is what closing its
# window does).
$Conns = New-Object Collections.ArrayList
$PageSeen = $null # when the page was first asked for
$LastSeen = [DateTime]::UtcNow
$Dropped = $false
$Listener = $null
function Close-Conn($c) {
    try { $c.client.Close() } catch {}
    $c.closed = $true
    $Conns.Remove($c)
    if ($c.ours) { $script:LastSeen = [DateTime]::UtcNow }
}
function Send-Reply($c, [int]$code, $body, [string]$kind = 'text/plain; charset=utf-8') {
    if ($body -is [string]) { $body = $Utf8.GetBytes($body) }
    if ($null -eq $body) { $body = New-Object byte[] 0 }
    $names = @{ 200 = 'OK'; 400 = 'Bad Request'; 404 = 'Not Found'; 409 = 'Conflict'; 413 = 'Payload Too Large'; 500 = 'Internal Server Error' }
    $head = $Latin1.GetBytes("HTTP/1.1 $code $($names[$code])`r`nContent-Type: $kind`r`nCache-Control: no-store`r`nConnection: close`r`nContent-Length: $($body.Length)`r`n`r`n")
    try { $c.stream.Write($head, 0, $head.Length); $c.stream.Write($body, 0, $body.Length); $c.stream.Flush() } catch { if ($c.ours) { $script:Dropped = $true } } # (the page is gone)
    Close-Conn $c
}
# The page closed this request (its window was closed).
function Test-Gone($c) {
    try { $c.client.Client.Poll(0, [Net.Sockets.SelectMode]::SelectRead) -and $c.client.Available -eq 0 } catch { $true }
}
# "GET /<token>/", "POST /<token>/store", "POST /<token>/save?name=...", "POST /<token>/mod?install"
function Invoke-Request($c, [string]$method, [string]$path, [byte[]]$body) {
    if (-not $path.StartsWith("/$Token/", $Ordinal)) { return (Send-Reply $c 404 'Not found.') }
    $c.ours = $true; $script:Dropped = $false
    $rest = $path.Substring($Token.Length + 2)
    if ($method -eq 'GET') {
        if ($rest) { return (Send-Reply $c 404 'Not found.') }
        Update-Status # (the page starts with the state as it is now)
        if (-not $PageSeen) { $script:PageSeen = [DateTime]::UtcNow }
        return (Send-Reply $c 200 (Get-Page) 'text/html; charset=utf-8')
    }
    $name, $arg = $rest.Split([char[]]'?', 2)
    if ($null -eq $arg) { $arg = '' }
    switch -CaseSensitive ($name) {
        'store' {
            $ok = $body.Length -and $body[0] -eq 123 -and (Set-StoreFile 'editor.json' $body)
            if ($ok) { Send-Reply $c 200 '' } else { Send-Reply $c 500 'Couldn''t keep the layouts.' }
        }
        'lang' { $code = Get-LangOf $arg; if ($code) { $script:Lang = $code; Update-Status }; Send-Reply $c 200 '' }
        'save' { $r = Save-Layout (Get-QueryName $arg) $body; Send-Reply $c $r[0] $r[1] }
        'controls' { $d = Read-Bytes $Controls; Send-Reply $c 200 $(if ($d) { [Convert]::ToBase64String($d) } else { '' }) }
        'controls-write' { $r = Write-Controls $body; Send-Reply $c $r[0] $r[1] }
        'controls-wait' { $c.wait = 'controls'; $c.arg = $arg; $c.until = [DateTime]::UtcNow.AddSeconds(25); $c.next = [DateTime]::UtcNow }
        'status-wait' { Update-Status; $c.wait = 'status'; $c.arg = $arg; $c.until = [DateTime]::UtcNow.AddSeconds(25) } # (what happened outside, like the game being closed, is seen here)
        { $_ -cin 'mod', 'choose-folder', 'show-folder', 'find-game', 'record', 'export', 'updates' } {
            if ($name -eq 'export') { $arg = Get-QueryName $arg }
            try { $said = Invoke-PageCall $name $arg $body; Send-Reply $c 200 ([string]$said) }
            catch { $e = $_.Exception; while ($e.InnerException) { $e = $e.InnerException }; Send-Reply $c 500 ([string]$e.Message) } # (the reason, in its own words)
        }
        default { Send-Reply $c 404 'Not found.' }
    }
}
# What has arrived on a connection: once the request is whole, it's answered.
function Read-Conn($c) {
    $n = $c.client.Available
    if ($n -le 0) {
        if ((Test-Gone $c) -or ([DateTime]::UtcNow - $c.since).TotalSeconds -gt 15) { Close-Conn $c } # (closed, or no request within 15 seconds)
        return
    }
    $chunk = New-Object byte[] ([Math]::Min($n, 65536))
    $got = $c.stream.Read($chunk, 0, $chunk.Length)
    $c.buf.Write($chunk, 0, $got)
    if ($c.headEnd -lt 0) {
        $head = $Latin1.GetString($c.buf.GetBuffer(), 0, [int][Math]::Min($c.buf.Length, 16384))
        $end = $head.IndexOf("`r`n`r`n", $Ordinal)
        if ($end -lt 0) { if ($c.buf.Length -ge 16384) { Close-Conn $c }; return }
        $c.headEnd = $end + 4
        $c.head = $head.Substring(0, $end)
        $c.want = $(if ($c.head -match '(?im)^content-length:\s*(\d{1,9})\s*$') { [int]$Matches[1] } else { 0 })
        if ($c.want -gt $MaxReq) { $c.done = $true; return (Send-Reply $c 413 'Too large.') }
    }
    if ($c.buf.Length -lt $c.headEnd + $c.want) { return }
    $line = $c.head.Split([char[]]"`r`n")[0].Split([char[]]' ')
    $body = New-Object byte[] ([int]$c.want)
    [Array]::Copy($c.buf.GetBuffer(), [int]$c.headEnd, $body, 0, [int]$c.want)
    $c.done = $true
    if ($line.Count -lt 2 -or ($line[0] -cne 'GET' -and $line[0] -cne 'POST')) { return (Send-Reply $c 404 'Not found.') }
    Invoke-Request $c $line[0] $line[1] $body
}
# The requests that wait: answered once what they wait for has changed, or after 25 seconds.
function Step-Waiting($c) {
    $now = [DateTime]::UtcNow
    if (Test-Gone $c) { $script:Dropped = $true; return (Close-Conn $c) }
    if ($c.wait -eq 'status') {
        if ([string]$StatusSeq -ne $c.arg -or $now -ge $c.until) { Send-Reply $c 200 ('{"seq":' + $StatusSeq + ',"status":' + $StatusNow + '}') 'application/json; charset=utf-8' }
    } elseif ($now -ge $c.next) { # (the file is looked at twice a second while a page waits)
        $c.next = $now.AddMilliseconds(500)
        $stamp = Get-ControlsStamp
        if ($stamp -ne $c.arg -or $now -ge $c.until) { Send-Reply $c 200 $stamp }
    }
}
# Edge (on every Windows 10/11 PC) or Chrome, whose --app mode is a plain window; otherwise the default browser.
function Open-Editor([string]$url) {
    $exe = $null
    foreach ($name in 'msedge.exe', 'chrome.exe') {
        foreach ($root in 'HKLM:', 'HKCU:') {
            $p = Get-RegString "$root\SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\$name" '(default)'
            if ($p) { $p = $p.Trim('"') }
            if (-not $exe -and $p -and (Test-File $p)) { $exe = $p }
        }
    }
    foreach ($guess in 'C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe', 'C:\Program Files\Microsoft\Edge\Application\msedge.exe') { if (-not $exe -and (Test-File $guess)) { $exe = $guess } }
    if ($exe) { Start-Process -FilePath $exe -ArgumentList "--app=$url", '--window-size=1200,860' }
    else { Start-Process -FilePath $url }
}
# One pass over the connections: new ones are taken, requests that have arrived are answered,
# and those that wait get their answer when it's due. True when something came in.
function Step-Server {
    $did = $false
    while ($Listener.Pending()) {
        $client = $Listener.AcceptTcpClient(); $client.SendTimeout = 15000; $client.NoDelay = $true
        [void]$Conns.Add(@{ client = $client; stream = $client.GetStream(); buf = (New-Object IO.MemoryStream); headEnd = -1; head = ''; want = 0; done = $false; closed = $false
                            ours = $false; wait = $null; arg = ''; until = [DateTime]::UtcNow; next = [DateTime]::UtcNow; since = [DateTime]::UtcNow })
        $did = $true
    }
    foreach ($c in @($Conns)) {
        if ($c.closed) { continue }
        if ($c.wait) { Step-Waiting $c }
        elseif (-not $c.done) { if ($c.client.Available -gt 0) { $did = $true }; Read-Conn $c }
    }
    $did
}
# Sleeps until something arrives on a connection or one is closed, a quarter of a second at most.
function Wait-Server {
    $read = New-Object Collections.ArrayList
    [void]$read.Add($Listener.Server)
    foreach ($c in $Conns) { [void]$read.Add($c.client.Client) }
    try { [Net.Sockets.Socket]::Select($read, $null, $null, 250000) } catch { Start-Sleep -Milliseconds 100 }
}
# While a message or a file dialog of this script's is up, Windows runs its own loop on this
# thread. This timer only fires in such a loop, and keeps the editor's requests answered
# meanwhile: the status line behind the message is up to date, and another button gets "busy".
$Pump = New-Object Windows.Forms.Timer
$Pump.Interval = 50
$Pump.add_Tick({ try { [void](Step-Server) } catch {} })
function Start-Server([bool]$open) {
    $script:Listener = New-Object Net.Sockets.TcpListener([Net.IPAddress]::Loopback, 0) # this computer only; port 0: any free one
    $Listener.Start()
    $url = "http://127.0.0.1:$(([Net.IPEndPoint]$Listener.LocalEndpoint).Port)/$Token/"
    Update-Status
    if (-not $open) { [Console]::Out.WriteLine($url); [Console]::Out.Flush() }
    else {
        Write-Host (T 'wasdmod is running: the key layout editor is open in your browser.')
        Write-Host (T 'To quit, close the editor''s window, or press Ctrl+C here.')
        Open-Editor $url
    }
    $Pump.Start()
    $checked = -not $open
    try {
        while ($true) {
            $did = Step-Server
            $idle = ([DateTime]::UtcNow - $LastSeen).TotalSeconds
            if (@($Conns | Where-Object { $_.ours }).Count) { $idle = 0 }
#if !NEXUS
            if (-not $checked -and $PageSeen -and ([DateTime]::UtcNow - $PageSeen).TotalSeconds -gt 3) { $checked = $true; Invoke-DailyCheck }
#endif
            # Until the editor is closed: the page dropped a request that waited (an open page always has
            # one waiting, status-wait) and has asked nothing for 10 seconds since. A page that only went
            # quiet may be asleep in a browser tab: the script stays for it (half a day at most).
            if (($Dropped -and $idle -gt 10) -or $idle -gt 43200) { break }
            if (-not $did) { Wait-Server }
        }
    } finally {
        $Pump.Stop()
        foreach ($c in @($Conns)) { try { $c.client.Close() } catch {} }
        $Listener.Stop()
    }
}

# ---------------------------------------------------------------- entry
$commands = 'status', 'install', 'uninstall', 'on', 'off', 'find'
$rest = @($args | Where-Object { $_ -ne '--no-open' })
$Headless = @($args) -contains '--no-open'
$command = $null
if ($rest.Count -and $commands -contains $rest[0]) { $command = $rest[0]; $rest = @($rest | Select-Object -Skip 1) }
if ($rest.Count) { # the game's folder or exe, after the command
    $Game = Get-DirFromPick ([string]$rest[0])
    if (-not $Game) { Write-Output 'Game not found there; pass its folder or executable.'; exit 1 }
} else {
    $kept = Read-Bytes (Get-StoreFile 'game-folder') # one picked by hand wins while it exists
    if ($kept) { $kept = $Utf8.GetString($kept).Trim(); if ($kept.IndexOfAny([IO.Path]::GetInvalidPathChars()) -lt 0 -and (Test-GameDir $kept)) { $Game = $kept } }
    if (-not $Game) { $Game = Find-Game }
}
if (-not $command) {
    if (-not (Get-Payload 'Key Layout Editor.html').Length -or -not (Get-Payload 'xinput1_4.dll').Length) {
        Write-Output 'Key Layout Editor.html and xinput1_4.dll must be in the folder this script is in: unzip the whole wasdmod folder.'; exit 1
    }
    Set-Language
    Start-Server (-not $Headless)
    exit 0
}
$Headless = $true
if (-not $Game) { Write-Output $(if ($command -eq 'find') { 'not found' } else { 'Game not found; pass its folder or executable.' }); exit 1 }
$code = 0
switch ($command) {
    'find' { Write-Output $Game }
    'status' {
        $code = Get-InstallState $Game
        $line = $StateNames[$code]
        if ($code -in $INSTALLED_LATEST, $INSTALLED_OLDER, $TURNED_OFF) { $line += ', key layout: ' + (Get-Leaf (Get-ActiveSettings $Game)) }
        Write-Output $line
    }
    'install' { $code = Install-Mod $Game $false; Write-Output $(if ($code) { Get-ErrorText $code } else { 'Installed.' }) }
    'uninstall' { $code = Uninstall-Mod $Game $false; Write-Output $(if ($code) { Get-ErrorText $code } else { 'Uninstalled.' }) }
    default {
        $on = $command -eq 'on'
        $code = $(if ((Get-InstallState $Game) -eq $NOT_INSTALLED) { 1 } else { Set-ModOn $Game $on })
        Write-Output $(if ($code) { 'Couldn''t switch it (not installed, or the file is in use).' } elseif ($on) { 'Turned on.' } else { 'Turned off.' })
    }
}
exit $code
