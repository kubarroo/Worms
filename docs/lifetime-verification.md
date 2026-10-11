# Lifetime Verification

## Automated Coverage

`ApplicationTests/TestResourceManager.cpp` verifies cache reuse, normalized and
absolute paths, missing/corrupt-file diagnostics and retry, platform restart, cache retention
across scene restart, renderer mismatch rejection, and shutdown with cached audio.

Tests in `ApplicationTests/TestGameLifetime.cpp` use SDL `dummy` drivers and the `software` renderer. They verify logic and resources without an interactive gameplay window.

| Scenario | Verification |
| --- | --- |
| Worm removal | Successor selection, destruction of the owning object, and release of its entities and body |
| Last team death | No active worm, subscription removal, completion of the death effect, and counters returning to their initial values |
| Projectile explosion | Effect and removal request creation, projectile removal through the game loop, and no remaining body or subscription |
| Particle expiry | Processing through addition and removal queues, release of the system entity and particle entities |
| Terrain deformation | Reduced opaque pixel count, a working replacement texture, and stable entity, body, and subscription counts |
| Repeated removal request | One cleanup and one object destruction |
| Collision callbacks | Removing the callback's own subscription without removing other listeners, reporting an exception after the physics step |
| Repeated startup and shutdown | Empty containers and subscription registry, no application resources, and SDL, audio, and ImGui shut down |
| Partial initialization | Cleanup after failure and safe repeated `CleanUp()` |
| Scene-local physics | Independent factories and contact registries, equal entity IDs in separate worlds, and cleanup or failed startup of one scene without invalidating another scene's physics |
| Camera target loss | Existing timeout, a consumable signal, and one turn transition coordinated by the scene |
| Same-scene restart | Three cycles on the same renderer, with active and pending projectiles/particles and a pending removal; restored active worm, camera zoom, weapon charge, counters, and empty queues |
| Full startup rollback | Missing worm, weapon, map, or music assets release both gameplay and platform resources; retry succeeds without explicit cleanup |
| Platform rollback and RAII | Renderer/audio initialization failure rolls back immediately; retry and destruction without explicit cleanup release the platform |
| Shutdown dependency order | Objects observe detached physics callbacks but live worlds, renderer, audio device, and shared weapon texture; addition/removal requests are rejected during cleanup |
| Shutdown with active audio | Looping music and sound are stopped before object cleanup; cache and ImGui remain available until scene borrowers are destroyed |
| Recreated scenes and audio | Three separately constructed scenes reuse the same texture, sound and music; each destruction stops playback without closing the platform; playback works again before final shutdown |
| Corrupt assets | Image, sound and music failures include the filename, repeated attempts fail without caching an invalid asset, and replacing the file permits loading and deduplicated reuse |
| Session state | Held input and pending weapon change are reset; the first frame after resetting the frame clock has zero elapsed time |

Repeated cycles compare live entities, Box2D bodies, subscriptions, and
object queues. Tests check that weapon and particle-system asset references are
cleared without freeing cached textures, and that the map's private texture and
surface are released. Additional tests verify shared worm/particle textures,
continued death-sound playback after team deletion, and projectile assets surviving
WeaponManager destruction. These are not global SDL allocation counters or proof
that no leaks exist.

EnTT tests count living entities through the entity view, not storage capacity
or free-slot counters. A repeated large-population test verifies component cleanup
and invalidation of all retained handles. Behavior tests retain coverage of camera
target loss, projectile removal, particle cleanup, callback lifetimes, scene
restart, and partial-initialization rollback. Custom ECS storage tests are obsolete.

Mutable-resource tests compare full terrain pixel snapshots and Box2D chain geometry
for two maps, deform and clean one, and verify the other and a reinitialized map.
Generated team textures are tested for distinct identity and independent color,
alpha, and blend state; surviving health bars render after worm/team removal.
Texture destruction follows RAII; tests never query a freed SDL pointer to prove release.

## Running Tests

After building `worms_tests`:

```powershell
ctest --test-dir build/debug --output-on-failure
```

Running all tests repeatedly in one process helps detect state left behind by earlier tests. The working directory must contain the game assets:

```powershell
Push-Location Worms
try {
    & ../build/debug/ApplicationTests/worms_tests.exe --gtest_repeat=10 --gtest_shuffle --gtest_random_seed=12345
} finally {
    Pop-Location
}
```

## Memory Diagnostics

`TestMain.cpp` enables MSVC Debug heap diagnostics through `WORMS_CRT_DIAGNOSTICS=1`. It checks heap integrity before and after each test and enables a CRT leak report at process exit. Reports go to standard error without a modal dialog.

```powershell
$previous = $env:WORMS_CRT_DIAGNOSTICS
$env:WORMS_CRT_DIAGNOSTICS = '1'
Push-Location Worms
try {
    & ../build/debug/ApplicationTests/worms_tests.exe --gtest_repeat=10 --gtest_shuffle --gtest_random_seed=12345
} finally {
    Pop-Location
    $env:WORMS_CRT_DIAGNOSTICS = $previous
}
```

- An invalid heap causes a test assertion failure. The leak report at process exit requires separate analysis and does not itself change the program's exit code to indicate failure.
- Passing tests do not replace inspection of the leak report. Retained singleton and library buffers must be distinguished from resources that should have been released.
- The CRT checks allocations handled by the corresponding Debug runtime. It does not automatically cover all memory used by external libraries, drivers, or the GPU.
- The camera test initializes the timer subsystem within a local scope and shuts down SDL after its objects have been destroyed. After the entire suite, the runner reports any remaining active subsystems, then performs a final `SDL_Init(SDL_INIT_TIMER)` and `SDL_Quit()`. This cleans timer and thread data also created by API calls outside full SDL initialization, for example in failure tests. The CRT report remains enabled.
- AddressSanitizer requires a separate instrumented build. Having its DLL in an MSVC installation does not mean that an ordinary Debug build detects use-after-free. This stage does not enable ASan instrumentation.

## Interactive Checks

Headless tests do not replace verification with normal audio and graphics drivers. During gameplay, check worm death, elimination of the last team, explosions on contact, visible terrain deformation, particle fading, and window closure while a projectile and effects are active.

## Historical Phase-1 Execution Status

- Before adding tests for this stage, the existing suite was run: **70/70 tests passed**.
- Six lifetime-cycle tests were added, together with checks of selected resource owners and post-cleanup state. An attempt to build the new changes was stopped by MSVC error `C1902` inside the sandbox; a build outside the sandbox was not approved.
- The provided `lifetime-crt.log` confirms 10 runs of 76 tests without test failures. The final CRT report contained five blocks totaling 213 bytes. A separate camera test run reproduced 85 bytes of SDL timer state, while initialization failure tests reproduced allocations related to thread data and the error buffer. Diagnostics must be repeated after the runner cleanup changes; the earlier report does not yet confirm that there are no leaks.
- Results after the latest SDL cleanup fix, the final CRT report, and interactive checks must be recorded separately after they have actually been performed. Earlier passing tests do not replace another diagnostics run.

## Phase-2 Step-4 Verification

- The Debug build of `worms` and `worms_tests` completed successfully. The local
  MSVC build used `/Z7` to avoid the sandbox's `/Zi` PDB-manager issue.
- After the factory follow-up fixes, CTest passed **111/111 tests**, including
  scene-local physics, failed startup of a second scene, camera target-loss
  signaling, and turn coordination. Two new tests cover initial positions for
  all four body variants and borrowed metadata for every body/fixture variant.
  Compile-time assertions verify that all eight factory methods accept named
  metadata and reject temporary or const metadata arguments.
- A focused selection of 27 contact and scene-isolation tests passed three
  shuffled iterations in one process, starting with random seed `12345`.
- Source checks found no remaining `ContactManager::Get()`,
  `ColliderFactory::Get()`, or `noTargetEvent` references in gameplay or tests.
- Interactive gameplay checks, CRT leak diagnostics, and ASan were not repeated
  for this step. Passing tests do not establish complete memory-leak freedom or
  full isolation of the still-shared input, rendering, and audio services.

## Phase-2 Step-5 Verification

- The Debug build of `worms` and `worms_tests` completed successfully using the
  local `/Z7` workaround. Existing numeric-conversion warnings remain.
- CTest passed **119/119 tests**. Eight tests were added for platform rollback
  and destruction, input/frame-clock reset, same-scene restart, full game startup
  rollback, rejection of duplicate startup, and shutdown dependency order.
- After the final ownership assertions and restart checks were added, the full
  suite passed **three shuffled iterations of 119 tests in one process**, with
  `WORMS_CRT_DIAGNOSTICS=1` and starting seed `12345`.
- The log at `build/debug/phase2-step5-crt.log` confirms CRT diagnostics were
  enabled, all 357 test executions passed, and no heap-integrity failure or
  exit-time CRT leak report was emitted. This covers the Debug CRT allocations
  observed by this run, not all driver/GPU allocations or use-after-free risks.
- No ASan-instrumented build or interactive gameplay verification was performed.
  Manual checks still include firing, explosions, camera target loss, turn
  changes, team elimination, and closing with active effects.
- Lifecycle operations are coordinated outside active update/render calls.
  Scene restart retains the renderer and platform, but input, frame timing,
  and audio remain shared services rather than fully isolated per-scene state.

## Phase-3 Step-6 Verification

- Verified on 2026-10-11 using a separate MSVC Debug-runtime test executable at
  build/debug/adapter-verification/full_tests.exe. The local build used /Z7 and
  the existing dependency libraries; project presets were not changed.
- The full suite passed **148/148 tests**, including corrupt-file retry,
  deduplication across recreated scenes, and shutdown with looping music/sounds.
  Output is saved at build/debug/phase3-step6-tests.log.
- With WORMS_CRT_DIAGNOSTICS=1, the complete suite passed **three shuffled
  iterations of 148 tests** (444 executions), starting with seed 12345.
  build/debug/phase3-step6-crt.log confirms diagnostics were enabled, no heap
  integrity failures, no remaining initialized SDL subsystems, and no exit-time
  CRT leak report. This does not cover all driver/GPU memory or replace ASan.
- The corrupt-sound test initially found an open input file after decode failure:
  SDL_mixer's unsupported-format path did not close its owned SDL_RWops.
  Sound now retains stream ownership in local RAII and calls Mix_LoadWAV_RW
  with freesrc=0. The repaired-file retry and all diagnostics passed after this fix.
- The shutdown audit confirmed the existing order: stop playback, destroy scene,
  release ResourceManager cache, shut down ImGui, close audio, release the
  rendering adapter/native renderer/window, and finally SDL_Quit. The scene
  cleanup probe checks stopped playback while cache, worlds, renderer, ImGui,
  and the audio device are still alive; post-cleanup checks confirm shutdown
  and repeated Clean remains safe.
- No ASan-instrumented build or interactive normal-driver check was performed.
  Remaining manual checks: close the window during music and a weapon sound,
  close with projectiles/particles active, and verify ordinary playback and
  rendering after restart. Headless success is not a claim of complete leak freedom.

## SDL3 Migration Verification

- Migrated platform initialization, SDL main entry point, ImGui backends, rendering,
  surface/pixel operations, events and audio to SDL3. EnTT migration is separate.
- Verified Debug and Release builds with the configured Clang presets against
  SDL 3.4.16, SDL_image 3.4.4 and SDL_mixer 3.2.0 on 2026-10-11.
  Logs: build/sdl3-build.log and build/sdl3-release-build.log.
- CTest passed **154/154 tests**; output is saved at build/sdl3-ctest.log.
  New coverage includes padded contour rows and low/zero alpha, nearest-filtered
  cached textures, bounded/reusable effect tracks, finite playback, independent
  mixers, mismatched scene/cache mixers, and close/quit events during playback.
  Frame rendering checks logical game coordinates and window-space ImGui across
  consecutive frames. Existing corrupt-file retry and shutdown tests remain active.
- The final Debug executable passed **three shuffled iterations of 154 tests**
  (462 executions), with WORMS_CRT_DIAGNOSTICS=1 and seeds 12345 through 12347.
  build/sdl3-crt.log confirms CRT diagnostics, no heap-integrity failure, no
  remaining initialized SDL subsystems, and no exit-time CRT leak report.
  This does not cover all driver/GPU allocations and does not replace ASan.
- SDL_Quit invalidates pixel-format details and clears hints. Contour tests no
  longer keep a global format-details pointer; test drivers are reapplied at each
  startup so retries do not inadvertently switch to native drivers.
- An early CRT run found Windows input-device-name allocations after audio-driver
  initialization failure. Initializing SDL audio before video eliminated this
  rollback issue in the isolated and full-suite runs, without modifying SDL sources.
- Manual normal-driver gameplay and an ASan-instrumented build were not performed.
  Check music/effects, aiming and charging, terrain destruction, debug geometry,
  UI scaling, and closing during active effects on the target system.

## Phase-4 Final-Step Verification

- Verified on 2026-10-11. Debug and Release builds completed after removing
  the custom ECS directory, its CMake target, and the unused ECS exception.
  Gameplay and tests now depend directly on `EnTT::EnTT`; owner-aware handles
  are defined in `Core/EntityHandle.h`.
- CTest passed **192/192 tests** in `build/debug`. A separate configuration and
  full build from scratch in `build/entt-clean-verification` also completed,
  and its CTest run passed **192/192 tests**.
- The full Debug suite passed **three shuffled iterations in one process**,
  starting with seed `12345`: **576/576 test executions**.
- The population test creates and clears 100,000 entities in each of two
  cycles, checking live entity counts, component cleanup, and invalidation of
  every retained handle. Existing camera, projectile, particle, callback,
  restart, and startup-rollback behavior tests remain active.
- Source and CMake checks found no remaining custom ECS manager/storage,
  capacity-limit, free-slot counter, or `worms_ecs` references.
- CRT leak diagnostics, ASan instrumentation, and interactive gameplay checks
  were not performed for this step. Passing tests do not prove complete
  memory-leak freedom.
