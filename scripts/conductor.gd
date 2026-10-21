class_name Conductor
extends Node
## The clock. Plays a song or a generated metronome and tells the game where the beat is.
## Adapted from A Race for the Sun's Conductor (a port of Circle's PcMusicManager):
## - Song: it plays on the audio thread and the game clock follows it (playback position + time since
##   the last mix - output latency). Small drifts are smoothed, big ones (> 80 ms) snap.
## - Metronome: the clock is real time, and each click is started early by the output latency so it
##   is heard exactly on the beat.
## song_time is the moment a sound is heard. Inputs are judged at song_time - offset, where offset is
## the player's audio-latency setting (headphones, TVs and Bluetooth add delay the engine cannot see).

signal beat(index: int)              # the floor pulse: song_time reached a beat
signal window_closed(index: int)     # that beat's on-beat window is over for inputs

const SNAP_DRIFT := 0.08

var grid: BeatGrid
var player: AudioStreamPlayer
var playing := false
var song_time := 0.0
var lead_in := 2.0
var offset := 0.0            # s, from the latency setting
var window := 0.1            # s, half width of the on-beat window
var metronome := true
var click_over_song := false
var song_name := ""
var end_time := INF

var _clock := 0.0
var _start_us := 0
var _frame_us := 0
var _audio_started := false
var _last_beat := 0
var _last_closed := 0
var _last_click := 0
var _click_hi: AudioStreamPlayer
var _click_lo: AudioStreamPlayer


func _ready() -> void:
	player = AudioStreamPlayer.new()
	add_child(player)
	_click_hi = _make_click(1568.0, 0.06, 0.8)
	_click_lo = _make_click(784.0, 0.06, 0.65)


func setup_metronome(bpm: float) -> void:
	metronome = true
	grid = BeatGrid.new([{"time": 0.0, "beat": 60.0 / bpm, "anchor": 0.0, "meter": 4}])
	player.stream = null
	end_time = INF
	song_name = "Metronome %d BPM" % int(round(bpm))


## sections: {time, beat (s), anchor, meter}, from the song's red timing points.
func setup_song(sections: Array, stream: AudioStream, title: String) -> void:
	metronome = false
	grid = BeatGrid.new(sections)
	player.stream = stream
	end_time = stream.get_length()
	song_name = title


func start() -> void:
	playing = true
	_audio_started = false
	player.stop()
	_start_us = Time.get_ticks_usec()
	_frame_us = _start_us
	_clock = -lead_in
	song_time = _clock
	var b := int(floor(grid.beat_float(song_time)))
	_last_beat = b
	_last_closed = b
	_last_click = b


func stop() -> void:
	playing = false
	player.stop()


func _process(delta: float) -> void:
	if not playing:
		return
	_frame_us = Time.get_ticks_usec()
	if metronome:
		_clock = (_frame_us - _start_us) / 1000000.0 - lead_in
	else:
		_clock += delta
		if not _audio_started:
			if _clock >= 0.0:
				player.play(_clock)
				_audio_started = true
		elif player.playing:
			var audio_t := player.get_playback_position() + AudioServer.get_time_since_last_mix() - AudioServer.get_output_latency()
			var drift := audio_t - _clock
			if absf(drift) > SNAP_DRIFT:
				_clock = audio_t
			else:
				_clock += drift * clampf(delta * 8.0, 0.0, 1.0)
	song_time = maxf(song_time, _clock)

	var b := int(floor(grid.beat_float(song_time)))
	while _last_beat < b:
		_last_beat += 1
		beat.emit(_last_beat)
	var closed := int(floor(grid.beat_float(song_time - offset - window)))
	while _last_closed < closed:
		_last_closed += 1
		window_closed.emit(_last_closed)
	_clicks()
	if not metronome and song_time > end_time + 0.5:
		start()


func _clicks() -> void:
	var heard := song_time + AudioServer.get_output_latency()
	var cb := int(floor(grid.beat_float(heard)))
	if cb <= _last_click:
		return
	_last_click = cb
	if not metronome and not click_over_song:
		return
	if heard < 0.0 and not metronome:
		return
	if cb % maxi(int(grid.section(heard).get("meter", 4)), 1) == 0:
		_click_hi.play()
	else:
		_click_lo.play()


func _make_click(freq: float, length: float, gain: float) -> AudioStreamPlayer:
	var rate := 44100
	var count := int(rate * length)
	var data := PackedByteArray()
	data.resize(count * 2)
	for i in count:
		var t := float(i) / rate
		var env := exp(-t / (length * 0.2))
		var v := sin(TAU * freq * t) * env * gain
		data.encode_s16(i * 2, int(clampf(v, -1.0, 1.0) * 32767.0))
	var wav := AudioStreamWAV.new()
	wav.format = AudioStreamWAV.FORMAT_16_BITS
	wav.mix_rate = rate
	wav.stereo = false
	wav.data = data
	var p := AudioStreamPlayer.new()
	p.stream = wav
	p.max_polyphony = 4
	add_child(p)
	return p


# --- Queries ------------------------------------------------------------------------------------

## The time an input made right now is judged at: live (not the last frame) and minus the latency setting.
func input_time() -> float:
	return song_time + (Time.get_ticks_usec() - _frame_us) / 1000000.0 - offset


## The nearest beat to an input and how far off it was: {beat, err (s, + = late), on}.
func judge(t: float = NAN) -> Dictionary:
	if is_nan(t):
		t = input_time()
	var b := int(round(grid.beat_float(t)))
	var err := t - grid.time_of_beat(b)
	return {"beat": b, "err": err, "on": absf(err) <= window}


func meter_at_beat(b: int) -> int:
	return maxi(int(grid.section(grid.time_of_beat(b)).get("meter", 4)), 1)


## 1 to meter: where a beat falls in its bar.
func beat_in_bar(b: int) -> int:
	return posmod(b, meter_at_beat(b)) + 1


## The last beat of the bar (the 4th in 4/4): the DPW kill beat.
func is_kill_beat(b: int) -> bool:
	return beat_in_bar(b) == meter_at_beat(b)


func beat_float(t: float = NAN) -> float:
	return grid.beat_float(song_time if is_nan(t) else t)


func time_of_beat(b: float) -> float:
	return grid.time_of_beat(b)


func beat_length() -> float:
	return grid.beat_length(song_time)


func bpm() -> float:
	return 60.0 / beat_length()


## 0 on a beat, rising to 1 just before the next one.
func phase() -> float:
	var bf := grid.beat_float(song_time)
	return bf - floor(bf)


## Adaptive Beat Snapping (Pulse devlog, 10 May 2026): an action declares an ideal duration and its
## end is snapped to the closest half beat of the song, so a jump always lands on the music.
func snapped_duration(ideal: float) -> float:
	if not playing:
		return ideal
	var now := song_time
	var bn := grid.beat_float(now)
	var best := ideal
	var best_err := INF
	var b := ceilf(bn * 2.0) / 2.0
	for _i in 64:
		var d := grid.time_of_beat(b) - now
		if d >= ideal * 0.6:
			var e := absf(d - ideal)
			if e < best_err:
				best_err = e
				best = d
			elif d > ideal:
				break
		b += 0.5
	return best
