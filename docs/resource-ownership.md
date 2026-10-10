# Phase 3 Resource Ownership Contract

This is the current contract for phase 3, steps 1 through 3. The older
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
| Cached textures, sounds and music | App through ResourceManager | SceneContext and asset callers |
| Weapon/projectile textures and weapon sounds | ResourceManager | Weapon, Projectile, Sprite components |
| Charge-bar texture | ResourceManager | Weapon rendering |
| Worm sprite and jump sound | ResourceManager | Worm, its Sprite component and playback |
| Generated team health-bar texture | WormTeam | Its worms and HealthBar objects |
| Team death sound | ResourceManager | WormTeam and playback |
| Particle texture | ResourceManager | ParticleSystem and its particles' Sprite components |
| Mutable terrain surface and current texture | Map | Contour generation and its Sprite component |
| Music | ResourceManager through Music | GameScene controls playback |

GameScene detaches borrowers; it never clears the shared cache. App releases the cache
after destroying the scene. WeaponManager owns configurations, not asset caches.
WormTeam keeps its texture alive until its worms and health bars have been removed.
Map owns independent mutable terrain pixels and the replacement texture; terrain is
not a shared mutable cache entry.

## Shutdown

1. Game stops music and all audio channels without closing the device.
2. Game destroys its scene, including private resources and current scene-owned assets.
   GameScene also stops playback for standalone scene cleanup and startup rollback.
3. App releases ResourceManager and its cache after all scene borrowers.
4. App shuts down ImGui backends and its context.
5. App closes the audio device.
6. App resets the renderer, then the window.
7. App calls SDL_Quit for its initialized runtime.

App::Clean also stops playback so it works independently. Only successfully acquired
platform resources are released; failed initialization runs the same cleanup path.
Standalone scenes borrow a live platform and do not close it. Their current cleanup
stops all playback on the shared mixer, so audio isolation between simultaneous scenes
is not provided by this contract.

## ResourceManager

App owns a ResourceManager tied to its renderer, created after renderer/audio
initialization and before the scene. Scenes borrow it through SceneContext and must
use the same renderer. Headless contexts may use a manager with a null renderer;
texture loading then throws. Audio loading requires an open mixer device.

GetTexture, GetSound and GetMusic load on first access and return borrowed assets.
Failed loads throw SDL_Exception with the filename and library error; no failed entry
is cached. Sound and Music constructors also report errors this way.

App::SetAssetRoot configures an explicit base directory before initialization. The
default is the working directory at initialization for existing launch workflows.
The manager stores an absolute base directory, so later working-directory changes
do not affect lookup. Paths are normalized with std::filesystem::path::lexically_normal
without filesystem queries on cache lookup. Relative, absolute and normalized aliases
share an entry; symbolic-link aliases are not resolved. Cache keys use filesystem
path equality; arbitrary case aliases are not folded, even on Windows.

Gameplay obtains shared file assets through ResourceManager. Object cleanup removes
entities/components and clears borrowed pointers without freeing cached assets.
Mutable terrain and generated team textures retain private owners. Map loads its
own source surface using the manager's asset root and builds private textures; its
pixel data is never shared with another map.

Missing-file tests use a fresh manager with a separate asset directory. Successful
loads remain cached after scene startup failure; restoring the missing file permits
retry on that same manager. Restarting a scene does not require files already cached.

The initial cache keeps assets until full application cleanup. No per-entry eviction
or hot reload is allowed while borrowers exist. Borrowed pointers remain valid until
cache cleanup; recreation of the platform invalidates all old handles. A texture cache
must never be reused with a different renderer. This lifetime rule avoids requiring
shared_ptr for every asset.
