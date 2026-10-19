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
this is not a reduction over rendered triangles or material displacement.
Empty views disable collision; returning samples restore the original collision
mode. Per-instance body setups avoid modifying shared/archetype collision data.

`RefreshObjectViewCollisionBox` remains callable by existing Blueprints but now
returns its reference transform unchanged and clears `ObjectViewPlacementOffset`.
It does not move the component or its axes. Previously it bottom-anchored the box
separately for translation-only and rotated bounds, and sent the difference to
Niagara as a placement offset. That moved the reference particle when toggling
alignment. Both modes now keep the same reference particle placement; only the
aligned context geometry and the fitted collision shape change. Existing
Blueprint function signatures and Niagara parameter names are retained.

## Validation

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
