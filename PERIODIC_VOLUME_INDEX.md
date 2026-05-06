# Periodic Volume Index – Concept, Encoding, and HLSL Helper

## Overview

When querying a trajectory dataset that was generated inside a **periodic simulation box**
(e.g. a molecular-dynamics simulation), neighbouring particles can be located in a periodic
*image* of the original box rather than in the original box itself.  In the current query
pipeline, sample positions are returned in their **original simulation coordinates** (always
inside the original box `[0, Extent]`).  To visualise these samples correctly relative to an
unwrapped query trajectory that may extend outside the box, each sample must be shifted by a
whole-number multiple of the box size.

The `ResultVolumeIndices` Niagara integer array encodes exactly this shift for every result
sample point in a compact, shader-friendly format.

---

## Virtual Volume Grid

Consider the original simulation box as the *central* volume with index **0**.  Surrounding
it in all three spatial directions is an infinite grid of identical *image* volumes, each
identified by a signed integer triple `(ix, iy, iz)` where each component counts how many
box-lengths away from the original volume that image sits:

```
                +---------+---------+---------+
   iz = +1      | (-1,+1) | ( 0,+1) | (+1,+1) |
                +---------+---------+---------+
   iz =  0      | (-1, 0) | ( 0, 0) | (+1, 0) |   ← original box = index 0
                +---------+---------+---------+
   iz = -1      | (-1,-1) | ( 0,-1) | (+1,-1) |
                +---------+---------+---------+
                  ix = -1   ix =  0   ix = +1
```

*(shown here for the XZ plane; the Y axis works identically)*

The scheme is **infinitely extendable** in all directions: any `(ix, iy, iz)` triple is
valid.  For typical minimum-image periodic queries `|ix|, |iy|, |iz| ≤ 1`.

---

## Index Encoding (C++ / Blueprint)

Each triple `(ix, iy, iz)` is packed into a single `int32` by storing each signed component
as one byte (two's-complement representation):

| Bits    | Component | Range        |
|---------|-----------|--------------|
| 7 .. 0  | ix        | −127 … +127  |
| 15 .. 8 | iy        | −127 … +127  |
| 23 .. 16| iz        | −127 … +127  |
| 31 .. 24| (unused)  | always 0     |

`(0, 0, 0)` → **index 0** (original simulation box).

The C++ helper used internally:

```cpp
// Encode signed (ix, iy, iz) into a single int32 volume index.
// Each component must be in the range -127..127.
static int32 EncodeVolumeIndex(int32 ix, int32 iy, int32 iz)
{
    return (ix & 0xFF) | ((iy & 0xFF) << 8) | ((iz & 0xFF) << 16);
}
```

---

## HLSL Helper Functions (copy-ready)

Add these functions to your Niagara custom HLSL module.

### Niagara User Parameters required

| Parameter name        | Type    | Description                                      |
|-----------------------|---------|--------------------------------------------------|
| `ResultVolumeIndices` | Int Array | Per-sample volume index (parallel to `ResultPoints`). |
| `PeriodicVolumeExtent`| Vector  | Periodic box size in world units per axis (X, Y, Z). `ZeroVector` when non-periodic. |

### Decode volume index → integer offset triple

```hlsl
// Decode a packed volume index (produced by the SpatialHashedTrajectory plugin)
// into a signed 3D grid offset (ix, iy, iz).
//
// Each component is stored in one byte using two's-complement encoding.
// Index 0 always decodes to (0, 0, 0) – the original simulation box.
// Valid component range: -127..127 (−128 is reserved and never produced).
//
// @param VolumeIndex  Integer value from the ResultVolumeIndices array.
// @return             Signed 3D offset triple.
int3 DecodeVolumeIndex(int VolumeIndex)
{
    int ix = VolumeIndex & 0xFF;
    int iy = (VolumeIndex >> 8)  & 0xFF;
    int iz = (VolumeIndex >> 16) & 0xFF;

    // Sign-extend from unsigned byte (0-255) to signed integer (-128..127).
    if (ix >= 128) ix -= 256;
    if (iy >= 128) iy -= 256;
    if (iz >= 128) iz -= 256;

    return int3(ix, iy, iz);
}
```

### Compute world-space offset for a given volume index

```hlsl
// Return the world-space offset that must be *added* to a raw sample position
// to translate it into the correct periodic image.
//
// Usage:
//   float3 WorldPos = RawSamplePosition + GetVolumeWorldOffset(VolumeIndex, PeriodicVolumeExtent);
//
// @param VolumeIndex        Value from ResultVolumeIndices (0 = no offset needed).
// @param PeriodicVolumeExtent  Box size in world units (Niagara user param of the same name).
// @return                   World-space translation vector.
float3 GetVolumeWorldOffset(int VolumeIndex, float3 PeriodicVolumeExtent)
{
    int3 ijk = DecodeVolumeIndex(VolumeIndex);
    return float3(
        (float)ijk.x * PeriodicVolumeExtent.x,
        (float)ijk.y * PeriodicVolumeExtent.y,
        (float)ijk.z * PeriodicVolumeExtent.z
    );
}
```

### Example: placing a particle at the correct world position

```hlsl
// Inside a Niagara particle update script:

int   SampleIdx   = /* index into ResultPoints / ResultVolumeIndices */;
float3 RawPos     = SamplePositionArray[SampleIdx];
int    VolIdx     = ResultVolumeIndices[SampleIdx];

// PeriodicVolumeExtent is the Niagara Vector user parameter set by the plugin.
float3 Offset     = GetVolumeWorldOffset(VolIdx, PeriodicVolumeExtent);
float3 WorldPos   = RawPos + Offset;
```

When `PeriodicVolumeExtent` is `(0, 0, 0)` (non-periodic dataset) `GetVolumeWorldOffset`
returns `(0, 0, 0)` for any index, so the code is safe to use unconditionally.

---

## Data Flow Summary

```
C++ query pipeline (QueryPositionsBatchedAsync)
  │
  ├─ For each result sample point:
  │    VolumeIndex = ComputeVolumeIndex(SamplePos, UnwrappedQueryPos, PeriodicExtent)
  │
  └─ Stored in FTrajectorySamplePoint::VolumeIndex
       │
       └─> TransferResultsToNiagara()
             │
             ├─ ResultVolumeIndices  (Int Array)  → parallel to ResultPoints
             └─ PeriodicVolumeExtent (Vector)     → resolved box size
                   │
                   └─> Niagara HLSL
                         GetVolumeWorldOffset(VolumeIndex, PeriodicVolumeExtent)
                           → world-space offset to apply to RawSamplePosition
```
