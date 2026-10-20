# Trajectory inspection

Result inspection is shared by the path and object views of one
`ATrajectoryQueryNiagaraActor`. It is independent of the query focus and of
`FUserTrajectorySelection`, so controls do not replace the query configuration.

`GetInspectableTrajectoryCount()` includes the focus at index **0**, followed by
the context trajectories present in the current display upload. These are display
indices, not dataset indices or source IDs. `GetInspectableTrajectoryId()` returns
the source ID; a synthetic focus can have ID -1. Focus and context belong to their
respective actor dataset directories, and equal IDs across those roles do not imply
equal source identity.

Use `SelectTrajectoryByIndex`, `ClearTrajectoryInspection`, and
`SetShowOnlySelectedTrajectory` to change inspection. Invalid indices leave the
selection unchanged. A new successfully started query clears the selection;
progressive uploads preserve a context selection by source ID if it is still
present. CPU sample-buffer cleanup retains the small display index/ID mapping.
`OnTrajectoryInspectionChanged` updates controls after membership or settings change.

## UI

The project's `WBP_ResultVisControl` includes a native
`UTrajectoryInspectionWidget` row. It provides an integer slider, index/source-ID
label, **Only selected**, and **Clear selection**. The existing result Blueprint
binds the time configurator's controls to its result actor immediately after
`SetInitialValues`, through `BindInspectionControls`. Custom UI can instead call
`SetVisualizationActor` on the widget directly.

The original time/animation grid retains a 500-pixel-high section. Inspection is
placed below it, and `BP_TimeConfigurator` renders a 1000 x 650 panel to accommodate
both sections without overlapping controls.

Moving the slider selects immediately. Releasing its handle also selects its
current index, allowing index zero or a single focus trajectory to be selected.
Empty results disable the controls. Clearing restores the normal view even when
the isolation preference remains checked; selecting again reapplies that preference.

## Niagara

Both supplied result systems declare:

| Parameter | Type | Meaning |
| --- | --- | --- |
| `User.SelectedTrajectoryIndex` | Int | Focus=0; context render index + 1 |
| `User.HasTrajectorySelection` | Bool | Enables inspection; false restores normal display |
| `User.TrajSelectOnly` | Bool | Suppresses nonmatching context samples when inspection is enabled |

Final update modules preserve selected samples' existing encoded RGB and set
unselected RGB to white, preserving their alpha. Isolation additionally zeros
unselected context alpha, ribbon width, sprite size, and mesh scale. The query
trajectory and query particle remain visible as references when a context
trajectory is isolated. Existing time, zone,
category, particle visibility and shadow rules continue to apply. Samples are not
killed or respawned when inspection changes.

The focus ribbon, context ribbons, focus point markers, and collision markers
participate. Radius guides remain contextual decorations. Visible-bounds export
uses the resulting sample visibility and includes the query sphere's full radius
from `User.ParticleRadius` and the existing display scale. This radius is stored
before the grab transform, preventing grab scaling from being applied twice.
Isolation therefore refits collision geometry around the selected context and
the query reference, including the full sphere. Axes follow the fitted geometry
without changing the sample layout or grab references. Old asynchronous
bounds callbacks are invalidated when inspection changes.

Custom Niagara systems must implement these parameters and final modules to
support inspection; uploading the parameters alone does not alter rendering.

## Validation

`TrajectoryBLab.Query.TrajectoryInspection` tests display-index/source-ID mapping,
invalid input, progressive reordering, cleanup retention, focus independence,
selection clearing, missing trajectories and empty data. Headset slider usability
and appearance should also be checked in the target runtime.

Validation on 2026-10-20: the UE 5.6 Editor target builds; the two Niagara systems
compile with D3D12/SM6 enabled, and the modified Blueprints compile. All 12
`TrajectoryBLab.Query` tests and a local synthetic D3D12 PIE check pass. The latter
checks binding through the actual time configurator's initialization path,
focus/context isolation in both views, query-reference retention for an unmatched
context index, sphere bounds growing with the user radius, and restoration
without respawning. An offscreen render of the actual panel was reviewed for
layout. The local fixture remains in the parent's ignored `codex/` folder.
No real dataset or headset interaction was used for these checks.
