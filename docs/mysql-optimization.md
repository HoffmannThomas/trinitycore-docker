# MySQL Optimization Guide

## Overview

MySQL 8.4 is used for TrinityCore's auth, world, characters, and hotfixes databases. Default configuration is conservative; tuning significantly improves startup and runtime performance.

## Recommended Configuration

### docker-compose.yml
```yaml
db:
  image: mysql:8.4
  environment:
    MYSQL_ROOT_PASSWORD: ${MYSQL_ROOT_PASSWORD:-trinity}
  command: >
    --innodb-buffer-pool-size=2G
    --innodb-buffer-pool-instances=4
    --innodb-read-io-threads=8
    --innodb-write-io-threads=8
    --innodb-log-file-size=256M
    --innodb-flush-log-at-trx-commit=2
    --innodb-flush-method=O_DIRECT
    --max-connections=200
    --tmp-table-size=64M
    --max-heap-table-size=64M
  deploy:
    resources:
      limits:
        cpus: '4.0'
        memory: 4G
```

### Settings Explained

| Setting | Default | Recommended | Impact |
|---------|---------|-------------|--------|
| `innodb_buffer_pool_size` | 128MB | 2GB | **Major** — caches data in RAM |
| `innodb_buffer_pool_instances` | 1 | 4 | Reduces lock contention |
| `innodb_read_io_threads` | 4 | 8 | Parallel read operations |
| `innodb_write_io_threads` | 4 | 8 | Parallel write operations |
| `innodb_log_file_size` | 48MB | 256MB | Reduces checkpoint frequency |
| `innodb_flush_log_at_trx_commit` | 1 | 2 | Faster writes (slightly less durable) |
| `innodb_flush_method` | fsync | O_DIRECT | Avoids double buffering |
| `max_connections` | 151 | 200 | Handles more concurrent players |
| `tmp_table_size` | 16MB | 64MB | Larger temp tables in memory |
| `max_heap_table_size` | 16MB | 64MB | Larger memory tables |

### Expected Impact
- **Startup**: 15-25% faster
- **Runtime**: 20-40% better query throughput
- **Memory**: +2GB RAM usage

### Notes
- `innodb_log_file_size` is deprecated in MySQL 8.4; use `innodb_redo_log_capacity` instead
- Buffer pool should be ~70-80% of available RAM
- CPU limit of 4 cores is sufficient for most workloads