# Independent visibility controls

The original WidgetVisibility contract is superseded by five Blueprint-visible
FUserTrajectorySelection booleans: NeighborhoodRadiusVisibility,
BehaviorRadiusVisibility, ZonesRadiusVisibility (default false),
TransformationIndicatorVisibility and ParticleVisibility (default true).

The query and sample actors copy/upload them on live and initial/progressive
updates. UpdateUserParameter has independent neighborhood/behavior/zones inputs;
its existing NewVisibility input carries particles. NS_FRNN_Result now separates
the neighborhood, behavior, zone, transformation-indicator and focus-particle gates.
The pushed BP_QueryResultVisualization wiring is retained and extended for zones.

The consuming project's [VISIBILITY_CONTROLS.md](../../VISIBILITY_CONTROLS.md)
documents Blueprint preset capture/restore, field migrations, validation, and the
remaining manual selection-struct filling. No helper scripts are project content.
