# Rendering decisions and sources

Checked 2026-09-05 against the installed UE 5.8.2 source and official vendor documentation.

- [NVIDIA DLSS](https://developer.nvidia.com/rtx/dlss): current official UE 5.8 DLSS 4.5 package; installed plugin 8.7.2, NGX 310.6.0, Streamline 2.11.1 matches the July 2026 package. Use the official libraries and runtime capability queries, never merely label TSR as DLSS. The 4070 Super supports 2x FG, not Blackwell multi-frame generation. DLSS model preset 0 allows the installed SDK's recommended model.
- [Epic MegaLights](https://dev.epicgames.com/documentation/en-us/unreal-engine/megalights-in-unreal-engine): stochastic direct lighting and area shadows with HWRT; constant sampling cost; volume lighting uses a shared traced froxel volume. Every local light explicitly selects RayTracing shadow method. Screen traces are disabled. No directional lights.
- [Epic hardware ray tracing](https://dev.epicgames.com/documentation/en-us/unreal-engine/hardware-ray-tracing-in-unreal-engine): Lumen hardware tracing with hit lighting for reflections. D3D12 SM6 is required by the project. No non-RT gameplay fallback.

The latest/best effects are selected for this scene, not every mutually exclusive renderer switched on. Full real-time path tracing at high FPS is not promised on a 4070 Super. The game must preserve high-quality hardware-ray-traced direct shadows, GI and reflections while using denoising and reconstruction. Quality and performance are established through measured runtime evidence, not config files alone.

No baked lightmaps; no unshadowed fill; emissive panels only accompany a controllable light and go black with it. Meshes are opaque and deliberately simple, avoiding masked proxy mismatches. Ray tracing culling is disabled for this compact facility so offscreen geometry continues to cast shadows. The material/geometry budget is deliberately small to leave GPU time for lighting.

## Presets

All presets retain MegaLights hardware-ray-traced direct lighting and soft shadows, hardware Lumen, full-resolution reflection tracing at the internal render resolution, hardware-ray-traced short-range contact occlusion, volumetric fog, bloom, and Ray Reconstruction on supported NVIDIA hardware.

| F2 preset | Reconstruction | Direct-light sampling | Lumen hit lighting | Reflection bounces |
| --- | --- | --- | --- | --- |
| Quality | DLSS Quality, about 66.7% internal resolution | Half-rate MegaLights | Reflections; hardware-traced GI uses the lighting cache | 2 |
| Smooth | DLSS Balanced, about 58% internal resolution | Quarter-rate MegaLights | Same as Quality | 2 |
| Showcase | DLAA, 100% internal resolution | Full-rate MegaLights | Both GI and reflections; higher final-gather sampling | 4 |

F3 toggles ordinary 2x DLSS Frame Generation, with capability checking and Reflex. It does not double simulation or input sampling. The HUD separates render FPS from presentation FPS. F6 freezes gameplay and hides the HUD, but continues temporal lighting accumulation.

Full-resolution reflection tracing was retained after checkerboard tracing produced visible speckling in this scene. Higher image quality costs real GPU time; Showcase is a lighting demonstration mode, not a promise of 60 FPS on a 4070 Super.

## Integration details

- Every environment instance uses a project-owned Nanite block or pipe. The Nanite fallback retains the original triangle count; no simplified shadow proxy. Nanite also supplies the instanced geometry's Lumen surface data.
- Ray Reconstruction receives raw reflection samples. Lumen's temporal and bilateral reflection filters are disabled while RR is active, avoiding double denoising.
- The UE 5.8 NVIDIA Blueprint helpers use implicit CVar priorities for some settings. Cooked runtime tests exposed rejected constructor-priority writes. The game establishes explicit priorities for the affected reflection filter and DLSS-G variables, then uses the official capability-checked APIs.
- The runtime mesh audit compares retained cooked triangle metadata, not editor-only build settings or CPU Nanite pages released after GPU upload.
- The audit requests real foreground window focus before measuring Frame Generation. Both Unreal and the NVIDIA SDK independently suspend FG for background windows, so overriding an engine focus flag is insufficient. The report records actual focused and generated-frame sample counts; no FPS multiplier is fabricated. Normal play retains ordinary focus behavior.
- A non-RT launch presents an explicit refusal screen and cannot start a shift. Other ray-tracing GPUs can use TSR when NVIDIA DLSS is unavailable; gameplay never falls back to non-RT lighting.
