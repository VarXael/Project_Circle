class_name PulseSettings
extends RefCounted
## Player settings kept between runs, in user://pulse.cfg.

const PATH := "user://pulse.cfg"

## DPW miss rule: what the damage stored on an enemy does when a beat passes without a kill.
## Giuseppe (2026-10-21): "otherwise they heal back to full or smh. Or maybe they still take damage
## idk. I genuinely have to test this". So it is a setting, switched in game with T.
enum MissRule { HEAL, KEEP, HALVE }
const MISS_RULE_NAMES := ["heal to full", "keep the damage", "lose half"]

var offset_ms := 0.0        # audio latency: + means you hear the beat later than the game plays it
var window_ms := 160.0      # half width of the on-beat window (the Unreal OnBeatWindowMs)
var miss_rule := MissRule.HEAL
var sensitivity := 0.0022
var song_click := false     # metronome click over the song


func load_cfg() -> void:
	var c := ConfigFile.new()
	if c.load(PATH) != OK:
		return
	offset_ms = c.get_value("audio", "offset_ms", offset_ms)
	window_ms = c.get_value("rhythm", "window", window_ms)
	miss_rule = c.get_value("rhythm", "miss_rule", miss_rule)
	sensitivity = c.get_value("input", "sensitivity", sensitivity)
	song_click = c.get_value("audio", "song_click", song_click)


func save_cfg() -> void:
	var c := ConfigFile.new()
	c.set_value("audio", "offset_ms", offset_ms)
	c.set_value("rhythm", "window", window_ms)
	c.set_value("rhythm", "miss_rule", miss_rule)
	c.set_value("input", "sensitivity", sensitivity)
	c.set_value("audio", "song_click", song_click)
	c.save(PATH)


func miss_rule_name() -> String:
	return MISS_RULE_NAMES[miss_rule]
