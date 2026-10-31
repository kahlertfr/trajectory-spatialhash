# Visible-sample collision bounds

The result collision shapes now fit the samples that Niagara considers visible.
Full-result bounds still control the original layout and scale. Filtering and
switching object-view alignment do not recenter the visualization or change its
interaction reference transforms.

## Niagara contract

Both `NS_FRNN_Result` and `NS_FRNN_Result_OnlyQuery` expose
`User.VisibleBounds` (`UNiagaraDataInterfaceVisibleBounds`) and
`User.VisibleBoundsReceiver` (object). `ATrajectoryQueryNiagaraActor` binds itself
as the receiver before activation. This is a GPU-only data interface.

The existing path/object transform modules save their already-computed position
before the additional grab matrix in `Particles.BoundsPosition`, together with
`Particles.BoundsView` (0=path, 1=object). A final `ExportVisibleBounds` module
reads the existing `Particles.Visible` and final color alpha. It contributes
sample centers to a shared atomic min/max reduction. Time, radius, category,
periodic-image, and object-rotation rules are not repeated in C++.

Query-sphere extent update (2026-10-20): `CalculateParticleScale` additionally
stores `Emitter.ParticleBoundsRadius = abs(User.ParticleRadius * displayScale)` using
its existing radius/scale inputs, before the additional grab scaling. The final
`ExportQuerySphereBounds` module contributes that radius for each visible query
sphere in path and object view. Other samples still contribute their centers.
Collision geometry therefore encloses the full sphere, and increasing the user
particle radius expands the grab box. The query trajectory and sphere remain
visible during context isolation; existing explicit visibility controls still apply.

The full result system collects the focus ribbon, context ribbons, and focus
point markers in both views. Decorative radius indicators, direction arrows,
and collision-event markers do not enlarge sample collision bounds. The
focus-only system collects the path focus ribbon.

Each view uses seven uint32 words: three encoded minima, three maxima, and a
sample count. Sign-preserving float ordering permits negative coordinates and
a zero-initialized buffer. A zero count produces an invalid/empty box.
Only 56 bytes are returned per active simulation render graph, regardless of
sample count. The dispatcher's post-render event queues asynchronous readback
after all Niagara emitters/stages have contributed; reading back from an
individual emitter's PostSimulate would return partial bounds.

Callbacks use weak actor references and a display revision; cancellation,
new uploads, and control changes invalidate earlier callbacks. There is no
synchronous GPU wait in the runtime implementation. Pending readbacks can lag
the display by several frames. The reduction currently runs each active frame;
large-sample GPU cost and headset frame rates still require profiling.
Bounds revisions are separate from query generations: changing visualization
controls must not invalidate the current query's loading or batch callbacks.

## Collision and Blueprint contract

`BP_QueryResultVisualization` retains `PathVisBox`, `RelativeVisBox`,
`PathVisOrigin`, and `RelativeVisOrigin`. The two component templates now use
`UVisibleSampleBoxComponent`, a UBoxComponent subclass. The C++ receiver reads
those existing Blueprint references and converts the pre-grab bounds into each
reference frame. All contributing emitters in the supplied systems use local
space: the conversion is Niagara component local-to-world followed by the
inverse saved world-space reference origin. Compose these matrices before
transforming the box so a rotated intermediate world AABB cannot inflate it.
Applying just the inverse origin incorrectly cancels the visualization's world
placement and fits collision near world origin. Use the Niagara component's
transform, not just the actor transform, because it may have an attachment offset.
The current grabbed box transform must not replace the saved reference origin.
Renaming these four Blueprint fields requires updating the
receiver contract. Custom result Blueprints must provide the same fields and
component subclass to use automatic fitting.

The collision geometry has an offset center within its component. Component
transforms, grab pivots, attached children, and Niagara interaction matrices
remain stable. A 0.1 cm local interaction margin surrounds sample centers;
the sphere radius is included as described above. This is not a reduction over
arbitrary rendered triangles or material displacement.
Empty views disable collision; returning samples restore the original collision
mode. Per-instance body setups avoid modifying shared/archetype collision data.

`RefreshObjectViewCollisionBox` returns its reference transform unchanged. It
sets one upward `ObjectViewPlacementOffset` from the union of full translation-only
and rotated object bounds, using the existing path fit scale and source-Y to
Unreal-Z mapping. Both modes therefore keep the same reference particle position,
while neither fitted sample collision box extends below the original object-view
axes origin (including the 0.1 cm interaction margin). The axes origin is the
existing bottom placement origin, not the collision component's center pivot.
Filters and alignment switches do not change this offset. Progressive data uploads
recompute it when the full bounds change. Grab pivots and component transforms
remain unchanged. Niagara already applies this parameter before grabbing and
exports bounds after placement; no asset migration is needed.

## Validation

### Axes corner placement (2026-10-20)

`ReceiveVisibleBounds` also resolves the Blueprint's `PathAxes` and `RelativeAxes`
scene-component fields after fitting their respective boxes. Each axes component
is positioned at the fitted collision geometry's local minimum (-X back, -Y left,
-Z bottom), including the 0.1 cm interaction margin. The offset collision center
is included; using only negative box extent would place the axes incorrectly.
`GetVisibleSampleLocalBounds` exposes these bounds, returning an invalid box for
an empty view.

The supplied Blueprint already parents each axes component to its box. The corner
update enables absolute scale, retaining the axes' own configured scale as world
scale while location and rotation still follow the box. Enlarging, shrinking, or
nonuniformly scaling either view therefore moves its axes with the fitted corner
without resizing the axes. This is applied even for empty/hidden views, before
their first valid bounds arrive. Custom components are attached with their world
transform preserved before setting the local location. Grabbing moves the axes
with the box without waiting for another GPU readback.
The axes temporarily hide for an empty view and restore their previous hidden
state when samples return. Bounds updates retain the existing asynchronous delay.
No Blueprint or Niagara asset migration is required.

This moves only the decorative axes. The original bottom placement origin
described above remains the fixed reference for `ObjectViewPlacementOffset`;
moving the axes does not reposition samples or change grab pivots.

The UE 5.6 Editor build and visible-bounds automation tests validate the collision
minimum, grabbing, refitting, empty-view restoration, and the real Blueprint's
box/axes callback contract. Axes scale checks cover both views under repeated
uniform and nonuniform scaling, rotation, and visibility restoration.
Interactive headset appearance remains a manual check.

### Earlier collision validation

This ports commit `82d113375c9678d14436453119cadd3121dfae48` onto the current
zone-visibility implementation. The existing transform source graphs only
differed by their bounds output pins, so those graphs and the bounds export
module were transferred without replacing the current system/emitter assets or
their newer zone/filter modules. Both Blueprint box templates were changed to
the new component class and their variable nodes refreshed.

The UE 5.6 TrajectoryBLabEditor target builds. The result Blueprint and both
Niagara systems compile, including D3D12/SM6 GPU shaders. The eight query tests
pass, covering signed-coordinate reduction, collision pivot and independent
scaling, stale readbacks after cancellation, stable object-view placement under
a scaled/rotated grab, displayed distances, sampling and cleanup. Migration and
validation helpers remain in the consuming worktree's ignored `codex/` folder.
The world-placement regression additionally checks translated, rotated, scaled
and large-world simulation origins, independent boxes, and refitting during a
scaled/rotated grab.

A synthetic D3D12 PIE check also passed: the full result system returned valid,
distinct path/object bounds through the asynchronous GPU callback; switching to
the focus-only system returned an empty object bound, and moving its display
window outside all sample timesteps returned an empty path bound. This used
generated sample data and a temporary editor test helper. The full system keeps
a focus marker visible, so its bounds need not become empty with the ribbon time
window alone.

The follow-up PIE fixture also places the Niagara component at (1200,-800,350)
with rotation and scale 1.5. It uses the Blueprint's reflected box/origin contract
and verifies that both actual collision component centers match the GPU sample
bounds transformed into world space, without moving their reference pivots.
Empty path bounds also disable the actual collision component in this fixture.

Headset interaction and real-data filtering still require manual validation.
No external datasets were accessed or modified.
