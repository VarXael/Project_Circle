# Project Pulse

One arena where the world keeps the beat, not the player. The player is a port of the Unreal Pulse controller (`NoRhythmVersion`, May 2026): quick acceleration, three double jumps, an unlimited dash, the Kinetic Jump, Magnetic Slam, ground pound into a dash, and the Kunai with its Zip Strike. The floor pulses on the beat: standing on it gives 1.4x max speed and a full clip. Enemies use DPW on the bar: every hit through the bar is stored, and a hit on the 4th beat cashes it in; if the store reaches the enemy's threshold, it dies.

This is the Godot slice of Project Pulse (branch `godot-prototype`). The original game, Project Circle, is in Unreal on the other branches of this repository.

## Controls

| Key | Action |
| --- | --- |
| 1 / 2 / 3 | Menu: metronome 110 BPM, metronome 140 BPM, Immortal Flame (only if it is on this PC) |
| WASD, mouse | Move, look |
| Space | Jump; in the air, double jump (3). With none left near the ground: Magnetic Slam |
| RMB or Shift | Dash (also in the air). Jump during a dash, or within 0.2 s of it: Kinetic Jump |
| Ctrl | Ground pound; holding WASD on landing turns it into a dash |
| LMB | Fire (6 bullets). Hitting an enemy or firing on the beat gives the double jumps back; a headshot refills the clip |
| E or Mouse 4 | Kunai: throw it; press again while it is stuck to zip to it (wall: bounce, floor or enemy: slide), or while it returns for a recall boost |
| R | Reload |
| T | DPW miss rule after the 4th beat: heal to full, keep the damage, lose half |
| [ and ] | Audio latency offset, 5 ms steps (Shift: 20 ms) |
| O | Set the offset from your last shots (fire on the clicks first) |
| - and = | Width of the on-beat window (default ±160 ms, as in Unreal) |
| K | Metronome click over the song |
| H | Hide the help |
| Esc | Free the mouse (click to capture it again) |
| Backspace | Back to the menu |

The Unreal input context binds Space, LMB, RMB, Shift, Ctrl and Mouse 4, but which key went to which action could not be read from the asset: the pairing above is a guess.

Settings are saved in `user://pulse.cfg`.

## How it works

| File | What |
| --- | --- |
| `scripts/conductor.gd` | The clock: plays the song or a generated metronome, emits the beat and the end of each beat's input window, judges inputs, Adaptive Beat Snapping. Adapted from A Race for the Sun's Conductor. |
| `scripts/beat_grid.gd` | Beat grid from osu! red timing points (copied from A Race for the Sun). |
| `scripts/song_source.gd` | Finds a song on this PC and reads its beat from its osu! map. |
| `scripts/player.gd` | Port of `UPcQPlayerMovementComponent`, `APcQPlayerCharacter` and the `PlayerMovementConfig` asset: movement, pistol, Kunai, camera. |
| `scripts/enemy.gd` | DPW on the bar (kill on the 4th beat), with the miss rule. |
| `scripts/hud.gd` | Crosshair, Rhythm Bar, cooldowns, shot timing, settings. |
| `shaders/floor.gdshader` | The pulsing floor. |

**Songs are never in this repository or in a build.** The repository is public and the songs are commercial. `song_source.gd` reads Immortal Flame at runtime from `user://songs/immortal_flame/` or from `D:/Game Development Projects/A_Race_For_The_Sun/songs/immortal_flame/`. Web builds only have the metronome.

## Running

- `Play.bat` plays it, `Open in Godot Editor.bat` opens the editor (Godot 4.7; set `GODOT` to point elsewhere).
- Autotest: `Godot --path . -- --autotest [--song]` runs an autopilot for 13 s, prints stats and saves screenshots to `test_shots/`.

## Context

The design lives in the Obsidian vault (`VarXel/Projects/Project Pulse/`): the card, the handout, the logs. Giuseppe's original notes are in `VarXel/Games/Pulse (Project Circle, A race for the sun)/` and `VarXel/Games/Brutal FPS/`. Builds are released with the VarXel wiki's `release-build` script (see `VarXel/CLAUDE.md`).
