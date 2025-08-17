#!/bin/bash
# Bash completion for nx - High-performance CLI notes application
# Add to ~/.bashrc or ~/.bash_completion:
#   source /path/to/nx/completions/nx.bash

_nx_completions() {
    local cur prev words cword
    _init_completion || return

    # Main commands
    local commands=(
        "new" "edit" "view" "ls" "rm" "mv"
        "grep" "open" "backlinks" "tags"
        "export" "ask" "summarize" "tag-suggest" "title" "rewrite"
        "tasks" "suggest-links" "outline" "ui"
        "notebook" "attach" "import" "tpl" "meta"
        "reindex" "backup" "gc" "doctor" "config"
        "encrypt" "sync"
    )

    # Global options
    local global_opts=(
        "--help" "--version" "--help-all" "--json"
        "--verbose" "--quiet" "--config" "--notes-dir" "--no-color"
    )

    # If we're completing the first argument (command)
    if [[ $cword -eq 1 ]]; then
        COMPREPLY=($(compgen -W "${commands[*]} ${global_opts[*]}" -- "$cur"))
        return 0
    fi

    # Get the command (first non-option argument)
    local cmd=""
    local i=1
    while [[ $i -lt $cword ]]; do
        if [[ ${words[$i]} != -* ]]; then
            cmd=${words[$i]}
            break
        fi
        ((i++))
    done

    case "$cmd" in
        new)
            case "$prev" in
                --tags|-t)
                    # Complete with existing tags
                    _nx_complete_tags
                    return 0
                    ;;
                --nb|--notebook)
                    _nx_complete_notebooks
                    return 0
                    ;;
                *)
                    COMPREPLY=($(compgen -W "--tags --nb --notebook --template --from --editor --ai-title --ai-tags" -- "$cur"))
                    ;;
            esac
            ;;
        edit|view|rm|summarize|tag-suggest|title|rewrite|tasks|suggest-links)
            if [[ $cur != -* ]]; then
                _nx_complete_note_ids
                return 0
            fi
            case "$cmd" in
                summarize)
                    COMPREPLY=($(compgen -W "--style --apply --max-length" -- "$cur"))
                    ;;
                tag-suggest)
                    COMPREPLY=($(compgen -W "--apply --max-tags" -- "$cur"))
                    ;;
                title)
                    COMPREPLY=($(compgen -W "--apply --style" -- "$cur"))
                    ;;
                rewrite)
                    COMPREPLY=($(compgen -W "--tone --style --apply" -- "$cur"))
                    ;;
                tasks)
                    COMPREPLY=($(compgen -W "--priority --format --apply" -- "$cur"))
                    ;;
                suggest-links)
                    COMPREPLY=($(compgen -W "--apply --max-links" -- "$cur"))
                    ;;
            esac
            ;;
        mv)
            case "$prev" in
                --nb|--notebook)
                    _nx_complete_notebooks
                    return 0
                    ;;
                *)
                    if [[ $cur != -* ]]; then
                        _nx_complete_note_ids
                        return 0
                    fi
                    COMPREPLY=($(compgen -W "--nb --notebook" -- "$cur"))
                    ;;
            esac
            ;;
        ls)
            case "$prev" in
                --tag)
                    _nx_complete_tags
                    return 0
                    ;;
                --nb|--notebook)
                    _nx_complete_notebooks
                    return 0
                    ;;
                --since|--until)
                    # Date completion - suggest common formats
                    COMPREPLY=($(compgen -W "today yesterday week month year" -- "$cur"))
                    return 0
                    ;;
                *)
                    COMPREPLY=($(compgen -W "--tag --nb --notebook --since --until --limit --sort" -- "$cur"))
                    ;;
            esac
            ;;
        grep)
            if [[ $cur != -* ]]; then
                # Don't complete query text, let user type freely
                return 0
            fi
            COMPREPLY=($(compgen -W "--regex --content --case-sensitive --limit" -- "$cur"))
            ;;
        open)
            if [[ $cur != -* ]]; then
                # Complete with note titles for fuzzy matching
                _nx_complete_note_titles
                return 0
            fi
            ;;
        backlinks)
            if [[ $cur != -* ]]; then
                _nx_complete_note_ids
                return 0
            fi
            ;;
        export)
            if [[ $cword -eq 2 || ($cword -eq 3 && ${words[2]} == -*) ]]; then
                COMPREPLY=($(compgen -W "md json pdf html" -- "$cur"))
                return 0
            fi
            case "$prev" in
                --to)
                    _filedir -d
                    return 0
                    ;;
                --since|--until)
                    COMPREPLY=($(compgen -W "today yesterday week month year" -- "$cur"))
                    return 0
                    ;;
                *)
                    COMPREPLY=($(compgen -W "--to --since --until --tag --notebook" -- "$cur"))
                    ;;
            esac
            ;;
        notebook)
            if [[ $cword -eq 2 || ($cword -eq 3 && ${words[2]} == -*) ]]; then
                COMPREPLY=($(compgen -W "list create rename delete info" -- "$cur"))
                return 0
            fi
            local subcmd=${words[2]}
            case "$subcmd" in
                rename|delete|info)
                    if [[ $cur != -* ]]; then
                        _nx_complete_notebooks
                        return 0
                    fi
                    ;;
                delete)
                    COMPREPLY=($(compgen -W "--force" -- "$cur"))
                    ;;
                list|info)
                    COMPREPLY=($(compgen -W "--stats" -- "$cur"))
                    ;;
            esac
            ;;
        attach)
            case "$cword" in
                2)
                    _nx_complete_note_ids
                    return 0
                    ;;
                3)
                    _filedir
                    return 0
                    ;;
                *)
                    case "$prev" in
                        --name)
                            # Let user type custom name
                            return 0
                            ;;
                        *)
                            COMPREPLY=($(compgen -W "--name --copy --move" -- "$cur"))
                            ;;
                    esac
                    ;;
            esac
            ;;
        import)
            if [[ $cword -eq 2 || ($cword -eq 3 && ${words[2]} == -*) ]]; then
                COMPREPLY=($(compgen -W "dir file obsidian notion" -- "$cur"))
                return 0
            fi
            case "$prev" in
                --format)
                    COMPREPLY=($(compgen -W "obsidian notion markdown" -- "$cur"))
                    return 0
                    ;;
                dir|file|obsidian|notion)
                    _filedir
                    return 0
                    ;;
                *)
                    COMPREPLY=($(compgen -W "--format --recursive --notebook" -- "$cur"))
                    ;;
            esac
            ;;
        tpl)
            if [[ $cword -eq 2 || ($cword -eq 3 && ${words[2]} == -*) ]]; then
                COMPREPLY=($(compgen -W "list add remove show use" -- "$cur"))
                return 0
            fi
            local subcmd=${words[2]}
            case "$subcmd" in
                remove|show|use)
                    if [[ $cur != -* ]]; then
                        _nx_complete_templates
                        return 0
                    fi
                    ;;
                add)
                    case "$prev" in
                        --file)
                            _filedir
                            return 0
                            ;;
                        *)
                            COMPREPLY=($(compgen -W "--file --force" -- "$cur"))
                            ;;
                    esac
                    ;;
                use)
                    case "$prev" in
                        --var|-v)
                            # Let user type variable assignments
                            return 0
                            ;;
                        *)
                            COMPREPLY=($(compgen -W "--var" -- "$cur"))
                            ;;
                    esac
                    ;;
            esac
            ;;
        meta)
            if [[ $cword -eq 2 ]]; then
                _nx_complete_note_ids
                return 0
            fi
            case "$prev" in
                --set)
                    # Let user type key=value
                    return 0
                    ;;
                --remove)
                    # Could complete with existing metadata keys, but complex to implement
                    return 0
                    ;;
                *)
                    COMPREPLY=($(compgen -W "--set --remove --list" -- "$cur"))
                    ;;
            esac
            ;;
        reindex)
            COMPREPLY=($(compgen -W "rebuild optimize validate stats" -- "$cur"))
            ;;
        backup)
            if [[ $cword -eq 2 || ($cword -eq 3 && ${words[2]} == -*) ]]; then
                COMPREPLY=($(compgen -W "create list restore verify cleanup" -- "$cur"))
                return 0
            fi
            local subcmd=${words[2]}
            case "$subcmd" in
                create)
                    case "$prev" in
                        --to)
                            _filedir
                            return 0
                            ;;
                        *)
                            COMPREPLY=($(compgen -W "--to --compress --metadata" -- "$cur"))
                            ;;
                    esac
                    ;;
                restore)
                    if [[ $cur != -* ]]; then
                        _filedir -o plusdirs -X '!*.tar.gz'
                        return 0
                    fi
                    COMPREPLY=($(compgen -W "--force --verify" -- "$cur"))
                    ;;
                verify)
                    if [[ $cur != -* ]]; then
                        _filedir -o plusdirs -X '!*.tar.gz'
                        return 0
                    fi
                    ;;
                cleanup)
                    COMPREPLY=($(compgen -W "--keep --older-than" -- "$cur"))
                    ;;
            esac
            ;;
        gc)
            if [[ $cword -eq 2 || ($cword -eq 3 && ${words[2]} == -*) ]]; then
                COMPREPLY=($(compgen -W "cleanup optimize vacuum stats all" -- "$cur"))
                return 0
            fi
            COMPREPLY=($(compgen -W "--dry-run --force" -- "$cur"))
            ;;
        doctor)
            case "$prev" in
                --category|-c)
                    COMPREPLY=($(compgen -W "config storage git tools performance" -- "$cur"))
                    return 0
                    ;;
                *)
                    COMPREPLY=($(compgen -W "--fix --verbose --quick --category" -- "$cur"))
                    ;;
            esac
            ;;
        config)
            if [[ $cword -eq 2 || ($cword -eq 3 && ${words[2]} == -*) ]]; then
                COMPREPLY=($(compgen -W "get set list path validate reset" -- "$cur"))
                return 0
            fi
            local subcmd=${words[2]}
            case "$subcmd" in
                get|set|reset)
                    if [[ $cur != -* ]]; then
                        _nx_complete_config_keys
                        return 0
                    fi
                    ;;
            esac
            ;;
        encrypt)
            case "$prev" in
                --key-file|-k|--output-key|-o)
                    _filedir
                    return 0
                    ;;
                *)
                    if [[ $cur != -* ]]; then
                        _nx_complete_note_ids
                        return 0
                    fi
                    COMPREPLY=($(compgen -W "--decrypt --all --key-file --generate-key --output-key" -- "$cur"))
                    ;;
            esac
            ;;
        sync)
            if [[ $cword -eq 2 || ($cword -eq 3 && ${words[2]} == -*) ]]; then
                COMPREPLY=($(compgen -W "status init clone pull push sync" -- "$cur"))
                return 0
            fi
            case "$prev" in
                --remote|-r)
                    # Git URL completion is complex, just return
                    return 0
                    ;;
                --branch|-b)
                    COMPREPLY=($(compgen -W "main master develop" -- "$cur"))
                    return 0
                    ;;
                --strategy|-s)
                    COMPREPLY=($(compgen -W "merge rebase fast-forward" -- "$cur"))
                    return 0
                    ;;
                *)
                    COMPREPLY=($(compgen -W "--remote --branch --message --force --strategy --user-name --user-email" -- "$cur"))
                    ;;
            esac
            ;;
    esac

    return 0
}

# Helper functions to complete nx-specific items
_nx_complete_note_ids() {
    # Try to get note IDs from nx ls command (if available)
    if command -v nx >/dev/null 2>&1; then
        local note_ids
        note_ids=$(nx ls --json 2>/dev/null | jq -r '.[].id' 2>/dev/null || nx ls 2>/dev/null | awk '{print $1}' | head -20)
        if [[ -n $note_ids ]]; then
            COMPREPLY=($(compgen -W "$note_ids" -- "$cur"))
        fi
    fi
}

_nx_complete_note_titles() {
    if command -v nx >/dev/null 2>&1; then
        local titles
        titles=$(nx ls --json 2>/dev/null | jq -r '.[].title' 2>/dev/null || nx ls 2>/dev/null | cut -d' ' -f2- | head -20)
        if [[ -n $titles ]]; then
            COMPREPLY=($(compgen -W "$titles" -- "$cur"))
        fi
    fi
}

_nx_complete_notebooks() {
    if command -v nx >/dev/null 2>&1; then
        local notebooks
        notebooks=$(nx notebook list --json 2>/dev/null | jq -r '.[].name' 2>/dev/null || nx notebook list 2>/dev/null | awk '{print $1}')
        if [[ -n $notebooks ]]; then
            COMPREPLY=($(compgen -W "$notebooks" -- "$cur"))
        fi
    fi
}

_nx_complete_tags() {
    if command -v nx >/dev/null 2>&1; then
        local tags
        tags=$(nx tags --json 2>/dev/null | jq -r '.[]' 2>/dev/null || nx tags 2>/dev/null | awk '{print $1}')
        if [[ -n $tags ]]; then
            COMPREPLY=($(compgen -W "$tags" -- "$cur"))
        fi
    fi
}

_nx_complete_templates() {
    if command -v nx >/dev/null 2>&1; then
        local templates
        templates=$(nx tpl list --json 2>/dev/null | jq -r '.[].name' 2>/dev/null || nx tpl list 2>/dev/null | awk '{print $1}')
        if [[ -n $templates ]]; then
            COMPREPLY=($(compgen -W "$templates" -- "$cur"))
        fi
    fi
}

_nx_complete_config_keys() {
    local config_keys=(
        "root" "notes_dir" "attachments_dir" "trash_dir" "index_file"
        "editor" "indexer" "encryption" "age_recipient"
        "sync" "git_remote" "git_user_name" "git_user_email"
        "default_notebook"
        "ai.provider" "ai.model" "ai.api_key" "ai.max_tokens" "ai.temperature"
        "performance.cache_size_mb" "performance.max_file_size_mb"
    )
    COMPREPLY=($(compgen -W "${config_keys[*]}" -- "$cur"))
}

# Register the completion function
complete -F _nx_completions nx