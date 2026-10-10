# Scene Lifetimes

`App` owns the platform: SDL initialization, window, renderer, events, and ImGui.
`Game` owns `GameScene` and releases it before the platform is shut down.

## Dependency Context

`SceneContext` borrows the renderer, ECS world, physics world, `ObjectCommands`,
collider factory, and contact manager.
It is not a resource owner or a global service locator. Objects store a copy of
the context, so a temporary context value is safe; its referenced dependencies
must still outlive the object. A null renderer is supported for headless tests.

`ObjectCommands` exposes only queued addition and removal. Gameplay objects
cannot access scene internals or perform immediate insertion through this API.
Input, frame timing, and logging still use global services. Physics services are
scene-owned: `ColliderFactory` is constructed with a world and contact manager,
and cannot be rebound. Worm, projectile, and map initialization checks that both
services match the context before allocation. Collider event methods use the
contact manager supplied by the factory, never a global registry.

## Ownership

| Owner | Owned resources | Borrowed dependencies |
| --- | --- | --- |
| `GameScene` | ECS, physics world, collider factory, contact manager, camera, weapon, map, projectiles, particles, managers, music | App renderer |
| `WormManager` | Teams | Context, camera, weapon |
| `WormTeam` | Worms, health-bar texture, death sound | None |
| `Worm` | Sprite texture, jump sound, health bar, collider wrapper | Camera, team health-bar texture, context |
| `Camera` | Focus point | Context |
| `WeaponManager` | Weapon configurations, shared textures and sounds | Renderer, scene-owned weapon |
| `Weapon` / `Projectile` | Object-specific resources | Shared weapon-manager textures and sounds, context |

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

Scene cleanup disconnects physics callbacks, cleans teams while camera and
weapon still exist, then cleans scene objects. Shared weapon resources outlive
their borrowers. The ECS and physics worlds are released last. A failed startup
object remains owned until dependent managers have been cleaned and destroyed.

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
startup and cleanup can affect global audio playback.
