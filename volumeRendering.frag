#version 330 core

in vec3 pixelPosition;

uniform vec3 eye;
uniform int render_mode;
uniform float iso_value;

uniform sampler3D tex;
uniform sampler1D transferFunction;

void main(){
	vec3 rayDirection = normalize(pixelPosition - eye);
	float dt = 0.001f;
	vec3 position = pixelPosition;

	// maximum intensity projection
	if (render_mode == 0) {
		float maxValue = 0.0;
		while (all(lessThanEqual(vec3(-1.0), position)) && all(lessThanEqual(position, vec3(1.0)))) {
			vec3 texCoord = (position + vec3(1.0)) / 2;
			float voxelValue = texture(tex, texCoord).r;
			if (maxValue < voxelValue) maxValue = voxelValue;
	
			position += dt * rayDirection;
		}
		gl_FragColor = vec4(vec3(maxValue), 1.0);
	}
	// alpha compositing
	else if (render_mode == 1) {
		vec4 color = vec4(0.0);
		while (all(lessThanEqual(vec3(-1.0), position)) && all(lessThanEqual(position, vec3(1.0)))) {
			vec3 texCoord = (position + vec3(1.0)) / 2;
			float voxelValue = texture(tex, texCoord).r;
			vec4 transferFunctionValue = texture(transferFunction, voxelValue);
			transferFunctionValue.a = pow(transferFunctionValue.a, 5);
			//color = color + (1.0 - color.a) * transferFunctionValue;
			vec3 rgb = color.rgb + (1.0 - color.a) * transferFunctionValue.a * transferFunctionValue.rgb;
			float alpha = color.a + (1.0 - color.a) * transferFunctionValue.a;
			color = vec4(rgb, alpha);

			if (color.a > 0.95) break;
			position += dt * rayDirection;
		}
		gl_FragColor = color;
	}
	// iso-surface rendering
	else if (render_mode == 2) {
		dt = 0.01f;
		int level = 0;
		vec3 lastPosition = position;
		while (all(lessThanEqual(vec3(-1.0), position)) && all(lessThanEqual(position, vec3(1.0)))) {
			vec3 texCoord = (position + vec3(1.0)) / 2;
			float voxelValue = texture(tex, texCoord).r;
			if (iso_value < voxelValue) {
				if (level < 3) {
					level++;
					position = lastPosition;
					dt /= 2;
					continue;
				}
				else {
					// compute normal
					vec3 size = textureSize(tex, 0);
					vec3 diff = 1 / size;
					float dx = (texture(tex, texCoord + vec3(diff.r, 0.0, 0.0)).r - texture(tex, texCoord - vec3(diff.r, 0.0, 0.0)).r) / size.r;
					float dy = (texture(tex, texCoord + vec3(0.0, diff.g, 0.0)).r - texture(tex, texCoord - vec3(0.0, diff.g, 0.0)).r) / size.g;
					float dz = (texture(tex, texCoord + vec3(0.0, 0.0, diff.b)).r - texture(tex, texCoord - vec3(0.0, 0.0, diff.b)).r) / size.b;
					vec3 normal = -normalize(vec3(dx, dy, dz));

					// phong lighting
					vec3 light = normalize(vec3(-1.0, -1.0, -1.0));
					vec3 diffuse = max(dot(light, normal), 0.0) * vec3(1.0, 0.0, 0.0);
				
					vec3 reflect = 2.0 * dot(light, normal) * normal - light;
					vec3 view = -rayDirection;
					vec3 specular = pow(max(dot(reflect, view), 0.0), 10) * vec3(1.0);

					vec3 ambient = vec3(0.1);

					gl_FragColor = vec4(diffuse + specular + ambient, 1.0);
					return;
				}
			}
	
			lastPosition = position;
			position += dt * rayDirection;
		}
		gl_FragColor = vec4(0.0);
	}
}