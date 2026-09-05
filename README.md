# AFTERLIGHT

**Every light has a price.**

A first-person escape horror game set in a sealed underground facility. Recover the night supervisor's access card, repair auxiliary power, vent the coolant and call the surface lift. A rigid maintenance automaton follows light and noise. Switch lights off, isolate room circuits or permanently break fixtures to hide your route.

Built in Unreal Engine 5.8.2 for Windows / DirectX 12, with a low-poly industrial style and minimal stepped animation. This project requires hardware ray tracing.

## Play

On this development machine:

```powershell
.\Scripts\Play.ps1
```

When a packaged build exists, this launches `Builds\Windows\Afterlight.exe`. Otherwise it runs the native game through Unreal. Press **Enter** to begin. The editor host map is intentionally empty: `AFacility::Build` constructs the full environment when play starts.

| Control | Action |
| --- | --- |
| WASD / mouse | Walk / look |
| E | Use equipment, collect items, switch a fixture or breaker |
| F | Toggle handheld light |
| Left mouse | Break the aimed light within 4.6 metres |
| Shift / Ctrl | Run / crouch |
| Esc / Enter | Pause / resume |
| F1 | Read the shift report and objective hints |
| F2 | Cycle Quality, Smooth and Showcase rendering |
| F3 | Toggle supported 2x DLSS Frame Generation |
| F4 | Show render FPS, GPU time and presentation FPS |
| F6 | Freeze the scene and hide the HUD; press again to resume |
| [ / ] | Adjust mouse sensitivity |
| R | Restart from pause or an ending |
| Q | Quit from a menu or ending |

## Build from source

Install UE **5.8.2**, Visual Studio's C++ game development tools and a Windows SDK. Install NVIDIA's official **DLSS 4.5 UE 5.8 plugin package** into the engine's `Engine\Plugins\Marketplace` folder. This project uses DLSS, StreamlineDLSSG and StreamlineReflex plus their supplied dependencies. Tested installed package: **8.7.2 / NGX 310.6.0 / Streamline 2.11.1**.

[Download official NVIDIA plugins](https://developer.nvidia.com/rtx/dlss). Proprietary Unreal and NVIDIA source/binaries are not included in this repository.

```powershell
.\Scripts\Build.ps1 -Target Editor
# Assets are committed. Regenerate them only when modifying their source recipe:
.\Scripts\GenerateAssets.ps1
.\Scripts\Build.ps1 -Target Package
```

The scripts accept `-EngineRoot` when Unreal is installed elsewhere. No global npm packages or external art dependencies are needed. Sounds are original procedural synthesis; their reproducible recipe is in `Scripts/GenerateAssets.py`.

## Rendering

MegaLights provides hardware-ray-traced area-light visibility and direct shadows. Hardware Lumen supplies GI and hit-lit reflections. DLSS Super Resolution, Ray Reconstruction and Reflex are enabled through NVIDIA's capability-checked native APIs. The 4070 Super supports ordinary 2x Frame Generation; 50-series multi-frame modes are not offered on it.

There are no baked lights, skylights, sunlight or unshadowed fill lights. Screen-space light/GI/reflection fallback is disabled. Every physical mesh batch casts shadows and remains in the ray tracing scene. All fixtures, their emissive diffusers and the handheld lamp can go dark. The small opaque geometry budget leaves GPU time for lighting.

The renderer still uses rasterized primary visibility, denoising and temporal reconstruction. It is not advertised as a fully path-traced game. See [rendering decisions](docs/RENDERING.md) for the precise implementation and official sources.

## Validation and target hardware

Target: i7-14700K, **16 GB RAM**, RTX 4070 Super 12 GB; **60 rendered FPS at 1440p DLSS Quality** is the performance target. Showcase uses DLAA and more ray samples, and can cost substantially more. Frame Generation presentation FPS is reported separately from real rendered frames.

The available validation machine is an i7-14700F with **32 GB** and an RTX 4070 Super. Its memory usage can be measured, but it cannot prove testing on an exact 16 GB configuration.

```powershell
.\Scripts\Audit.ps1
.\Scripts\Audit.ps1 -Packaged
```

The runtime audit writes screenshots and a JSON report under the running project's `Saved\Evidence`. It checks actual rendering capabilities, the shadow contract, a complete blackout, irreversible fixture destruction, prerequisite rejection and the card-to-lift interaction chain. Benchmark samples exclude camera transitions. Enemy pursuit, physical traversal, settings and final packaged verification are additional completion gates in [the design](docs/DESIGN.md).

Development is ongoing. A successful compile alone is not treated as proof of visual quality, performance or finished gameplay.
