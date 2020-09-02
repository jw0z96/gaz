#version 430

uniform sampler3D dftTexture;
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

	// const uint numSamples = uint(dftSampleCount * smoothingFactor);
	const uint numSamples = 32;
	for (uint i = 0; i < numSamples; ++i)
	{
		float offset = ((dftLastIndex - i) % dftSampleCount) / float(dftSampleCount); // z offset, since we can't use textureOffset
		float falloff = 1.0f - (float(i) / float(numSamples));
		amp = max(
			amp,
			texture(
				dftTexture,
				vec3(logUV, offset)
			).r * falloff
		);
	}

	amp /= 48.0f;

	// float amp = texture(dftTexture, vec3(logUV, dftLastIndex)).r / 48.0f;
	fragColour = vec4(vec3(uv, 1.0f) * amp, 1.0f);
	// fragColour = vec4(uv, 0.0f, 1.0f);
}
