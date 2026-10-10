# Scene Lifetimes

`App` owns the platform and ResourceManager: SDL initialization, window, renderer,
events, ImGui, shared file assets, and the rendering/audio/input adapters.
`Game` owns `GameScene` and releases it before the platform is shut down.

## Dependency Context

`SceneContext` borrows the renderer, ECS world, physics world, `ObjectCommands`,
collider factory, contact manager, ResourceManager, Renderer2D, Audio, and Input.
It is not a resource owner or a global service locator. Objects store a copy of
the context, so a temporary context value is safe; its referenced dependencies
must still outlive the object. A null renderer is supported for headless tests.

`ObjectCommands` exposes only queued addition and removal. Gameplay objects
cannot access scene internals or perform immediate insertion through this API.
Input is per-application and supplied explicitly. Frame timing and logging still use global services. Physics services are
scene-owned: `ColliderFactory` is constructed with a world and contact manager,
and cannot be rebound. Worm, projectile, and map initialization checks that both
services match the context before allocation. Collider event methods use the
contact manager supplied by the factory, never a global registry.

## Ownership

| Owner | Owned resources | Borrowed dependencies |
| --- | --- | --- |
| `App` / `ResourceManager` | Platform and shared textures, sounds, music | None |
| `GameScene` | ECS, physics world, collider factory, contact manager, camera, weapon, map, projectiles, particles, managers | App renderer, ResourceManager and adapters, music |
| `WormManager` | Teams | Context, camera, weapon |
| `WormTeam` | Worms, health-bar texture | Cached death sound and Audio |
| `Worm` | Health bar, collider wrapper | Cached sprite and jump sound, camera, team health-bar texture, context |
| `Camera` | Focus point | Context |
| `WeaponManager` | Weapon configurations | ResourceManager, Input, scene-owned weapon |
| `Weapon` / `Projectile` | Object-specific entities and physics | Cached textures and sounds, context |
| `ParticleSystem` | Particle entities and effect state | Cached particle texture, context |

Box2D owns its bodies. Object cleanup explicitly destroys the bodies it created
while the physics world is still alive. ECS components borrow texture/body pointers.
Fixtures also borrow `PhysicsInfo` from their owning gameplay objects. Factory
methods require an explicit non-const lvalue reference, rejecting temporary or
omitted metadata. The referenced data must remain at a stable address until the
body or fixture is destroyed, including any destruction-time contact callbacks.
The factory neither copies nor owns this metadata; the caller enforces its lifetime.

## Initialization And Cleanup

Gameplay constructors store configuration or accept ownership of existing
resources; they do not create entities, bodies, or subscriptions. Resource
creation occurs in explicit `Initialise` calls. Initializing an already active
object is rejected without cleaning its existing resources.

`GameScene` initializes its queued objects. `WormManager` initializes teams and
worms before transferring worms into their teams. Worms initialize their health
bars, and cameras initialize their focus points. This preserves the separate
team update path without updating or rendering worms twice through the scene.

Initialization failures clean partial resources; configuration is retained for
retry. Owners call `CleanUp` before destroying initialized objects. Context
access on an uninitialized or cleaned object is rejected. Scene insertion
consumes its argument even on rejection and attempts cleanup before destruction.

Teams accept worms only between `Initialise` and `CleanUp`. Rejected initialized
worms are cleaned before destruction. Cleanup retains the team's texture so the
same team can be initialized again. Worm spawn positions are explicit constructor
configuration. Managers advance spawn slots only after an entire team succeeds
and reset the slot counter during cleanup; failed initialization does not shift
positions or affect subsequent scene starts.

Scene cleanup follows this order:

1. Disable updates and reject new addition/removal requests; stop audio playback.
2. Detach the Box2D contact listener and debug draw.
3. Clean teams while the camera and weapon still exist.
4. Clean the failed startup object, active objects, and pending additions.
   Pending additions are discarded without being initialized.
5. Consume deferred contact errors and clear collision subscriptions.
6. Destroy the worm manager, clear removal requests, and destroy scene objects.
7. Destroy weapon configurations and debug draw, detach music, and destroy the
   retained failed startup object, then the context, factory, physics world,
   contact manager, and ECS world. The application-owned cache remains alive.

Cached assets outlive their scene borrowers, and the renderer remains
available throughout scene cleanup. A failed startup object remains owned until
dependent managers have been cleaned and destroyed. Cleanup is idempotent and
`noexcept`; individual object cleanup failures are logged without stopping the
release of remaining resources. Custom cleanup implementations must still
release their resources correctly; catching an exception cannot repair their
internal ownership mistakes.

`Game::Clean()` destroys its scene before `App::Clean()` shuts down the ImGui
renderer backend, platform backend, and context, then audio, renderer, window,
and SDL. Both cleanup entry points are `noexcept` and safe to call repeatedly.
The `Game` destructor releases gameplay first; the `App` destructor also releases
platform resources when no explicit cleanup was performed.

## Rollback And Restart

`App::InitWindow()` rolls back acquired platform resources on initialization
failure. `Game::InitWindow()` also releases the platform when scene creation or
initialization fails. Both preserve the original exception and permit a retry
on the same instance without an intermediate `Clean()` call. Rejecting a second
initialization of an already running instance does not tear down that instance.

The same `GameScene` can be initialized again after `CleanUp()`, using the same
live renderer. Each initialization creates new worlds, managers, camera, weapon,
and teams. Successful scene initialization resets shared input state and the
frame clock; application cleanup resets them too. A failed scene initialization
does not reset the input or frame clock of another running scene.

The application loop calls `Time::UpdateFrameTime()`. The first call after
`Time::ResetFrameClock()` yields zero elapsed time, excluding startup/restart
delays from simulation. Independent `Time::Timer` instances still measure
camera and projectile timeouts. Tests and external scene drivers may set
`Time::deltaTime` explicitly instead.

## Camera And Turn Coordination

Camera reports target loss through a consumable signal rather than a callback
capturing a gameplay manager. The signal becomes available after the existing
1.5-second timeout and is emitted once for a continuous target-loss episode.
Tracking a target, assigning a new target, or cleaning the camera clears stale
signals. The timer still resets when a valid target is observed.

`GameScene` consumes the signal immediately after the scene camera's update and
calls `WormManager::OnCameraTargetLost()`. This preserves the turn transition's
position in the object update order. The manager retains the existing rule that
an already selected successor team is not skipped after team elimination.

## Isolation Limits

Each scene has its own physics world, collider factory, collision subscriptions,
and deferred collision exceptions. Equal entity or subscription IDs in separate
registries do not share state. Scene cleanup affects only its own physics
services, and a fixture cannot be created through a factory for another world.

This is physics isolation, not full parallel-scene support: input, frame timing,
the renderer, and SDL_mixer playback are still shared. In particular, scene
startup and cleanup can affect global audio playback, and successful scene
startup resets shared input and frame timing. Application cleanup resets both
as well. Scene lifecycle operations must be coordinated by the owner outside
active update/render calls; cleanup is not an interrupt of an executing frame.
