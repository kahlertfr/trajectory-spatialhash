# TrajectoryBLab Blueprint replay bridge

`BP_QueryResultVisualization` keeps its existing Query event, Run Query (Async)
node, control initialization and final-success presentation chain. Preset replay
uses those same paths through the real application explorer.

The asset calls project-owned `UVisualizationPresetWorkflow` helpers before the
async query and after its success/failure presentation. The helpers locate a
replay coordinator only through the actor's explicit owner chain (result →
explorer → coordinator). Ordinary manual queries continue without a coordinator.
Replay acquisition applies its saved advanced settings and uses whole-result
false; manual queries preserve their existing whole-result true behavior.

BeginPlay now resolves context metadata through dataset discovery, so presenting
a streamed context source does not require loading its trajectories globally.
The shared query path also makes result controls available after a loading-only
preset. Selection-manager callbacks are guarded during replay restoration.

These are application Blueprint bridges, consistent with this asset's existing
references to TrajectoryBLab configurator Blueprints. Plugin C++ query execution,
data formats and Niagara systems are unchanged. See the consuming project's
`VISUALIZATION_PRESETS.md` for the coordinator and typed widget interfaces.
