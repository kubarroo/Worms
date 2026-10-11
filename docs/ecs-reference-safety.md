# Safe ECS Entity And Component References

## Scope

These rules apply to the scene-owned EnTT registry. See [Scene Lifetimes](Architecture.md)
for the current ownership and cleanup contract. The custom ECS has been removed;
entity types and owner-aware handles are defined in `Core/EntityHandle.h`.

## Component References

- A pointer, reference, or `std::reference_wrapper` to a component represents borrowed, short-lived access, not ownership of the component.
- Do not store them in object fields or across frames. A long-lived observer stores an entity handle and retrieves the component on every use.
- Removing components or entities may move other components in storage. After such an operation, an earlier reference may point to another entity's data.
- Do not retain references across a callback or function call that may remove an entity or component. This also applies to `b2World::DestroyBody()`, which may invoke end-contact callbacks.
- Copy required input data before such a call. After it returns, recheck entity validity and retrieve the component to which the result will be written.
- `Camera::X()` and `Camera::Y()` return values, while `FocusPoint::GetPos()` returns a copy of `Position`. Change camera position through its methods, not through a retained component reference.

## Observer Handles

- `EntityId` is `entt::entity` and includes a slot and version. Entity zero is valid; `entt::null` represents an invalid ID. Reused slots have a new version.
- Long-lived references to other objects' entities use `EntityHandle`, obtained through `GameObject::GetHandle()`. Represent a missing target with an empty `std::optional`.
- Check `IsValid(registry, handle)` before using a handle. It checks registry ownership and `registry.valid(handle.id)`. EnTT validity alone does not reject an ID from another scene.
- Entity validity alone does not guarantee the presence of `Position` or another component. After checking the handle, retrieve the required component through `registry.try_get<T>()`.
- Losing the entity or a required component clears the target. An observer does not automatically adopt a new entity under the same number or resume tracking when a component is added again. The target must be set explicitly.
- When the weapon loses its parent, it resets its charge and neither fires nor renders the charge bar. Setting the same valid parent again preserves the charge; changing the parent resets it.
- Handles and objects storing a borrowed `SceneContext` must not outlive its services. Detach or destroy them before the scene's registry is destroyed. A handle does not extend registry lifetime, and its `owner` field is used only for identity comparison.
- Entity owners may continue to use `EntityId` within their own lifetimes. Do not remove their entities independently of their owners while leaving them with stale identifiers.

## Particle Callbacks And System Iteration

- Systems use `registry.view<...>()`, not persistent subscription sets. Their update/render order is explicit in `GameScene`.
- `ParticleUpdater` snapshots matching entity IDs and callable handles before invocation, then rechecks entity validity and component membership and reacquires writable components. Added particles wait for a later update; reused slots are not mistaken for old entities.
- `ParticleCharacteristic` retains the callable through shared ownership. Mutable callback state survives updates and storage compaction; copies share that state. Removing or replacing the current component does not destroy the executing callable.
- Other systems do not promise arbitrary structural mutation during view iteration. Request gameplay object changes through `QueueAdd`, `RequestDestroy`, or `RetireObject`; execute them at safe boundaries.
- Physical object removal must use its cleanup process, not raw `registry.destroy()`. Object initialization/cleanup and collider creation reject a locked Box2D world.
- Scene cleanup requested during a frame, physics step, or guarded object cleanup is deferred. Never destroy/reset the scene itself from an executing callback, including cleanup callbacks.

## Change Verification

When changing observers and callbacks, check component compaction, target removal, required component removal, entity number reuse before the next update, and observer cleanup before world destruction.
