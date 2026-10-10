# Phase 3 Resource Ownership Contract

This is the current contract for phase 3, step 1. The older
`ownership-and-cleanup.md` is a historical phase-1 proposal. Scene ownership is
described in `Architecture.md`.

## Rules

- Every resource has exactly one owner. Owning handles use RAII; raw pointers and
  references only borrow resources and never release them.
- A borrower must be detached or destroyed before its owner releases the resource.
  Removing a Sprite component does not destroy its texture.
- SDL textures belong to the renderer that created them and must be released before
  that renderer. SceneContext borrows the renderer; it does not own the platform.
- Sound and Music exclusively own Mix_Chunk and Mix_Music, respectively. Playback
  must stop before their owners are destroyed, while the audio device is still open.
- Cleanup supports partial initialization and repeated calls. Shutdown must continue
  releasing remaining resources if an object's cleanup reports an error.

## Current Owners

| Resource | Owner | Borrowers |
| --- | --- | --- |
| Window and renderer | App: WindowPtr and RendererPtr | GameScene, SceneContext, rendering objects, ImGui |
| Audio device and SDL/ImGui initialization | App | Scene and audio wrappers |
| Weapon/projectile textures and weapon sounds | WeaponManager | Weapon, Projectile, Sprite components |
| Charge-bar texture | Weapon | Its rendering code |
| Worm sprite and jump sound | Worm | Its Sprite component and playback |
| Generated team health-bar texture and death sound | WormTeam | Its worms and HealthBar objects |
| Particle texture | ParticleSystem | Its particles' Sprite components |
| Mutable terrain surface and current texture | Map | Contour generation and its Sprite component |
| Music | GameScene through Music | Audio playback |

GameScene cleans active and pending objects before releasing WeaponManager's assets.
WormTeam keeps its texture alive until its worms and health bars have been removed.
Map owns independent mutable terrain pixels and the replacement texture; terrain is
not a shared mutable cache entry.

## Shutdown

1. Game stops music and all audio channels without closing the device.
2. Game destroys its scene, including private resources and current scene-owned assets.
   GameScene also stops playback for standalone scene cleanup and startup rollback.
3. After ResourceManager is introduced, release its cache here, after all borrowers.
4. App shuts down ImGui backends and its context.
5. App closes the audio device.
6. App resets the renderer, then the window.
7. App calls SDL_Quit for its initialized runtime.

App::Clean also stops playback so it works independently. Only successfully acquired
platform resources are released; failed initialization runs the same cleanup path.
Standalone scenes borrow a live platform and do not close it. Their current cleanup
stops all playback on the shared mixer, so audio isolation between simultaneous scenes
is not provided by this contract.

## ResourceManager Contract For Subsequent Steps

App will own a ResourceManager tied to its renderer. Shared immutable textures,
sounds, and music move from the owners above into that manager; mutable terrain and
generated team textures retain their private owners. Scenes borrow the manager through
SceneContext. There is no ResourceManager implementation in step 1.

The initial cache keeps assets until full application cleanup. No per-entry eviction
or hot reload is allowed while borrowers exist. Borrowed pointers remain valid until
cache cleanup; recreation of the platform invalidates all old handles. A texture cache
must never be reused with a different renderer. This lifetime rule avoids requiring
shared_ptr for every asset.
