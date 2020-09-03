#version 330 core

uniform uvec3 cubeDimensions;
uniform mat4 view;
uniform mat4 projection;

uniform sampler3D dftTexture;
uniform uint dftLastIndex;
uniform uint dftSampleCount;

out vec3 xyz;

out float amplitude;

void main()
{
	uint index = uint(gl_VertexID);
	float x = float(index % cubeDimensions.x) / float(cubeDimensions.x);

	index /= cubeDimensions.x;
	float y = float(index % cubeDimensions.y) / float(cubeDimensions.y);

	index /= cubeDimensions.y;
	float z = float(index % cubeDimensions.z) / float(cubeDimensions.z);

	xyz = vec3(x, y, z);

	// add half step offset
	xyz += vec3(0.5f) / cubeDimensions;

	vec3 uvw = xyz.yxz;
	uvw.z = mod(uvw.z + (float(dftLastIndex) / float(dftSampleCount)), 1.0f);

	// TODO: remap uvw.y for a better fit of the spectrum range (0hz -> 22khz)
	// uvw.y = 1.0f - pow(uvw.y, 0.10f);
	// uvw.y = pow(uvw.y, 8.0f);
	// uvw.y = min(uvw.y * 100000.0f, 1.0f);

	amplitude = texture(dftTexture, uvw).r / 24.0f;
	// amplitude = 1.0f;

	// effectively 'discard' the point if the amplitude is too small
	if (amplitude < 0.1f)
	{
		gl_Position = vec4(-100.0f); // this should be way out of the viewport
		gl_PointSize = 0.0f;
	}
	else
	{
		// apply transforms & center the cube
		vec4 transformedPos = projection * view * vec4(xyz - vec3(0.5f), 1.0f);

		gl_Position = transformedPos;
		gl_PointSize = 35.0f / gl_Position.w; // shitty size attenuation
	}
}
