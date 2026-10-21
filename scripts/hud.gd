class_name Hud
extends Control
## Crosshair, the Rhythm Bar (devlog, "UI & Player Feedback": a track near the crosshair, beats moving
## right to left about 4 beats ahead, a strike zone that flashes on the beat) with the bar's kill beat
## marked, the combo feed (the Unreal PushCombo labels and colours), double jumps, ammo, speed,
## shot timing (to set the audio latency) and the settings.

const BEATS_AHEAD := 4.0
const KILL_COLOR := Color(1.0, 0.3, 0.25)

var conductor: Conductor
var player: Player
var settings: PulseSettings
var main: Node

var errors: Array = []          # last shot timing errors (s), newest last
var _feed: Array = []           # {text, color, t}
var _hitmark := 0.0
var _hitmark_color := Color.WHITE
var _pulse := 0.0
var show_help := true


func _ready() -> void:
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_IGNORE


func feed(text: String, color: Color) -> void:
	_feed.append({"text": text, "color": color, "t": 1.2})
	if _feed.size() > 6:
		_feed.pop_front()


## Only shots aimed at a beat count toward the average (shots between beats would drag it).
func on_shot(_on_beat: bool, err: float) -> void:
	if absf(err) > minf(0.15, conductor.beat_length() * 0.25):
		return
	errors.append(err)
	if errors.size() > 8:
		errors.pop_front()


func on_hit(_enemy: Node, _dmg: float, _head: bool, result: String) -> void:
	_hitmark = 1.0
	match result:
		"kill":
			_hitmark_color = KILL_COLOR
			feed("DPW KILL", KILL_COLOR)
		"short":
			_hitmark_color = Color(0.45, 0.8, 1.0)
			feed("4th beat, not enough", Color(0.45, 0.8, 1.0))
		_:
			_hitmark_color = Color(1, 1, 1)


func on_beat(_b: int) -> void:
	_pulse = 1.0


func avg_error() -> float:
	if errors.is_empty():
		return 0.0
	var s := 0.0
	for e in errors:
		s += e
	return s / errors.size()


func _process(dt: float) -> void:
	_hitmark = maxf(_hitmark - dt * 4.0, 0.0)
	_pulse = maxf(_pulse - dt * 5.0, 0.0)
	for f in _feed:
		f.t -= dt
	_feed = _feed.filter(func(f): return f.t > 0.0)
	queue_redraw()


func _draw() -> void:
	var font := ThemeDB.fallback_font
	var vs := size
	var c := vs * 0.5
	if not conductor.playing:
		main.draw_menu(self, font)
		return

	# crosshair (flashes on an on-beat action, like the Unreal OnBeatFlash) and hit marker
	var cc := Color(1, 1, 1, 0.85).lerp(Color(0.3, 1, 1), clampf(player.beat_flash / 0.35, 0.0, 1.0))
	draw_line(c + Vector2(-9, 0), c + Vector2(-3, 0), cc, 2)
	draw_line(c + Vector2(3, 0), c + Vector2(9, 0), cc, 2)
	draw_line(c + Vector2(0, -9), c + Vector2(0, -3), cc, 2)
	draw_line(c + Vector2(0, 3), c + Vector2(0, 9), cc, 2)
	if _hitmark > 0.0:
		var hc := _hitmark_color
		hc.a = _hitmark
		for d in [Vector2(1, 1), Vector2(-1, 1), Vector2(1, -1), Vector2(-1, -1)]:
			draw_line(c + d * 8, c + d * 16, hc, 3)

	_draw_rhythm_bar(c + Vector2(0, 70), font)

	# ammo pips
	var ap := Vector2(vs.x - 60, vs.y - 50)
	for i in Player.MAX_AMMO:
		var filled := i < player.ammo and not player.reloading
		draw_rect(Rect2(ap + Vector2(-i * 18, 0), Vector2(12, 26)), Color(1, 0.85, 0.4) if filled else Color(1, 1, 1, 0.15))
	if player.reloading:
		draw_string(font, ap + Vector2(-110, -12), "reloading", HORIZONTAL_ALIGNMENT_LEFT, -1, 16, Color(1, 1, 1, 0.7))
	var kunai_txt: String = ["kunai ready", "kunai thrown", "kunai stuck: E to zip", "kunai returning: E to boost"][player.kunai]
	draw_string(font, Vector2(vs.x - 260, vs.y - 70), kunai_txt, HORIZONTAL_ALIGNMENT_LEFT, -1, 15, Color(1, 1, 1, 0.7))

	# double jumps, state, speed
	for i in Player.MAX_DJ:
		var on := i < player.dj
		draw_circle(Vector2(38 + i * 26, vs.y - 92), 8, Color(0.27, 0.67, 1) if on else Color(1, 1, 1, 0.15))
	draw_string(font, Vector2(30 + Player.MAX_DJ * 26 + 10, vs.y - 86), "double jumps", HORIZONTAL_ALIGNMENT_LEFT, -1, 14, Color(1, 1, 1, 0.6))
	var st: String = Player.State.keys()[player.state].replace("_", " ").to_lower()
	draw_string(font, Vector2(30, vs.y - 56), st, HORIZONTAL_ALIGNMENT_LEFT, -1, 16, Color(1, 1, 1, 0.6))
	var sp := "%.1f m/s  (max %.1f)" % [player.hspeed(), player.max_speed()]
	if player.pulse_boost > 0.0 and player.state == Player.State.GROUNDED:
		sp += "   x1.4 PULSE"
	draw_string(font, Vector2(30, vs.y - 26), sp, HORIZONTAL_ALIGNMENT_LEFT, -1, 18, Color(1.0, 0.55, 0.25) if player.pulse_boost > 0.0 else Color(1, 1, 1, 0.8))

	# combo feed, under the rhythm bar, newest on top
	var fy := c.y + 130
	for i in range(_feed.size() - 1, -1, -1):
		var f: Dictionary = _feed[i]
		var col: Color = f.color
		col.a = clampf(f.t / 0.3, 0.0, 1.0)
		draw_string(font, Vector2(0, fy), f.text, HORIZONTAL_ALIGNMENT_CENTER, vs.x, 20, col)
		fy += 24

	# top left: song and settings
	var lines := [
		"%s   %.0f BPM   kills %d" % [conductor.song_name, conductor.bpm(), main.kills],
		"DPW: store damage through the bar, kill on beat 4.  Miss rule [T]: %s" % settings.miss_rule_name(),
		"latency offset [ / ]: %+d ms    beat window - / =: +-%d ms" % [int(settings.offset_ms), int(settings.window_ms)],
		"your shots: %+d ms on average %s   [O] set offset from them" % [int(avg_error() * 1000.0), "(late)" if avg_error() > 0.0 else "(early)"],
	]
	var y := 28.0
	for l in lines:
		draw_string(font, Vector2(20, y), l, HORIZONTAL_ALIGNMENT_LEFT, -1, 16, Color(1, 1, 1, 0.85))
		y += 22
	if show_help:
		var help := [
			"WASD move   mouse look   Space jump / double jump (3)   RMB or Shift dash   Ctrl ground pound",
			"Jump while dashing (or within 0.2 s): Kinetic Jump.   No double jumps left near the ground: Magnetic Slam",
			"LMB fire (hit an enemy or fire on the beat: double jumps back)   E or Mouse 4 Kunai: throw, then again to zip",
			"On the floor when the beat hits: x1.4 speed and a full clip.",
			"K click over the song   H hide this   Esc free the mouse   Backspace menu",
		]
		y += 8
		for l in help:
			draw_string(font, Vector2(20, y), l, HORIZONTAL_ALIGNMENT_LEFT, -1, 14, Color(1, 1, 1, 0.55))
			y += 19


func _draw_rhythm_bar(center: Vector2, font: Font) -> void:
	var w := 420.0
	var left := center.x - w * 0.5 + 30.0
	var y := center.y
	var bl := conductor.beat_length()
	var span := BEATS_AHEAD * bl
	draw_line(Vector2(left - 30, y), Vector2(left + w - 30, y), Color(1, 1, 1, 0.18), 2)
	# the on-beat window around the strike zone, measured on judged time (latency included)
	var ww := conductor.window / span * (w - 30)
	draw_rect(Rect2(left - ww, y - 12, ww * 2, 24), Color(0.45, 0.8, 1.0, 0.12))
	var t := conductor.input_time()
	var bf := conductor.beat_float(t)
	var nearest := int(round(bf))
	var zone := Color(1, 0.55, 0.25)
	if conductor.is_kill_beat(nearest) and absf(bf - nearest) < 0.5:
		zone = KILL_COLOR
	zone = zone.lerp(Color.WHITE, _pulse)
	draw_arc(Vector2(left, y), 13 + 5 * _pulse, 0, TAU, 32, zone, 2 + 2 * _pulse)
	var b0 := int(floor(bf))
	for k in range(0, int(BEATS_AHEAD) + 2):
		var b := b0 + k
		var dt := conductor.time_of_beat(b) - t
		if dt < -bl * 0.5 or dt > span:
			continue
		var x := left + dt / span * (w - 30)
		var a := clampf(1.0 - dt / span, 0.25, 1.0)
		if conductor.is_kill_beat(b):
			var kc := KILL_COLOR
			kc.a = a
			draw_circle(Vector2(x, y), 9.0, kc)
		else:
			draw_circle(Vector2(x, y), 5.0, Color(1, 1, 1, a))
	# where we are in the bar: 1 2 3 4
	var cur := int(floor(bf + 0.001))
	var in_bar := conductor.beat_in_bar(cur)
	var meter := conductor.meter_at_beat(cur)
	for i in meter:
		var col := Color(1, 1, 1, 0.9) if i + 1 == in_bar else Color(1, 1, 1, 0.25)
		if i + 1 == meter:
			col = KILL_COLOR if i + 1 == in_bar else Color(KILL_COLOR, 0.4)
		draw_string(font, Vector2(center.x - meter * 11 + i * 22, y + 36), str(i + 1), HORIZONTAL_ALIGNMENT_CENTER, 22, 18, col)
