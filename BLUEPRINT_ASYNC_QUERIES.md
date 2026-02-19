# Blueprint Async Query Task Nodes

## Overview

This guide explains how to use async query task nodes in Blueprints to prevent the game thread from being blocked during trajectory queries.

## Why Use Async Task Nodes?

**Problem: Synchronous Methods Block the Game Thread**

When you use synchronous query methods like `Query Radius With Distance Check`, the game thread must wait for disk I/O to complete:
- ❌ Frame rate drops/stutters during queries
- ❌ Game feels unresponsive
- ❌ Not acceptable for production/shipping

**Solution: Async Task Nodes Run on Background Threads**

Async task nodes perform I/O on background threads and deliver results via output execution pins:
- ✅ Game thread continues uninterrupted
- ✅ Smooth, consistent frame rate
- ✅ Professional, production-ready
- ✅ Multiple queries can run simultaneously

## Available Async Task Nodes

All async task nodes follow the Blueprint Async Action pattern:
1. Call the static factory method to create the task
2. The node has an output execution pin that fires when complete
3. Results are provided as output parameters
4. Game continues immediately after calling the node

### 1. Query Radius Async

Query trajectories within a radius at a single timestep.

**Blueprint Usage:**
```
Event Tick (or any event)
├─ Call "Query Radius Async"
│  ├─ World Context Object: Self
│  ├─ Manager: Your Spatial Hash Table Manager reference
│  ├─ Dataset Directory: "C:/Data/MyTrajectories"
│  ├─ Query Position: Actor Location or custom position
│  ├─ Radius: 500.0
│  ├─ Cell Size: 10.0
│  └─ Time Step: Current simulation time step
├─ Other game logic continues immediately here (non-blocking!)
└─ "On Complete" output pin:
   ├─ Fires when query finishes
   └─ Results (Array of Spatial Hash Query Result)
      └─ ForEach Loop: Results
         ├─ Get: Trajectory ID
         ├─ Get: Sample Points
         └─ Process each trajectory sample:
            ├─ Position (Vector)
            ├─ Time Step (Integer)
            └─ Distance (Float)
```

**Use Case:** Find all trajectories near a point at a specific time.

### 2. Query Dual Radius Async

Query with both inner and outer radius simultaneously - more efficient than two separate queries.

**Blueprint Usage:**
```
On Player Action
├─ Call "Query Dual Radius Async"
│  ├─ World Context Object: Self
│  ├─ Manager: Your Spatial Hash Table Manager reference
│  ├─ Dataset Directory: "C:/Data/MyTrajectories"
│  ├─ Query Position: Target Location
│  ├─ Inner Radius: 200.0 (close range)
│  ├─ Outer Radius: 500.0 (far range)
│  ├─ Cell Size: 10.0
│  └─ Time Step: Current time
├─ Other game logic continues immediately here
└─ "On Complete" output pin:
   ├─ Fires when query finishes
   ├─ Inner Results (Array of Spatial Hash Query Result)
   ├─ Outer Results (Array of Spatial Hash Query Result)
   ├─ Process Inner Results:
   │  └─ ForEach: High priority handling for close trajectories
   └─ Process Outer Results:
      └─ ForEach: Lower priority handling for distant trajectories
```

**Use Case:** Different behavior for near vs far trajectories (e.g., high detail close, low detail far).

### 3. Query Time Range Async

Query trajectories that pass within radius of a point over multiple timesteps.

**Blueprint Usage:**
```
On Analysis Start
├─ Call "Query Time Range Async"
│  ├─ World Context Object: Self
│  ├─ Manager: Your Spatial Hash Table Manager reference
│  ├─ Dataset Directory: "C:/Data/MyTrajectories"
│  ├─ Query Position: Analysis Point
│  ├─ Radius: 300.0
│  ├─ Cell Size: 10.0
│  ├─ Start Time Step: 0
│  └─ End Time Step: 1000
├─ Show "Loading..." UI (game still responsive)
└─ "On Complete" output pin:
   ├─ Fires when query finishes
   ├─ Results (Array of Spatial Hash Query Result)
   ├─ Hide "Loading..." UI
   └─ ForEach: Results
      ├─ Get: Trajectory ID
      └─ Get: Sample Points (all samples in time range)
         └─ ForEach: Sample Points
            ├─ Visualize trajectory path
            └─ Analyze trajectory behavior
```

**Use Case:** Analyze which trajectories pass through a region over time.

### 4. Query Trajectory Async

Query trajectories that interact with a moving query trajectory over time.

**Blueprint Usage:**
```
On Trajectory Analysis
├─ Call "Query Trajectory Async"
│  ├─ World Context Object: Self
│  ├─ Manager: Your Spatial Hash Table Manager reference
│  ├─ Dataset Directory: "C:/Data/MyTrajectories"
│  ├─ Query Trajectory ID: 42 (the trajectory to follow)
│  ├─ Radius: 250.0 (interaction radius)
│  ├─ Cell Size: 10.0
│  ├─ Start Time Step: 0
│  └─ End Time Step: 500
├─ Begin visualization setup
└─ "On Complete" output pin:
   ├─ Fires when query finishes
   ├─ Results (Array of Spatial Hash Query Result)
   └─ ForEach: Results
      ├─ Get: Trajectory ID
      └─ Visualize trajectory interactions:
         ├─ Draw lines between query trajectory and result
         └─ Highlight collision/near-miss events
```

**Use Case:** Find trajectories that come close to a specific trajectory (collision analysis, interaction detection).

## Best Practices

### 1. Store Manager Reference

Create and store your Spatial Hash Table Manager reference:

```
Event BeginPlay
├─ Create Object: Spatial Hash Table Manager
├─ Promote to Variable: "Manager"
└─ Load Hash Tables on Manager
```

### 2. Check for Empty Results

Always check if results are empty before processing:

```
On Complete Execution Pin
├─ Array Length: Results
├─ Branch: Length > 0
│  ├─ True: Process results
│  └─ False: Handle no results case
```

### 3. Don't Call Async Task Nodes Every Frame

Async task nodes are for potentially expensive operations. Don't spam them:

**❌ Bad - Calling every frame:**
```
Event Tick
└─ Query Radius Async  // DON'T DO THIS
```

**✅ Good - Call when needed:**
```
Custom Event "On Player Action"
└─ Query Radius Async  // Only when needed

Event Tick + Timer
├─ Get Time Seconds
├─ Modulo: 1.0 (query once per second)
└─ If == 0: Query Radius Async
```

### 4. Show Loading Indicators

For longer queries, show a loading indicator:

```
On Query Start
├─ Show "Loading..." widget
└─ Call async query task node

"On Complete" Execution Pin
└─ Hide "Loading..." widget
```

### 5. Handle Errors Gracefully

Async queries can fail (missing files, etc.). Always handle empty results:

```
"On Complete" Execution Pin
├─ Results parameter
├─ Array Length: Results
├─ Branch: Length > 0
│  ├─ True: Process results
│  └─ False: 
│     ├─ Print String: "No trajectories found"
│     └─ Continue with fallback behavior
```

## Performance Considerations

### Memory Usage

- Async queries load trajectory data from disk
- Large time ranges = more memory used
- Results are delivered on game thread

### Concurrent Queries

- Multiple async queries can run simultaneously
- Each query uses a background thread
- Results arrive independently via their output pins

### Query Frequency

Recommended query patterns:
- **Once on level load**: Pre-load commonly used data
- **On demand**: When player performs an action
- **Periodic**: Once per second or slower
- **❌ Not every frame**: Too expensive

## Troubleshooting

### "No results found"

**Possible causes:**
- Hash tables not loaded (call `Load Hash Tables` first)
- Dataset directory path incorrect
- Cell size doesn't match loaded hash tables
- No trajectories exist in query region

**Solution:**
1. Check hash tables are loaded: `Is Hash Table Loaded`
2. Verify dataset path is correct and absolute
3. Ensure cell size matches

### "Game still stutters"

**Possible causes:**
- Using synchronous methods by mistake
- Processing too many results on game thread
- Query result is very large

**Solutions:**
- Verify you're using async task nodes (Query Radius Async, etc.)
- Process results in smaller batches
- Reduce query radius or time range

### "Output pin doesn't fire"

**Possible causes:**
- Manager reference is null
- Query failed silently
- Task node not properly connected

**Solutions:**
- Verify Manager variable is valid before calling
- Check Output Log for error messages
- Ensure task node's output pins are connected

## Complete Example Blueprint

Here's a complete example showing proper async query usage:

```
=== Event Graph ===

Event BeginPlay
├─ Create Object: Spatial Hash Table Manager
│  └─ Assign to "Manager" variable
├─ Call "Load Hash Tables" on Manager
│  ├─ Dataset Directory: "C:/Data/Trajectories"
│  ├─ Cell Size: 10.0
│  ├─ Start Time Step: 0
│  ├─ End Time Step: 1000
│  └─ Auto Create: true
└─ Set "bHashTablesReady" = true

Custom Event "Query Nearby Trajectories"
├─ Branch: bHashTablesReady
│  ├─ False: Print "Hash tables not ready"
│  └─ True: Continue
├─ Get Actor Location
│  └─ Assign to "Query Position"
├─ Set "bQueryInProgress" = true
├─ Show Loading Widget
├─ Call "Query Radius Async"
│  ├─ World Context Object: Self
│  ├─ Manager: Manager variable
│  ├─ Dataset Directory: "C:/Data/Trajectories"
│  ├─ Query Position: Query Position
│  ├─ Radius: 500.0
│  ├─ Cell Size: 10.0
│  └─ Time Step: Current Time Step
└─ "On Complete" output pin:
   ├─ Results parameter
   ├─ Set "bQueryInProgress" = false
   ├─ Hide Loading Widget
   ├─ Print String: Append("Found ", ToString(Array Length(Results)), " trajectories")
   ├─ Branch: Array Length > 0
   │  ├─ True: Process Results
   │  │  └─ ForEach: Results
   │  │     ├─ Get: Trajectory ID
   │  │     └─ Get: Sample Points
   │  │        └─ Visualize trajectory
   │  └─ False: Print "No trajectories in range"
   └─ Continue with game logic
```

## Migration from Sync to Async

If you're currently using synchronous methods, here's how to migrate:

**Before (Synchronous - blocks game thread):**
```
Event Tick
├─ Call "Query Radius With Distance Check"  // Blocks here!
│  └─ Returns: Results
└─ Process Results immediately
```

**After (Async - non-blocking):**
```
Event Tick + Timer (every 1 second)
├─ Call "Query Radius Async"  // Returns immediately
│  ├─ World Context Object: Self
│  ├─ Manager: Your manager reference
│  └─ ...other parameters
└─ "On Complete" output pin:
   ├─ Results parameter
   └─ Process Results when ready
```

**Key differences:**
1. Use async task node instead of synchronous method
2. Results come via output execution pin, not return value
3. Processing happens on the output pin execution path
4. Game continues while query runs on background thread

## Summary

- ✅ Always use async task nodes for production
- ✅ Use output execution pins for result handling
- ✅ Check for empty results
- ✅ Show loading indicators for long queries
- ✅ Don't query every frame
- ✅ Handle errors gracefully
- ❌ Don't use synchronous methods if frame rate matters
- ❌ Don't process massive results immediately

For C++ usage, see [ASYNC_QUERY_METHODS.md](ASYNC_QUERY_METHODS.md).
