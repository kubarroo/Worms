# Lifetime Verification

## Automated Coverage

`ApplicationTests/TestResourceManager.cpp` verifies cache reuse, normalized and
absolute paths, missing-file diagnostics and retry, platform restart, cache retention
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
| Session state | Held input and pending weapon change are reset; the first frame after resetting the frame clock has zero elapsed time |

Repeated cycles compare available entity slots, Box2D bodies, subscriptions, and
object queues. Tests check that weapon and particle-system asset references are
cleared without freeing cached textures, and that the map's private texture and
surface are released. Additional tests verify shared worm/particle textures,
continued death-sound playback after team deletion, and projectile assets surviving
WeaponManager destruction. These are not global SDL allocation counters or proof
that no leaks exist.

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
