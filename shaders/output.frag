#version 430

#define DFT_SIZE 512 // Added manually for now - this should be numsamples / 2

uniform float u_dftLeft[DFT_SIZE];
uniform float u_dftRight[DFT_SIZE];

in vec2 uv;

out vec4 fragColour;

void main()
{
	float leftDftVal = u_dftLeft[int((1.0f - uv.y) * DFT_SIZE)] / 48.0f;
	float rightDftVal = u_dftRight[int(uv.y * DFT_SIZE)] / 48.0f;

	fragColour = vec4(leftDftVal, rightDftVal, 0.0f, 1.0f);
	// fragColour = vec4(uv, 0.0f, 1.0f);
}
