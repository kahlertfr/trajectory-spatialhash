# Visible-sample collision bounds

The result collision shapes now fit the samples that Niagara considers visible.
Full-result path/object bounds still control the original layout, scale, axes,
and interaction reference transforms; filtering does not recenter the visualization.

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

## Collision and Blueprint contract

`BP_QueryResultVisualization` retains `PathVisBox`, `RelativeVisBox`,
`PathVisOrigin`, and `RelativeVisOrigin`. The two component templates now use
`UVisibleSampleBoxComponent`, a UBoxComponent subclass. The C++ receiver reads
those existing Blueprint references and converts the pre-grab bounds into each
reference frame. Renaming these four Blueprint fields requires updating the
receiver contract. Custom result Blueprints must provide the same fields and
component subclass to use automatic fitting.

The collision geometry has an offset center within its component. Component
transforms, grab pivots, attached children, and Niagara interaction matrices
remain stable. A 0.1 cm local interaction margin surrounds sample centers;
this is not a reduction over rendered triangles or material displacement.
Empty views disable collision; returning samples restore the original collision
mode. Per-instance body setups avoid modifying shared/archetype collision data.

## Validation

The UE 5.6 TrajectoryBLabEditor target builds. The result Blueprint and both
Niagara systems compile, including their GPU shaders. Query automation includes
signed-coordinate reduction and collision pivot/physics-shape checks, alongside
the existing query sampling tests. Helpers used for asset migration live only
under the consuming worktree's ignored `codex/` directory.

A synthetic headless GPU simulation was attempted. Unreal's renderer crashed
when flushing its manually created commandlet-world effects dispatcher, so this
does not establish successful runtime readback. Interactive PIE/headset validation
remains required: narrow and empty time windows, category/radius filters, periodic
crossings, rotated object view, repeated queries/cancellation, and grab/scale.
No external datasets were accessed or modified.
