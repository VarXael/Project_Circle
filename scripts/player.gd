class_name Player
extends CharacterBody3D
## The Pulse movement and the pistol, from Giuseppe's devlogs (9, 10 and 16 May 2026):
## - The floor is the metronome: standing on it when the beat hits gives a 1.4x max speed boost and
##   refills the pistol (10 May). Dashing makes you immune to the pulse.
## - Jump on the beat while grounded: Super Jump (big horizontal boost, synced arc). Off the beat: normal jump.
## - Jump in the air: double jump. Off the beat it costs the cooldown; on the beat it bypasses and resets it.
## - Firing on the beat resets the mobility cooldowns, double jump included (open question 4, answered 2026-10-21).
## - Dash: ground dash; in the air it is a ground pound, which turns into a dash on landing if WASD is held.
## - Jump, double jump and dash times use Adaptive Beat Snapping; gravity is recomputed from the apex
##   height and the snapped air time, so the landing falls on the music.
## - Landing buffer (Reverse Coyote Time): a jump or dash pressed while the ground is within 1.2 m
##   (the 120 units of the devlog) is held and runs the frame you land.
## Every number below is a first guess for the slice.

signal event(text: String, kind: String)                       # for the HUD feed
signal shot(on_beat: bool, err: float)
signal hit(enemy: Node, damage: int, headshot: bool, result: String)

# Movement
const MAX_SPEED := 9.0
const BASE_BPM := 110.0             # BPM Speed Scaling: max speed scales with the song's BPM
const GROUND_ACCEL := 12.0
const FRICTION := 7.0
const AIR_ACCEL := 5.0
const PULSE_SPEED_MULT := 1.4
const JUMP_APEX := 1.6
const JUMP_IDEAL := 0.65            # open question 3: the notes say 0.5, 0.65 or 0.85 s
const SUPER_APEX := 3.2
const SUPER_IDEAL := 1.1
const SUPER_BOOST := 9.0
const DJ_APEX := 1.2
const DJ_IDEAL := 0.5
const DJ_COOLDOWN_BEATS := 4.0
const DASH_SPEED := 18.0
const DASH_IDEAL := 0.3
const DASH_COOLDOWN_BEATS := 3.0
const POUND_SPEED := 32.0
const BUFFER_DIST := 1.2
const BUFFER_TIME := 0.35
const MAX_HSPEED := 24.0
# Pistol (16 May: 6 bullets, reload takes time, a ground pulse refills it)
const CLIP := 6
const RELOAD_TIME := 1.0
const FIRE_GAP := 0.1
const RANGE := 200.0
const DMG_BODY := 1
const DMG_HEAD := 2

var conductor: Conductor
var settings: PulseSettings
var camera: Camera3D
var autopilot := false

var gravity := 20.0
var dj_used := false
var dj_cd := 0.0
var dash_cd := 0.0
var dashing := 0.0
var dash_dir := Vector3.ZERO
var pounding := false
var boost := 0.0                    # time left on the pulse speed boost
var ammo := CLIP
var reload_left := 0.0
var fire_gap := 0.0
var last_pulse_time := -100.0
var pulses := 0
var supers := 0

var _buffer := ""
var _buffer_left := 0.0
var _buffer_on := false
var _was_floor := true
var _pitch := 0.0
var _move := Vector2.ZERO          # autopilot input


func _ready() -> void:
	var shape := CollisionShape3D.new()
	var cap := CapsuleShape3D.new()
	cap.radius = 0.4
	cap.height = 1.8
	shape.shape = cap
	shape.position.y = 0.9
	add_child(shape)
	camera = Camera3D.new()
	camera.position.y = 1.6
	camera.fov = 95.0
	camera.near = 0.05
	add_child(camera)
	collision_layer = 1
	collision_mask = 1
	floor_snap_length = 0.3
	_set_arc(JUMP_APEX, JUMP_IDEAL)


func _unhandled_input(e: InputEvent) -> void:
	if e is InputEventMouseMotion and Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
		rotate_y(-e.relative.x * settings.sensitivity)
		_pitch = clampf(_pitch - e.relative.y * settings.sensitivity, -1.5, 1.5)
		camera.rotation.x = _pitch
	if not conductor.playing:
		return
	if e.is_action_pressed("jump"):
		jump_pressed()
	elif e.is_action_pressed("dash"):
		dash_pressed()
	elif e.is_action_pressed("fire") and Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
		fire()
	elif e.is_action_pressed("reload"):
		_start_reload()


func _physics_process(dt: float) -> void:
	dj_cd = maxf(dj_cd - dt, 0.0)
	dash_cd = maxf(dash_cd - dt, 0.0)
	boost = maxf(boost - dt, 0.0)
	fire_gap = maxf(fire_gap - dt, 0.0)
	if reload_left > 0.0:
		reload_left -= dt
		if reload_left <= 0.0:
			ammo = CLIP
	if _buffer_left > 0.0:
		_buffer_left -= dt
		if _buffer_left <= 0.0:
			_buffer = ""

	var wish := wish_dir()
	var h := Vector3(velocity.x, 0.0, velocity.z)
	var top := max_speed()
	if dashing > 0.0:
		dashing -= dt
		h = dash_dir * maxf(h.length(), DASH_SPEED)
	elif is_on_floor():
		var sp := h.length()
		if sp > 0.0:
			var drop := maxf(sp, 2.0) * FRICTION * dt
			# above the top speed, friction only brings you back down to it
			if sp > top:
				drop = minf(drop, sp - top) if wish != Vector3.ZERO else drop
			h *= maxf(sp - drop, 0.0) / sp
		h = _accelerate(h, wish, top, GROUND_ACCEL, dt)
	else:
		h = _accelerate(h, wish, maxf(top, h.length()), AIR_ACCEL, dt)
	if h.length() > MAX_HSPEED:
		h = h.normalized() * MAX_HSPEED
	velocity.x = h.x
	velocity.z = h.z
	if pounding:
		velocity.y = -POUND_SPEED
	elif not is_on_floor():
		velocity.y -= gravity * dt
	move_and_slide()

	var on_floor := is_on_floor()
	if on_floor and not _was_floor:
		_landed()
	_was_floor = on_floor


func _accelerate(h: Vector3, wish: Vector3, top: float, accel: float, dt: float) -> Vector3:
	if wish == Vector3.ZERO:
		return h
	var current := h.dot(wish)
	var add := minf(top - current, accel * top * dt)
	if add <= 0.0:
		return h
	return h + wish * add


func wish_dir() -> Vector3:
	var v := _move if autopilot else Input.get_vector("left", "right", "back", "forward")
	if v == Vector2.ZERO:
		return Vector3.ZERO
	var b := global_transform.basis
	var d := -b.z * v.y + b.x * v.x
	d.y = 0.0
	return d.normalized()


func max_speed() -> float:
	var scale := clampf(conductor.bpm() / BASE_BPM, 0.75, 1.6) if conductor.playing else 1.0
	return MAX_SPEED * scale * (PULSE_SPEED_MULT if boost > 0.0 else 1.0)


func hspeed() -> float:
	return Vector2(velocity.x, velocity.z).length()


# --- The floor pulse ----------------------------------------------------------------------------

func on_pulse(_b: int) -> void:
	last_pulse_time = conductor.song_time
	if is_on_floor():
		_take_pulse()


func _take_pulse() -> void:
	if dashing > 0.0:
		return
	boost = conductor.beat_length() * 1.05
	pulses += 1
	if ammo < CLIP or reload_left > 0.0:
		ammo = CLIP
		reload_left = 0.0
		event.emit("PULSE  ammo", "pulse")
	else:
		event.emit("PULSE", "pulse")


func _landed() -> void:
	dj_used = false
	_set_arc(JUMP_APEX, JUMP_IDEAL)
	# landing just after a beat still counts as standing on it
	if conductor.song_time - last_pulse_time <= conductor.window:
		_take_pulse()
	if pounding:
		pounding = false
		var wish := wish_dir()
		if wish != Vector3.ZERO:
			_ground_dash(wish, true)
		else:
			velocity.x = 0.0
			velocity.z = 0.0
	if not _buffer.is_empty():
		var action := _buffer
		var was_on := _buffer_on
		_buffer = ""
		if action == "jump":
			_ground_jump(was_on or conductor.judge().on)
		elif action == "dash":
			dash_pressed()


# --- Actions ------------------------------------------------------------------------------------

func jump_pressed() -> void:
	var j := conductor.judge()
	if is_on_floor():
		_ground_jump(j.on)
	elif _near_ground():
		_hold("jump", j.on)
	else:
		_double_jump(j.on)


func _ground_jump(on_beat: bool) -> void:
	dashing = 0.0
	if on_beat:
		var t := _set_arc(SUPER_APEX, SUPER_IDEAL)
		var dir := wish_dir()
		if dir == Vector3.ZERO:
			dir = -global_transform.basis.z
			dir.y = 0.0
			dir = dir.normalized()
		var h := Vector3(velocity.x, 0.0, velocity.z) + dir * SUPER_BOOST
		velocity.x = h.x
		velocity.z = h.z
		supers += 1
		event.emit("SUPER JUMP  %.2fs" % t, "super")
	else:
		_set_arc(JUMP_APEX, JUMP_IDEAL)
	velocity.y = _launch_speed
	_was_floor = false


func _double_jump(on_beat: bool) -> void:
	if dj_used:
		return
	if on_beat:
		dj_cd = 0.0
		event.emit("ON-BEAT DOUBLE JUMP", "beat")
	elif dj_cd > 0.0:
		return
	else:
		dj_cd = DJ_COOLDOWN_BEATS * conductor.beat_length()
	dj_used = true
	pounding = false
	_set_arc(DJ_APEX, DJ_IDEAL)
	velocity.y = _launch_speed


func dash_pressed() -> void:
	if is_on_floor():
		if dash_cd > 0.0:
			return
		var dir := wish_dir()
		if dir == Vector3.ZERO:
			dir = -global_transform.basis.z
			dir.y = 0.0
			dir = dir.normalized()
		_ground_dash(dir, false)
	elif _near_ground():
		_hold("dash", false)
	elif not pounding:
		pounding = true
		event.emit("GROUND POUND", "move")


func _ground_dash(dir: Vector3, free: bool) -> void:
	dash_dir = dir
	dashing = conductor.snapped_duration(DASH_IDEAL)
	if not free:
		dash_cd = DASH_COOLDOWN_BEATS * conductor.beat_length()


func _hold(action: String, on_beat: bool) -> void:
	_buffer = action
	_buffer_on = on_beat
	_buffer_left = BUFFER_TIME


func _near_ground() -> bool:
	if velocity.y > 0.0:
		return false
	var q := PhysicsRayQueryParameters3D.create(global_position + Vector3.UP * 0.1, global_position + Vector3.DOWN * BUFFER_DIST, 1)
	q.exclude = [get_rid()]
	return not get_world_3d().direct_space_state.intersect_ray(q).is_empty()


var _launch_speed := 0.0

## Adaptive Beat Snapping: the air time is snapped to the music, gravity follows from it.
func _set_arc(apex: float, ideal: float) -> float:
	var t := conductor.snapped_duration(ideal) if conductor else ideal
	t = maxf(t, 0.2)
	gravity = 8.0 * apex / (t * t)
	_launch_speed = 4.0 * apex / t
	return t


# --- Pistol -------------------------------------------------------------------------------------

func fire() -> void:
	if fire_gap > 0.0 or reload_left > 0.0:
		return
	if ammo <= 0:
		_start_reload()
		return
	ammo -= 1
	fire_gap = FIRE_GAP
	var j := conductor.judge()
	if j.on:
		dj_cd = 0.0
		dash_cd = 0.0
		dj_used = false
	shot.emit(j.on, j.err)
	var from := camera.global_position
	var to := from - camera.global_transform.basis.z * RANGE
	var q := PhysicsRayQueryParameters3D.create(from, to, 1 | 2)
	q.exclude = [get_rid()]
	var r := get_world_3d().direct_space_state.intersect_ray(q)
	if not r.is_empty() and r.collider.has_meta("enemy"):
		var en: Node = r.collider.get_meta("enemy")
		var head: bool = r.collider.get_meta("part", "") == "head"
		var dmg := DMG_HEAD if head else DMG_BODY
		var result: String = en.take_hit(dmg, j.on)
		hit.emit(en, dmg, head, result)
	if ammo == 0:
		_start_reload()


func _start_reload() -> void:
	if reload_left <= 0.0 and ammo < CLIP:
		reload_left = RELOAD_TIME


# --- Autotest -----------------------------------------------------------------------------------

func auto_aim(target: Vector3) -> void:
	var to := target - camera.global_position
	var yaw := atan2(-to.x, -to.z)
	rotation.y = yaw
	_pitch = atan2(to.y, Vector2(to.x, to.z).length())
	camera.rotation.x = _pitch


func set_auto_move(v: Vector2) -> void:
	_move = v
