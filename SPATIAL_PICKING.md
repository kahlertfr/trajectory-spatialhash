# Fingertip trajectory picking

Spatial picking previews a trajectory and commits it through the same
`SelectTrajectoryByIndex` API used by the slider. The confirmed selection stays
unchanged until confirmation. The path and object views share that selection.

## Interaction

1. In the result controls, press **Enable hand selector** for the exploration to
   inspect. There is one local hand selector per world.
2. Tap the small trigger near the **left index knuckle with the left thumb**.
   The right index fingertip sphere appears and picking starts.
3. Point at a visible trajectory in either view. Its display index is shown in
   the panel and on the hand trigger, and its normal colors preview the selection.
4. Tap the left-thumb trigger again to confirm. The fingertip sphere hides and
   the confirmed selection is shared by both views. A tap with no fresh hit exits
   without replacing the previous selection.

The separate thumb trigger lets the right index remain over the target during
confirmation. The result controls also expose **Confirm**, **Cancel picking**,
and **Disable hand selector**. Cancellation, disabling, lost hand tracking, or
destroying the exploration preserves the previous confirmed selection. Tracking
recovery requires releasing the trigger before another tap is accepted.

While picking, isolation is temporarily suspended so another context trajectory
can be chosen; the isolation preference is restored afterward. Other filters
remain effective. Collision boxes and axes keep their current fit during the
preview so the target does not move as candidates change. Changing visualization
controls or replacing the uploaded display cancels the preview; pause an active
time animation before starting the selector.

## Ownership and Blueprint API

`ATrajectoryQueryNiagaraActor` owns:

- `BeginSpatialPicking(WorldPosition, Radius)`; radius is in world centimeters,
  clamped to 0.1–20 cm, with a 1.5 cm default.
- `UpdateSpatialPickingPosition(WorldPosition)`.
- `ConfirmSpatialPicking()` and `CancelSpatialPicking()`.
- Read-only active state, candidate display index, position, and radius.

`OnTrajectoryInspectionChanged` updates the controls for both candidate and
confirmed state. Moving the slider cancels spatial preview. Candidate indices
are transient and are invalidated when display membership changes.

`ATrajectorySpatialPicker` creates the new fingertip sphere, thumb trigger, and
hand label at runtime. It does not reuse `BP_Probe`. Its `UpdateHandPose` entry
point allows another tracking implementation or deterministic tests to supply
world-space poses. Its default XR input can be disabled for that integration.

The optional `ITrajectoryHandPoseProvider` modular feature adapts a project's
rendered hand skeleton. TrajectoryBLab registers an adapter for its existing
Meta `UOculusXRHandComponent` instances, honoring custom bone mappings and
requiring high tracking confidence for both hands. The generic plugin falls back
to the engine XR hand-tracking API when no project adapter is registered. The
Meta dependency belongs to the project module, not the generic trajectory plugin.

## Niagara and GPU readback

The result actor uploads `User.TrajSelectPosition` (world vector),
`User.TrajSelectActive` (bool), and `User.TrajSelectRadius` (world centimeters).
Use the actor APIs rather than updating these parameters alone: they also manage
the preview session, readback validation, and confirmation.

Final picking modules preserve the existing visibility, color, and visible-bounds
behavior. They feed displayed geometry to `User.VisibleBounds.Consider`. Sphere
picking includes the rendered query-sphere radius. Collision markers participate;
decorative radius guides and direction arrows do not create selection candidates.

The GPU reduces a nearest-distance score per display trajectory and view using
32-bit atomic maximum. The game thread receives two scores per trajectory, not
the full sample buffers. Readback runs only during active picking, throttled to
20 Hz, and does not synchronously wait for the GPU. Confirmed selection does not
retain the CPU query sample buffers.

World distances account for the Niagara component transform and view grab
transforms. The probe is converted to simulation space using double-precision
CPU transforms before distance vectors are converted back to world units.
Periodic offsets, object alignment, display time, trajectory boundaries, and
visible zone masks are respected when testing ribbon segments.

Readbacks carry a session, display count, timestamp, and probe position. Old
sessions, changed membership, results older than 0.3 seconds, and results from a
probe position farther than 0.75 probe radii away are rejected. Equal distances
choose the lower display index. A 0.2 cm hysteresis retains a current candidate
at crossings. Confirmation requires a fresh candidate; cancellation never changes
the committed selection.

## Validation

`TrajectoryBLab.Query.SpatialPicking` covers preview/commit separation, candidate
ties and hysteresis across views, cancellation, stale sessions, stale timestamps,
movement invalidation, slider override, and display replacement. Local editor
fixtures in the consuming worktree's ignored `codex/` directory exercise actual
GPU picking, hand-trigger input, and the control panel. The Unreal 5.6 Editor
target builds successfully. All 15 query and local GPU tests pass (two with
warnings), including segment picking in both views with transformed components
and grab transforms, confirmation, zone filtering, and tracking-loss cancellation.
Device hand tracking,
trigger placement, and headset usability require a headset check.
