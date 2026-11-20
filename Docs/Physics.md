# Physics

The physics is Jolt Physics 5.6.0. It steps at a fixed 1/60 s; `OnFixedUpdate` runs after each step.

## Bodies

- `PhysicalBox`, `PhysicalSphere`, `PhysicalCapsule`, `PhysicalHeightField` and `PhysicalMesh` are the colliders.
- A capsule's `height` is its straight part: the body is `height + 2 × radius` tall, centred on its actor.
- A collider scales with its actor: a capsule's radius follows the X scale and its straight part the Y scale.
- A negative scale mirrors what is drawn, not what collides: a mesh collider or a height field under a mirrored actor
  collides as the unmirrored shape, as with Bullet before. Mirror their data instead.
- A linear or angular factor only frees or locks an axis: 0 locks it, anything else frees it whole. A body with no axis
  free is simulated as kinematic.
- A static body (`SetStatic(true)`, `static` in a file) never moves by itself and only follows its transform when the
  transform changes; it ignores the kinematic setting. Level geometry and props stand on static bodies.
- A height field (`Actor:AddPhysicalHeightField`, then `SetHeights(sampleCount, spacing, heights)`) is static. The heights
  are row-major, along +X first, then +Z: the sample (x, z) is at index `z * sampleCount + x + 1`, at
  `(x * spacing, height, z * spacing)` from the actor, each cell cut along its (x, z)–(x + 1, z + 1) diagonal. It
  returns `true`, or `false` and why. Bodies rest about 2 cm into it. A height of `math.huge` is a hole: no triangle
  touching that sample collides, so a body falls through (a cave's mouth, a pit).
- A mesh collider (`Actor:AddPhysicalMesh`, then `SetTriangles(positions, indices)`, or `SetMesh` with a `MeshData`)
  is static too. Its triangles are in the actor's space and collide from their front only, the side their
  counter-clockwise winding faces: a ray or a body coming from behind passes through. Build it once; changing its
  triangles rebuilds the whole shape.
- A height field's heights and a mesh collider's triangles come from scripts and are not saved with the scene or the
  prefab: the script that sets them sets them again when the scene loads.

## Triggers and contacts

- A trigger has no collision response. A collider can be switched to a trigger and back at run time
  (`PhysicalObject:SetTrigger`).
- `OnTriggerStay` comes once per physics step, so not every frame above 60 FPS, for every collider in the trigger that
  is not itself a trigger: a test of who stands in a trigger must allow for gaps.

## Moving bodies

- The physics step runs before the `OnFixedUpdate` that sets a velocity: reading the velocity back returns the order
  given, even against a wall. Measure real speed from displacement.
- A script that rewrites the linear velocity every step wipes any impulse; apply pushes through the same script.
- A body moved in `OnFixedUpdate` only moves on fixed steps: anything sampled per frame (a camera) must follow an
  interpolated visual, not the body itself.
- A kinematic body follows its transform by jumping to it at each step, at no speed: it pushes what is in its way but
  does not carry what stands on it. A moving platform carries its riders in script, or moves them as it moves.

## Queries

- `Physics.Raycast(origin, direction, distance)` returns `nil`, or a `RaycastHit` with `FirstResultObject` and
  `ResultObjects`: the objects the ray crosses, in no particular order and without distances or points. To find where a
  ray meets something, bisect the distance. Triggers are reported like any other object.
- Only one ray is cast per call; there is no sphere or capsule cast and no overlap test.

## Ragdolls

- `SkinnedMeshRenderer:StartRagdoll(parts, velocity)` hands the skeleton over to capsules joined by swing-twist
  constraints, from its current pose; `StopRagdoll`, `IsRagdoll`, `AddRagdollImpulse`.
- A ragdoll's bodies have a layer of their own: they fall on static and kinematic bodies and on each other, go through
  dynamic bodies, raise no collision event, and raycasts never return them.
- While a ragdoll drives a skeleton, the animation does not; the bones it does not move keep their pose relative to
  their parent.
- A part's `Radius` and `Length` are in metres in the world, whatever the scale of the actor and its bones: a smaller
  character passes smaller values.
- A ragdoll collides with the kinematic body of its own character, which still stands where the character fell: turn
  that body off, or make it a trigger, before starting the ragdoll.
