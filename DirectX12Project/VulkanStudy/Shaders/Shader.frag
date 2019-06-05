#version 450
#extension GL_ARB_separate_shader_objects : enable

//Coming from the vertex shader through slot 0 of the interpolator (i.e. like COLOR0 in hlsl)
layout(location = 0) in vec3 fragColor;

//Color output (i.e. like SV_Target0 in HLSL)
layout(location = 0) out vec4 outColor;

//Fragment shader entry point
void main() 
{
	outColor = vec4(fragColor, 1.0);
}