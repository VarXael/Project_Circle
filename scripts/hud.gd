class_name Hud
extends Control
## Crosshair, the Rhythm Bar (devlog, "UI & Player Feedback": a track near the crosshair, beats moving
## right to left about 4 beats ahead, a strike zone that flashes on the beat), the pistol, the
## cooldowns, shot timing (to set the audio latency), the settings and an event feed.

const BEATS_AHEAD := 4.0

var conductor: Conductor
var player: Player
var settings: PulseSettings
var main: Node

var errors: Array = []          # last shot timing errors (s), newest last
var _feed: Array = []           # {text, kind, t}
var _hitmark := 0.0
var _hitmark_color := Color.WHITE
var _pulse := 0.0
var show_help := true

const COLORS := {
	"pulse": Color(1.0, 0.55, 0.25), "super": Color(1.0, 0.85, 0.2), "beat": Color(0.45, 0.8, 1.0),
	"kill": Color(1.0, 0.35, 0.3), "move": Color(0.8, 0.8, 0.85), "info": Color(0.75, 0.9, 0.75),
}


func _ready() -> void:
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_IGNORE


func feed(text: String, kind: String) -> void:
	_feed.append({"text": text, "kind": kind, "t": 1.4})
	if _feed.size() > 6:
		_feed.pop_front()


## Only shots aimed at a beat count toward the average (shots between beats would drag it).
func on_shot(_on_beat: bool, err: float) -> void:
	if absf(err) > minf(0.15, conductor.beat_length() * 0.25):
		return
	errors.append(err)
	if errors.size() > 8:
		errors.pop_front()


func on_hit(_enemy: Node, _dmg: int, _head: bool, result: String) -> void:
	_hitmark = 1.0
	match result:
		"kill":
			_hitmark_color = COLORS.kill
			feed("DPW KILL", "kill")
		"short":
			_hitmark_color = COLORS.beat
			feed("on beat, not enough", "beat")
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

	# crosshair and hit marker
	var cc := Color(1, 1, 1, 0.85)
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
	for i in Player.CLIP:
		var filled := i < player.ammo and player.reload_left <= 0.0
		draw_rect(Rect2(ap + Vector2(-i * 18, 0), Vector2(12, 26)), Color(1, 0.85, 0.4) if filled else Color(1, 1, 1, 0.15))
	if player.reload_left > 0.0:
		draw_string(font, ap + Vector2(-110, -12), "reloading", HORIZONTAL_ALIGNMENT_LEFT, -1, 16, Color(1, 1, 1, 0.7))

	# cooldowns and speed
	var bl := conductor.beat_length()
	_bar(Vector2(30, vs.y - 90), "double jump", 1.0 - player.dj_cd / (Player.DJ_COOLDOWN_BEATS * bl), player.dj_used, font)
	_bar(Vector2(30, vs.y - 62), "dash", 1.0 - player.dash_cd / (Player.DASH_COOLDOWN_BEATS * bl), false, font)
	var sp := "speed %.1f m/s" % player.hspeed()
	if player.boost > 0.0:
		sp += "   x1.4 PULSE"
	draw_string(font, Vector2(30, vs.y - 24), sp, HORIZONTAL_ALIGNMENT_LEFT, -1, 18, COLORS.pulse if player.boost > 0.0 else Color(1, 1, 1, 0.8))

	# feed, under the rhythm bar, newest on top
	var fy := c.y + 120
	for i in range(_feed.size() - 1, -1, -1):
		var f: Dictionary = _feed[i]
		var col: Color = COLORS.get(f.kind, Color.WHITE)
		col.a = clampf(f.t / 0.4, 0.0, 1.0)
		draw_string(font, Vector2(0, fy), f.text, HORIZONTAL_ALIGNMENT_CENTER, vs.x, 22, col)
		fy += 26

	# top left: song and settings
	var lines := [
		"%s   %.0f BPM   kills %d" % [conductor.song_name, conductor.bpm(), main.kills],
		"DPW miss rule [T]: %s" % settings.miss_rule_name(),
		"latency offset [ / ]: %+d ms    beat window - / =: +-%d ms" % [int(settings.offset_ms), int(settings.window_ms)],
		"your shots: %+d ms on average %s   [O] set offset from them" % [int(avg_error() * 1000.0), "(late)" if avg_error() > 0.0 else "(early)"],
	]
	var y := 28.0
	for l in lines:
		draw_string(font, Vector2(20, y), l, HORIZONTAL_ALIGNMENT_LEFT, -1, 16, Color(1, 1, 1, 0.85))
		y += 22
	if show_help:
		var help := [
			"WASD move   mouse look   Space jump (on the beat while grounded: Super Jump; in the air: double jump)",
			"Shift dash (in the air: ground pound)   LMB fire (on the beat: resets dash and double jump)   R reload",
			"Standing on the floor when the beat hits: x1.4 speed and a full clip.",
			"Enemies store your hits. A hit on the beat cashes them in: enough and it dies.",
			"C click over the song   H hide this   Esc free the mouse   Backspace menu",
		]
		y += 8
		for l in help:
			draw_string(font, Vector2(20, y), l, HORIZONTAL_ALIGNMENT_LEFT, -1, 14, Color(1, 1, 1, 0.55))
			y += 19


func _bar(at: Vector2, label: String, f: float, used: bool, font: Font) -> void:
	f = clampf(f, 0.0, 1.0)
	draw_rect(Rect2(at, Vector2(140, 10)), Color(1, 1, 1, 0.12))
	var col := Color(0.45, 0.8, 1.0) if f >= 1.0 and not used else Color(1, 1, 1, 0.4)
	draw_rect(Rect2(at, Vector2(140 * f, 10)), col)
	draw_string(font, at + Vector2(150, 10), label + (" (used)" if used else ""), HORIZONTAL_ALIGNMENT_LEFT, -1, 14, Color(1, 1, 1, 0.7))


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
	var zone := Color(1, 0.55, 0.25).lerp(Color.WHITE, _pulse)
	draw_arc(Vector2(left, y), 13 + 5 * _pulse, 0, TAU, 32, zone, 2 + 2 * _pulse)
	var t := conductor.input_time()
	var b0 := int(floor(conductor.beat_float(t)))
	for k in range(0, int(BEATS_AHEAD) + 2):
		var b := b0 + k
		var dt := conductor.time_of_beat(b) - t
		if dt < -bl * 0.5 or dt > span:
			continue
		var x := left + dt / span * (w - 30)
		var a := clampf(1.0 - dt / span, 0.2, 1.0)
		var meter := int(conductor.grid.section(conductor.time_of_beat(b)).get("meter", 4))
		var down := b % maxi(meter, 1) == 0
		draw_circle(Vector2(x, y), 7.0 if down else 5.0, Color(1, 1, 1, a))
