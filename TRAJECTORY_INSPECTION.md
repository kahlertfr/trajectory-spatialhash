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
participate. Both collision-marker emitters (`QueryResultCollisions_0` for path
view and `QueryResultCollisions001_0` for object view) finish with the enabled
`InspectCollisionMarkers` module, so unselected collision points are suppressed
in both views. Radius guides remain contextual decorations. Visible-bounds export
uses the resulting sample visibility and includes the query sphere's full radius
from `User.ParticleRadius` and the existing display scale. This radius is stored
before the grab transform, preventing grab scaling from being applied twice.
Isolation therefore refits collision geometry around the selected context and
the query reference, including the full sphere. Axes follow the fitted geometry
without changing the sample layout or grab references. Old asynchronous
bounds callbacks are invalidated when inspection changes.

Custom Niagara systems must implement these parameters and final modules to
support inspection; uploading the parameters alone does not alter rendering.

## Spatial ribbon selection

The fingertip selector can select the focus ribbon in path view and context
ribbons in path and object view. Collision markers, query particle meshes,
sprites, arrows and radius guides never contribute picking candidates.

**Confirmation input:** both supplied Niagara systems expose
`User.ConfirmTrajectorySelection` as a **Bool**, default false. Set it on the
result actor's Niagara component: true on confirmation press, false on release.
Keep each value for at least one Niagara tick. A rising edge captures the current
selector sphere and requests one pick. Holding true does not select repeatedly.
Proximity alone previews the nearest candidate but never changes inspection; there is no dwell or hover gesture.
The user supplies the confirmation-input Blueprint wiring.

`BP_QueryResultVisualization.SetTrajSelector(Location, Radius)` preserves the
existing probe parameters and also calls `UpdateTrajectorySelector`. The legacy
Radius input is the selector's minimum world scale for the standard 100 cm
sphere mesh; the bridge multiplies it by 50 to obtain the world radius in cm.
Native/custom callers use `UpdateTrajectorySelector(WorldPosition, WorldRadius,
bEnabled)` directly, with radius already in centimeters.

`BP_TrajectorSelector` sends its pose every active tick (the old 0.2-second delay
is bypassed). Deactivation immediately cancels pending picks, including safe
handling before a visualization actor has been assigned. Pose updates also expire
after `TrajectorySelectorTimeout` (default 0.15 seconds), so a disconnected or
stopped selector cannot submit later picks. The committed inspection is preserved.

`User.RibbonPicking` is a GPU-only `UNiagaraDataInterfaceRibbonPicking` and
`User.RibbonPickingReceiver` is bound to the result actor before activation.
Final `ExportFocusRibbonPicking` and `ExportContextRibbonPicking` modules record
only ribbon emitters' final positions, visibility and width. They use actual
query-array length and flat sample indices, with focus display index 0 and
context render index + 1. They run after the existing inspection/visibility
modules. Hidden endpoints cannot form selectable segments, and adjacent samples
from different trajectories are never connected.

After all emitters contribute, a compute pass tests the captured world-space
sphere against consecutive visible ribbon samples. Segment distance catches hits
between sparsely sampled endpoints. Width is treated as a capsule around the
centerline, using the larger endpoint half width and the component's largest
axis scale; this is a conservative ribbon-width tolerance rather than a
camera-facing triangle/material intersection. Final positions already include
periodic offsets, object alignment, placement and grab transforms. The Niagara
component's remaining linear transform is applied once; subtracting its world
translation from the probe avoids large-world float precision loss.

One atomic minimum per trajectory and view preserves distance/index association
without racing a separate winning-ID write. Only those minima are read back,
asynchronously. The nearest candidate wins; exact ties prefer path view, then
object view, and lower display index within each view. A miss preserves the
current selection. Successful hits call `SelectTrajectoryByIndex`, so both views,
isolation, visible bounds and the existing controls update through the same API.
No application-owned CPU trajectory cache is retained for picking.

Display revisions, selector sessions, confirmation frame numbers and a short
request lifetime reject stale callbacks. Changing the display while a pick is
pending requires a fresh confirmation. The GPU sample buffer and reduction run
while the selector is active, providing continuous candidate feedback; inactive
picking binds a tiny dummy buffer. Custom systems must supply the ribbon export modules as well as the
parameters; setting the confirmation bool alone is insufficient.

### Candidate width preview

While the fingertip sphere is active, the same asynchronous GPU picker supplies
`CandidateTrajectoryIndex`, independently of `SelectedTrajectoryIndex`. The
candidate's ribbon is shown at **3x normal width** in both path and object view.
`CandidateRibbonWidthMultiplier` on the result actor adjusts this factor (minimum
1); all encoded colors and existing visibility/isolation rules are preserved.
There is no automatic selection or dwell gesture.

The supplied systems expose `User.CandidateTrajectoryIndex` (Int, default -1) and
`User.CandidateRibbonWidthMultiplier` (Float, default 3). The result actor maintains
both parameters automatically; no additional Blueprint wiring is required.
Only the final ribbon exporters apply the width change. Markers, particle meshes,
sprites and guides are unaffected. Export occurs before width multiplication, so
the preview cannot enlarge its own hit tolerance or bias candidate selection.
Upstream width initialization restores the base width on every update; the
multiplier never compounds across frames.

A miss, deactivation, changed selector radius, stale selector pose, or changed
display revision clears the preview. Old GPU results from a different selector
session or display are rejected, as are candidate results whose captured probe
position is too far from the current fingertip. The preview can trail the hand
by the asynchronous readback latency. Confirmation still uses the existing
confirmation-input flow; the preview does not alter its activation/deactivation
wiring.

Candidate validation extends the confirmation test with live candidate changes,
misses, stale poses and expiry. The Niagara integration test captures actual GPU
particle datasets to compare normal and preview ribbon widths in both views and
checks that repeated frames do not compound the multiplier.

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

Spatial-picking validation (2026-10-20): the TrajectoryBLabEditor target builds.
Both result Niagara systems compile for D3D12/SM6 and both modified Blueprints
compile. `TrajectoryBLab.Query.RibbonPicking.Confirmation` covers rising edges,
held input, misses, focus/context mapping, revisions, deactivation, invalid radius
and expiry. `GPUSegments` executes the reduction shader for between-sample hits,
hidden samples, trajectory boundaries, coincident endpoints, object view, width
and nonuniform component scale. `NiagaraIntegration` exercises actual ribbon
export, asynchronous readback and confirmation in a synthetic Unreal world,
including object alignment and transformed components. No headset or real-dataset
test was performed.
