#!/bin/sh
# Builds xinput1_4.dll (and a small test program) with Apple clang + the lld-link
# that ships with the Rust toolchain. No Windows SDK or CRT is needed.
set -e
cd "$(dirname "$0")"
if [ -z "$LLD" ]; then
    for c in ~/.rustup/toolchains/stable-*/lib/rustlib/*/bin/gcc-ld/lld-link ~/.rustup/toolchains/*/lib/rustlib/*/bin/gcc-ld/lld-link; do
        "$c" --version >/dev/null 2>&1 && { LLD="$c"; break; }
    done
fi
[ -x "$LLD" ] || { echo "lld-link not found; set LLD=/path/to/lld-link"; exit 1; }
mkdir -p build
printf 'LIBRARY KERNEL32.dll\nEXPORTS\n%s\n' FreeLibrary GetSystemDirectoryA LoadLibraryA GetProcAddress GetModuleFileNameA GetPrivateProfileIntA GetPrivateProfileStringA GetCurrentProcessId CreateFileA WriteFile CloseHandle GetStdHandle ExitProcess GetCommandLineA GetTickCount64 Sleep CreateThread GetPrivateProfileSectionA GetFileAttributesExA QueryPerformanceCounter QueryPerformanceFrequency GetModuleHandleA VirtualProtect FindFirstFileA FindNextFileA FindClose ReadFile GetFileSize DeleteFileA CopyFileA GetFileAttributesA MoveFileExA SetFileTime GetSystemTimeAsFileTime GetProcessHeap HeapAlloc HeapFree GetEnvironmentVariableA CreateToolhelp32Snapshot Process32First Process32Next > build/kernel32.def
printf 'LIBRARY USER32.dll\nEXPORTS\n%s\n' GetAsyncKeyState GetForegroundWindow GetWindowThreadProcessId keybd_event mouse_event SetWindowsHookExW CallNextHookEx GetRawInputData EnumWindows CreateWindowExA DefWindowProcA PostMessageA PeekMessageA TranslateMessage GetFocus SetFocus RegisterClassA ShowWindow SetWindowPos GetClientRect ClientToScreen SetLayeredWindowAttributes GetMessageA DispatchMessageA SetTimer InvalidateRect BeginPaint EndPaint FillRect IsWindowVisible GetDC ReleaseDC GetCursorPos MapVirtualKeyA SetCursorPos ClipCursor RegisterClassA IsDialogMessageA PostQuitMessage SendMessageA SetWindowTextA MessageBoxA EnableWindow LoadCursorA LoadIconA AdjustWindowRect SetProcessDPIAware GetSysColorBrush > build/user32.def
printf 'LIBRARY GDI32.dll\nEXPORTS\n%s\n' CreateFontA SelectObject SetTextColor SetBkMode TextOutA CreateSolidBrush GetTextExtentPoint32A CreatePen Ellipse GetStockObject DeleteObject Polygon GetDeviceCaps > build/gdi32.def
printf 'LIBRARY IMM32.dll\nEXPORTS\n%s\n' ImmAssociateContext ImmCreateContext > build/imm32.def
"$LLD" /lib /machine:x64 /def:build/kernel32.def /out:build/kernel32.lib >/dev/null
"$LLD" /lib /machine:x64 /def:build/user32.def /out:build/user32.lib >/dev/null
"$LLD" /lib /machine:x64 /def:build/imm32.def /out:build/imm32.lib >/dev/null
"$LLD" /lib /machine:x64 /def:build/gdi32.def /out:build/gdi32.lib >/dev/null
printf 'LIBRARY ADVAPI32.dll\nEXPORTS\n%s\n' RegOpenKeyExA RegQueryValueExA RegCloseKey > build/advapi32.def
printf 'LIBRARY SHELL32.dll\nEXPORTS\n%s\n' ShellExecuteA > build/shell32.def
printf 'LIBRARY COMDLG32.dll\nEXPORTS\n%s\n' GetOpenFileNameA > build/comdlg32.def
for l in advapi32 shell32 comdlg32; do "$LLD" /lib /machine:x64 /def:build/$l.def /out:build/$l.lib >/dev/null; done
CFLAGS="--target=x86_64-pc-windows-msvc -O2 -fno-stack-protector -fno-builtin -Wall -Wno-incompatible-pointer-types -Wno-int-conversion"
clang $CFLAGS -c xinput_wasd.c -o build/xinput_wasd.obj
"$LLD" /dll /nodefaultlib /entry:DllMain /machine:x64 /def:xinput1_4.def /out:build/xinput1_4.dll build/xinput_wasd.obj build/kernel32.lib build/user32.lib build/gdi32.lib
clang $CFLAGS -c test_load.c -o build/test_load.obj
"$LLD" /nodefaultlib /entry:mainCRTStartup /subsystem:console /machine:x64 /out:build/test_load.exe build/test_load.obj build/kernel32.lib build/user32.lib build/imm32.lib
rm -f build/*.obj build/*.exp build/xinput1_4.lib
# Offline copy of the key configurator (the published page gets its document
# wrapper from the host; a local file needs its own doctype).
{ printf '<!doctype html>\n<html lang="en">\n<meta charset="utf-8">\n<meta name="viewport" content="width=device-width,initial-scale=1">\n'; cat configurator.html; } > "Keybinder.html"
# Windows installer: one exe with the DLL, default settings and the editor inside.
python3 pack_windows.py
clang $CFLAGS -c installer.c -o build/installer.obj
SETUP="build/Dungeons II Controller Mod Setup.exe"
"$LLD" /nodefaultlib /entry:start /subsystem:windows /machine:x64 /out:"$SETUP" build/installer.obj build/installer.res \
    build/kernel32.lib build/user32.lib build/gdi32.lib build/advapi32.lib build/shell32.lib build/comdlg32.lib
rm -f build/*.obj build/payload.h
# Ready-to-use packages: dist/Windows (the installer) and dist/Mac (double-click scripts).
mkdir -p dist/Windows dist/Mac
cp "$SETUP" dist/Windows/
cp build/xinput1_4.dll wasdmod.ini install.command uninstall.command dist/Mac/
cp Keybinder.html "dist/Mac/Key Layout Editor.html"
echo "Built build/xinput1_4.dll, Keybinder.html, $SETUP and dist/"
