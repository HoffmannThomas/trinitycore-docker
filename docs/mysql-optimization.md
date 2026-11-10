# MySQL Optimization Guide

## Overview

The `db` service runs `mysql:8.4` and hosts the auth, world, characters, and hotfixes databases. The Compose file sets a few server flags for TrinityCore's workload. The values below are starting points, not measured results. Measure on your own hardware before relying on them.

## What Compose sets

From `docker-compose.yml` (`db` service):

```yaml
command: >
  --innodb-buffer-pool-size=2G
  --innodb-buffer-pool-instances=4
  --innodb-read-io-threads=8
  --innodb-write-io-threads=8
  --innodb-redo-log-capacity=536870912
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

## Redo log setting (MySQL 8.4)

MySQL 8.4 removed `innodb_log_file_size` and `innodb_log_files_in_group` in favor of `innodb_redo_log_capacity`. Setting `innodb_log_file_size` still works, but MySQL logs a deprecation warning and computes the capacity from it.

- `--innodb-redo-log-capacity=536870912` (512 MiB) is the value MySQL reported when the deprecated setting was used with `innodb_log_file_size=256M`. It keeps the previously intended capacity.
- Do not set both options.

## Settings explained

| Setting | MySQL default (8.4) | Compose value | Notes |
|---------|---------------------|---------------|-------|
| `innodb_buffer_pool_size` | 128 MiB | 2 GiB | Caches table and index pages in RAM. Must fit inside the container memory limit. |
| `innodb_buffer_pool_instances` | 8 when the pool is 1 GiB or larger, otherwise 1 | 4 | Splits the pool to reduce mutex contention. |
| `innodb_read_io_threads` | 4 | 8 | Background read threads. |
| `innodb_write_io_threads` | 4 | 8 | Background write threads. |
| `innodb_redo_log_capacity` | MySQL-computed | 512 MiB | Size of the redo log. Larger values reduce checkpoint frequency and use more disk. |
| `innodb_flush_log_at_trx_commit` | 1 | 2 | Writes the log at each commit but flushes to disk about once per second. A crash of the OS or host can lose up to about one second of commits. |
| `innodb_flush_method` | fsync | O_DIRECT | Bypasses the OS page cache for data files. |
| `max_connections` | 151 | 200 | Upper bound on client connections (worldserver and bnetserver use pools). |
| `tmp_table_size` / `max_heap_table_size` | 16 MiB | 64 MiB | In-memory temporary table limits. |

## Resource assumptions

- The `db` container is limited to 4 CPUs and 4 GiB of memory. The 2 GiB buffer pool plus connection and temp-table overhead must fit in that limit. If you lower the memory limit, lower the buffer pool too.
- The host needs enough free RAM for the limit above and for the worldserver container.
- The values were chosen for a single-host, single-realm setup. They have not been benchmarked against this project's workload.

## Checking the values

After the `db` container is recreated with the new flags:

```bash
docker compose exec -T db mysql -uroot -pYOUR_ROOT_PASSWORD -e "SHOW VARIABLES WHERE Variable_name IN ('innodb_buffer_pool_size','innodb_redo_log_capacity','max_connections');" 2>&1 | grep -v Warning
```

Also check the `db` logs for the deprecation warning about `innodb_log_file_size`. It should not appear after the change.

## Applying a change

The Compose flags take effect when the `db` container is recreated, not on a plain restart:

```bash
docker compose up -d db
```

Recreating `db` interrupts the other services. Restart `bnetserver` and `worldserver` afterward if they lose their database connections.
