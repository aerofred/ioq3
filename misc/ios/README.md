# ioquake3 on iOS

This repository now includes an initial iOS port path inspired by the local `SmokinGuns` project:

- SDL2 for `iphoneos`
- touch controls overlay
- safe-area aware control layout
- helper scripts to build QVMs, build the iOS client, and package an `.app`

## Prerequisites

- macOS with Xcode and the iPhoneOS SDK
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
- The generated app bundle is intended as a packaging baseline. Signing/deployment is still handled on the Apple side.
- LAN multiplayer requires the local network entitlement text already declared in `Info.plist`.
