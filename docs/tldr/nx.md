# nx

> High-performance CLI notes application with AI integration.
> Supports Markdown notes, full-text search, notebook organization, and AI-powered features.
> More information: <https://nx-notes.dev>.

- Create a new note with title and tags:

`nx new "{{Meeting Notes}}" --tags {{work,meeting}} --nb {{projects}}`

- List all notes or filter by tag/notebook:

`nx ls`

- List notes with specific tag since a date:

`nx ls --tag {{work}} --since {{2024-01-01}}`

- Search for content across all notes:

`nx grep "{{algorithm}}" --regex`

- Edit an existing note:

`nx edit {{note_id}}`

- View a note in the terminal:

`nx view {{note_id}}`

- Launch interactive TUI interface:

`nx ui`

- Create a backup of all notes:

`nx backup create {{~/backups/notes-backup.tar.gz}} --compress`

- Ask AI questions about your notes:

`nx ask "{{What did I learn about machine learning?}}"`

- Generate AI summary of a note:

`nx summarize {{note_id}} --style {{bullets}} --apply`

- Import notes from a directory:

`nx import dir {{~/Documents/notes}} --format {{obsidian}} --recursive`

- Export notes to different formats:

`nx export {{md}} --to {{~/exports}} --since {{2024-01-01}}`