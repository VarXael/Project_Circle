extends Node
## Project Pulse, slice: one arena. The floor pulses on the beat; the player is a port of the Unreal
## Pulse controller (player.gd); enemies use DPW on the bar (enemy.gd). Builds the whole scene in code.
## Autotest: `-- --autotest [--song]` plays the metronome (or the song, muted) with an autopilot, saves shots to test_shots/ and quits.

const ARENA_HALF := 45.0
const WALL_HEIGHT := 30.0

var conductor: Conductor
var player: Player
var hud: Hud
var settings := PulseSettings.new()
var enemies: Array = []
var kills := 0
var song: SongSource
var floor_mat: ShaderMaterial

var _pulse := 0.0
var _auto := false
var _auto_t := 0.0
var _auto_phase := 0.0
var _shot_i := 0
var _stat_t := 0.0
const _SHOTS := [4.0, 7.5, 11.0]


func _ready() -> void:
	settings.load_cfg()
	_setup_input()
	conductor = Conductor.new()
	add_child(conductor)
	conductor.beat.connect(_on_beat)
	conductor.window_closed.connect(_on_window_closed)
	_apply_settings()
	conductor.setup_metronome(110.0)
	song = SongSource.find(SongSource.IMMORTAL_FLAME)
	_build_world()

	hud = Hud.new()
	hud.conductor = conductor
	hud.player = player
	hud.settings = settings
	hud.main = self
	var layer := CanvasLayer.new()
	add_child(layer)
	layer.add_child(hud)
	player.combo.connect(hud.feed)
	player.shot.connect(hud.on_shot)
	player.hit.connect(hud.on_hit)

	_auto = OS.get_cmdline_user_args().has("--autotest")
	if _auto:
		player.autopilot = true
		var with_song := OS.get_cmdline_user_args().has("--song")
		_start(2 if with_song and song else 0)
		conductor.player.volume_db = -80.0


func _setup_input() -> void:
	var keys := {"forward": [KEY_W, KEY_UP], "back": [KEY_S, KEY_DOWN], "left": [KEY_A, KEY_LEFT],
		"right": [KEY_D, KEY_RIGHT], "jump": [KEY_SPACE], "dash": [KEY_SHIFT], "reload": [KEY_R],
		"ground_pound": [KEY_CTRL], "melee": [KEY_E]}
	for action in keys:
		if not InputMap.has_action(action):
			InputMap.add_action(action)
		for k in keys[action]:
			var ev := InputEventKey.new()
			ev.physical_keycode = k
			InputMap.action_add_event(action, ev)
	# The Unreal input context binds Space, LMB, RMB, Shift, Ctrl and Mouse 4; which key went to which
	# action could not be read from the asset, so this pairing is a guess (RMB dash is from the commits).
	for pair in [["fire", MOUSE_BUTTON_LEFT], ["dash", MOUSE_BUTTON_RIGHT], ["melee", MOUSE_BUTTON_XBUTTON1]]:
		if not InputMap.has_action(pair[0]):
			InputMap.add_action(pair[0])
		var mb := InputEventMouseButton.new()
		mb.button_index = pair[1]
		InputMap.action_add_event(pair[0], mb)


func _apply_settings() -> void:
	conductor.offset = settings.offset_ms / 1000.0
	conductor.window = settings.window_ms / 1000.0
	conductor.click_over_song = settings.song_click


# --- World --------------------------------------------------------------------------------------

func _build_world() -> void:
	var env := Environment.new()
	env.background_mode = Environment.BG_COLOR
	env.background_color = Color(0.02, 0.02, 0.035)
	env.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env.ambient_light_color = Color(0.35, 0.37, 0.45)
	env.ambient_light_energy = 1.0
	env.glow_enabled = true
	env.glow_intensity = 0.9
	env.glow_bloom = 0.15
	env.fog_enabled = true
	env.fog_light_color = Color(0.05, 0.05, 0.08)
	env.fog_density = 0.012
	var we := WorldEnvironment.new()
	we.environment = env
	add_child(we)
	var sun := DirectionalLight3D.new()
	sun.rotation_degrees = Vector3(-55, 35, 0)
	sun.light_energy = 0.7
	sun.light_color = Color(0.8, 0.82, 1.0)
	add_child(sun)

	floor_mat = ShaderMaterial.new()
	floor_mat.shader = load("res://shaders/floor.gdshader")
	var wall_mat := StandardMaterial3D.new()
	wall_mat.albedo_color = Color(0.2, 0.2, 0.25)

	var s := ARENA_HALF * 2.0
	_box(Vector3(0, -0.5, 0), Vector3(s, 1, s), floor_mat)
	for i in 4:
		var horizontal := i < 2
		var sign := 1.0 if i % 2 == 0 else -1.0
		var pos := Vector3(0, WALL_HEIGHT * 0.5, sign * (ARENA_HALF + 0.5)) if horizontal else Vector3(sign * (ARENA_HALF + 0.5), WALL_HEIGHT * 0.5, 0)
		var size := Vector3(s + 2, WALL_HEIGHT, 1) if horizontal else Vector3(1, WALL_HEIGHT, s + 2)
		_box(pos, size, wall_mat)
	# platforms to jump onto (they pulse too) and walls to zip to and bounce off
	_box(Vector3(16, 1.5, 16), Vector3(8, 3, 8), floor_mat)
	_box(Vector3(-16, 1.5, -16), Vector3(8, 3, 8), floor_mat)
	_box(Vector3(-20, 3.0, 14), Vector3(6, 6, 6), floor_mat)
	_box(Vector3(20, 3.0, -14), Vector3(6, 6, 6), floor_mat)
	_box(Vector3(0, 6.0, 0), Vector3(4, 12, 4), wall_mat)
	_box(Vector3(-30, 8.0, -30), Vector3(3, 16, 3), wall_mat)
	_box(Vector3(30, 8.0, 30), Vector3(3, 16, 3), wall_mat)

	player = Player.new()
	player.conductor = conductor
	player.settings = settings
	player.position = Vector3(0, 0.1, 25)
	add_child(player)
	player.enemies = enemies

	for i in 6:
		var e := Enemy.new()
		add_child(e)
		e.position = Vector3(randf_range(-25, 25), 0, randf_range(-25, 10))
		e.setup(i >= 4, conductor, ARENA_HALF)
		e.died.connect(func(_en): kills += 1)
		enemies.append(e)


func _box(pos: Vector3, size: Vector3, mat: Material) -> void:
	var body := StaticBody3D.new()
	body.position = pos
	var cs := CollisionShape3D.new()
	var shape := BoxShape3D.new()
	shape.size = size
	cs.shape = shape
	body.add_child(cs)
	var mi := MeshInstance3D.new()
	var mesh := BoxMesh.new()
	mesh.size = size
	mi.mesh = mesh
	mi.material_override = mat
	body.add_child(mi)
	add_child(body)


# --- Flow ---------------------------------------------------------------------------------------

func _start(choice: int) -> void:
	match choice:
		0:
			conductor.setup_metronome(110.0)
		1:
			conductor.setup_metronome(140.0)
		2:
			if song == null:
				return
			var stream := song.load_stream()
			if stream == null:
				hud.feed("could not load the song", Color(1, 0.3, 0.3))
				return
			conductor.setup_song(song.sections, stream, song.title)
	kills = 0
	hud.errors.clear()
	conductor.start()
	if not _auto:
		Input.mouse_mode = Input.MOUSE_MODE_CAPTURED


func _to_menu() -> void:
	conductor.stop()
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE


func _on_beat(b: int) -> void:
	_pulse = 1.0
	player.on_pulse(b)
	hud.on_beat(b)
	for e in enemies:
		e.on_beat(b)
	if _auto:
		_auto_fire()


func _on_window_closed(b: int) -> void:
	for e in enemies:
		e.on_window_closed(b, settings.miss_rule)


## A shot's line, for a moment: cyan on the beat, red off it (as the Unreal debug lines).
func tracer(from: Vector3, to: Vector3, on_beat: bool) -> void:
	var mesh := ImmediateMesh.new()
	var mat := StandardMaterial3D.new()
	mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	mat.albedo_color = Color(0.2, 1, 1) if on_beat else Color(1, 0.25, 0.2)
	mesh.surface_begin(Mesh.PRIMITIVE_LINES, mat)
	mesh.surface_add_vertex(from)
	mesh.surface_add_vertex(to)
	mesh.surface_end()
	var mi := MeshInstance3D.new()
	mi.mesh = mesh
	mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(mi)
	get_tree().create_timer(0.4 if on_beat else 0.1).timeout.connect(mi.queue_free)


func _unhandled_input(e: InputEvent) -> void:
	if e is InputEventMouseButton and e.pressed and conductor.playing and Input.mouse_mode != Input.MOUSE_MODE_CAPTURED:
		Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
		get_viewport().set_input_as_handled()
		return
	if not (e is InputEventKey and e.pressed and not e.echo):
		return
	var k: int = e.physical_keycode
	if not conductor.playing:
		if k == KEY_1:
			_start(0)
		elif k == KEY_2:
			_start(1)
		elif k == KEY_3:
			_start(2)
		elif k == KEY_ESCAPE:
			get_tree().quit()
		return
	var step := 20.0 if e.shift_pressed else 5.0
	match k:
		KEY_ESCAPE:
			Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
		KEY_BACKSPACE:
			_to_menu()
		KEY_BRACKETLEFT:
			settings.offset_ms = clampf(settings.offset_ms - step, -300.0, 300.0)
		KEY_BRACKETRIGHT:
			settings.offset_ms = clampf(settings.offset_ms + step, -300.0, 300.0)
		KEY_MINUS:
			settings.window_ms = clampf(settings.window_ms - 10.0, 30.0, 250.0)
		KEY_EQUAL:
			settings.window_ms = clampf(settings.window_ms + 10.0, 30.0, 250.0)
		KEY_T:
			settings.miss_rule = (settings.miss_rule + 1) % PulseSettings.MISS_RULE_NAMES.size()
			hud.feed("miss rule: " + settings.miss_rule_name(), Color(0.75, 0.9, 0.75))
		KEY_O:
			if hud.errors.size() >= 4:
				settings.offset_ms = clampf(settings.offset_ms + hud.avg_error() * 1000.0, -300.0, 300.0)
				hud.errors.clear()
				hud.feed("offset set to %+d ms" % int(settings.offset_ms), Color(0.75, 0.9, 0.75))
			else:
				hud.feed("fire 4+ shots on the clicks first", Color(0.75, 0.9, 0.75))
		KEY_K:
			settings.song_click = not settings.song_click
		KEY_H:
			hud.show_help = not hud.show_help
		_:
			return
	_apply_settings()
	settings.save_cfg()


func _process(dt: float) -> void:
	_pulse = maxf(_pulse - dt * 4.0, 0.0)
	floor_mat.set_shader_parameter("pulse", _pulse)
	floor_mat.set_shader_parameter("phase", conductor.phase() if conductor.playing else 0.0)
	floor_mat.set_shader_parameter("origin", player.global_position)
	if _auto:
		_autotest(dt)


# --- Menu ---------------------------------------------------------------------------------------

func draw_menu(c: Control, font: Font) -> void:
	var vs := c.size
	c.draw_rect(Rect2(Vector2.ZERO, vs), Color(0, 0, 0, 0.55))
	var y := vs.y * 0.3
	c.draw_string(font, Vector2(0, y), "PROJECT PULSE", HORIZONTAL_ALIGNMENT_CENTER, vs.x, 48, Color(1, 0.55, 0.25))
	y += 40
	c.draw_string(font, Vector2(0, y), "The world keeps the beat. Acting on it is your choice.", HORIZONTAL_ALIGNMENT_CENTER, vs.x, 18, Color(1, 1, 1, 0.7))
	y += 60
	var options := ["[1]  Metronome, 110 BPM", "[2]  Metronome, 140 BPM"]
	if song:
		options.append("[3]  %s (%.0f BPM)" % [song.title, song.bpm()])
	else:
		options.append("[3]  Immortal Flame: not found on this PC")
	for i in options.size():
		var col := Color(1, 1, 1, 0.35) if i == 2 and song == null else Color(1, 1, 1, 0.95)
		c.draw_string(font, Vector2(0, y), options[i], HORIZONTAL_ALIGNMENT_CENTER, vs.x, 24, col)
		y += 36
	y += 20
	c.draw_string(font, Vector2(0, y), "Latency offset %+d ms. Set it in game: fire on the clicks, then press O." % int(settings.offset_ms), HORIZONTAL_ALIGNMENT_CENTER, vs.x, 16, Color(1, 1, 1, 0.6))


# --- Autotest -----------------------------------------------------------------------------------

func _nearest_enemy() -> Enemy:
	var best: Enemy = null
	var bd := INF
	for e in enemies:
		if e.alive:
			var d: float = e.position.distance_to(player.position)
			if d < bd:
				bd = d
				best = e
	return best


func _aim_at(e: Enemy) -> void:
	player.auto_aim(e.head_center() if randf() < 0.5 else e.center())


## On every beat: fire at the nearest enemy (beats 1 to 3 store damage, beat 4 cashes it in),
## sometimes jump, dash or throw the Kunai.
func _auto_fire() -> void:
	var e := _nearest_enemy()
	if e:
		_aim_at(e)
		player.fire()
	var r := randf()
	if r < 0.3:
		player.jump_pressed()
	elif r < 0.45:
		player.enter_dash()
	elif r < 0.55:
		player.kunai_action()


func _autotest(dt: float) -> void:
	_auto_t += dt
	player.set_auto_move(Vector2(sin(_auto_t * 0.7) * 0.6, 0.6))
	var ph := conductor.phase()
	# one shot between beats stores more damage
	if _auto_phase < 0.5 and ph >= 0.5:
		var e := _nearest_enemy()
		if e:
			_aim_at(e)
			player.fire()
	_auto_phase = ph
	_stat_t += dt
	if _stat_t >= 1.0:
		_stat_t = 0.0
		print("[auto] t=%.1f song=%.2f bpm=%.0f kills=%d pulses=%d state=%s dj=%d ammo=%d speed=%.1f max=%.1f y=%.1f kunai=%d fps=%d" % [
			_auto_t, conductor.song_time, conductor.bpm(), kills, player.pulses, Player.State.keys()[player.state], player.dj,
			player.ammo, player.hspeed(), player.max_speed(), player.global_position.y, player.kunai, Engine.get_frames_per_second()])
	if _shot_i < _SHOTS.size() and _auto_t >= _SHOTS[_shot_i]:
		_shot_i += 1
		DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path("res://test_shots"))
		get_viewport().get_texture().get_image().save_png("res://test_shots/shot_%02d.png" % _shot_i)
	if _auto_t > 13.0:
		print("[auto] done: kills=%d pulses=%d song=%s" % [kills, player.pulses, str(song.title if song else "none")])
		get_tree().quit()
