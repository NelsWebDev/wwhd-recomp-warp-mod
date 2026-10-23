# Map warp

A code mod for the [Wind Waker HD port](https://github.com/ZeldaWWHDRecomp/ZeldaWWHDRecomp) that
warps Link to another place on the map: any Great Sea island, or a dungeon.

## Controls

- Hold **ZL + ZR** and press **Minus** to open or close the list.
- **D-pad up/down** chooses (hold to scroll), **left/right** jumps a page.
- **A** warps, **B** closes.
- **X** switches to a square picker: left/right chooses the letter, up/down the number
  (A1 is Forsaken Fortress), **A** warps.

While the list is open the game gets an empty controller, so no other button does anything.

## What it covers

All 49 sea chart squares (the `sea` stage, one room each) and the five dungeons. The warp uses
the game's own `dComIfGp_setNextStage`, so the game does the fade and the scene change.

Experimental. Spawn points use point 0 and are untested in every region; warping into a dungeon
you haven't reached can skip story events. Save before using it. USA installs only so far.

## Install

1. Download `map-warp.zip` from the Releases page and unzip it.
2. In the port: **Settings (F1) → Mods → Installed packages → Choose folder**, pick the
   `map-warp` folder, confirm trust, run the setup, enable the mod and restart.

Code mods must be enabled in the port (the normal `wwhd` build, not `wwhd-decomp`). Native-code
trust is required; Android guest mods are not supported.

## Build from source

You need a clang with the PowerPC target and `ld.lld` (macOS: `brew install llvm lld`; Apple's
own clang has no PowerPC target), and the SDK headers that ship with the port.

```sh
SDK=/path/to/WindWakerHD/sdk/guest/include ./build.sh
```

This writes the installable folder `build/map-warp/` and `build/map-warp.zip`. Compiled files are
never committed.

## How it works

- The game's single controller read (`0x0273E3FC`, not in the public SDK) is hooked: the menu
  reads the pad there and zeroes the game's copy while it is open.
- `daPy_lk_c::procTactWait_init` (starting the Wind Waker) is replaced to refuse while the menu
  is open; only one mod may replace a given function.
- Link's step function is hooked to draw the list with the HUD API and trigger the warp.

## Licence

MPL-2.0, see [LICENSE](LICENSE). SDK declarations come from the port's public SDK, which derives
from the public CC0 decomp. No game files are included.
