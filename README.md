# Project Pulse

Code for Project Pulse.

## Context

This code is one part of a project. The design lives in the Obsidian vault, not here:

| What | Where |
| --- | --- |
| Portfolio card (what the site shows) | `Projects_Vault/Portfolio/Projects/Project Pulse.md` |
| Workspace: handout, devlog, design notes | `Projects_Vault/Portfolio/Project Pulse/` |
| Original notes (never edited) | `The Union/` (see the card's `sources`) |

## Running

- `Play.bat` plays it, `Open in Godot Editor.bat` opens the editor (Godot 4.7; set `GODOT` to point elsewhere).

## Publishing to the portfolio

From the portfolio repo (`VarXael.github.io`):

```
npm run publish-build -- "Project Pulse"
```

It exports the web and Windows builds, puts the web build on the site, uploads the Windows zip to GitHub Releases and records both on the card. Builds go to `export/`, which git ignores.
