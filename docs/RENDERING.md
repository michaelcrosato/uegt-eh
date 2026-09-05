# Rendering decisions and sources

Checked 2026-09-05 against the installed UE 5.8.2 source and official vendor documentation.

- [NVIDIA DLSS](https://developer.nvidia.com/rtx/dlss): current official UE 5.8 DLSS 4.5 package; installed plugin 8.7.2, NGX 310.6.0, Streamline 2.11.1 matches the July 2026 package. Use the official libraries and runtime capability queries, never merely label TSR as DLSS. The 4070 Super supports 2x FG, not Blackwell multi-frame generation. DLSS model preset 0 allows the installed SDK's recommended model.
- [Epic MegaLights](https://dev.epicgames.com/documentation/en-us/unreal-engine/megalights-in-unreal-engine): stochastic direct lighting and area shadows with HWRT; constant sampling cost; volume lighting uses a shared traced froxel volume. Every local light explicitly selects RayTracing shadow method. Screen traces are disabled. No directional lights.
- [Epic hardware ray tracing](https://dev.epicgames.com/documentation/en-us/unreal-engine/hardware-ray-tracing-in-unreal-engine): Lumen hardware tracing with hit lighting for reflections. D3D12 SM6 is required by the project. No non-RT gameplay fallback.

The latest/best effects are selected for this scene, not every mutually exclusive renderer switched on. Full real-time path tracing at high FPS is not promised on a 4070 Super. The game must preserve high-quality hardware-ray-traced direct shadows, GI and reflections while using denoising and reconstruction. Quality and performance are established through measured runtime evidence, not config files alone.

No baked lightmaps; no unshadowed fill; emissive panels only accompany a controllable light and go black with it. Meshes are opaque and deliberately simple, avoiding masked proxy mismatches. Ray tracing culling is disabled for this compact facility so offscreen geometry continues to cast shadows. The material/geometry budget is deliberately small to leave GPU time for lighting.
