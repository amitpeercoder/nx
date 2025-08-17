# nx tpl

> Template management for creating reusable note structures.
> Templates can contain variables and standard content for consistent note creation.
> More information: <https://nx-notes.dev/docs/templates>.

- List all available templates:

`nx tpl list`

- Show details of a specific template:

`nx tpl show "{{meeting-notes}}"`

- Create a new template from scratch:

`nx tpl create "{{daily-journal}}" --description "{{Daily reflection template}}" --category "{{personal}}"`

- Create a template from an existing file:

`nx tpl create "{{project-plan}}" --from-file {{~/templates/project.md}} --category "{{work}}"`

- Edit an existing template in your editor:

`nx tpl edit "{{meeting-notes}}"`

- Create a note from a template:

`nx tpl use "{{meeting-notes}}" --title "{{Team Standup}}" --vars "{{date=2024-01-15,attendees=Alice,Bob}}"`

- Search templates by name or content:

`nx tpl search "{{meeting}}"`

- Delete a template:

`nx tpl delete "{{old-template}}"`

- Install built-in templates:

`nx tpl install`

- Create a template and force overwrite existing:

`nx tpl create "{{notes}}" --force --description "{{Updated template}}"`