# nx notebook

> Manage notebooks for organizing notes into collections.
> Notebooks provide hierarchical organization for your notes.
> More information: <https://nx-notes.dev/docs/notebooks>.

- List all notebooks:

`nx notebook list`

- List notebooks with detailed information in JSON format:

`nx notebook list --json`

- Create a new notebook:

`nx notebook create "{{Work Projects}}" "{{Notes for work-related projects}}"`

- Get detailed information about a notebook:

`nx notebook info "{{Work Projects}}"`

- Rename an existing notebook:

`nx notebook rename "{{Old Name}}" "{{New Name}}"`

- Delete an empty notebook:

`nx notebook delete "{{Notebook Name}}"`

- Force delete a notebook even if it contains notes:

`nx notebook delete "{{Notebook Name}}" --force`

- Create a note in a specific notebook:

`nx new "{{Project Plan}}" --nb "{{Work Projects}}" --tags {{planning,work}}`

- Move a note to a different notebook:

`nx mv {{note_id}} --nb "{{Archive}}"`

- List all notes in a specific notebook:

`nx ls --nb "{{Work Projects}}"`