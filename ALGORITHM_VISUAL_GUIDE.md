# Z-Order Spatial Hash Table Algorithm - Visual Guide

## What This Algorithm Does

```
INPUT: Trajectory positions at a time step
┌─────────────────────────────────────┐
│ Trajectory 1: (5.2, 8.7, 3.1)      │
│ Trajectory 2: (15.8, 12.3, 7.4)    │
│ Trajectory 3: (6.1, 9.2, 2.8)      │
│ Trajectory 4: (25.5, 18.9, 11.2)   │
│ ...                                  │
└─────────────────────────────────────┘

OUTPUT: Spatial hash table for fast queries
┌─────────────────────────────────────┐
│ Z-Order Key │ Trajectory IDs        │
├─────────────┼──────────────────────┤
│ 0x00000042  │ [1, 3]               │
│ 0x000001A3  │ [2]                  │
│ 0x0000F8E7  │ [4, 7, 12]           │
│ ...         │ ...                  │
└─────────────────────────────────────┘
```

## How It Works

### Step 1: Partition Space into Grid

```
3D Space (BBox: 0,0,0 → 30,30,30, Cell Size: 10)
         
         Z
         ↑
         │     ┌──┬──┬──┐
         │    /│ /│ /│ /│
         │   ┌─┼┬─┼┬─┼┐│
         │  /│/│/│/│/│/│
         │ ┌─┼┬─┼┬─┼┐││
         │ │●││●││ ││││  ← Trajectories
         │ ├─┼┼─┼┼─┼┤││
         │ │ ││ ││●││││
         └─┴─┴┴─┴┴─┴┘│→ Y
          /│ /│ /│ /│/
         ┴─┴─┴─┴─┴─┘
        ↙ X

Each trajectory falls into a cell based on its position.
```

### Step 2: Calculate Z-Order Keys

```
Cell (0,0,0): 
  Binary: x=000, y=000, z=000
  Interleaved: 000000000
  Z-Order Key: 0

Cell (1,0,0):
  Binary: x=001, y=000, z=000
  Interleaved: 000000001
  Z-Order Key: 1

Cell (0,1,0):
  Binary: x=000, y=001, z=000
  Interleaved: 000000010
  Z-Order Key: 2

Cell (1,1,0):
  Binary: x=001, y=001, z=000
  Interleaved: 000000011
  Z-Order Key: 3

Pattern: Bits are interleaved as xyz xyz xyz...
Result: Nearby cells have similar keys!
```

### Step 3: Collect Trajectory IDs

```
Process each trajectory:
  Trajectory 1 at (5.2, 8.7, 3.1)
    → Cell (0, 0, 0)
    → Z-Order Key: 0
    → Add ID 1 to cell 0

  Trajectory 2 at (15.8, 12.3, 7.4)
    → Cell (1, 1, 0)
    → Z-Order Key: 3
    → Add ID 2 to cell 3

  Trajectory 3 at (6.1, 9.2, 2.8)
    → Cell (0, 0, 0)
    → Z-Order Key: 0
    → Add ID 3 to cell 0

Result:
  Cell 0 (Z-Key: 0) → [1, 3]
  Cell 3 (Z-Key: 3) → [2]
```

### Step 4: Create Sorted Hash Table

```
Hash Table Entries (sorted by Z-Order Key):
┌───────────────┬─────────────┬───────────────┐
│ Z-Order Key   │ Start Index │ Traj Count    │
├───────────────┼─────────────┼───────────────┤
│ 0x00000000    │ 0           │ 2             │ → [1, 3]
│ 0x00000003    │ 2           │ 1             │ → [2]
│ 0x0000000F    │ 3           │ 3             │ → [4, 7, 12]
│ ...           │ ...         │ ...           │
└───────────────┴─────────────┴───────────────┘

Trajectory IDs Array:
[1, 3, 2, 4, 7, 12, ...]
 └─┬─┘ │  └───┬───┘
 Cell 0│   Cell 15
      Cell 3
```

## Querying the Hash Table

### Example: Find trajectories in cell at position (5.5, 8.0, 3.0)

```
1. Convert to cell coordinates:
   Cell = floor((5.5-0)/10, (8.0-0)/10, (3.0-0)/10)
        = (0, 0, 0)

2. Calculate Z-Order key:
   Key = ZOrder(0, 0, 0) = 0

3. Binary search in entries:
   [0, 3, 15, 42, 108, ...]
    ↑
   Found at index 0!

4. Read trajectory IDs:
   Entry[0]: StartIndex=0, Count=2
   IDs = [1, 3]

Result: Trajectories 1 and 3 are in this cell
```

## Why Z-Order Curves?

### Spatial Locality Visualization

```
2D Z-Order Curve (extends to 3D):

  ┌─┬─┬─┬─┐
3 │C│D│◄│ │  Numbers show Z-Order key
  ├─┼─┼─┼─┤  visiting order
2 │B│◄│ │ │
  ├─┼─┼─┼─┤  Letters show path through
1 │A│ │ │ │  space
  ├─┼─┼─┼─┤
0 │●→→→ │ │  ● = Start (0,0) = Key 0
  └─┴─┴─┴─┘
   0 1 2 3

Path: 0→1→2→3 (A) → 4→5→6→7 (B) → 8→9→10→11 (C) → ...

Nearby cells in 2D/3D space = nearby keys in 1D!
```

### Benefits

```
Traditional Hash:
  Cell (0,0,0) → Hash: 7382947
  Cell (1,0,0) → Hash: 2847291  ❌ No relation!
  Cell (0,1,0) → Hash: 9234872

Z-Order Hash:
  Cell (0,0,0) → Key: 0         ✓ Sequential!
  Cell (1,0,0) → Key: 1
  Cell (0,1,0) → Key: 2
  
→ Enables binary search
→ Range queries efficient
→ Cache-friendly access
```

## Performance Summary

```
Operation          | Complexity      | Example (1M trajectories, 100K cells)
-------------------|-----------------|-------------------------------------
Build Hash Table   | O(n log n)      | ~20ms (parallel)
Find Cell          | O(log k)        | ~17 comparisons (log₂ 100,000)
Range Query        | O(log k × m)    | Depends on query size
Memory (In-RAM)    | O(k)            | 64 bytes + 100K×16 = 1.6 MB
Memory (On-Disk)   | O(n)            | 1.6 MB + 1M×4 = ~5.6 MB

n = trajectories, k = occupied cells, m = trajectories per cell
```

## Code Flow

```
BuildHashTableForTimeStep()
  │
  ├─► Initialize header (time step, cell size, bbox)
  │
  ├─► For each trajectory sample:
  │    ├─► WorldToCellCoordinates(position)
  │    │     └─► cellX = floor((x - bboxMin.x) / cellSize)
  │    │         cellY = floor((y - bboxMin.y) / cellSize)
  │    │         cellZ = floor((z - bboxMin.z) / cellSize)
  │    │
  │    ├─► CalculateZOrderKey(cellX, cellY, cellZ)
  │    │     └─► Interleave bits: SplitBy3(X) | (SplitBy3(Y)<<1) | (SplitBy3(Z)<<2)
  │    │
  │    └─► CellMap[key].Add(trajectoryID)
  │
  ├─► Sort Z-Order keys
  │
  ├─► Build entries array:
  │    ├─► For each key (sorted):
  │    │    ├─► Create entry: (key, startIndex, count)
  │    │    └─► Append trajectory IDs to flat array
  │
  └─► Save to binary file
        ├─► Header (64 bytes)
        ├─► Entries (k × 16 bytes)
        └─► Trajectory IDs (n × 4 bytes)
```

## Real-World Example

```
Dataset: 1000 time steps, 10,000 trajectories, cell size 10m

Without Spatial Hash:
  Query: "Find trajectories within 50m of point P"
  → Must check all 10,000 trajectories
  → Distance calculation: 10,000 × sqrt(dx²+dy²+dz²)
  → Time: ~1ms per query

With Z-Order Spatial Hash:
  Query: Same
  → Determine cells in radius (≈ 125 cells for 50m radius)
  → Binary search: log₂(100,000) = 17 comparisons
  → Check only ~50 trajectories in relevant cells
  → Time: ~0.01ms per query
  
Speedup: 100× faster! 🚀
```

## Summary

The Z-Order spatial hash table algorithm:

1. **Partitions** 3D space into uniform grid cells
2. **Indexes** cells using Z-Order (Morton) keys for spatial locality
3. **Collects** trajectory IDs in each cell
4. **Sorts** by Z-Order key for efficient binary search
5. **Enables** fast spatial queries on massive datasets

**Result**: O(log k) queries on sorted 1D array instead of O(n) brute force searches!
