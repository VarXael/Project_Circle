class_name Player
extends CharacterBody3D
## A port of Giuseppe's Unreal Pulse controller (Project_Circle, branch NoRhythmVersion, 2026-05-16):
## UPcQPlayerMovementComponent, APcQPlayerCharacter and the PlayerMovementConfig asset.
## Same states, same order of operations, same numbers (cm converted to m). What differs is noted
## where it happens. Values the config asset overrides over the C++ defaults are marked (asset).
##
## Movement: interpolated ground and air acceleration (VInterpTo), overspeed decays slowly toward the
## max speed, gravity recomputed every frame from the jump peak and an air time scaled by the song's
## BPM, 3 double jumps refilled on landing and by hitting enemies, an unlimited dash with steering,
## the Kinetic Jump (jump and dash within 0.2 s of each other), Magnetic Slam (jump in the air with
## no double jumps left near the ground: slam down and jump on landing), ground pound into a dash,
## the ground pulse (1.4x max speed while grounded, pistol refill, buffered for landing), the Kunai
## (throw, stick, Zip Strike, wall bounce, zip slide, recall boost, enemy step) and the air hang.

signal combo(text: String, color: Color)
signal shot(on_beat: bool, err: float)
signal hit(enemy: Node, damage: float, headshot: bool, result: String)
signal pulse_hit

enum State { GROUNDED, IN_AIR, GROUND_POUNDING, DASHING, RECALL_LUNGING }
enum Kunai { IN_HAND, THROWN, STUCK, RETURNING }

# PlayerMovementConfig
const REFERENCE_BPM := 50.0            # (asset) C++ default 100
const SPEED_SCALE_MIN := 0.7
const SPEED_SCALE_MAX := 1.5
const BASE_MAX_SPEED := 10.0
const GROUND_ACCEL := 30.0
const GROUND_FRICTION := 25.0
const AIR_ACCEL := 15.0
const HARD_CAP_MULT := 4.0
const OVERSPEED_DECAY := 2.0
const JUMP_PEAK := 3.5                 # (asset) 350 cm, C++ default 260
const IDEAL_JUMP_AIR := 0.8            # (asset) C++ default 0.85
const SUPER_JUMP_HORIZ := 4.2
const JUMP_BUFFER := 0.2
const MAGNETIC_SLAM := 25.0
const MAX_DJ := 3                      # (asset) C++ default 2
const IDEAL_DJ_AIR := 0.8              # (asset) C++ default 0.5
const GP_SLAM_SPEED := 28.0
const GP_IMMUNITY_BEATS := 2.0
const IDEAL_PULSE_DURATION := 0.6
const DASH_MULT := 1.55
const IDEAL_DASH := 0.35
const DASH_STEER := 30.0
const DASH_JUMP_BOOST := 2.0
const POST_DASH_IMMUNITY_BEATS := 0.25
const ENEMY_STEP_LIFT := 8.0
const KUNAI_THROW_SPEED := 60.0
const KUNAI_MAX_DIST := 50.0
const KUNAI_STUCK_TIME := 2.0
const KUNAI_RETURN_SPEED := 120.0
const RECALL_IMPULSE := 40.0           # (asset) 4000 cm, C++ default 2000
# APcQPlayerCharacter
const GUN_DAMAGE := 25.0
const GUN_HEAD_MULT := 3.0
const GUN_BEAT_MULT := 2.0
const GUN_COOLDOWN := 0.2
const BULLET_RADIUS := 0.25
const MAX_AMMO := 6
const RELOAD_TIME := 1.2
const KUNAI_DAMAGE := 45.0
const FOV := 90.0
const BEAT_PUNCH := 3.0
const SLIDE_DROP := 0.18
const SLIDE_FOV := 8.0
const SLIDE_SPEED := 8.0
const EYE := 1.56                      # capsule centre 0.96 + camera 0.60
const TERMINAL := 40.0

var conductor: Conductor
var settings: PulseSettings
var enemies: Array = []
var camera: Camera3D
var autopilot := false

var state := State.GROUNDED
var dj := MAX_DJ
var dash_timer := 0.0
var dash_max := 0.0
var dash_dir := Vector3.FORWARD
var dash_power := 1.0
var immunity := 0.0
var pulse_boost := 0.0
var last_dash := -1.0
var last_jump := -1.0
var jump_buffered := false
var jump_buffer_t := 0.0
var gp_buffered := false
var gp_buffer_t := 0.0
var pulse_buffered := false
var pulse_buffer_t := 0.0
var beat_flash := 0.0
var sweep_t := 0.0
var zip_normal := Vector3.UP
var hang := 0.0
var saved_hang := Vector3.ZERO

var ammo := MAX_AMMO
var reloading := false
var reload_t := 0.0
var gun_cd := 0.0

var kunai := Kunai.IN_HAND
var kunai_mesh: MeshInstance3D
var kunai_vel := Vector3.ZERO
var kunai_target := Vector3.ZERO
var kunai_normal := Vector3.UP
var kunai_will_stick := false
var kunai_enemy: Node3D = null
var kunai_offset := Vector3.ZERO
var kunai_stuck_t := 0.0
var recall_buffer := 0.0

var pulses := 0
var _now := 0.0
var _pitch := 0.0
var _move := Vector2.ZERO
var _beat_fov := 0.0
var _dip := 0.0
var _slide := 0.0
var _gun: MeshInstance3D
var _look := Vector2.ZERO
var _sway := Vector3.ZERO
var _recoil := Vector3.ZERO
var _was_floor := true


func _ready() -> void:
	var shape := CollisionShape3D.new()
	var cap := CapsuleShape3D.new()
	cap.radius = 0.42
	cap.height = 1.92
	shape.shape = cap
	shape.position.y = 0.96
	add_child(shape)
	camera = Camera3D.new()
	camera.position.y = EYE
	camera.keep_aspect = Camera3D.KEEP_WIDTH       # Unreal's FOV is horizontal
	camera.fov = FOV
	camera.near = 0.05
	add_child(camera)
	collision_layer = 1
	collision_mask = 1
	floor_snap_length = 0.3
	floor_max_angle = deg_to_rad(50.0)

	var gun_mat := StandardMaterial3D.new()
	gun_mat.albedo_color = Color(0.2, 0.22, 0.28)
	gun_mat.emission_enabled = true
	gun_mat.emission = Color(0.1, 0.6, 0.9) * 0.6
	_gun = MeshInstance3D.new()
	var gm := BoxMesh.new()
	gm.size = Vector3(0.04, 0.06, 0.18)
	_gun.mesh = gm
	_gun.material_override = gun_mat
	_gun.position = Vector3(0.2, -0.16, -0.42)
	_gun.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	camera.add_child(_gun)
	var k_mat := StandardMaterial3D.new()
	k_mat.albedo_color = Color(0.85, 0.85, 0.9)
	k_mat.emission_enabled = true
	k_mat.emission = Color(1.0, 0.3, 0.3) * 0.4
	kunai_mesh = MeshInstance3D.new()
	var km := PrismMesh.new()
	km.size = Vector3(0.03, 0.2, 0.01)
	kunai_mesh.mesh = km
	kunai_mesh.material_override = k_mat
	kunai_mesh.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	camera.add_child(kunai_mesh)
	_kunai_in_hand_pose()


func _unhandled_input(e: InputEvent) -> void:
	if e is InputEventMouseMotion and Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
		rotate_y(-e.relative.x * settings.sensitivity)
		_pitch = clampf(_pitch - e.relative.y * settings.sensitivity, -1.55, 1.55)
		_look = e.relative
	if not conductor.playing:
		return
	if e.is_action_pressed("jump"):
		jump_pressed()
	elif e.is_action_pressed("dash"):
		enter_dash()
	elif e.is_action_pressed("ground_pound"):
		ground_pound_pressed()
	elif e.is_action_pressed("melee"):
		kunai_action()
	elif e.is_action_pressed("fire") and Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
		fire()
	elif e.is_action_pressed("reload"):
		_start_reload()


# --- Helpers (GetCurrentBeatIntervalSec, GetAdaptiveTime, GetSmoothScaledTime, ...) -------------

func beat_interval() -> float:
	return conductor.beat_length() if conductor.playing else 60.0 / REFERENCE_BPM


func speed_scale() -> float:
	var bpm := conductor.bpm() if conductor.playing else REFERENCE_BPM
	return clampf(bpm / REFERENCE_BPM, SPEED_SCALE_MIN, SPEED_SCALE_MAX)


## The ideal time, snapped to the nearest half beat (at least half a beat).
func adaptive_time(ideal: float) -> float:
	var interval := beat_interval()
	var beats := roundf(ideal / interval * 2.0) / 2.0
	return maxf(0.5, beats) * interval


## The ideal time, shortened as the BPM rises (BPM speed scaling).
func smooth_scaled(base: float) -> float:
	return base / speed_scale()


func rhythm_gravity() -> float:
	var t := smooth_scaled(IDEAL_JUMP_AIR)
	return 8.0 * JUMP_PEAK / (t * t)


func max_speed() -> float:
	var s := BASE_MAX_SPEED * speed_scale()
	if pulse_boost > 0.0 and state == State.GROUNDED:
		s *= 1.4
	return s


func hspeed() -> float:
	return Vector2(velocity.x, velocity.z).length()


func wish_dir() -> Vector3:
	var v := _move if autopilot else Input.get_vector("left", "right", "back", "forward")
	if v == Vector2.ZERO:
		return Vector3.ZERO
	var b := global_transform.basis
	var d := -b.z * v.y + b.x * v.x
	d.y = 0.0
	return d.normalized()


func _forward() -> Vector3:
	var f := -global_transform.basis.z
	f.y = 0.0
	return f.normalized()


func _vinterp(current: Vector3, target: Vector3, dt: float, speed: float) -> Vector3:
	return current + (target - current) * clampf(dt * speed, 0.0, 1.0)


func _combo(text: String, c: Color) -> void:
	combo.emit(text, c)


# --- Tick ---------------------------------------------------------------------------------------

func _physics_process(dt: float) -> void:
	_now += dt
	_tick_weapons(dt)
	_tick_kunai(dt)

	# Air hang (the "matrix hit stop"): almost frozen, then the saved velocity comes back.
	if hang > 0.0:
		hang -= dt
		velocity = saved_hang * 0.05
		move_and_slide()
		if hang <= 0.0:
			velocity = saved_hang
		_update_camera(dt)
		return

	var wish := wish_dir()
	var on_floor := is_on_floor()
	var h := Vector3(velocity.x, 0.0, velocity.z)
	var top := max_speed()
	if on_floor and state != State.RECALL_LUNGING and velocity.y <= 0.01:
		# PhysWalking
		if state != State.DASHING:
			var eff := h.length() if (h.length() > top and wish != Vector3.ZERO) else top
			h = _vinterp(h, wish * eff, dt, GROUND_FRICTION if wish == Vector3.ZERO else GROUND_ACCEL)
	elif state != State.GROUND_POUNDING and state != State.RECALL_LUNGING:
		# PhysFalling (a dash in the air keeps air control, as in Unreal)
		if wish != Vector3.ZERO:
			var eff := maxf(h.length(), top)
			h = _vinterp(h, wish * eff, dt, AIR_ACCEL)
	velocity.x = h.x
	velocity.z = h.z
	if state != State.RECALL_LUNGING and (not on_floor or velocity.y > 0.01):
		velocity.y = maxf(velocity.y - rhythm_gravity() * dt, -TERMINAL)
	move_and_slide()

	on_floor = is_on_floor()
	if on_floor and not _was_floor and velocity.y <= 0.01:
		_landed()
	_was_floor = on_floor
	if on_floor and state == State.IN_AIR:
		state = State.GROUNDED
	elif not on_floor and state == State.GROUNDED:
		state = State.IN_AIR

	_tick_sweeps(dt)
	_tick_dash(dt)

	# Overspeed decays slowly back to the max speed, under a hard cap.
	if state != State.DASHING and state != State.RECALL_LUNGING:
		var cur := hspeed()
		var mx := max_speed()
		if cur > mx:
			var new_h := maxf(cur - OVERSPEED_DECAY * dt * ((cur - mx) / mx), mx)
			new_h = minf(new_h, mx * HARD_CAP_MULT)
			var d2 := Vector3(velocity.x, 0.0, velocity.z).normalized()
			velocity.x = d2.x * new_h
			velocity.z = d2.z * new_h

	immunity = maxf(immunity - dt, 0.0)
	beat_flash = maxf(beat_flash - dt, 0.0)
	pulse_boost = maxf(pulse_boost - dt, 0.0)
	if pulse_buffered:
		pulse_buffer_t -= dt
		pulse_buffered = pulse_buffer_t > 0.0
	if jump_buffered:
		jump_buffer_t -= dt
		jump_buffered = jump_buffer_t > 0.0
	if gp_buffered:
		gp_buffer_t -= dt
		gp_buffered = gp_buffer_t > 0.0
	_update_camera(dt)


func _tick_dash(dt: float) -> void:
	if state != State.DASHING and state != State.RECALL_LUNGING:
		return
	dash_timer -= dt
	if dash_timer <= 0.0:
		if state == State.RECALL_LUNGING:
			if zip_normal.y < 0.7:
				var wish := wish_dir()
				var bounce := zip_normal
				if wish != Vector3.ZERO:
					bounce = (zip_normal + wish * 1.5).normalized()
				velocity = bounce * max_speed() * 0.85
				velocity.y = 6.0
				state = State.IN_AIR
				dj = MAX_DJ
				apply_air_hang(0.12)
				_combo("WALL BOUNCE", Color(1, 0.8, 0.2))
			else:
				var d := Vector3(dash_dir.x, 0.0, dash_dir.z).normalized()
				velocity = d * max_speed() * DASH_MULT
				velocity.y = 0.0
				state = State.DASHING
				dash_timer = 0.35
				dash_max = 0.35
				apply_air_hang(0.08)
				_combo("ZIP SLIDE", Color(0.2, 1, 0.8))
				return
		exit_dash()
	elif state == State.DASHING and is_on_floor():
		var wish := wish_dir()
		if wish != Vector3.ZERO:
			var h := _vinterp(Vector3(velocity.x, 0.0, velocity.z), wish * max_speed() * DASH_MULT, dt, DASH_STEER)
			velocity.x = h.x
			velocity.z = h.z
			dash_dir = h.normalized()


## Dashing, zipping or just after a recall boost, touching an enemy hits it and steps off it.
func _tick_sweeps(dt: float) -> void:
	var sweeping := state == State.DASHING or state == State.RECALL_LUNGING
	if sweep_t > 0.0 and state == State.IN_AIR:
		sweep_t -= dt
		sweeping = true
	if not sweeping:
		return
	var from := global_position + Vector3.UP * 0.96
	var to := from + velocity * dt
	for e in enemies:
		if e.alive and _segment_dist(e.center(), from, to) < 1.0 + e.radius():
			_damage_enemy(e, KUNAI_DAMAGE, false)
			dash_dir = Vector3(velocity.x, 0.0, velocity.z).normalized()
			enemy_step()
			apply_air_hang(0.12)
			sweep_t = 0.0
			break


func _segment_dist(p: Vector3, a: Vector3, b: Vector3) -> float:
	var ab := b - a
	var t := 0.0 if ab.length_squared() < 0.0001 else clampf((p - a).dot(ab) / ab.length_squared(), 0.0, 1.0)
	return p.distance_to(a + ab * t)


# --- The floor pulse ----------------------------------------------------------------------------

func on_pulse(_b: int) -> void:
	_beat_fov = BEAT_PUNCH
	if immunity > 0.0:
		return
	if not is_on_floor():
		if velocity.y < 0.0 and state == State.IN_AIR:
			pulse_buffered = true
			pulse_buffer_t = conductor.window
		return
	if state != State.GROUNDED:
		return
	_take_pulse()
	if jump_buffered:
		jump_buffered = false
		normal_jump()


func _take_pulse() -> void:
	pulse_boost = adaptive_time(IDEAL_PULSE_DURATION)
	beat_flash = 0.35
	pulses += 1
	pulse_hit.emit()
	if ammo < MAX_AMMO or reloading:
		ammo = MAX_AMMO
		reloading = false
		reload_t = 0.0
		_combo("PULSE AMMO REFUND", Color(0.2, 1, 0.8))


func _landed() -> void:
	sweep_t = 0.0
	hang = 0.0
	if state == State.RECALL_LUNGING:
		return
	if state == State.GROUND_POUNDING:
		immunity = GP_IMMUNITY_BEATS * beat_interval()
		if wish_dir() == Vector3.ZERO:
			state = State.GROUNDED
			velocity = Vector3.ZERO
		else:
			enter_dash()
			_combo("SLAM → DASH", Color(1, 0.4, 0.1))
		if jump_buffered:
			jump_buffered = false
			exit_dash()
			normal_jump()
		return
	state = State.GROUNDED
	dj = MAX_DJ
	if pulse_buffered and immunity <= 0.0:
		pulse_buffered = false
		_take_pulse()
		if jump_buffered:
			jump_buffered = false
			normal_jump()
		return
	pulse_buffered = false
	if gp_buffered:
		gp_buffered = false
		ground_pound_pressed()
		return
	if jump_buffered:
		jump_buffered = false
		normal_jump()


# --- Actions ------------------------------------------------------------------------------------

func jump_pressed() -> void:
	last_jump = _now
	if state == State.DASHING or state == State.RECALL_LUNGING or _now - last_dash <= 0.2:
		dash_jump()
		return
	match state:
		State.GROUNDED:
			normal_jump()
		State.IN_AIR:
			if dj > 0:
				double_jump()
			elif can_buffer_landing():
				jump_buffered = true
				jump_buffer_t = JUMP_BUFFER
				velocity.y = minf(velocity.y, -MAGNETIC_SLAM)
				_dip = 1.0
				_combo("MAGNETIC SLAM", Color(1, 0.3, 1))


func normal_jump() -> void:
	velocity.y = rhythm_gravity() * smooth_scaled(IDEAL_JUMP_AIR) * 0.5
	state = State.IN_AIR
	_was_floor = false
	_combo("JUMP", Color(0.85, 0.85, 0.85))


func double_jump() -> void:
	velocity.y = rhythm_gravity() * smooth_scaled(IDEAL_DJ_AIR) * 0.5
	dj -= 1
	beat_flash = 0.35
	_combo("DOUBLE JUMP", Color(0.27, 0.67, 1))


## The Kinetic Jump: jump while dashing, or jump and dash within 0.2 s of each other.
func dash_jump() -> void:
	var d := wish_dir()
	if d == Vector3.ZERO:
		d = Vector3(velocity.x, 0.0, velocity.z).normalized()
	if d != Vector3.ZERO:
		var boost := (DASH_JUMP_BOOST + SUPER_JUMP_HORIZ) * maxf(1.0, dash_power * 0.8)
		var launch := minf(hspeed() + boost, BASE_MAX_SPEED * HARD_CAP_MULT)
		velocity.x = d.x * launch
		velocity.z = d.z * launch
	if state == State.DASHING or state == State.RECALL_LUNGING:
		exit_dash()
	state = State.IN_AIR
	sweep_t = 0.0
	velocity.y = rhythm_gravity() * smooth_scaled(IDEAL_JUMP_AIR) * 0.5
	_was_floor = false
	_combo("KINETIC JUMP", Color(0.1, 0.8, 1))


func enter_dash() -> void:
	last_dash = _now
	if not is_on_floor() and _now - last_jump <= 0.2:
		dash_jump()
		return
	var d := wish_dir()
	if d == Vector3.ZERO:
		d = Vector3(velocity.x, 0.0, velocity.z).normalized()
	if d == Vector3.ZERO:
		d = _forward()
	var spd := max_speed() * DASH_MULT
	velocity = Vector3(d.x * spd, 0.0, d.z * spd)
	dash_dir = d
	dash_power = 1.0
	var dur := adaptive_time(IDEAL_DASH)
	dash_timer = dur
	dash_max = dur
	state = State.DASHING
	immunity = maxf(immunity, dur + 0.05)
	_combo("DASH", Color(1, 0.55, 0.15))


func exit_dash() -> void:
	state = State.GROUNDED if is_on_floor() else State.IN_AIR
	dash_timer = 0.0
	dash_max = 0.0
	immunity = maxf(immunity, POST_DASH_IMMUNITY_BEATS * beat_interval())


func ground_pound_pressed() -> void:
	if state == State.DASHING or state == State.RECALL_LUNGING:
		return
	if state == State.IN_AIR:
		if can_buffer_landing():
			gp_buffered = true
			gp_buffer_t = JUMP_BUFFER
			return
		state = State.GROUND_POUNDING
		velocity.y = -GP_SLAM_SPEED
		velocity.x *= 0.25
		velocity.z *= 0.25
		_combo("GROUND POUND", Color(1, 0.35, 0.1))


func enemy_step() -> void:
	state = State.IN_AIR
	global_position.y += 0.05
	var wish := wish_dir()
	var bounce := -dash_dir if wish == Vector3.ZERO else wish
	var gm := maxf(1.0, rhythm_gravity() / 9.8)
	velocity.y = ENEMY_STEP_LIFT * gm
	var push := max_speed() * 1.25
	velocity.x = bounce.x * push
	velocity.z = bounce.z * push
	dj = MAX_DJ
	_was_floor = false
	_combo("ENEMY STEP", Color(1, 0.3, 0.4))


func apply_air_hang(duration: float) -> void:
	if hang <= 0.0:
		saved_hang = velocity
	hang = maxf(hang, duration)


func reset_mobility() -> void:
	dj = MAX_DJ
	_combo("MOBILITY RESET!", Color(0.2, 1, 0.4))


## Sweep down (capsule half height + this frame's fall over the buffer time + 1.2 m).
func can_buffer_landing() -> bool:
	if velocity.y >= 0.0:
		return false
	var c := global_position + Vector3.UP * 0.96
	var dist := 0.96 + absf(velocity.y) * JUMP_BUFFER + 1.2
	var q := PhysicsRayQueryParameters3D.create(c, c + Vector3.DOWN * dist, 1)
	q.exclude = [get_rid()]
	return not get_world_3d().direct_space_state.intersect_ray(q).is_empty()


# --- Pistol -------------------------------------------------------------------------------------

func _tick_weapons(dt: float) -> void:
	gun_cd = maxf(gun_cd - dt, 0.0)
	if reloading:
		reload_t -= dt
		if reload_t <= 0.0:
			ammo = MAX_AMMO
			reloading = false
			_combo("RELOADED", Color(0.5, 0.5, 0.5))


func _start_reload() -> void:
	if not reloading and ammo < MAX_AMMO:
		reloading = true
		reload_t = RELOAD_TIME
		_combo("RELOADING...", Color(1, 0.2, 0.2))


func fire() -> void:
	if gun_cd > 0.0 or reloading:
		return
	if ammo <= 0:
		_start_reload()
		return
	ammo -= 1
	gun_cd = GUN_COOLDOWN
	var j := conductor.judge()
	shot.emit(j.on, j.err)
	_recoil = (_recoil + Vector3(0.0, 0.01, 0.04)).clamp(Vector3.ZERO, Vector3(0.0, 0.03, 0.08))
	# Decided 2026-10-21 (Pulse 00): firing on the beat also resets the double jumps.
	if j.on:
		dj = MAX_DJ
		beat_flash = 0.35
	var from := camera.global_position
	var dir := -camera.global_transform.basis.z
	var q := PhysicsRayQueryParameters3D.create(from, from + dir * 50.0, 1)
	q.exclude = [get_rid()]
	var r := get_world_3d().direct_space_state.intersect_ray(q)
	var wall_t: float = from.distance_to(r.position) if not r.is_empty() else 50.0
	# The bullet is a 25 cm sphere: test it against each enemy's head and body.
	var best: Node3D = null
	var best_t := wall_t
	var head := false
	for e in enemies:
		if not e.alive:
			continue
		var hc: Vector3 = e.head_center()
		var th: float = (hc - from).dot(dir)
		if th > 0.0 and th < best_t and (from + dir * th).distance_to(hc) < e.head_radius() + BULLET_RADIUS:
			best = e
			best_t = th
			head = true
		var tb: float = (e.center() - from).dot(dir)
		if tb > 0.0 and tb < best_t and _ray_to_axis(from, dir, e) < e.radius() + BULLET_RADIUS:
			best = e
			best_t = tb
			head = false
	var end := from + dir * best_t
	get_parent().tracer(_gun.global_position, end, j.on)
	if best:
		var dmg := GUN_DAMAGE * (GUN_HEAD_MULT if head else 1.0) * (GUN_BEAT_MULT if j.on else 1.0)
		var result := _damage_enemy(best, dmg, head, j)
		reset_mobility()
		if j.on:
			_combo("POWER SHOT ★", Color(0.2, 1, 1))
		if head and result != "kill":
			gun_cd = 0.0
			ammo = MAX_AMMO
			reloading = false
			_combo("HEADSHOT + RELOAD!", Color(1, 0.1, 0.1))
	if ammo == 0 and not reloading:
		_start_reload()


func _ray_to_axis(from: Vector3, dir: Vector3, e: Node3D) -> float:
	# distance from the ray to the enemy's vertical body axis, at the closest height
	var a: Vector3 = e.body_bottom()
	var b: Vector3 = e.body_top()
	var best := INF
	for i in 9:
		var p := a.lerp(b, i / 8.0)
		var t := maxf((p - from).dot(dir), 0.0)
		best = minf(best, (from + dir * t).distance_to(p))
	return best


func _damage_enemy(e: Node3D, dmg: float, head: bool, j: Dictionary = {}) -> String:
	if j.is_empty():
		j = conductor.judge()
	var result: String = e.take_hit(dmg, j)
	hit.emit(e, dmg, head, result)
	return result


# --- Kunai (TrySwordAction, UpdateSwordPhysics, ExecuteSwordRecall) -----------------------------

func kunai_action() -> void:
	match kunai:
		Kunai.IN_HAND:
			_throw_kunai()
		Kunai.THROWN:
			recall_buffer = 0.5
			_combo("ZIP BUFFERED", Color(0.8, 0.8, 1))
		_:
			_recall()


func _throw_kunai() -> void:
	var from := camera.global_position
	var dir := -camera.global_transform.basis.z
	var q := PhysicsRayQueryParameters3D.create(from, from + dir * KUNAI_MAX_DIST, 1 | 2)
	q.exclude = [get_rid()]
	var r := get_world_3d().direct_space_state.intersect_ray(q)
	kunai_enemy = null
	if not r.is_empty():
		kunai_target = r.position
		kunai_normal = r.normal
		kunai_will_stick = true
		if r.collider.has_meta("enemy"):
			kunai_enemy = r.collider.get_meta("enemy")
	else:
		kunai_target = from + dir * KUNAI_MAX_DIST
		kunai_will_stick = false
	kunai = Kunai.THROWN
	var gt := kunai_mesh.global_transform
	camera.remove_child(kunai_mesh)
	get_parent().add_child(kunai_mesh)
	kunai_mesh.global_transform = gt
	kunai_vel = (kunai_target - gt.origin).normalized() * KUNAI_THROW_SPEED
	if not is_on_floor():
		velocity.y = maxf(velocity.y, 0.0)
		velocity.x *= 0.3
		velocity.z *= 0.3
	_combo("KUNAI THROWN", Color(0.2, 0.8, 1))


func _tick_kunai(dt: float) -> void:
	recall_buffer = maxf(recall_buffer - dt, 0.0)
	if kunai == Kunai.IN_HAND:
		return
	var at := kunai_mesh.global_position
	if kunai == Kunai.THROWN:
		var dist := at.distance_to(kunai_target)
		var step := KUNAI_THROW_SPEED * dt
		if step >= dist:
			kunai_mesh.global_position = kunai_target
			if kunai_will_stick:
				kunai = Kunai.STUCK
				kunai_stuck_t = KUNAI_STUCK_TIME
				if kunai_enemy:
					kunai_offset = kunai_target - kunai_enemy.global_position
				apply_air_hang(0.08)
				if kunai_enemy and kunai_enemy.alive:
					_damage_enemy(kunai_enemy, KUNAI_DAMAGE, false)
					ammo = MAX_AMMO
					reloading = false
					_combo("KUNAI STICK + RELOAD", Color(1, 0.2, 0.2))
			else:
				kunai = Kunai.RETURNING
			if recall_buffer > 0.0:
				recall_buffer = 0.0
				_recall()
				return
		else:
			kunai_mesh.global_position = at + kunai_vel * dt
			kunai_mesh.rotate_object_local(Vector3.RIGHT, 35.0 * dt)
	elif kunai == Kunai.STUCK:
		if kunai_enemy:
			if kunai_enemy.alive:
				kunai_mesh.global_position = kunai_enemy.global_position + kunai_offset
			else:
				kunai_enemy = null
				kunai = Kunai.RETURNING
		kunai_stuck_t -= dt
		if kunai_stuck_t <= 0.0 or global_position.distance_to(kunai_mesh.global_position) > KUNAI_MAX_DIST:
			kunai = Kunai.RETURNING
	if kunai == Kunai.RETURNING:
		var target := camera.global_position - camera.global_transform.basis.z * 0.4
		var d := (target - kunai_mesh.global_position).normalized()
		kunai_mesh.global_position += d * KUNAI_RETURN_SPEED * dt
		kunai_mesh.rotate_object_local(Vector3.RIGHT, 35.0 * dt)
	if (kunai == Kunai.STUCK or kunai == Kunai.RETURNING) and (global_position + Vector3.UP * 0.96).distance_to(kunai_mesh.global_position) < 1.0:
		_retrieve_kunai()


func _recall() -> void:
	if kunai == Kunai.STUCK:
		var target := kunai_mesh.global_position
		var normal := kunai_normal
		if kunai_enemy:
			target = kunai_enemy.global_position + Vector3.UP * 0.96
			normal = Vector3.UP
		else:
			target = kunai_target + kunai_normal * (0.42 + 0.1)
		var dist := global_position.distance_to(target)
		var lunge := (target - camera.global_position).normalized()
		_retrieve_kunai()
		_recall_dash(lunge, maxf(0.1, dist), normal)
	else:
		_retrieve_kunai()
		_recall_impulse()


## The Zip Strike: pulled to the Kunai in 0.09 s, in a straight line, no gravity.
func _recall_dash(dir: Vector3, dist: float, normal: Vector3) -> void:
	last_dash = _now
	state = State.RECALL_LUNGING
	dash_dir = dir
	dash_power = 1.0
	zip_normal = normal
	var zip := 0.09
	var spd := clampf(dist / zip, 40.0, 300.0)
	velocity = dir * spd
	if hang > 0.0:
		saved_hang = velocity
	dash_timer = zip
	dash_max = zip
	immunity = maxf(immunity, zip + 0.05)
	_was_floor = false
	_combo("ZIP STRIKE", Color(1, 0.1, 0.1))


func _recall_impulse() -> void:
	last_dash = _now
	state = State.IN_AIR
	if velocity.y < 0.0:
		velocity.y = 0.0
	var d := wish_dir()
	if d == Vector3.ZERO:
		d = Vector3(velocity.x, 0.0, velocity.z).normalized()
	if d == Vector3.ZERO:
		d = _forward()
	velocity += d * RECALL_IMPULSE
	velocity.y += 4.0
	var cap := BASE_MAX_SPEED * 6.0
	if velocity.length() > cap:
		velocity = velocity.normalized() * cap
	if hang > 0.0:
		saved_hang = velocity
	dj = MAX_DJ
	_was_floor = false
	_combo("RECALL BOOST [100%]", Color(0.2, 1, 0.6))
	sweep_t = 0.6


func _retrieve_kunai() -> void:
	kunai = Kunai.IN_HAND
	kunai_enemy = null
	if kunai_mesh.get_parent() != camera:
		kunai_mesh.get_parent().remove_child(kunai_mesh)
		camera.add_child(kunai_mesh)
	_kunai_in_hand_pose()


func _kunai_in_hand_pose() -> void:
	kunai_mesh.position = Vector3(-0.22, -0.17, -0.42)
	kunai_mesh.rotation = Vector3(deg_to_rad(-70.0), 0.0, 0.0)


# --- Camera and sway (UpdateCameraEffects, UpdateWeaponSway) ------------------------------------

func _update_camera(dt: float) -> void:
	_beat_fov = lerpf(_beat_fov, 0.0, clampf(dt * 12.0, 0.0, 1.0))
	_dip = lerpf(_dip, 0.0, clampf(dt * 15.0, 0.0, 1.0))
	var slide_target := 1.0 if state == State.DASHING and is_on_floor() else 0.0
	_slide = lerpf(_slide, slide_target, clampf(dt * SLIDE_SPEED, 0.0, 1.0))
	camera.position.y = EYE - SLIDE_DROP * _slide - _dip * 0.15
	camera.rotation.x = _pitch + deg_to_rad(-5.0) * _dip
	camera.fov = FOV + SLIDE_FOV * _slide - _beat_fov
	# weapon sway and recoil
	_look = _look.lerp(Vector2.ZERO, clampf(dt * 15.0, 0.0, 1.0))
	var local_v := global_transform.basis.inverse() * velocity
	var target := Vector3(clampf(-local_v.x * 0.003, -0.05, 0.05), clampf(local_v.y * 0.002, -0.06, 0.06), clampf(local_v.z * 0.003, -0.05, 0.08))
	target += Vector3(-_look.x, _look.y, 0.0) * 0.0006
	if _beat_fov > 0.1:
		target.y -= 0.015
	if reloading:
		target.y -= 0.1
	_sway = _sway.lerp(target, clampf(dt * 12.0, 0.0, 1.0))
	_recoil = _recoil.lerp(Vector3.ZERO, clampf(dt * 15.0, 0.0, 1.0))
	_gun.position = Vector3(0.2, -0.16, -0.42) + _sway + Vector3(0.0, _recoil.y, _recoil.z)
	_gun.rotation = Vector3(_recoil.z * 2.0, 0.0, 0.0)


# --- Autotest -----------------------------------------------------------------------------------

func auto_aim(target: Vector3) -> void:
	var to := target - camera.global_position
	rotation.y = atan2(-to.x, -to.z)
	_pitch = atan2(to.y, Vector2(to.x, to.z).length())
	camera.rotation.x = _pitch


func set_auto_move(v: Vector2) -> void:
	_move = v
