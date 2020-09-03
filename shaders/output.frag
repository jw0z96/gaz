#version 430

// uniform sampler3D dftTexture;
// uniform uint dftLastIndex;
// uniform uint dftSampleCount;

// // uniform float smoothing;
// #define smoothingFactor 1.0f

in vec3 xyz;

in float amplitude;

out vec4 fragColour;

void main()
{
	// effectively 'discard' the point
	if (amplitude < 0.1f) // maybe this check can be earlier
	{
		discard;
	}


	// vec2 logUV = uv;
	// // logUV.x = pow(logUV.x, 2.0f);
	// // logUV.x = sin(logUV.x * (1.0f - (3.14159 / 2.0f)));

	// float amp = 0.0f;

	// // const uint numSamples = uint(dftSampleCount * smoothingFactor);
	// const uint numSamples = 32;
	// for (uint i = 0; i < numSamples; ++i)
	// {
	// 	float offset = ((dftLastIndex - i) % dftSampleCount) / float(dftSampleCount); // z offset, since we can't use textureOffset
	// 	float falloff = 1.0f - (float(i) / float(numSamples));
	// 	amp = max(
	// 		amp,
	// 		texture(
	// 			dftTexture,
	// 			vec3(logUV, offset)
	// 		).r * falloff
	// 	);
	// }

	// amp /= 48.0f;

	// float amp = texture(dftTexture, vec3(logUV, dftLastIndex)).r / 48.0f;
	// fragColour = vec4(vec3(uv, 1.0f) * amp, 1.0f);
	// fragColour = vec4(uv, 0.0f, 1.0f);

	// // float delta = 0.5f;
	// // // // float alpha = 1.0f - smoothstep(1.0f - delta, 1.0f + delta, length(gl_PointCoord - vec2(0.5)));
	// // float alpha = 1.0f - smoothstep(1.0f - delta, 1.0f + delta, dot(cxy, cxy));
	// if (dot(cxy, cxy) > 1.0)
	// {
	// 	discard;
	// }

	float intensity = min(amplitude, 1.0f) * xyz.z;

	vec2 cxy = 2.0f * gl_PointCoord - 1.0f;
	float falloff = 1.0f - dot(cxy, cxy);


	fragColour = vec4(xyz * intensity * falloff, 1.0f);
	// fragColour = vec4(xyz, 0.5);
}
