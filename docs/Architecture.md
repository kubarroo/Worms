# Scene Lifetimes

`App` owns the platform: SDL initialization, window, renderer, events, and ImGui.
`Game` owns `GameScene` and releases it before the platform is shut down.

## Dependency Context

`SceneContext` borrows the renderer, ECS world, physics world, and `ObjectCommands`.
It is not a resource owner or a global service locator. Objects store a copy of
the context, so a temporary context value is safe; its referenced dependencies
must still outlive the object. A null renderer is supported for headless tests.

`ObjectCommands` exposes only queued addition and removal. Gameplay objects
cannot access scene internals or perform immediate insertion through this API.
Existing global input and collision/physics services remain transitional.
Until the physics factory becomes scene-owned, worm, projectile, and map
initialization verifies that its world matches the context before allocation.

## Ownership

| Owner | Owned resources | Borrowed dependencies |
| --- | --- | --- |
| `GameScene` | ECS, physics world, camera, weapon, map, projectiles, particles, managers, music | App renderer |
| `WormManager` | Teams | Context, camera, weapon |
| `WormTeam` | Worms, health-bar texture, death sound | None |
| `Worm` | Sprite texture, jump sound, health bar, collider wrapper | Camera, team health-bar texture, context |
| `Camera` | Focus point | Context |
| `WeaponManager` | Weapon configurations, shared textures and sounds | Renderer, scene-owned weapon |
| `Weapon` / `Projectile` | Object-specific resources | Shared weapon-manager textures and sounds, context |

Box2D owns its bodies. Object cleanup explicitly destroys the bodies it created
while the physics world is still alive. ECS components borrow texture/body pointers.

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
