# nx System Maintenance

> System maintenance commands for optimization, backup, and health monitoring.
> Keep your notes system running efficiently and safely.
> More information: <https://nx-notes.dev/docs/maintenance>.

## Backup Operations

- Create a compressed backup:

`nx backup create {{~/backups/notes-$(date +%Y%m%d).tar.gz}} --compress`

- List all available backups:

`nx backup list`

- Restore from a backup:

`nx backup restore {{~/backups/notes-20240115.tar.gz}}`

- Verify backup integrity:

`nx backup verify {{~/backups/notes-20240115.tar.gz}}`

- Clean up old backups:

`nx backup cleanup --keep {{5}}`

## Search Index Management

- Rebuild the search index:

`nx reindex rebuild --force`

- Optimize index for better performance:

`nx reindex optimize`

- Validate index integrity:

`nx reindex validate`

- Show index statistics:

`nx reindex stats`

## Garbage Collection

- Run all cleanup operations:

`nx gc all --force`

- Preview cleanup operations without changes:

`nx gc cleanup --dry-run`

- Optimize storage and remove unused data:

`nx gc optimize`

- Vacuum database for space reclamation:

`nx gc vacuum`

- Show storage usage statistics:

`nx gc stats`

## System Health

- Run comprehensive health check:

`nx doctor`

- Quick health check (essential only):

`nx doctor --quick`

- Check specific category:

`nx doctor --category {{storage}}`

- Auto-fix detected issues:

`nx doctor --fix --verbose`