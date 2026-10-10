# Phase 3 Resource Ownership Contract

This is the current contract for phase 3, steps 1 through 5. The older
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
| Music | ResourceManager through Music | GameScene requests playback through Audio |
| Renderer2D, Audio, Input and SDL input translator | App | SceneContext and gameplay |

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
6. App releases Renderer2D, then resets the native renderer and window.
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

## Mutable Resources

Cached textures are shared read-only assets. Borrowers must not write pixels or
persistently change color/alpha modulation, blend mode, or scale mode. SDL exposes
mutable native handles, so this is an API contract rather than const enforcement.
Any future per-instance rendering state must be applied and restored by the renderer,
or use a privately owned texture. Current gameplay does not modify cached texture state.

Every Map independently loads source pixels, owns its surface and current texture,
and rebuilds only its own physics body. Source-image caching is not introduced.
Deforming or destroying one map must leave other maps and the source asset unchanged;
reinitialization loads the original terrain again.

WormTeam exclusively owns its generated health-bar texture. All its worms' health bars
borrow that texture. Different teams have different textures. Removing a worm leaves
the team texture alive. CleanUp removes borrowers but retains the texture for team
restart; destroying the team releases it. Member declaration order also destroys
worms before the texture if destruction occurs without an explicit CleanUp call.

## Platform Adapters

Renderer2D borrows the native renderer and draws sprites, optional source rectangles,
rotation/pivots, and debug lines. Coordinates remain screen-space integers; the camera
and rendering systems retain world-to-screen conversion. It never owns or changes
borrowed textures. Native handles remain available for ImGui, asset loading, and
private mutable texture generation. These native resource operations have not been
moved behind a new asset/backend hierarchy.

Audio plays borrowed Sound/Music assets; those wrappers only own decoded data.
Playback counts are total plays: 1 means once, -1 means indefinitely; zero and values
below -1 are rejected. Sound playback returns a channel index or -1 if unavailable.
Stops are idempotent, including before device initialization or after shutdown.
Volume is normalized to [0, 1]. The current mixer and stop operations remain global:
simultaneous scenes do not have isolated audio buses.

Input stores platform-independent actions per App. SdlInputAdapter translates events
after forwarding them to ImGui. BeginFrame clears press/release edges, not held state.
One-shot actions consume a press once per frame; repeated SDL keydown events are ignored.
Opposite movement keys cancel, and releasing one restores the other held direction.
Existing key bindings are retained; disabled team/worm hotkeys are not activated.

Focus loss and ImGui keyboard capture clear actions and disable gameplay input.
Re-enabling requires a fresh non-repeat keydown. An interruption is retained for the
current frame even if focus returns in the same event batch. A persistent interruption
counter lets Weapon cancel charge even when its updates were skipped; BeginFrame and
Reset never erase that counter. Scene restart resets action state, while full
platform initialization/shutdown also resets the SDL translator.

App::HandleEvents is a no-op before platform initialization and after cleanup,
including failed startup. Renderer2D::DrawLine uses the backend directly without
allocating a temporary vector.
