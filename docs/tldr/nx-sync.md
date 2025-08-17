# nx sync

> Git synchronization operations for backing up and sharing notes.
> Enables version control and collaborative note management.
> More information: <https://nx-notes.dev/docs/sync>.

- Check synchronization status:

`nx sync status`

- Initialize a Git repository for notes:

`nx sync init --remote {{git@github.com:user/notes.git}}`

- Clone an existing notes repository:

`nx sync clone {{git@github.com:user/notes.git}} --branch {{main}}`

- Pull latest changes from remote:

`nx sync pull`

- Pull with specific merge strategy:

`nx sync pull --strategy {{rebase}} --no-auto-resolve`

- Push local changes to remote:

`nx sync push --message "{{Updated project notes}}"`

- Force push (use with caution):

`nx sync push --force`

- Perform bidirectional sync (pull + push):

`nx sync sync --message "{{Daily sync}}"`

- Resolve merge conflicts manually:

`nx sync resolve --strategy {{manual}}`

- Resolve conflicts using local version:

`nx sync resolve --strategy {{ours}}`

- Resolve conflicts for specific files:

`nx sync resolve --files {{note1.md,note2.md}} --strategy {{theirs}}`

- Set Git user information for commits:

`nx sync push --user-name "{{John Doe}}" --user-email "{{john@example.com}}"`