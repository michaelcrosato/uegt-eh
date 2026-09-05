"""Run with UnrealEditor-Cmd -run=pythonscript -script=<this file>.

Creates the project-owned materials, original synthesized sounds and empty host map.
The native Facility actor authors the complete level at runtime. No external art needed.
"""
import math
from pathlib import Path
import random
import struct
import wave
import unreal

ROOT = Path(unreal.Paths.project_dir())
LIB = unreal.MaterialEditingLibrary
ASSETS = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def expression(mat, cls, **properties):
    node = LIB.create_material_expression(mat, cls)
    for key, value in properties.items():
        node.set_editor_property(key, value)
    return node


def material(name, color, roughness, metallic=0.0, glow=False):
    path = "/Game/Materials/M_" + name
    if ASSETS.does_asset_exist(path):
        mat = unreal.load_asset(path)
        LIB.delete_all_material_expressions(mat)
    else:
        mat = TOOLS.create_asset("M_" + name, "/Game/Materials", unreal.Material, unreal.MaterialFactoryNew())
    LIB.set_base_material_usage(mat, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES, True)
    LIB.set_base_material_usage(mat, unreal.MaterialUsage.MATUSAGE_NANITE, True)
    tint = expression(mat, unreal.MaterialExpressionVectorParameter,
                      parameter_name="Tint", default_value=unreal.LinearColor(*color, 1.0))
    LIB.connect_material_property(tint, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = expression(mat, unreal.MaterialExpressionScalarParameter,
                       parameter_name="Roughness", default_value=roughness)
    LIB.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    metal = expression(mat, unreal.MaterialExpressionConstant, r=metallic)
    LIB.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
    if glow:
        strength = expression(mat, unreal.MaterialExpressionScalarParameter, parameter_name="Glow", default_value=0.0)
        multiply = expression(mat, unreal.MaterialExpressionMultiply)
        LIB.connect_material_expressions(tint, "RGB", multiply, "A")
        LIB.connect_material_expressions(strength, "", multiply, "B")
        LIB.connect_material_property(multiply, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    LIB.layout_material_expressions(mat)
    LIB.recompile_material(mat)
    ASSETS.save_loaded_asset(mat)


for name, rgb, rough, metal in [
    ("Wall", (0.31, 0.37, 0.34), 0.72, 0.05),
    ("Teal", (0.045, 0.19, 0.18), 0.34, 0.45),
    ("Amber", (0.64, 0.29, 0.065), 0.30, 0.18),
    ("Metal", (0.25, 0.29, 0.29), 0.24, 0.87),
    ("DarkMetal", (0.035, 0.05, 0.055), 0.28, 0.8),
    ("Floor", (0.075, 0.10, 0.105), 0.23, 0.25),
    ("FloorWet", (0.055, 0.078, 0.08), 0.075, 0.42),
    ("Concrete", (0.10, 0.12, 0.12), 0.95, 0),
    ("Ceramic", (0.68, 0.72, 0.62), 0.27, 0.05),
    ("Seam", (0.015, 0.020, 0.021), 0.8, 0),
    ("Cardboard", (0.32, 0.24, 0.14), 0.9, 0),
    ("Screen", (0.016, 0.038, 0.033), 0.085, 0.36),
    ("Mirror", (0.80, 0.86, 0.85), 0.025, 1),
    ("Glove", (0.045, 0.052, 0.049), 0.95, 0),
]:
    material(name, rgb, rough, metal)
material("Lamp", (0.8, 0.92, 1), 0.28, glow=True)

# Nanite gives instanced low-poly meshes Lumen surface-cache support. Keep the
# complete low-poly fallback mesh, so ray-traced shadows match the visible mesh.
for source, name in [("Cube", "SM_Block"), ("Cylinder", "SM_Pipe")]:
    path = "/Game/Geometry/" + name
    if not ASSETS.does_asset_exist(path):
        ASSETS.duplicate_asset("/Engine/BasicShapes/" + source, path)
    mesh = unreal.load_asset(path)
    settings = mesh.get_editor_property("nanite_settings")
    settings.set_editor_property("enabled", True)
    settings.set_editor_property("generate_fallback", unreal.NaniteGenerateFallback.ENABLED)
    settings.set_editor_property("fallback_target", unreal.NaniteFallbackTarget.PERCENT_TRIANGLES)
    settings.set_editor_property("fallback_relative_error", 0.0)
    settings.set_editor_property("fallback_percent_triangles", 1.0)
    mesh.set_editor_property("nanite_settings", settings)
    mesh.set_editor_property("support_ray_tracing", True)
    ASSETS.save_loaded_asset(mesh)

# Keep distance-field text coverage from the engine font material, but light the ink.
text_path = "/Game/Materials/M_Text"
if not ASSETS.does_asset_exist(text_path):
    ASSETS.duplicate_asset("/Engine/EngineMaterials/DefaultTextMaterialOpaque", text_path)
    text_mat = unreal.load_asset(text_path)
    emissive = LIB.get_material_property_input_node(text_mat, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    output = LIB.get_material_property_input_node_output_name(text_mat, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if emissive:
        LIB.connect_material_property(emissive, output, unreal.MaterialProperty.MP_BASE_COLOR)
    zero = expression(text_mat, unreal.MaterialExpressionConstant, r=0.0)
    LIB.connect_material_property(zero, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    text_mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    LIB.recompile_material(text_mat)
    ASSETS.save_loaded_asset(text_mat)


def synth(name, duration, sample):
    output = ROOT / "Saved" / "GeneratedAudio" / ("A_" + name + ".wav")
    output.parent.mkdir(parents=True, exist_ok=True)
    rng = random.Random(900 + len(name))
    rate = 24000
    pcm = bytearray()
    state = 0.0
    for i in range(int(duration * rate)):
        t = i / rate
        noise = rng.uniform(-1, 1)
        state = 0.94 * state + 0.06 * noise
        value = sample(t, noise, state)
        value = max(-0.95, min(0.95, value))
        pcm.extend(struct.pack("<h", int(value * 32767)))
    with wave.open(str(output), "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(rate)
        wav.writeframes(pcm)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(output))
    task.set_editor_property("destination_path", "/Game/Audio")
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    TOOLS.import_asset_tasks([task])
    sound = unreal.load_asset("/Game/Audio/A_" + name)
    if name == "Drone":
        sound.set_editor_property("looping", True)
    ASSETS.save_loaded_asset(sound)


tau = 2 * math.pi
synth("Drone", 12, lambda t, n, s:
      0.19*math.sin(tau*48*t) + 0.10*math.sin(tau*72*t)*(0.6+0.4*math.sin(tau*t/12))
      + 0.045*math.sin(tau*143*t) + s*0.3)
synth("Step", 0.28, lambda t, n, s:
      (0.34*math.sin(tau*(100-60*t)*t) + n*0.27)*math.exp(-t*26)*min(1, t*500))
synth("Warden", 0.7, lambda t, n, s:
      (0.40*math.sin(tau*58*t) + 0.12*math.sin(tau*311*t) + n*0.16)*math.exp(-t*8)*min(1, t*1000))
synth("Switch", 0.13, lambda t, n, s:
      (n*0.32 + 0.15*math.sin(tau*1200*t))*math.exp(-t*80)*min(1,t*1000))
synth("Break", 1.2, lambda t, n, s:
      n*0.54*math.exp(-t*8) + 0.13*math.sin(tau*2761*t)*math.exp(-t*5)
      + 0.12*math.sin(tau*1893*t)*math.exp(-t*4))
synth("Pickup", 0.65, lambda t, n, s:
      0.22*(math.sin(tau*660*t)+0.4*math.sin(tau*990*t))*math.exp(-t*7)*min(1,t*70))
synth("Alarm", 3, lambda t, n, s:
      0.27*math.sin(tau*(220+55*math.sin(tau*t))*t)*min(1,t*8)*min(1,(3-t)*4))
synth("Caught", 2, lambda t, n, s:
      (0.34*math.sin(tau*(54-9*t)*t)+s*0.6+0.12*math.sin(tau*93*t))*math.exp(-t*1.8)*min(1,t*30))
synth("Escape", 4, lambda t, n, s:
      (0.13*math.sin(tau*220*t)+0.08*math.sin(tau*330*t)+0.06*math.sin(tau*440*t))
      *min(1,t*2)*math.exp(-t*0.8))

map_path = "/Game/Maps/Sublevel09"
if not ASSETS.does_asset_exist(map_path):
    level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    level.new_level(map_path)
    level.save_current_level()
unreal.log("AFTERLIGHT ASSETS GENERATED: 16 materials, 9 original sounds, Sublevel09 map.")
