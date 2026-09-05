# AFTERLIGHT / Sublevel 09

An original, compact, first-person escape horror game. The player is a night-shift technician trapped in an underground light research facility. The Warden is a rigid, block-built maintenance automaton that locates the player through light and noise. No skeletal animation, facial rigs, open world, daylight, or licensed art packs.

## Complete playable loop

Enter from the emergency airlock. Read the maintenance notice. Recover an access card from Records and a ceramic fuse from the Workshop. Use the access card to open the plant security shutter. Install the fuse in the generator. Vent the coolant pressure in the Pump Room. Arm the lift evacuation console and survive the countdown, then enter the lift. Capture leads to an explicit retry state; escape has a distinct ending and statistics. Restart resets all doors, pickups, lights, AI, and objective state.

Light is a tool and a liability: wall breakers control room circuits; fixtures can be individually switched or smashed; the handheld lamp can always be toggled. Destruction is irreversible until a new run, but cannot destroy mission items or prevent escape. A dark, quiet player is harder to track. Sprinting and smashing attract the Warden. Rigid movement updates are intentionally stepped; the camera remains smooth.

## Art direction

Chunky industrial geometry, layered wall panels, ribs, cable trays, doors, machinery, lockers, bold painted signage, tiny bolts, amber polymer and oxidized teal. A wet, reflective floor catches large soft area lights. Actual shadow-casting bars cut beams through restrained volumetric haze. No ambient fill light or outdoor sky. Bright source panels extinguish alongside their actual light components.

## Hardware and rendering contract

Target: i7-14700K, 16 GB system RAM, RTX 4070 Super 12 GB. Performance target: 60 real rendered FPS at 2560x1440 with DLSS Quality; optional 2x Frame Generation. Quality and Showcase presets retain hardware ray tracing. Performance measurements must distinguish rendered and presented frames and identify measurement hardware. This machine is i7-14700F / 32 GB / RTX 4070 Super, so the exact 16 GB configuration cannot be claimed tested here.

Direct light and shadow visibility use MegaLights HWRT. Indirect diffuse and reflections use hardware Lumen, with hit lighting for reflections. No baked lights, skylights, screen-space GI/reflection fallback or shadow maps. Opaque low-poly meshes use full geometry in the ray tracing scene and remain shadow casters. Volumetric lighting uses MegaLights' ray-traced froxel volume (volumetric integration and temporal reconstruction are approximations). This is a real-time hybrid primary-visibility renderer, not an offline path tracer.

## Completion gates

- Editor and game compile; assets regenerate from the committed script; packaged Win64 game launches.
- Real GPU runtime proves HWRT, DLSS Super Resolution, Ray Reconstruction and supported Frame Generation; non-RT runtime gives a clear refusal to play.
- All light and physical mesh components satisfy the shadow/RT contract; blackout and destruction verified in actual frames.
- Movement/collision, interactions through line of sight, every objective prerequisite, enemy detection, loss, retry, escape and settings validated.
- Indoor geometry, horror art direction, legible UI and actual lighting visually inspected at multiple cameras.
- Measured representative camera route at 1440p: frame times, resource usage, rendering flags and limitations recorded.
- Public GitHub repository `uegt-eh` contains source, assets, reproducible scripts, controls, design and evidence; no engine/plugin proprietary source or credentials committed.
