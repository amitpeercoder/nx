# nx AI Commands

> AI-powered features for enhanced note management and content generation.
> Requires API configuration for OpenAI, Claude, or other AI providers.
> More information: <https://nx-notes.dev/docs/ai>.

- Ask questions about your note collection:

`nx ask "{{What are my key insights about productivity?}}"`

- Generate a summary of a note:

`nx summarize {{note_id}} --style {{bullets}}`

- Apply AI summary directly to the note:

`nx summarize {{note_id}} --style {{paragraph}} --apply`

- Get AI tag suggestions for a note:

`nx tag-suggest {{note_id}}`

- Apply suggested tags automatically:

`nx tag-suggest {{note_id}} --apply`

- Get better title suggestions:

`nx title {{note_id}} --apply`

- Rewrite note content with different tone:

`nx rewrite {{note_id}} --tone {{professional}}`

- Apply rewritten content to the note:

`nx rewrite {{note_id}} --tone {{casual}} --apply`

- Extract action items and tasks from a note:

`nx tasks {{note_id}}`

- Filter tasks by priority level:

`nx tasks {{note_id}} --priority {{high}}`

- Find and suggest links to related notes:

`nx suggest-links {{note_id}} --apply`

- Generate a hierarchical outline for a topic:

`nx outline "{{Project Management Best Practices}}"`

- Create notes from outline sections:

`nx outline "{{Learning Plan}}" --create`