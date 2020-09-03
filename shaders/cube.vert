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

	float x = float(index % cubeDimensions.x) /
		float(cubeDimensions.x);

	float y = float((index / cubeDimensions.x) % cubeDimensions.y) /
		float(cubeDimensions.y);

	float z = float((index / (cubeDimensions.x * cubeDimensions.y)) % cubeDimensions.z) /
		float(cubeDimensions.z);

	xyz = vec3(x, y, z);

	// add half step offset
	xyz += vec3(0.5f) / cubeDimensions;

	// apply transforms & center the cube
	vec4 transformedPos = projection * view * vec4(xyz - vec3(0.5f), 1.0f);

	gl_Position = transformedPos;
	gl_PointSize = 20.0f / gl_Position.w; // shitty size attenuation

	vec3 uvw = xyz.yxz;
	uvw.z = mod(uvw.z + (float(dftLastIndex) / float(dftSampleCount)), 1.0f);

	amplitude = texture(dftTexture, uvw).r / 24.0f;

	// effectively 'discard' the point
	if (amplitude < 0.1f) // maybe this check can be earlier
	{
		gl_PointSize = 0.0f;
	}
}
