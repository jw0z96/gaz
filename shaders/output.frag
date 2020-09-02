#version 430

uniform sampler2DArray dftTexture;
uniform uint dftLastIndex;
uniform uint dftSampleCount;

// uniform float smoothing;
#define smoothingFactor 1.0f

in vec2 uv;

out vec4 fragColour;

void main()
{
	vec2 logUV = uv;
	// logUV.x = pow(logUV.x, 2.0f);
	// logUV.x = sin(logUV.x * (1.0f - (3.14159 / 2.0f)));

	float amp = 0.0f;

	const uint numSamples = uint(dftSampleCount * smoothingFactor);
	for (uint i = 0; i < numSamples; ++i)
	{
		float j = (dftLastIndex - i) % dftSampleCount;
		float falloff = 1.0f - (float(i) / float(numSamples));
		amp = max(amp, texture(dftTexture, vec3(logUV, j)).r * falloff);
	}

	// amp /= 32.0f;
	amp /= 48.0f;

	// float amp = texture(dftTexture, vec3(logUV, dftLastIndex)).r / 48.0f;
	fragColour = vec4(vec3(uv, 1.0f) * amp, 1.0f);
	// fragColour = vec4(uv, 0.0f, 1.0f);
}
