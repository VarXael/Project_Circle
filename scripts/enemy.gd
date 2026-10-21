class_name Enemy
extends Node3D
## DPW on the bar. Brutal FPS, section 1: enemies do not die from staggered damage, only from a set
## amount of damage within one window. Pulse version, Giuseppe (2026-10-21, after v1): "id maybe work
## around 4/4 instead of each beat, player can accumulate damage on enemies then do something and kill
## them on the 4th beat. So there is more time to overall understand whats going on".
## So: every hit through the bar is stored. A hit inside the on-beat window of the 4th beat cashes the
## store in: if it reaches the threshold, the enemy dies. When the 4th beat's window closes and the enemy
## is alive, the miss rule (a setting, T in game) decides what happens to the store: heal to full,
## keep it, or lose half. Damage numbers are the Unreal ones (pistol 25, head x3, on the beat x2,
## Kunai 45). Moving one step on every beat is a placeholder, not a design decision.

signal died(enemy: Enemy)

var threshold := 100.0
var big := false
var stored := 0.0
var alive := true
var arena_half := 30.0
var conductor: Conductor

var _s := 1.0
var _body: MeshInstance3D
var _mat: StandardMaterial3D
var _label: Label3D
var _ring: MeshInstance3D
var _ring_mat: StandardMaterial3D
var _flash := 0.0
var _flash_color := Color.WHITE
var _target := Vector3.ZERO
var _respawn_beats := 0


func setup(is_big: bool, c: Conductor, half: float) -> void:
	big = is_big
	conductor = c
	arena_half = half
	threshold = 250.0 if big else 100.0
	_s = 1.5 if big else 1.0
	var s := _s

	_mat = StandardMaterial3D.new()
	_mat.albedo_color = Color(0.85, 0.2, 0.25) if big else Color(0.9, 0.45, 0.2)
	_mat.emission_enabled = true
	_mat.emission = Color.BLACK
	_body = MeshInstance3D.new()
	var cap := CapsuleMesh.new()
	cap.radius = 0.45 * s
	cap.height = 1.5 * s
	_body.mesh = cap
	_body.material_override = _mat
	_body.position.y = 0.75 * s
	add_child(_body)
	var head_mesh := MeshInstance3D.new()
	var sph := SphereMesh.new()
	sph.radius = 0.28 * s
	sph.height = 0.56 * s
	head_mesh.mesh = sph
	head_mesh.material_override = _mat
	head_mesh.position.y = 1.75 * s
	add_child(head_mesh)

	# collision for the Kunai (the pistol tests the shapes itself, with its 25 cm bullet)
	var body := StaticBody3D.new()
	body.collision_layer = 2
	body.collision_mask = 0
	body.set_meta("enemy", self)
	var cs := CollisionShape3D.new()
	var shape := CapsuleShape3D.new()
	shape.radius = 0.45 * s
	shape.height = 2.1 * s
	cs.shape = shape
	cs.position = Vector3(0, 1.05 * s, 0)
	body.add_child(cs)
	add_child(body)

	_ring_mat = StandardMaterial3D.new()
	_ring_mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	_ring = MeshInstance3D.new()
	var torus := TorusMesh.new()
	torus.inner_radius = 0.6 * s
	torus.outer_radius = 0.7 * s
	_ring.mesh = torus
	_ring.material_override = _ring_mat
	_ring.position.y = 0.05
	add_child(_ring)

	_label = Label3D.new()
	_label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	_label.no_depth_test = true
	_label.font_size = 40
	_label.fixed_size = true
	_label.pixel_size = 0.0009
	_label.outline_size = 10
	_label.position.y = 2.4 * s
	add_child(_label)
	_target = position
	_refresh()


func center() -> Vector3:
	return global_position + Vector3.UP * 0.9 * _s


func radius() -> float:
	return 0.45 * _s


func head_center() -> Vector3:
	return global_position + Vector3.UP * 1.75 * _s


func head_radius() -> float:
	return 0.28 * _s


func body_bottom() -> Vector3:
	return global_position + Vector3.UP * 0.3 * _s


func body_top() -> Vector3:
	return global_position + Vector3.UP * 1.3 * _s


## j is the conductor's judgement of the hit. Returns "stored", "short" (on the kill beat but not
## enough) or "kill".
func take_hit(dmg: float, j: Dictionary) -> String:
	if not alive:
		return "dead"
	stored += dmg
	if j.on and conductor.is_kill_beat(j.beat):
		if stored >= threshold:
			_die()
			return "kill"
		_flash_now(Color(0.4, 0.7, 1.0))
		_refresh()
		return "short"
	_flash_now(Color(1, 1, 1))
	_refresh()
	return "stored"


## A beat's window closed. After the kill beat, an enemy still alive gets the miss rule.
func on_window_closed(b: int, rule: int) -> void:
	if not alive or stored == 0.0 or not conductor.is_kill_beat(b):
		return
	match rule:
		PulseSettings.MissRule.HEAL:
			stored = 0.0
		PulseSettings.MissRule.HALVE:
			stored = floorf(stored / 2.0)
	_refresh()


func on_beat(_b: int) -> void:
	if not alive:
		_respawn_beats -= 1
		if _respawn_beats <= 0:
			_respawn()
		return
	if position.distance_to(_target) < 0.5 or randf() < 0.1:
		_target = Vector3(randf_range(-arena_half, arena_half) * 0.8, 0, randf_range(-arena_half, arena_half) * 0.8)
	var step := (_target - position)
	step.y = 0.0
	if step.length() > 1.5:
		step = step.normalized() * 1.5
	var tw := create_tween()
	tw.tween_property(self, "position", position + step, minf(0.15, conductor.beat_length() * 0.4)).set_trans(Tween.TRANS_QUAD).set_ease(Tween.EASE_OUT)


func _process(dt: float) -> void:
	if not alive:
		return
	_flash = maxf(_flash - dt * 5.0, 0.0)
	_mat.emission = _flash_color * _flash * 2.0
	_refresh_ring()


func _flash_now(c: Color) -> void:
	_flash = 1.0
	_flash_color = c


## Ready: one on-beat body shot (25 x 2) on the 4th beat would kill it.
func is_ready() -> bool:
	return stored + 50.0 >= threshold


func _refresh() -> void:
	_label.text = "%d / %d" % [int(stored), int(threshold)]
	_label.modulate = Color(1.0, 0.85, 0.2) if is_ready() else Color(1, 1, 1)
	_refresh_ring()


func _refresh_ring() -> void:
	var f := clampf(stored / threshold, 0.0, 1.0)
	var c := Color(1.0, 0.85, 0.2) if is_ready() else Color(0.3 + 0.7 * f, 0.3 + 0.7 * f, 0.35 + 0.65 * f)
	# on the kill beat the ring flashes
	var bf := conductor.beat_float() if conductor.playing else 0.0
	var next_kill := conductor.is_kill_beat(int(round(bf))) if conductor.playing else false
	if next_kill:
		var near := 1.0 - clampf(absf(bf - roundf(bf)) * 4.0, 0.0, 1.0)
		c = c.lerp(Color(1, 0.3, 0.25), near * 0.6)
	_ring_mat.albedo_color = c
	_ring.scale = Vector3.ONE * (0.6 + 0.8 * f)


func _die() -> void:
	alive = false
	stored = 0.0
	died.emit(self)
	_respawn_beats = 8
	for c in get_children():
		if c is StaticBody3D:
			c.process_mode = Node.PROCESS_MODE_DISABLED
	var tw := create_tween()
	_mat.emission = Color(1.0, 0.8, 0.3) * 3.0
	tw.tween_property(self, "scale", Vector3(1.6, 0.05, 1.6), 0.12)
	tw.tween_callback(func(): visible = false)


func _respawn() -> void:
	alive = true
	visible = true
	scale = Vector3.ONE
	position = Vector3(randf_range(-arena_half, arena_half) * 0.8, 0, randf_range(-arena_half, arena_half) * 0.8)
	_target = position
	for c in get_children():
		if c is StaticBody3D:
			c.process_mode = Node.PROCESS_MODE_INHERIT
	_refresh()
