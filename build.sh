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
printf 'LIBRARY KERNEL32.dll\nEXPORTS\n%s\n' FreeLibrary GetSystemDirectoryA LoadLibraryA GetProcAddress GetModuleFileNameA GetPrivateProfileIntA GetPrivateProfileStringA GetCurrentProcessId CreateFileA WriteFile CloseHandle GetStdHandle ExitProcess GetCommandLineA GetTickCount64 Sleep CreateThread GetPrivateProfileSectionA GetFileAttributesExA QueryPerformanceCounter QueryPerformanceFrequency GetModuleHandleA VirtualProtect FindFirstFileA FindNextFileA FindClose InitializeCriticalSection EnterCriticalSection LeaveCriticalSection ReadFile GetFileSize DeleteFileA CopyFileA GetFileAttributesA MoveFileExA SetFileTime GetSystemTimeAsFileTime GetProcessHeap HeapAlloc HeapFree GetEnvironmentVariableA CreateToolhelp32Snapshot Process32First Process32Next CreateDirectoryA GetLogicalDrives GetDriveTypeA GetLocalTime MultiByteToWideChar WideCharToMultiByte GetUserDefaultUILanguage > build/kernel32.def
printf 'LIBRARY USER32.dll\nEXPORTS\n%s\n' GetAsyncKeyState GetForegroundWindow GetWindowThreadProcessId keybd_event mouse_event SetWindowsHookExW CallNextHookEx GetRawInputData EnumWindows CreateWindowExA DefWindowProcA PostMessageA PeekMessageA TranslateMessage GetFocus SetFocus RegisterClassA ShowWindow SetWindowPos GetClientRect ClientToScreen SetLayeredWindowAttributes GetMessageA DispatchMessageA SetTimer InvalidateRect BeginPaint EndPaint FillRect IsWindowVisible GetDC ReleaseDC GetCursorPos MapVirtualKeyA SetCursorPos ClipCursor RegisterClassA IsDialogMessageA PostQuitMessage SendMessageA SetWindowTextA MessageBoxA EnableWindow LoadCursorA LoadIconA AdjustWindowRect SetProcessDPIAware GetSysColorBrush RegisterClassW CreateWindowExW DefWindowProcW GetMessageW DispatchMessageW IsDialogMessageW SendMessageW SetWindowTextW GetWindowTextW MessageBoxW DrawTextW BeginPaint EndPaint SetWindowLongPtrW CallWindowProcW TrackMouseEvent IsWindowEnabled LoadImageW DrawIconEx RedrawWindow DestroyWindow SetForegroundWindow > build/user32.def
printf 'LIBRARY GDI32.dll\nEXPORTS\n%s\n' CreateFontA SelectObject SetTextColor SetBkMode TextOutA CreateSolidBrush GetTextExtentPoint32A CreatePen Ellipse GetStockObject DeleteObject Polygon GetDeviceCaps TextOutW GetTextExtentPoint32W GetTextFaceA CreateCompatibleDC CreateCompatibleBitmap DeleteDC BitBlt > build/gdi32.def
printf 'LIBRARY IMM32.dll\nEXPORTS\n%s\n' ImmAssociateContext ImmCreateContext > build/imm32.def
"$LLD" /lib /machine:x64 /def:build/kernel32.def /out:build/kernel32.lib >/dev/null
"$LLD" /lib /machine:x64 /def:build/user32.def /out:build/user32.lib >/dev/null
"$LLD" /lib /machine:x64 /def:build/imm32.def /out:build/imm32.lib >/dev/null
"$LLD" /lib /machine:x64 /def:build/gdi32.def /out:build/gdi32.lib >/dev/null
printf 'LIBRARY ADVAPI32.dll\nEXPORTS\n%s\n' RegOpenKeyExA RegQueryValueExA RegCloseKey SystemFunction036 > build/advapi32.def
printf 'LIBRARY SHELL32.dll\nEXPORTS\n%s\n' ShellExecuteA ShellExecuteW SHGetFolderPathA > build/shell32.def
printf 'LIBRARY OLE32.dll\nEXPORTS\n%s\n' CoInitializeEx CoTaskMemFree > build/ole32.def
printf 'LIBRARY COMDLG32.dll\nEXPORTS\n%s\n' GetOpenFileNameW > build/comdlg32.def
printf 'LIBRARY WS2_32.dll\nEXPORTS\n%s\n' WSAStartup socket bind listen accept getsockname setsockopt recv send closesocket > build/ws2_32.def
printf 'LIBRARY DWMAPI.dll\nEXPORTS\n%s\n' DwmSetWindowAttribute > build/dwmapi.def
printf 'LIBRARY GDIPLUS.dll\nEXPORTS\n%s\n' GdiplusStartup GdipCreateFromHDC GdipDeleteGraphics GdipSetSmoothingMode GdipCreateSolidFill GdipDeleteBrush GdipCreatePen1 GdipDeletePen GdipSetPenStartCap GdipSetPenEndCap GdipCreatePath GdipDeletePath GdipAddPathArc GdipClosePathFigure GdipFillPath GdipDrawPath GdipFillEllipse GdipDrawLine GdipDrawArc > build/gdiplus.def
for l in advapi32 shell32 comdlg32 ws2_32 dwmapi gdiplus ole32; do "$LLD" /lib /machine:x64 /def:build/$l.def /out:build/$l.lib >/dev/null; done
VERSION=1.2.0 # shown in the apps, the log and the release
# The translations (lang/*.json) as C tables for the mod and the Windows setup, a
# file for the Mac app, and Keybinder.html (the key layout editor with every language).
python3 lang.py
CFLAGS="-DVERSION=\"$VERSION\" --target=x86_64-pc-windows-msvc -O2 -fno-stack-protector -fno-builtin -Wall -Wno-incompatible-pointer-types -Wno-int-conversion"
clang $CFLAGS -Ibuild -c xinput_wasd.c -o build/xinput_wasd.obj
"$LLD" /dll /brepro /nodefaultlib /entry:DllMain /machine:x64 /def:xinput1_4.def /out:build/xinput1_4.dll build/xinput_wasd.obj build/kernel32.lib build/user32.lib build/gdi32.lib
clang $CFLAGS -c test_load.c -o build/test_load.obj
"$LLD" /nodefaultlib /entry:mainCRTStartup /subsystem:console /machine:x64 /out:build/test_load.exe build/test_load.obj build/kernel32.lib build/user32.lib build/imm32.lib
rm -f build/*.obj build/*.exp build/xinput1_4.lib
# Windows installer: one exe with the DLL, both layouts and the editor inside, and
# Microsoft's WebView2Loader.dll for the editor window (BSD license, see README),
# from the WebView2 SDK on nuget.org: fetched once and checked.
WEBVIEW2_SDK=1.0.4258.31
if [ ! -f build/webview2/sdk/runtimes/win-x64/native/WebView2Loader.dll ]; then
    mkdir -p build/webview2
    curl -sfL -o build/webview2/sdk.nupkg "https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/$WEBVIEW2_SDK/microsoft.web.webview2.$WEBVIEW2_SDK.nupkg"
    echo "56f7f4b8bf9aee4b8efefbbdd4f67d5f74ebd1b100ed0806da71bf76af481aa9  build/webview2/sdk.nupkg" | shasum -a 256 -c - >/dev/null || { echo "The WebView2 SDK download doesn't match its checksum."; exit 1; }
    (cd build/webview2 && unzip -q -o sdk.nupkg -d sdk)
fi
WEBVIEW2_SDK=$WEBVIEW2_SDK python3 pack.py
clang $CFLAGS -c installer.c -o build/installer.obj
SETUP="build/wasdmod-Windows.exe"
"$LLD" /brepro /nodefaultlib /entry:start /subsystem:windows /machine:x64 /out:"$SETUP" build/installer.obj build/installer.res \
    build/kernel32.lib build/user32.lib build/gdi32.lib build/advapi32.lib build/shell32.lib build/comdlg32.lib build/ws2_32.lib build/dwmapi.lib build/gdiplus.lib build/ole32.lib
rm -f build/*.obj build/payload.h
# Mac app (mac/main.swift): the key layout editor in a window, with install and
# uninstall at the top, done by mac/wasdmod.sh. Put together outside this folder:
# iCloud's Desktop sync tags app folders with Finder data, which breaks the signature.
STAGE=$(mktemp -d "${TMPDIR:-/tmp}/wasdmod-dmg.XXXXXX")
APP="$STAGE/wasdmod.app"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
for arch in arm64 x86_64; do
    swiftc -swift-version 5 -O -target $arch-apple-macos11 mac/main.swift -o "$STAGE/wasdmod-$arch"
done
lipo -create "$STAGE/wasdmod-arm64" "$STAGE/wasdmod-x86_64" -output "$APP/Contents/MacOS/wasdmod"
rm "$STAGE"/wasdmod-arm64 "$STAGE"/wasdmod-x86_64
cat > "$APP/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>CFBundleExecutable</key><string>wasdmod</string>
  <key>CFBundleIdentifier</key><string>io.github.wanzho.wasdmod</string>
  <key>CFBundleName</key><string>wasdmod</string>
  <key>CFBundleDisplayName</key><string>wasdmod</string>
  <key>CFBundleIconFile</key><string>wasdmod</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>CFBundleShortVersionString</key><string>$VERSION</string>
  <key>CFBundleVersion</key><string>1</string>
  <key>LSMinimumSystemVersion</key><string>11.0</string>
  <key>NSHighResolutionCapable</key><true/>
  <key>NSHumanReadableCopyright</key><string>Minecraft Dungeons II controller mod. Not affiliated with Mojang or Microsoft.</string>
  <key>CFBundleDevelopmentRegion</key><string>en</string>
  <key>CFBundleAllowMixedLocalizations</key><true/>
  <key>CFBundleLocalizations</key><array>$(python3 -c 'import json; print("".join("<string>%s</string>" % ("pt-BR" if c == "pt" else c) for c in json.load(open("build/lang.json"))["order"]))')</array>
</dict></plist>
PLIST
cp build/xinput1_4.dll default.txt author.txt mac/wasdmod.sh LICENSE "$APP/Contents/Resources/"
cp build/lang.json "$APP/Contents/Resources/lang.json"
cp Keybinder.html "$APP/Contents/Resources/Key Layout Editor.html"
iconutil -c icns build/wasdmod.iconset -o "$APP/Contents/Resources/wasdmod.icns"
# Signed ad hoc: without a valid signature macOS calls a
# downloaded app "damaged", with no way to open it.
xattr -cr "$APP"
codesign --force --deep --sign - "$APP" 2>/dev/null
codesign --verify --deep --strict "$APP"
cp "mac/Read me.txt" "$STAGE/"
ln -s /Applications "$STAGE/Applications"
rm -f build/wasdmod-Mac.dmg
hdiutil create -quiet -volname wasdmod -srcfolder "$STAGE" -fs HFS+ -format UDZO build/wasdmod-Mac.dmg
rm -rf "$STAGE" build/dmg
# Manual install (Linux / Steam Deck, or Windows without the setup): the files in
# a wasdmod folder, zipped.
STAGE=$(mktemp -d "${TMPDIR:-/tmp}/wasdmod-zip.XXXXXX")
mkdir "$STAGE/wasdmod"
cp build/xinput1_4.dll default.txt author.txt "manual/Read me.txt" "$STAGE/wasdmod/"
cp LICENSE "$STAGE/wasdmod/LICENSE.txt"
cp Keybinder.html "$STAGE/wasdmod/Key Layout Editor.html"
rm -f build/wasdmod.zip
(cd "$STAGE" && zip -q -X -r "$OLDPWD/build/wasdmod.zip" wasdmod)
rm -rf "$STAGE"
# Ready-to-use downloads.
mkdir -p dist
cp "$SETUP" build/wasdmod-Mac.dmg build/wasdmod.zip dist/
echo "Built build/xinput1_4.dll, Keybinder.html and dist/ (wasdmod-Windows.exe, wasdmod-Mac.dmg, wasdmod.zip)"
