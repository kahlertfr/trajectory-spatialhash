# Orientation widget visibility

`FUserTrajectorySelection.WidgetVisibility` is a Blueprint-readable/writable bool,
defaulting to true. It controls the object-view orientation disc and arrow together.
Both indicators still require ObjectViewWithRotation and valid query direction data;
the arrow also retains its positive-radius check.

`ATrajectoryQueryNiagaraActor::UpdateUserParameter` has a new trailing optional
`NewWidgetVisibility` input (default true). The actor stores the value and uploads
`User.WidgetVisibility` on live parameter updates and initial/progressive display
uploads. ASampleTrajectoryNiagaraActor applies the selection field and uploads it
on live changes and scenario uploads. Native preset capture/replay also carries the
field; FVisualizationExplorationSettings inherits it and JSON serialization includes it.

NS_FRNN_Result exposes User.WidgetVisibility as a bool with default true. The
registered UpdateObjectViewOrientationDisc and UpdateObjectViewOrientationArrow
scratch modules read it in Particle Update and include it in the renderer-tag gate.
The existing particle lifetimes, positions, orientations, sizes, and other visibility
conditions are retained, including the user's latest arrow scale adjustment.
The overview has no orientation widgets and its existing renderers are unaffected.

## Blueprint wiring

No Blueprint graphs were changed or saved. After rebuilding/restarting the editor,
refresh the relevant split struct pins and Update User Parameter node. In
/SpatialHashedTrajectory/Visualization/BP_QueryResultVisualization, connect the
selection's WidgetVisibility to the new New Widget Visibility input. UI providers
that rebuild/split selections or preset settings should propagate the field so
updates to unrelated controls do not reset it to its default.

A read-only Unreal graph/variable scan found the following direct uses of
FUserTrajectorySelection. Paths are Unreal package paths:

| Blueprint | Observed use |
| --- | --- |
| /Game/Blueprints/BP_DatasetVisualizationLinkedUserSelection | Selection event forwarded to UpdateUserSelections |
| /Game/Blueprints/BP_Legend | Selection event and array reads |
| /Game/Blueprints/BP_Legend_Filter | Selection event and array reads |
| /Game/Blueprints/BP_Legend_Steps | Selection event and array reads |
| /Game/Blueprints/BP_Legend_Views | Selection event and array reads |
| /Game/Blueprints/BP_LegendFilter | Selection event and array reads |
| /Game/Blueprints/BP_MockUp | AddSelection, UpdateSelectionByTrajectoryId, GetSelectionByTrajectoryId |
| /Game/Blueprints/WBP_Categories | Selection reads/writes and event |
| /Game/Blueprints/WBP_QueryConfig | Selection creation/reads/writes |
| /Game/Blueprints/WBP_ResultVisControl | Selection reads/writes |
| /Game/Blueprints/WBP_Transformation | Selection reads/writes |
| /Game/Blueprints/WBP_VisConfig2 | Selection reads/writes |
| /Game/Blueprints/WBP_VisConfigurator | Selection reads/writes |
| /Game/Blueprints/WBP_Zones | Selection reads/writes |
| /SpatialHashedTrajectory/Visualization/BP_QueryResultVisualization | Selection event/reads and the UpdateUserParameter call |

Three additional Blueprints reference the derived FVisualizationExplorationSettings
struct, which also includes WidgetVisibility:

- /Game/Blueprints/BP_PresetManager
- /Game/Blueprints/WBP_SaveLoadPreset
- /Game/Blueprints/WBP_TrajectorySelection

Categories, QueryConfig, ResultVisControl, Transformation, VisConfig2,
VisConfigurator, and Zones also reference that derived settings struct.
Some listed assets are variants or readers rather than active field writers; the
list does not assume they all require new wiring.

Native-only consumers include /Game/Blueprints/BP_SelectionManager (inherits native
selection storage/API) and /Game/Blueprints/MySampleTrajectoryNiagaraActor (inherits
the native sample selection listener). No direct selection-struct graph references
were found in these two assets. BLabMap's level Blueprint had no selection-struct pins.

## Validation (2026-10-27)

UE 5.6 TrajectoryBLabEditor build passed. Native automation passed:
SpatialHash.VisibilityZones.Selection, TrajectoryBLab.Query.DisplayedDistance,
and TrajectoryBLab.Presets.RoundTrip. Checks cover default visibility, preserving
false/true through selection storage, live query upload, re-upload, and preset JSON.

The local Direct3D 12 GPU test TrajectoryBLab.OrientationWidgets.Visibility passed
all four rotation/widget combinations for both emitters. It exercised selection
manager events through the native sample actor, live restoration, re-upload/system
reinitialization while hidden, unchanged indicator geometry, and unaffected focus
particle size. No appearance-capture or headset validation was performed.

A fresh process audited 360 embedded module calls across all three Niagara systems:
no missing registrations, stale aliases, or source-opening failures. The asset was
edited through Unreal APIs; local editing/scanning/GPU-test helpers remain ignored
under codex/ and are not project changes.