# Context filtering

`ATrajectoryQueryNiagaraActor.ContextFilterMode` is editable and exposed to Blueprint:

| Mode | Result membership |
| --- | --- |
| `Neighborhood` (default) | Existing spatial hash search and radius checks |
| `AllContextTrajectories` | Every context trajectory with available samples in the requested time range, excluding the focus |

Set the actor's properties before calling the existing **Run Query (Async)** node.
The context dataset remains `DatasetDirectory`. Set `QueryDatasetDirectory` to the
focus trajectory's original dataset directory and `QueryTrajectoryId` to its original
ID. Equal normalized absolute dataset paths plus equal IDs identify the focus.
IDs from different datasets are independent. Path aliases through symlinks or
copied dataset directories are not resolved as the same dataset: supply the same
canonical source path in that case. Synthetic focus paths can leave the ID at `-1`.
Supplying an ID without its source directory fails the request rather than
guessing which context ID to remove. Existing Blueprint callers that supply only
positions must wire these two new identity properties for self-exclusion.

All-context mode reads `dataset-meta.bin` and `dataset-trajmeta.bin` asynchronously,
then loads samples from shards through the existing batched pipeline. It never
loads or builds spatial hashes, and ignores `OuterQueryRadius` for acquisition.
Both values of `bIncludeWholeResultTrajectorySamples` retain all available samples
within the requested time range in this mode. No samples are interpolated.
Trajectories with no available samples in that range produce no result.

In both modes, the inclusive result time range is determined by the minimum and
maximum query-sample timesteps. A longer configured `QueryTimeEnd` does not extend
it beyond the last supplied query sample. **Whole trajectory** disables spatial
sample clipping for matching trajectories; it never extends their result samples
before the query begins or after it ends. Shards may contain samples outside that
interval, but those samples are discarded before results are assembled.

Distances, periodic image indices, velocity, display sampling, sample budgets,
bounds, progressive uploads, and cancellation keep their existing behavior. If
periodic extent is left at zero, all-context mode derives it from dataset metadata;
set the actual simulation box explicitly if metadata bounds do not represent it.
Mode and exclusion are frozen for the running request; edits apply to the next run.

Result membership and display visibility are separate. Existing Niagara distance
filters, including **All**, still clip to the display outer radius. Increase that
display radius to inspect distant members. Display sampling may omit trajectories
without a sample on the shared sampling grid. No Niagara asset migration is required.

Regression tests: `SpatialHash.ContextFilter` covers dataset-aware identity,
all-context operation without hashes, distant results, both sample-retention options,
missing samples, periodic distances, batched callbacks, exclusion, neighborhood
regression, and disjoint time ranges.
