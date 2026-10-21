shader_type spatial;
// The floor is the metronome. A dark grid whose lines swell as the beat approaches, flash on the beat,
// and send a ring out from the player. Only upward faces carry the grid; sides get a lit top edge.

uniform float pulse = 0.0;        // 1 on the beat, decays
uniform float phase = 0.0;        // 0 on a beat, rising to 1 just before the next
uniform vec3 origin = vec3(0.0);  // where the ring starts (the player)
uniform vec3 base_color : source_color = vec3(0.035, 0.04, 0.055);
uniform vec3 pulse_color : source_color = vec3(1.0, 0.42, 0.16);
uniform float cell = 2.0;

varying vec3 wpos;
varying vec3 wnorm;

void vertex() {
	wpos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	wnorm = normalize((MODEL_MATRIX * vec4(NORMAL, 0.0)).xyz);
}

void fragment() {
	float up = step(0.5, wnorm.y);
	vec2 g = abs(fract(wpos.xz / cell - 0.5) - 0.5) * cell;
	float d = min(g.x, g.y);
	float w = 0.03 + 0.04 * pulse;
	float aa = fwidth(d) * 1.5;
	float line = 1.0 - smoothstep(w, w + aa, d);
	// fade lines that are thinner than a pixel, so far away they do not sparkle
	line *= clamp(1.0 - aa * 4.0, 0.0, 1.0);
	float anticip = pow(phase, 6.0) * 0.5;
	float r = length(wpos.xz - origin.xz);
	float ring_r = (1.0 - pulse) * 40.0;
	float ring = (1.0 - smoothstep(0.0, 1.2, abs(r - ring_r))) * pulse;
	float glow = up * (line * (0.12 + anticip + 2.0 * pulse) + 0.12 * pulse + ring * 1.5);
	ALBEDO = mix(vec3(0.16, 0.16, 0.2), base_color, up);
	ROUGHNESS = 0.7;
	EMISSION = pulse_color * glow + pulse_color * (1.0 - up) * pulse * 0.15;
}
