# Project Pulse

One arena where the world keeps the beat, not the player. The floor pulses on the beat; standing on it when the beat hits gives a speed boost and a full clip, jumping on the beat is a Super Jump, and firing on the beat resets your dash and double jump. Enemies use DPW on the beat: every hit is stored, and a hit on the beat cashes the store in; if it reaches the enemy's threshold, it dies. Acting on the beat is never required, it is a currency you choose to spend.

This is the Godot slice of Project Pulse (branch `godot-prototype`). The original game, Project Circle, is in Unreal on the other branches of this repository.

## Controls

| Key | Action |
| --- | --- |
| 1 / 2 / 3 | Menu: metronome 110 BPM, metronome 140 BPM, Immortal Flame (only if it is on this PC) |
| WASD, mouse | Move, look |
| Space | Jump. On the beat while grounded: Super Jump. In the air: double jump (on the beat it skips and resets the cooldown) |
| Shift | Dash. In the air: ground pound, which becomes a dash on landing if WASD is held |
| Left mouse | Fire (6 bullets). On the beat: resets dash and double jump |
| R | Reload |
| T | DPW miss rule: heal to full, keep the damage, lose half |
| [ and ] | Audio latency offset, 5 ms steps (Shift: 20 ms) |
| O | Set the offset from your last shots (fire on the clicks first) |
| - and = | Width of the on-beat window |
| C | Metronome click over the song |
| H | Hide the help |
| Esc | Free the mouse (click to capture it again) |
| Backspace | Back to the menu |

Settings are saved in `user://pulse.cfg`.

## How it works

| File | What |
| --- | --- |
| `scripts/conductor.gd` | The clock: plays the song or a generated metronome, emits the beat and the end of each beat's input window, judges inputs, Adaptive Beat Snapping. Adapted from A Race for the Sun's Conductor. |
| `scripts/beat_grid.gd` | Beat grid from osu! red timing points (copied from A Race for the Sun). |
| `scripts/song_source.gd` | Finds a song on this PC and reads its beat from its osu! map. |
| `scripts/player.gd` | Movement (floor pulse, Super Jump, double jump, dash, ground pound, landing buffer) and the pistol. |
| `scripts/enemy.gd` | DPW on the beat, with the miss rule. |
| `scripts/hud.gd` | Crosshair, Rhythm Bar, cooldowns, shot timing, settings. |
| `shaders/floor.gdshader` | The pulsing floor. |

**Songs are never in this repository or in a build.** The repository is public and the songs are commercial. `song_source.gd` reads Immortal Flame at runtime from `user://songs/immortal_flame/` or from `D:/Game Development Projects/A_Race_For_The_Sun/songs/immortal_flame/`. Web builds only have the metronome.

## Running

- `Play.bat` plays it, `Open in Godot Editor.bat` opens the editor (Godot 4.7; set `GODOT` to point elsewhere).
- Autotest: `Godot --path . -- --autotest [--song]` runs an autopilot for 13 s, prints stats and saves screenshots to `test_shots/`.

## Context

The design lives in the Obsidian vault (`VarXel/Projects/Project Pulse/`): the card, the handout, the logs. Giuseppe's original notes are in `VarXel/Games/Pulse (Project Circle, A race for the sun)/` and `VarXel/Games/Brutal FPS/`. Builds are released with the VarXel wiki's `release-build` script (see `VarXel/CLAUDE.md`).
