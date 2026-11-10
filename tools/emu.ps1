# PEPEL: copia el EBOOT al emulador de NOCTIS y lo lanza OCULTO (nunca aparece en pantalla).
# Usa el PPSSPP portable de noctis-psp y su vigia hider.ps1. Las capturas se sacan con
# noctis-psp\tools\emu\psp_eye.py (quedan en noctis-psp\build\).
$ErrorActionPreference = 'SilentlyContinue'
Add-Type @"
using System; using System.Runtime.InteropServices;
public class WH2 {
  public delegate bool P(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(P cb, IntPtr l);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int n);
  public static int HideAll(uint pid) { int n = 0;
    EnumWindows((h,l) => { uint p; GetWindowThreadProcessId(h, out p);
      if (p==pid && IsWindowVisible(h)) { ShowWindow(h, 0); n++; } return true; }, IntPtr.Zero);
    return n; }
}
"@
$home_repo = Split-Path -Parent $PSScriptRoot
$noctis = Join-Path (Split-Path -Parent $home_repo) 'noctis-psp'
$ppsspp = Join-Path $noctis 'ppsspp'
$ms = Join-Path $ppsspp 'memstick'
Get-Process PPSSPPWindows64 | Stop-Process -Force
Start-Sleep -Milliseconds 400

$dest = Join-Path (Join-Path (Join-Path $ms 'PSP') 'GAME') 'PEPEL'
New-Item -ItemType Directory -Force -Path $dest | Out-Null
Copy-Item (Join-Path (Join-Path (Join-Path $home_repo 'release') 'PEPEL') 'EBOOT.PBP') (Join-Path $dest 'EBOOT.PBP') -Force
$mus = Join-Path (Join-Path $home_repo 'assets') 'music.raw'   # [armas2]
if (Test-Path $mus) { Copy-Item $mus (Join-Path $dest 'music.raw') -Force }

$ini = Join-Path (Join-Path (Join-Path $ms 'PSP') 'SYSTEM') 'ppsspp.ini'
$c = [IO.File]::ReadAllLines($ini)
$c = $c -replace '^SoftwareRenderer = .*$', 'SoftwareRenderer = True' -replace '^InternalResolution = .*$', 'InternalResolution = 1' -replace '^GameVolume = .*$', 'GameVolume = 0'
[IO.File]::WriteAllLines($ini, $c)

$eboot = Join-Path $dest 'EBOOT.PBP'
$proc = Start-Process -FilePath (Join-Path $ppsspp 'PPSSPPWindows64.exe') -ArgumentList "`"$eboot`"" -WindowStyle Hidden -PassThru
$hider = Join-Path (Join-Path (Join-Path $noctis 'tools') 'emu') 'hider.ps1'
Start-Process powershell -WindowStyle Hidden -ArgumentList '-File', "`"$hider`"", '-ProcId', $proc.Id | Out-Null
$hid = 0; $t = [Diagnostics.Stopwatch]::StartNew()
while ($t.ElapsedMilliseconds -lt 8000) { $hid += [WH2]::HideAll([uint32]$proc.Id); Start-Sleep -Milliseconds 15 }
if (-not (Get-Process -Id $proc.Id)) { Write-Output 'PPSSPP se cerro (crash?)'; exit }
Write-Output ("HIDDEN ok pid={0} reocultadas={1}" -f $proc.Id, $hid)
