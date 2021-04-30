#version 330 core

in vec3 pixelPosition;

uniform sampler3D tex;
uniform sampler1D transferFunction;


void main(){

/* 

	Modify this to implement the followings
	1. Ray marching along the view direction
	2. Three rendering modes (MIP, Alpha compositing, Iso-surface rendering)

*/

	// calculate entry and exit point
	

	float voxelValue = texture(tex, pixelPosition).r; // read a value from 3D volume at the center (0.5,0.5,0.5)
	vec4 trasferFunctionValue = texture(transferFunction, voxelValue); // read transfer function value

	gl_FragColor = trasferFunctionValue;
}