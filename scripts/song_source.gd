class_name SongSource
extends RefCounted
## Finds a song on this PC and reads its beat from an osu! beatmap (red timing points only).
## Songs are never in this repository or in a build: the repository is public and the songs are
## commercial. They are read at runtime from a folder on the PC, so web builds only have the metronome.

const IMMORTAL_FLAME := [
	"user://songs/immortal_flame",
	"D:/Game Development Projects/A_Race_For_The_Sun/songs/immortal_flame",
]

var title := ""
var folder := ""
var audio_path := ""
var sections: Array = []      # {time, beat (s), anchor, meter}


static func find(paths: Array) -> SongSource:
	if OS.has_feature("web"):
		return null
	for p in paths:
		var dir := ProjectSettings.globalize_path(p)
		if DirAccess.dir_exists_absolute(dir):
			var s := SongSource.new()
			if s._read(dir):
				return s
	return null


func _read(dir: String) -> bool:
	folder = dir
	var d := DirAccess.open(dir)
	if d == null:
		return false
	var osu := ""
	for f in d.get_files():
		if f.ends_with(".osu") and osu.is_empty():
			osu = dir.path_join(f)
		if (f.ends_with(".mp3") or f.ends_with(".ogg")) and audio_path.is_empty():
			audio_path = dir.path_join(f)
	if osu.is_empty() or audio_path.is_empty():
		return false
	var file := FileAccess.open(osu, FileAccess.READ)
	if file == null:
		return false
	var block := ""
	var artist := ""
	while not file.eof_reached():
		var line := file.get_line().strip_edges()
		if line.is_empty() or line.begins_with("//"):
			continue
		if line.begins_with("["):
			block = line
			continue
		if block == "[Metadata]":
			if line.begins_with("Title:"):
				title = line.get_slice(":", 1).strip_edges()
			elif line.begins_with("Artist:"):
				artist = line.get_slice(":", 1).strip_edges()
		elif block == "[TimingPoints]":
			var p := line.split(",")
			if p.size() < 2:
				continue
			var uninherited := p[6].to_int() == 1 if p.size() > 6 else true
			var beat_ms := p[1].to_float()
			if uninherited and beat_ms > 0.0:
				var t := p[0].to_float() / 1000.0
				sections.append({"time": t, "beat": beat_ms / 1000.0, "anchor": t,
					"meter": maxi(p[2].to_int() if p.size() > 2 else 4, 1)})
	if sections.is_empty():
		return false
	sections[0].time = minf(sections[0].time, 0.0)
	if not artist.is_empty():
		title = "%s - %s" % [artist, title]
	return true


func load_stream() -> AudioStream:
	if audio_path.ends_with(".ogg"):
		return AudioStreamOggVorbis.load_from_file(audio_path)
	return AudioStreamMP3.load_from_file(audio_path)


func bpm() -> float:
	return 60.0 / float(sections[0].beat)
