# ioquake3 on iOS

This repository now includes an initial iOS port path inspired by the local `SmokinGuns` project:

- SDL2 for `iphoneos`
- touch controls overlay
- safe-area aware control layout
- helper scripts to build QVMs, build the iOS client, and package an `.app`

## Prerequisites

- macOS with Xcode and the iPhoneOS SDK
- CMake 3.25 or newer installed through Homebrew, MacPorts or CMake.app (the build script also
  finds these standard installations when launched from Xcode)
- `baseq3/` at the repository root
- an Apple Developer account if you want to install on a device

## Build

Open in Xcode:

```bash
open misc/ios/ioquake3.xcodeproj
```

Or build from the command line:

```bash
chmod +x misc/ios/*.sh
./misc/ios/setup_sdl2.sh
./misc/ios/build-ios.sh
```

Output:

```text
build/ios/ioquake3.app
```

## Notes

- The script builds host-side QVMs first, then builds the iOS client.
- Host QVM tools use the macOS SDK explicitly; their CMake configuration is
  refreshed on each build to clear cached iPhone SDK settings from Xcode.
- The generated app bundle is intended as a packaging baseline. Signing/deployment is still handled on the Apple side.
- LAN multiplayer requires the local network entitlement text already declared in `Info.plist`.

## Aim assist

Enable **Aim Assist** in **Touch Controls** or **Gamepad > Settings**. Both
switches control the same saved option, disabled by default. The console
equivalent is `seta in_aimAssist 1` (`0` to disable).

Touch look and the gamepad right stick slow down progressively near a visible,
living opponent within six degrees of the crosshair and 2048 game units. The
assist ignores teammates, invisible players, walls and closed doors. It does
not snap to targets or fire automatically. Physical mouse input and menu
navigation keep their normal sensitivity.

Run the standalone behavior checks on the host with
`sh misc/ios/tests/run-aim-assist.sh`.

## Replacing game data on iOS

Launch the app once, then open **Files > On My iPhone/iPad > ioquake3 > baseq3**.
Bundled `.pk3` files are copied into this writable directory on launch. Replace
them there, or add mod directories alongside `baseq3`. Fully quit the game before
editing packs, then relaunch it to load the changes.

Existing files are never overwritten by this setup. Missing bundled packs are
copied again on the next launch. Packs in Documents take priority over bundled
packs; the app bundle still provides the QVMs and default configuration.
Configurations, downloads and mods from the previous private storage location
are copied into Documents without replacing existing files. The old location is
retained as a backup and is no longer searched by default.
