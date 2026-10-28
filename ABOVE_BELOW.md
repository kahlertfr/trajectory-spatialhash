# Above / Below in simulation coordinates

`ETrajectoryColorEncoding::AboveBelow = 8` preserves values 0–7. It colors context
samples by their simulation-space height relative to the focus at the matched
query sample. Simulation up is +Y: the display convention maps `(x,y,z)` to
`(x,-z,y)`. Scene height, grab pose and movement alignment never classify colors.

## Niagara

`NS_FRNN_Result` registers `ColorEncodingAboveBelow` and calls it in the two context
result emitters (`QueryResult`, `RealtiveQueryResult`). It reads
`ResultQuerySampleIndices[Particles.SampleIndex]`, validates array indices, and
compares `result.y + resultImageY * PeriodicVolumeExtent.y` against the matched
query's equivalent height. Periodic image bytes are sign-extended. Height >= 0
uses Above; height < 0 uses Below. RGB changes preserve alpha and all existing
visibility, filtering, picking and width behavior. The focus keeps its own color.
The module runs after existing color/visibility operations and preserves their
alpha. Switching modes recomputes base colors normally on the next update.
No new dataset fields, query work or sampling arrays are needed.

## Orientation plane

The user's existing `ObjectView-OrientationPlan` emitter and `/Game/Meshes/UnitPlane`
are retained. `UpdateObjectViewOrientationPlane` uses the existing object anchor,
fit and placement transforms. Its normal is simulation +Y (display +Z), followed
by the linear grab transform. It does not use movement direction or query rotation.
The persistent particle is visible only without object-view rotation, while
`TransformationIndicatorVisibility` is enabled and query positions exist. It is
independent of focus/zone visibility, color mode and display-time-window changes.
The original front/back orientation disc remains unchanged.

`CalculateParticleScale.ParticleRadius` reads `User.QueryRadius` in this emitter.
`SetOrientationPlaneSize` multiplies the rendered radius by two because UnitPlane
spans [-0.5,+0.5]. Thus its square has side length equal to the query diameter.
The plane does not export bounds. Its renderer retains depth testing and visibility
tag filtering, and disables shadow casting.

## Shared palette and materials

`MPC_AboveBelowPalette` is the palette definition:

- Above: sRGB #CC79A7 (magenta), converted to linear RGB.
- Below: sRGB #009E73 (teal), converted to linear RGB.

`M_ObjectViewOrientationPlane` and `M_Color_AboveBelow` read this collection.
The native real/synthetic query actors upload the same collection values as
`User.AboveColor` and `User.BelowColor` on display upload and parameter updates.
Set palette defaults in the collection before loading/querying; runtime collection
changes require the actor's normal parameter update to refresh Niagara colors.
The saved Niagara defaults match the collection for direct asset previews.

The plane material is two-sided, unlit and translucent, with fill opacity 0.15 and
rectangular rim opacity 0.30. As with the front/back disc, each face represents the
hemisphere beyond the plane: the +normal face shows Below, the -normal face Above.
The legend has two discrete swatches: Below on the left, Above on the right.

## Project UI / persistence

WBP_VisConfigurator adds an Above / Below (Simulation) button with the existing
input guard, exclusive highlighting and broadcast path. Its preset-restore graph
restores the new button's selected state. BP_VisConfigurator's color legend switch
uses M_Color_AboveBelow with labels Below (-Y) and Above (+Y).
The project preset validator accepts value 8, rejects unknown larger values, and
serializes the stable enum name AboveBelow. Screenshot metadata uses that name.
Overview and the unused query-only system are not extended, matching FrontBack's
current query-result scope.

## Validation (2026-10-28)

The UE 5.6 Editor target and local authoring helper built successfully. Unreal's
supported asset APIs authored and compiled the Niagara and Blueprint assets.
D3D12 GPU sim-cache checks passed for actual colors in both views, nontrivial
query-index mappings, periodic +Y/-Y offsets, on-plane Above, rotation-mode color
invariance, plane normal/visibility, grab pose/scale, zero inner radius, time-window
stability, original-disc visibility and switching between FrontBack and AboveBelow.
Preset AboveBelow and existing RoundTrip tests passed; Screenshot.DisplaySettings
passed with coverage extended through encoding 8. The selector compiled, its
exclusive button highlighting passed, and a rendered preview was inspected. The
local authoring and GPU
validation helpers remain in the ignored parent codex directory.

No headset test or interactive real-dataset appearance check has been performed.
