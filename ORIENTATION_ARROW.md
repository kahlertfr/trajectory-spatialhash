# Object-view orientation arrow

`NS_FRNN_Result` uses the existing `ObjectView-OrientationArrow` emitter and
`/Game/Meshes/UnitArrow` added in the user's pushed revision.

The dedicated registered scratch module `UpdateObjectViewOrientationArrow` runs
in Particle Update. It retains the object-anchor placement and reference-direction
alignment, then moves the arrow base to the focus particle's forward surface:

```
arrowBase = focusCenter + alignedForward * renderedFocusRadius
arrowLength = 0.2 * renderedFocusRadius
```

UnitArrow points along local +Z, with its base/pivot at Z=0, tip at Z=1, and radial
extent 0.08. A uniform mesh scale of `0.2 * renderedFocusRadius` therefore produces
the requested total length while preserving its shape. The reference direction is
`User.QueryForwardDirections[0]`, with the same source-axis and grab transformations
as the disc. Changing the visible time window does not change the query reference.

The arrow's CalculateParticleScale input is `User.ParticleRadius`, matching
ObjectView-Particle rather than the disc's inner-zone radius. The arrow module
reads the resulting `Emitter.ParticleSize`, including fit and grab scale. It writes
Particles.Position, MeshOrientation, Scale, and VisibilityTag every update. The
inherited SetParticleSize call was removed so it cannot overwrite the arrow scale.
The existing arrow mesh and material settings were retained; the disc was not edited.

Renderer visibility is tag 0 only when `User.ObjectViewWithRotation` is true, a
query direction exists, and the focus radius is positive. Translation-only mode,
empty directions, and a zero particle radius hide the persistent arrow without
requiring a new query. Arrow visibility is independent of zone guides, particle
visibility, and color encoding, matching the orientation-disc mode gate.

## Validation

UE 5.6 editor-target build passed. The synthetic Direct3D 12 GPU automation
TrajectoryBLab.OrientationArrow.Runtime passed, comparing captured arrow and focus
particle states for surface placement, exact 0.2 length ratio, reference direction,
nonaccumulating position, multiple live particle radii, independent inner radius,
translation/rotation toggling, display-time changes, translated/rotated/scaled grab,
color and particle-visibility changes, zero radius, antiparallel direction, empty
direction arrays, and the periodic-edge sample scenario.

A fresh Unreal process audited 360 embedded module calls across result, query-only,
and overview Niagara systems. No missing registrations, stale aliases, or failed
source-opening handlers were found. The arrow module was added through Niagara's
editor reassignment API, with its active transform inputs preserved.

These are editor-API and GPU-state checks. No interactive visual or headset check
was performed. Asset-editing and validation helpers remain in the ignored codex/
folder and are not intended project changes.
WidgetVisibility (2026-10-27): the shared default-visible selection flag now gates both orientation indicators together, in addition to their rotation/data conditions. The user's later arrow-size adjustment is retained. See [WIDGET_VISIBILITY.md](WIDGET_VISIBILITY.md) for the native transfer API and Blueprint wiring inventory.

## Movement directions in translation-only mode (2026-10-27)

The user's `ObjectView-OrientationArrows` emitter now displays one persistent
`UnitArrow` mesh per available, retained focus sample. It uses the existing
`User.QueryPoints`, `User.QueryForwardDirections` and `User.QuerySampleTimeSteps`
arrays; display reduction is respected and no additional upload arrays are needed.

`CountMovementArrowSamples` runs before Spawn Burst Instantaneous in Emitter
Update and supplies its Spawn Count from the minimum of the three array lengths.
The burst retains the copied emitter's one-shot behavior. Particle UniqueID maps
each arrow to its sample for the lifetime of that query upload.

The registered scratch module `UpdateObjectViewMovementArrows` runs last in
Particle Update, after Initialize Color and Particle State. It preserves the
existing anchor, display-axis conversion and grab transformations, but reads each
sample's forward direction instead of direction zero. Each arrow starts at the
focus sphere's surface and has the user's current total length of 0.5 times the
rendered focus radius (including fit and grab scale).

Renderer visibility requires translation-only mode, TransformationIndicatorVisibility,
a positive focus radius, a valid sample, and an actual sample timestep inside the
inclusive VisTimeStart/VisTimeEnd interval. Hidden particles remain alive, so range
and mode changes update immediately without a new query. The original singular
rotation-mode arrow and orientation disc are unchanged.

The module writes Particles.Color using the existing ColorEncodingTimestep palette,
independently of User.ColorEncoding. Normalization uses QueryTimeStart/QueryTimeEnd,
or VisTimeStart/VisTimeEnd when TimeRangeSensitive is enabled, with the existing
single-timestep guard. The user's User.RibbonMaterial mesh-renderer binding and
material fallback are retained. Running the module last prevents the copied
Initialize Color module from replacing the timestep color with white.

Validation for movement arrows: the UE 5.6 Editor target builds and the saved
system compiles with Direct3D 12. The local full-editor GPU automation
`TrajectoryBLab.MovementArrows.Runtime` passes sample counts, actual sparse
nonzero timesteps, movement orientations (including antiparallel), palette
endpoints independent of trajectory color mode, inclusive and expanded windows,
mode/indicator/particle visibility, shared surface placement, stable repeated
updates, grab translation/rotation/scale, zero radius and empty arrays. The
otherwise trimmed Timestep attribute is retained only in memory for readback;
this instrumentation is not saved in the asset. Authoring/test helpers remain
ignored under the parent codex/ directory. No interactive visual or headset test
was performed.
