# Server Startup Optimization

## Current Startup Time: ~60-90 seconds

### Phase Breakdown
| Phase | Duration | Bottleneck |
|-------|----------|------------|
| DBC/Map data loading | ~30-45s | Single-threaded, I/O bound |
| Database loading | ~15-20s | Sequential queries |
| Script loading | ~1-3s | Cached, fast |
| Map initialization | ~15-20s | Grid loading |
| **Total** | **~60-90s** | — |

## Quick Wins (Config Only)

```ini
# worldserver.conf
GridUnload = 1
LoadAllGridsOnMaps = 0
Logger.root = 2  # Reduce from 5 (debug) to 2 (error)
UseProcessors = 0  # All cores
MapUpdate.Threads = 4
```

## Code-Level Improvements (PR Candidates)

### 1. Parallel Map Loading
- **File:** `src/server/game/Maps/MapManager.cpp`
- **Change:** Load maps in parallel threads
- **Impact:** 30-45s → 10-15s
- **Risk:** Medium (thread safety)

### 2. Async Database Loading
- **Change:** Use ThreadPool for DB queries
- **Impact:** 15-20s → 5-8s
- **Risk:** Low

### 3. DBC Cache
- **Change:** Cache parsed DBC data to disk
- **Impact:** 5-10s → 1-2s
- **Risk:** Medium

### 4. Reduced Logging During Startup
- **Change:** Suppress debug logs during initial load
- **Impact:** 2-3s saved
- **Risk:** Low

## Build Optimization (Already Applied)

| Technique | No-Change Rebuild | Single-File Rebuild |
|-----------|-------------------|---------------------|
| BuildKit cache mounts | 5.3s | 45s |
| ccache 4.11.2 | — | — |
| Multi-stage build | — | — |

## Storage Optimization
- Use SSD/NVMe for `/data` volume
- Use `tmpfs` for log files
- Docker volumes on fast storage

## Notes
- The 100% CPU on one core during startup is the single-threaded DBC/map loading
- This is inherent to TrinityCore's architecture
- Parallel loading would require significant code changes