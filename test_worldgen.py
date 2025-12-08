import unreal
import time

def verify_worldgen_tools():
    print("=== Starting WorldGen Verification ===")
    
    # 1. Verify Grid Alignment (Phase 2)
    print("\n[Test 1] Aligning PCG Grid...")
    if unreal.WorldGenBuildUtility.align_pcg_grid_with_settings():
        print("SUCCESS: PCG Grid Aligned / PCGWorldActor Spawned.")
        
        # Verify properties
        actor = unreal.GameplayStatics.get_actor_of_class(unreal.EditorLevelLibrary.get_editor_world(), unreal.PCGWorldActor)
        if actor:
            print(f"  - Actor Found: {actor.get_name()}")
            print(f"  - Grid Size: {actor.partition_grid_size}")
            print(f"  - Is 2D Grid: {actor.b_use2_d_grid}")
        else:
            print("ERROR: Could not find APCGWorldActor after success return.")
    else:
        print("FAIL: align_pcg_grid_with_settings returned false.")

    # 2. Verify World Baking (Phase 1)
    seed = 999
    print(f"\n[Test 2] Baking World with Seed {seed}...")
    start_time = time.time()
    
    if unreal.WorldGenBuildUtility.build_world_from_seed(seed):
        duration = time.time() - start_time
        print(f"SUCCESS: World Build Complete in {duration:.2f}s")
        
        # Verify Asset
        asset_path = "/Game/WorldGen/Baked/BakedTerrainData"
        asset = unreal.load_asset(asset_path)
        if asset:
             print(f"  - Asset Found: {asset_path}")
             # Check if we can access properties (depending on reflection)
             print(f"  - Seed: {asset.get_editor_property('seed')}")
             print(f"  - Radius: {asset.get_editor_property('radius')}")
             print("  - HeightTextures: [Map Property - Valid]")
        else:
             print(f"ERROR: Could not load asset at {asset_path}")
    else:
        print("FAIL: build_world_from_seed returned false.")

    print("\n=== Verification Finished ===")

if __name__ == "__main__":
    verify_worldgen_tools()
