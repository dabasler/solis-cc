# qSOLIS result cleanup

Cleanup is part of the same qSOLIS plugin binary as the calculator.

## GUI

Select one or more point clouds/meshes and choose:

```text
Plugins > SOLIS Cleanup...
```

Available operations:

- **Last SOLIS run** — removes Direct/Diffuse/Total fields sharing the most recent `runId` on each selected entity.
- **Last SOLIS batch** — removes all stored time points belonging to the most recent `batchId` on each selected entity.
- **Selected SOLIS batch** — selects a metadata-backed batch by ID/time and removes it across the selected entities.
- **All metadata-backed SOLIS results** — removes every result registered under `qSOLIS/result/*` while leaving unrelated scalar fields untouched.
- **Legacy SOLIS-named fields** — explicit opt-in name-based cleanup for old results that predate qSOLIS metadata. Only fields whose names begin with `SOLIS ` and are not already metadata-backed are removed.

The dialog reports the approximate field count before deletion. Metadata-backed
cleanup is the default/safe path. Legacy cleanup is deliberately separate because
it relies on naming rather than provenance metadata.

For meshes, cleanup operates on the associated vertex cloud where CloudCompare
stores scalar fields.

## CLI

The same plugin registers:

```text
-SOLIS_CLEAN LAST_RUN
-SOLIS_CLEAN LAST_BATCH
-SOLIS_CLEAN BATCH <batch-uuid>
-SOLIS_CLEAN ALL
-SOLIS_CLEAN LEGACY
```

Examples:

```bash
CloudCompare -SILENT -O project.bin -SOLIS_CLEAN LAST_BATCH
CloudCompare -SILENT -O project.bin -SOLIS_CLEAN LAST_RUN
CloudCompare -SILENT -O project.bin -SOLIS_CLEAN BATCH 01234567-89ab-cdef-0123-456789abcdef
CloudCompare -SILENT -O project.bin -SOLIS_CLEAN ALL
```

`ALL` removes metadata-backed qSOLIS results only. It does **not** remove legacy
name-only fields; use `LEGACY` explicitly for those.

With CloudCompare auto-save enabled, cleanup exports the modified entities with a
`SOLIS_CLEAN` suffix. Use `-AUTO_SAVE OFF` when the rest of your command pipeline
will save the entities itself.

Deletion is permanent once the modified project/entity is saved.


## Exposure results (schema 3)

Canopy exposure scalar fields are normal metadata-backed qSOLIS results and are handled by Last Run / All cleanup. Crown summaries stored under `qSOLIS/crownExposure/<runId>` are removed with their run. Legacy-name cleanup behavior is unchanged.
