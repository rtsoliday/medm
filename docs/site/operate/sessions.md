# Sessions and snapshots

Sessions restore a set of display windows. PV snapshots capture channel values. Use the appropriate feature for the state you need to restore.

## Named sessions

Start a saved session explicitly with:

```sh
bin/Linux-x86_64/qtedm --session name
```

Replace `name` with your saved session name. Session restore defaults to execute mode, validates the displays, reports omissions, and places windows on connected screens. Read [Named Sessions](../reference/adl#named-sessions) for the file format, stored fields, and restore behavior.

## PV snapshots

Read [PV Snapshots](../reference/adl#pv-snapshots) for the snapshot workflow and restore checks. Restoring channel values is separate from restoring display windows. Global read-only mode blocks snapshot writes.
