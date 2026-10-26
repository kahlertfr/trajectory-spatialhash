# Object-view orientation disc

`NS_FRNN_Result` uses the existing `ObjectView-OrientationDisc` emitter and
`/Game/Meshes/UnitDisc`. No arrow is added.

The dedicated system scratch module `UpdateObjectViewOrientationDisc` retains the
object-anchor placement and writes `Particles.MeshOrientation` and
`Particles.VisibilityTag` on every particle update. The renderer accepts tag 0;
translation-only mode and an empty direction array write tag 1. The particle stays
alive so toggling rotation restores the disc without a new query or system restart.

The disc normal uses `User.QueryForwardDirections[0]`, already computed from the
full-resolution unwrapped focus trajectory before display reduction. This is the
query's reference direction, not the direction at the visible window's start.
The source-to-display conversion is `(x,-z,y)`; the normal then follows the linear
part of `User.GrabTransformationRelativeVis`. The unit disc's +Z axis is aligned
with that normal, including the antiparallel case. The object anchor continues to
follow object placement and grab translation. Existing stationary/single-point
input uses the movement-alignment fallback of source +X.

Disc radius uses `User.QueryRadius`, the outer display radius, and the existing
fit/grab scale calculation. It is independent of inner-zone radius, zone-guide
visibility, trajectory particle visibility, and color encoding. The inherited
`ExportQuerySphereBounds` call is removed from this emitter: the orientation disc
is a visual guide and does not add a spherical extent to visible trajectory bounds.

## Material

`M_ObjectViewOrientationDisc` is two-sided, translucent, unlit, and supports Niagara
mesh particles. It retains normal depth testing. The visible surface describes
the hemisphere beyond the plane: its +normal face shows BackColor; its -normal
face shows FrontColor. This matches the trajectories seen through the boundary.

Editable material defaults match the current Front/Back encoding:

- FrontColor: linear RGB `(1,0.25,0.02)` (orange).
- BackColor: linear RGB `(0.02,0.35,1)` (blue).
- DiscOpacity: `0.15`.
- RimOpacity: `0.30`, blended over the outside 6% of the radial UVs.

The existing color module currently stores these colors as literal values, so a
future palette change must update both the color module and these material defaults.

## Validation

Validation details are recorded in the parent implementation guide. Local asset
editing and synthetic validation helpers live only in the ignored parent `codex/`
folder. Interactive dataset rendering and headset verification remain separate
from offscreen synthetic checks.
Validation (2026-10-26): UE 5.6 editor/helper build and Direct3D 12 Niagara
compilation succeeded; the saved system was valid and ready to run. GPU readback
checks passed for rotation-mode visibility, live off/on toggles, reference normal,
unchanged position/orientation after time-window changes, grab translation/rotation/
scale, antiparallel normals, and empty direction arrays. A zero inner-zone radius
still produces a positive disc radius. Saved material palette and opacity values
were verified through Unreal's material API.

Visual validation is incomplete: offscreen scene captures were black for both disc
faces, a regular mesh using the new material, and an independent opaque emissive
red-sphere control. The combined local runtime/capture automation therefore reports
failure even though its GPU state checks pass. This capture setup cannot establish
face appearance or transparent sorting. No interactive dataset or headset check was
performed. Initial commandlet-only rendering attempts also crashed in renderer
initialization; the editor automation lifecycle completed the GPU checks.

## Editor script registration repair (2026-10-26)

The embedded `TransformObjectAnchorToWorld` script is now included in the system's
Scratch Pad registry. Its six object-view call sites can be opened through Niagara's
normal source-navigation handler.

Eleven retained call aliases were updated through Niagara's `ReassignModuleScript`
editor API, which also migrates the affected parameter names:

| Previous displayed name | Referenced script / new displayed name | Calls |
| --- | --- | --- |
| TransformLocalToWorld | TransformObjectAnchorToWorld | 6 |
| TransformLocalToWorld | UpdateObjectViewOrientationDisc | 1 |
| ExportVisibleBounds | ExportInspectedContextBounds | 2 |
| ExportVisibleBounds | ExportQuerySphereBounds | 2 |

Active module input overrides and their links were compared before and after the
reassignment and preserved. Niagara discarded obsolete BBoxCenterLocal and
BBoxExtentLocal overrides on the seven anchor/disc calls; those scripts already
read System.BBoxCenterLocal and System.Extent directly.

A separate process reloaded all three project/plugin Niagara systems and checked
355 embedded module calls through their Scratch Pad view models, editable graphs,
and the actual stack-module OpenSourceAsset handler. All passed, with no missing
registrations, stale aliases, or source-navigation failures. The overview and
query-only systems required no changes. Direct3D 12 compilation of the repaired
result system passed with valid=1 and ready=1.

The post-repair GPU state-only automation also passed all nine checks (visibility, reference normal, time-window stability, grab transforms/scale, antiparallel and empty directions). Appearance capture checks were explicitly excluded because of the previously documented capture limitation.

WidgetVisibility (2026-10-27): the disc additionally requires User.WidgetVisibility, shared with the arrow and defaulting to true. See [WIDGET_VISIBILITY.md](WIDGET_VISIBILITY.md) for selection/native transfer and Blueprint wiring.
