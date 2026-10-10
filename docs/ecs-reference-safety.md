# Safe ECS Entity And Component References

## Scope

These rules apply to the current ECS and must be preserved during migration to EnTT. They complement the [ownership and cleanup contract](ownership-and-cleanup.md).

## Component References

- A pointer, reference, or `std::reference_wrapper` to a component represents borrowed, short-lived access, not ownership of the component.
- Do not store them in object fields or across frames. A long-lived observer stores an entity handle and retrieves the component on every use.
- Removing components or entities may move other components in storage. After such an operation, an earlier reference may point to another entity's data.
- Do not retain references across a callback or function call that may remove an entity or component. This also applies to `b2World::DestroyBody()`, which may invoke end-contact callbacks.
- Copy required input data before such a call. After it returns, recheck entity validity and retrieve the component to which the result will be written.
- `Camera::X()` and `Camera::Y()` return values, while `FocusPoint::GetPos()` returns a copy of `Position`. Change camera position through its methods, not through a retained component reference.

## Observer Handles

- `EntityId` is a slot number, not a persistent entity identity. Number `0` is a valid identifier, and a released number may be assigned again.
- Long-lived references to other objects' entities use `EntityHandle`, obtained through `World::GetHandle()`. Represent a missing target with an empty `std::optional`, not number `0`.
- Check `World::IsAlive(handle)` before using a handle. The check includes the slot number, generation, and handle owner.
- Entity validity alone does not guarantee the presence of `Position` or another component. After checking the handle, retrieve the required component through `TryGetComponent()`.
- Losing the entity or a required component clears the target. An observer does not automatically adopt a new entity under the same number or resume tracking when a component is added again. The target must be set explicitly.
- When the weapon loses its parent, it resets its charge and neither fires nor renders the charge bar. Setting the same valid parent again preserves the charge; changing the parent resets it.
- Handles and objects storing a borrowed `World*` must not outlive their world. They must be detached or destroyed before `World` is destroyed. A handle does not extend the world's lifetime, and its `owner` field is not intended to be dereferenced.
- Entity owners may continue to use `EntityId` within their own lifetimes. Do not remove their entities independently of their owners while leaving them with stale identifiers.

## Particle Callbacks And System Iteration

- Particle behavior functions called by `ParticleUpdater` compute and return values. They do not directly remove entities or add or remove components during system iteration.
- During iteration, a system uses component references and its subscribed entity set. Direct structural ECS changes from a callback may invalidate both kinds of access.
- If a callback needs to request structural changes, it records a command in a queue. Execute commands after iteration has finished, without retaining component references.
- A deferred component-change queue is not currently a general ECS mechanism. Before adding such behavior to callbacks, implement it and add tests, including removal of the current entity and reuse of its number.

## Change Verification

When changing observers and callbacks, check component compaction, target removal, required component removal, entity number reuse before the next update, and observer cleanup before world destruction.
