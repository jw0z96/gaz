#version 430

in vec3 xyz;

in float amplitude;

out vec4 fragColour;

void main()
{
	float intensity = min(amplitude, 1.0f) * xyz.z;

	const vec2 cxy = 2.0f * gl_PointCoord - 1.0f;
	float falloff = 1.0f - dot(cxy, cxy);

	fragColour = vec4(xyz * intensity * falloff, 1.0f);
}
