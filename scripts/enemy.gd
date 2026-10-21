class_name Enemy
extends Node3D
## DPW on the beat. Brutal FPS, section 1: enemies do not die from staggered damage, only from a set
## amount of damage within one brief window. Pulse version, in Giuseppe's words (2026-10-21): "you can
## cumulate damage on an enemy, then when the beat hits if you hit them they take that full damage,
## otherwise they heal back to full or smh. Or maybe they still take damage idk."
## So: every hit is stored. A hit inside the on-beat window cashes the store in: if it reaches the
## threshold the enemy dies. When a beat's window closes and the enemy is alive, the miss rule
## (a setting, T in game) decides what happens to the store: heal to full, keep it, or lose half.
## Moving one step on every beat is a placeholder, not a design decision.

signal died(enemy: Enemy)

var threshold := 3
var big := false
var stored := 0
var alive := true
var arena_half := 30.0
var conductor: Conductor

var _body: MeshInstance3D
var _mat: StandardMaterial3D
var _label: Label3D
var _ring: MeshInstance3D
var _ring_mat: StandardMaterial3D
var _flash := 0.0
var _flash_color := Color.WHITE
var _target := Vector3.ZERO
var _respawn_beats := 0
var _hover := 0.0


func setup(is_big: bool, c: Conductor, half: float) -> void:
	big = is_big
	conductor = c
	arena_half = half
	threshold = 6 if big else 3
	var s := 1.5 if big else 1.0
	_hover = 0.4 * s

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

	_add_hitbox("body", CapsuleShape3D.new(), Vector3(0, 0.75 * s, 0), s)
	_add_hitbox("head", SphereShape3D.new(), Vector3(0, 1.75 * s, 0), s)

	_ring_mat = StandardMaterial3D.new()
	_ring_mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	_ring_mat.albedo_color = Color(1, 1, 1)
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
	_label.font_size = 48
	_label.pixel_size = 0.012
	_label.outline_size = 10
	_label.position.y = 2.4 * s
	add_child(_label)
	_target = position
	_refresh()


func _add_hitbox(part: String, shape: Shape3D, at: Vector3, s: float) -> void:
	var body := StaticBody3D.new()
	body.collision_layer = 2
	body.collision_mask = 0
	body.set_meta("enemy", self)
	body.set_meta("part", part)
	var cs := CollisionShape3D.new()
	if shape is CapsuleShape3D:
		shape.radius = 0.45 * s
		shape.height = 1.5 * s
	elif shape is SphereShape3D:
		shape.radius = 0.3 * s
	cs.shape = shape
	cs.position = at
	body.add_child(cs)
	add_child(body)


## Returns what happened, for the HUD: "stored", "short" (on beat but not enough) or "kill".
func take_hit(dmg: int, on_beat: bool) -> String:
	if not alive:
		return "dead"
	stored += dmg
	if on_beat:
		if stored >= threshold:
			_die()
			return "kill"
		_flash_now(Color(0.4, 0.7, 1.0))
		_refresh()
		return "short"
	_flash_now(Color(1, 1, 1))
	_refresh()
	return "stored"


## A beat's window closed with the enemy still alive: the miss rule.
func on_window_closed(rule: int) -> void:
	if not alive or stored == 0:
		return
	match rule:
		PulseSettings.MissRule.HEAL:
			stored = 0
		PulseSettings.MissRule.HALVE:
			stored = stored / 2
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
	_body.position.y = 0.75 * (1.5 if big else 1.0) + _hover * (1.0 - conductor.phase()) * 0.3


func _flash_now(c: Color) -> void:
	_flash = 1.0
	_flash_color = c


func _refresh() -> void:
	_label.text = "%d / %d" % [stored, threshold]
	var ready := stored + Player.DMG_BODY >= threshold
	_label.modulate = Color(1.0, 0.85, 0.2) if ready else Color(1, 1, 1)
	var f := clampf(float(stored) / threshold, 0.0, 1.0)
	_ring_mat.albedo_color = Color(1.0, 0.85, 0.2) if ready else Color(0.3 + 0.7 * f, 0.3 + 0.7 * f, 0.35 + 0.65 * f)
	_ring.scale = Vector3.ONE * (0.6 + 0.8 * f)


func _die() -> void:
	alive = false
	stored = 0
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
