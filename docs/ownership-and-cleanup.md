# Resource Ownership And Object Lifetimes

## Status And Scope

This document defines the proposed contract for phase 1: stabilizing ownership, cleanup, and application shutdown. It does not imply that the rules described here have already been implemented.

This is the historical phase-1 proposal. For current scene ownership, local physics
services, and camera-to-turn coordination, see [Scene Lifetimes](Architecture.md).
For the current SDL and asset ownership contract, see
[Phase 3 Resource Ownership](resource-ownership.md).

It is based on an earlier project review. While this document was being written, the terminal tool did not allow the files to be read again, so the current code must be verified before implementation. This document does not cover migration to SDL3 or EnTT, introducing a scene, or a full resource manager.

## Ownership Rules

Rules for short-lived component access, entity handle validity, and callback restrictions are described in [Safe ECS Entity And Component References](ecs-reference-safety.md).

- Every resource has one explicit owner responsible for releasing it.
- A field stored by value or through unique_ptr represents ownership. A raw pointer or reference represents access without permission to delete.
- Shared access to a texture or sound does not automatically require shared_ptr. The owner must outlive every user of the resource.
- Sprite and RigidBody components store borrowed handles. Removing a component does not itself release the texture or Box2D body.
- Copying a resource owner is prohibited unless the class defines correct copy semantics. Moving must preserve addresses used by callbacks and userData, or update those references.
- Pointers start as nullptr, and identifiers start in an explicit invalid state compatible with the current ECS. Do not assume that identifier 0 is invalid.

## Ownership Table

The table describes the intended responsibilities within the existing class structure. It does not yet prescribe a specific RAII handle implementation.

| Resource / object | Owner | Observers / users | Release responsibility |
| --- | --- | --- | --- |
| SDL_Window, SDL_Renderer | App | Game and rendering objects | App; renderer before window, after all textures have been released |
| SDL, audio, and ImGui backend initialization | App | Entire application | App; shuts down only successfully initialized subsystems |
| ECS World and ECS managers | App / World | GameObject, systems, and gameplay managers | World after all objects have been detached |
| Systems and component storage | Respective ECS managers | World | Managers; verify polymorphic base destructors |
| b2World and debug draw | App | ColliderFactory and physics objects | App after objects have been removed and the listener detached |
| WormManager, WeaponManager, Music | Game | Gameplay logic | Game; Music before audio shutdown |
| WormTeam | WormManager | Active team and turn logic | WormManager after worms have been detached |
| Worm | WormTeam | Active worm, camera, weapon | WormTeam after Worm::CleanUp() |
| Worm health bar | Worm | Rendering and damage handling | Worm after the health-bar entity has been detached |
| Team health-bar texture | WormTeam | HealthBar objects belonging to its worms | WormTeam after all health bars have been destroyed |
| Map, Camera, Weapon, Projectile, ParticleSystem in the active list | GameObject::activeObjs | Game and managers | Container after CleanUp() on every initialized object |
| Objects in objsToAdd | GameObject::objsToAdd | Object creation logic | Container; ownership transfers to activeObjs on activation |
| Entries in objsToDelete | No ownership; command queue | Game loop | Removal request only; does not perform an independent delete |
| WeaponImpl and weapon configuration sounds / textures | WeaponManager | Weapon and Projectile | WeaponManager after all users have been detached |
| Charge-bar texture | Weapon | Weapon::Render() | Weapon before renderer destruction |
| Worm sprite texture, currently loaded separately | Worm | Worm Sprite component | Worm; HealthBar does not take ownership |
| Particle effect texture | ParticleSystem | Sprite components of its particles | ParticleSystem after particle entities have been removed |
| Terrain image data and its current texture | Map / its PhysicTexture wrapper | Terrain generation and map Sprite | Map through a single owner; check the surface and pixel buffer |
| Collider wrapper | Worm or Projectile | Logic of the corresponding object | Object owning the wrapper |
| Box2D body and fixtures | b2World physically stores them; the gameplay object is responsible for earlier removal | RigidBody, Collider, and callbacks | Object CleanUp() calls DestroyBody once; fixtures are removed with the body |
| Data referenced by body / fixture userData | Gameplay object providing the data | Box2D and ContactManager | Must remain alive until DestroyBody finishes; must not reference temporary arguments |
| Worm, sensor, health-bar, map, weapon, and projectile entities | Object that created them | ECS, camera, and gameplay logic | Creator's CleanUp() removes all of its entities |
| ParticleSystem entity and its particle entities | ParticleSystem | ECS systems | ParticleSystem removes both the particles and its own entity |
| FocusPoint and its entity | Camera / FocusPoint | Target tracking | Camera schedules entity cleanup while the ECS still exists |
| Mix_Chunk, Mix_Music | Sound, Music | Audio playback | Wrapper after resource use has stopped, before audio shutdown |
| Collision subscription | Object registering the callback | ContactManager stores the function | Object unregisters it before destroying state captured by the callback |
| Camera noTargetEvent | Camera stores the function; WormManager is responsible for the validity of the captured manager | Camera::Update() | Detach before WormManager destruction |

The ContactManager and ColliderFactory singletons do not own gameplay objects or the physics world. Their borrowed references and registries must be cleared before their dependencies are destroyed. The singleton reset mechanism must be checked against the current API.

## CleanUp() Contract

### Purpose And Caller

CleanUp() detaches an object from the running game world. The owner calls it before destroying an initialized object, both during gameplay and during application shutdown.

The destructor releases local resources owned by the object. We do not rely on a virtual CleanUp() call from the GameObject destructor: such a call would not execute the derived class implementation. App and GameObject must have virtual destructors if derived objects are deleted through base pointers.

### Requirements

- CleanUp() is idempotent: subsequent calls do not remove resources a second time.
- It works after partial initialization by checking resources that were actually acquired.
- It does not propagate exceptions during shutdown. Ultimately it should satisfy a noexcept contract; verify the operations it calls before adding the declaration.
- Once cleanup starts, the object does not execute Update(), Render(), or new callbacks that require its resources.
- It removes all entities created by the object, including auxiliary sensors and effects.
- It detaches subscriptions before removing state referenced by callbacks.
- DestroyBody runs outside the simulation step, while the Box2D world is unlocked. userData remains valid throughout the call.
- Body and entity handles are invalidated after release. Merely clearing Sprite.texture does not free the texture.
- Local textures may be released in the destructor, but only after removing components that use them and before destroying the renderer.

### Order For A Single Object

1. Mark the object inactive / being cleaned and prevent further updates.
2. Invalidate observer references or ensure target validity is checked before the next use.
3. Unregister callbacks capturing the object and stop external use of local resources.
4. Detach owned auxiliary objects if their cleanup still requires the parent's entities.
5. Destroy owned physics bodies, keeping userData and entities valid until the operation completes.
6. Remove remaining auxiliary entities and the main entity; invalidate handles.
7. Let the owner destroy the object. The destructor releases local resources.

The exact order for auxiliary objects follows their dependencies. Calling only the base CleanUp() is insufficient if the class creates additional entities, bodies, or subscriptions.

### Queues And Observers

- Requesting removal of the same object multiple times results in one cleanup and one destruction.
- Removal is processed at a safe point in the loop, outside iteration over the affected container and outside the Box2D step.
- Shutdown covers active objects and objects awaiting addition. The latter may already have acquired resources in their constructors.
- Active-worm and active-team pointers are updated before removing a worm or team.
- The camera target and weapon parent become invalid when their corresponding entities are removed. Identifier reuse must not accidentally attach them to a new object.
- Camera does not retain a Position*: the current component storage may move elements during removal. Components are retrieved through a valid entity identifier when needed.

## Application Shutdown Order

Shutdown must also work after initialization fails. The following order is proposed for the existing dependencies; it requires explicit coordination by Game and App because static containers do not guarantee correct destruction timing.

1. Stop the game loop, updates, object creation, and initiation of new audio effects.
2. Detach noTargetEvent and references to the active worm, team, camera target, and weapon parent. Detach collision callbacks of objects scheduled for removal.
3. Call CleanUp() on all active objects and partially initialized pending objects while the ECS, Box2D, and resource managers still exist. Detach worms, health bars, and FocusPoint.
4. Destroy objects using weapon configuration resources, including projectiles and Weapon. Empty activeObjs, objsToAdd, and objsToDelete; do not create new objects during cleanup.
5. Destroy WormManager and its teams, then WeaponManager. Health bars must be destroyed before team textures. Release remaining textures and gameplay object wrappers.
6. Stop music and channel playback, then release remaining Sound and Music objects. Verify how to stop resource use with the current SDL_mixer version.
7. Clear ContactManager, detach the listener and debug draw from Box2D, and reset ColliderFactory's borrowed b2World reference.
8. Destroy the ECS and Box2D worlds and debug draw after all gameplay object operations have completed.
9. Shut down ImGui backends and its context while the renderer and window still exist.
10. Shut down audio and auxiliary library subsystems according to the initialization that actually completed.
11. Destroy the renderer, then the window, and finally shut down SDL.

Normal exit and initialization failure handling use the same contract. Resources acquired in a constructor that throws must be protected by local RAII because the destructor of an incompletely constructed object will not run.

## Items To Confirm Before Implementation

- Current resource inventory and destructors of Sound, Music, PhysicTexture, and ECS base classes.
- Every texture creation and replacement path, including terrain rebuilding and failed loading.
- Lifetime of WeaponManager resources borrowed by Projectile.
- Construction and initialization of FocusPoint and cleanup of its entity.
- Callbacks executed during DestroyBody and rules for modifying the listener registry during dispatch.
- Every address passed to userData, especially temporary arguments and possible object moves.
- How to identify an invalid entity in the current ECS and protect against identifier reuse.

## Contract Acceptance Criteria

- Every resource has an identified owner and release location; observers do not delete or release borrowed handles.
- Cleanup covers removal during gameplay, normal shutdown, and partial initialization failure.
- Repeated cleanup and repeated removal requests do not cause double release.
- No objects, components, or callbacks using a dependency remain when it is destroyed.
- Repeated creation and removal cycles restore the expected entity, body, subscription, and resource counts. Memory assessment accounts for library pools and caches rather than requiring identical process memory usage.
