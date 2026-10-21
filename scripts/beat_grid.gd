class_name BeatGrid
extends RefCounted
## A beat grid made of sections: {time (start), beat (s per beat), anchor (a time a beat falls on)}.
## Two are used: the osu! grid (the song's real BPM, from the red timing points) and the Cadence
## grid (the same beats subdivided x2 or x4 where the song is intense).

var sections: Array = []
var _k0: Array = []        # first beat index of each section, relative to its anchor
var _cum: Array = []       # global beats before each section


func _init(list: Array) -> void:
	sections = list
	var total := 0.0
	for i in sections.size():
		var s: Dictionary = sections[i]
		var k0 := ceilf((s.time - s.anchor) / s.beat - 0.001)
		_k0.append(k0)
		_cum.append(total)
		if i + 1 < sections.size():
			var nxt: Dictionary = sections[i + 1]
			total += maxf(1.0, ceilf((nxt.time - s.anchor) / s.beat - 0.001) - k0)


func index(t: float) -> int:
	var i := 0
	while i + 1 < sections.size() and sections[i + 1].time <= t:
		i += 1
	return i


func section(t: float) -> Dictionary:
	return sections[index(t)]


func beat_length(t: float) -> float:
	return sections[index(t)].beat


func beat_float(t: float) -> float:
	var i := index(t)
	var s: Dictionary = sections[i]
	return _cum[i] + (t - s.anchor) / s.beat - _k0[i]


func time_of_beat(b: float) -> float:
	var i := 0
	while i + 1 < _cum.size() and _cum[i + 1] <= b:
		i += 1
	var s: Dictionary = sections[i]
	return s.anchor + (b - _cum[i] + _k0[i]) * s.beat
