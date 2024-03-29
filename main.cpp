/*
 * Skeleton code for AAA633 Spring 2021
 *
 * Won-Ki Jeong, wkjeong@korea.ac.kr
 *
 */

#include <stdio.h>
#include <GL/glew.h>
#include <GL/glut.h>

#include <cmath>
#include <iostream>
#include <assert.h>

#include "textfile.h"

// Input 3D volume (hard-coded)
#define FILE_NAME "../lung_256_256_128.raw"//tooth_100_90_160.raw"
#define W 256
#define H 256
#define D 128

// OpenGL window ID
int volumeRenderingWindow;
int transferFunctionWindow;

// 0: MIP, 1: Alpha blending, 2: Iso-surface rendering using Phong illumination
GLint render_mode = 0;

// iso-value (0~1, normalized)
GLfloat iso_value = 0.5;

// Shader program
GLuint p;

// texture object handles
GLuint objectTex, transferFunctionTex;

// Locations for shader uniform variables
GLint locObjectTex, locTransferFunctionTex;
GLint locEye, locRenderMode, locIsoValue;

float histogram[256];
float transferFunction[256*4];
bool transferFunctionChanged = false;

// window info
int width = 600;
int height = 600;
float aspect = 1;

// trackball
float sphereRadius = 300;
float eyeDistance = 7;
float eye[3] = { 0, -1, 0 };
float up[3] = { 0, 0, -1 };

// mouse input
bool leftClick, rightClick;
int lastX, lastY;


// vector helper function
float length(float v[3]) {
	return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

void cross(float v1[3], float v2[3], float result[3]) {
	float result0 = v1[1] * v2[2] - v1[2] * v2[1];
	float result1 = v1[2] * v2[0] - v1[0] * v2[2];
	float result2 = v1[0] * v2[1] - v1[1] * v2[0];
	result[0] = result0;
	result[1] = result1;
	result[2] = result2;
}

void matmul(float m[9], float v[3], float result[3]) {
	float result0 = m[0] * v[0] + m[1] * v[1] + m[2] * v[2];
	float result1 = m[3] * v[0] + m[4] * v[1] + m[5] * v[2];
	float result2 = m[6] * v[0] + m[7] * v[1] + m[8] * v[2];
	result[0] = result0;
	result[1] = result1;
	result[2] = result2;
}

void projectToArc(float x, float y, float result[3]) {
	float screenX = x - (float)width / 2;
	float screenY = (float)height / 2 - y;
	float arc = sqrtf(screenX * screenX + screenY * screenY);  // approximated arc length
	float angleY = arc / sphereRadius;
	float angleZ = atan2f(screenY, screenX);
	
	float result0 = sinf(angleY) * cosf(angleZ);
	float result1 = sinf(angleY) * sinf(angleZ);
	float result2 = cosf(angleY);
	result[0] = result0;
	result[1] = result1;
	result[2] = result2;

	float tmp[3];
	cross(up, eye, tmp);

	float rotation[9] = {
		tmp[0], up[0], eye[0],
		tmp[1], up[1], eye[1],
		tmp[2], up[2], eye[2]
	};
	matmul(rotation, result, result);
}


//
// Global idle function
//
void idle()
{
	if (transferFunctionChanged) {
		glutSetWindow(volumeRenderingWindow);
		transferFunctionChanged = false;
		glutPostRedisplay();
	}
}

// Do not change the location of this include
#include "transferfunction.h"


//
// Loading 3D volume from a file and make a 3D texture
//
void load3Dfile(char *filename,int w,int h,int d) 
{
	FILE *f = fopen(filename, "rb");
	unsigned char *data = new unsigned char[w*h*d];
	fread(data, 1, w*h*d, f);
	fclose(f);


	glGenTextures(1, &objectTex);
	glBindTexture(GL_TEXTURE_3D, objectTex);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	glTexImage3D(GL_TEXTURE_3D, 0, GL_RED, w, h, d, 0, GL_RED, GL_UNSIGNED_BYTE, data);


	for (int i = 0; i<256; i++) {
		histogram[i] = 0;
	}
	for (int i = 0; i < w*h*d; i++) {
		histogram[data[i]]++;
	}
	for (int i = 0; i<256; i++) {
		histogram[i] /= w*h*d;
	}

	delete[]data;
}


void init()
{
	load3Dfile(FILE_NAME, W, H, D);
	glUseProgram(p);

	// texture sampler location
	locObjectTex = glGetUniformLocation(p, "tex");
	locTransferFunctionTex = glGetUniformLocation(p, "transferFunction");

	//
	// Generate transfer function texture
	//
	for (int i = 0; i < 256; i++) {
		transferFunction[i * 4 + 0] = float(i) / 255.0;
		transferFunction[i * 4 + 1] = float(i) / 255.0;
		transferFunction[i * 4 + 2] = float(i) / 255.0;
		transferFunction[i * 4 + 3] = float(i) / 255.0;
	}

	glGenTextures(1, &transferFunctionTex);
	glBindTexture(GL_TEXTURE_1D, transferFunctionTex);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA, 256, 0, GL_RGBA, GL_FLOAT, transferFunction);


	//
	// Bind two textures
	//
	glActiveTexture(GL_TEXTURE0);
	glUniform1i(locObjectTex, 0);
	glBindTexture(GL_TEXTURE_3D, objectTex);

	glActiveTexture(GL_TEXTURE0 + 1);
	glUniform1i(locTransferFunctionTex, 1);
	glBindTexture(GL_TEXTURE_1D, transferFunctionTex);
	
	// Uniforms for raycast
	locEye = glGetUniformLocation(p, "eye");
	locRenderMode = glGetUniformLocation(p, "render_mode");
	locIsoValue = glGetUniformLocation(p, "iso_value");
	glUniform3f(locEye, eyeDistance * eye[0], eyeDistance * eye[1], eyeDistance * eye[2]);
	glUniform1i(locRenderMode, render_mode);
	glUniform1f(locIsoValue, iso_value);
}


//
// when the window size is changed
//
void changeSize(int w, int h) {

	// Prevent a divide by zero, when window is too short
	// (you cant make a window of zero width).
	if(h == 0) h = 1;
	
	width = w;
	height = h;
	aspect = (float)w / (float)h;

	// Set sphere radius
	if (w > h) sphereRadius = h / 2;
	else sphereRadius = w / 2;

	// Set the viewport to be the entire window
    glViewport(0, 0, w, h);
}

//
// Change render mode and iso-value
//
void keyboard(unsigned char key, int x, int y)
{	
	switch (key)
	{
		case 'p':
			// do something
			break;
		case '0':
			render_mode = 0;
			break;
		case '1':
			render_mode = 1;
			break;
		case '2':
			render_mode = 2;
			break;
		case '+':
			iso_value = fmin(1, iso_value + 0.02);
			std::cout << "Iso value: " << iso_value << std::endl;
			break;
		case '-':
			iso_value = fmax(0, iso_value - 0.02);
			std::cout << "Iso value: " << iso_value << std::endl;
			break;
		//default :
			// do nothing
	}

	glUseProgram(p);
	glUniform1i(locRenderMode, render_mode);
	glUniform1f(locIsoValue, iso_value);

	glutPostRedisplay();
}


//
// Main volume rendering code
//
void renderScene(void) 
{
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(60, aspect, 0.1, 50);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(eyeDistance * eye[0], eyeDistance * eye[1], eyeDistance * eye[2], 0, 0, 0, up[0], up[1], up[2]);

	glUseProgram(p);
	
	glBindTexture(GL_TEXTURE_1D, transferFunctionTex);
	glTexSubImage1D(GL_TEXTURE_1D, 0, 0, 256, GL_RGBA, GL_FLOAT, transferFunction);

	// draw a quad for 3D volume rendering
	// x=1
	//glColor3f(1, 0, 0);
	glBegin(GL_QUADS);
	glVertex3f(1, -1, -1);
	glVertex3f(1, 1, -1);
	glVertex3f(1, 1, 1);
	glVertex3f(1, -1, 1);
	glEnd();
	// x=-1
	glBegin(GL_QUADS);
	glVertex3f(-1, -1, -1);
	glVertex3f(-1, -1, 1);
	glVertex3f(-1, 1, 1);
	glVertex3f(-1, 1, -1);
	glEnd();
	// y=1
	//glColor3f(0, 1, 0);
	glBegin(GL_QUADS);
	glVertex3f(-1, 1, -1);
	glVertex3f(-1, 1, 1);
	glVertex3f(1, 1, 1);
	glVertex3f(1, 1, -1);
	glEnd();
	// y=-1
	glBegin(GL_QUADS);
	glVertex3f(-1, -1, -1);
	glVertex3f(1, -1, -1);
	glVertex3f(1, -1, 1);
	glVertex3f(-1, -1, 1);
	glEnd();
	// z=1
	//glColor3f(0, 0, 1);
	glBegin(GL_QUADS);
	glVertex3f(-1, -1, 1);
	glVertex3f(1, -1, 1);
	glVertex3f(1, 1, 1);
	glVertex3f(-1, 1, 1);
	glEnd();
	// z=-1
	glBegin(GL_QUADS);
	glVertex3f(-1, -1, -1);
	glVertex3f(-1, 1, -1);
	glVertex3f(1, 1, -1);
	glVertex3f(1, -1, -1);
	glEnd();

	glUseProgram(0);

	glutSwapBuffers();
}



void  mouseClick(int button, int state, int x, int y) 
{
	if (button == GLUT_LEFT_BUTTON) {
		if (state == GLUT_DOWN) {
			leftClick = true;
		}
		else if (state == GLUT_UP) {
			leftClick = false;
		}
	}
	else if (button == GLUT_RIGHT_BUTTON)
	{
		if (state == GLUT_DOWN) {
			rightClick = true;
		}
		else if (state == GLUT_UP) {
			rightClick = false;
		}
	}

	lastX = x;
	lastY = y;

	glutPostRedisplay();
}

void mouseMove(int x, int y) 
{
	if (leftClick) {
		// 1. Calculate from and to vector
		float from[3];
		float to[3];
		projectToArc(x, y, from);
		projectToArc(lastX, lastY, to);

		// 2. Calculate rotation axis & angle
		float axis[3];
		cross(from, to, axis);

		float axisLength = length(axis);
		if (axisLength != 0)
			for (int i = 0; i < 3; i++)
				axis[i] /= axisLength;
		float angle = asinf(axisLength);

		// 3. Rotate eye with the axis & angle
		float c = cosf(angle);
		float s = sinf(angle);
		float t = 1 - c;
		float rotation[9] = {
			t * axis[0] * axis[0] + c,				t * axis[0] * axis[1] - s * axis[2],	t * axis[0] * axis[2] + s * axis[1],
			t * axis[0] * axis[1] + s * axis[2],	t * axis[1] * axis[1] + c,				t * axis[1] * axis[2] - s * axis[0],
			t * axis[0] * axis[2] - s * axis[1],	t * axis[1] * axis[2] + s * axis[0],	t * axis[2] * axis[2] + c
		};
		
		matmul(rotation, eye, eye);
		matmul(rotation, up, up);
		float eyeLength = length(eye);
		float upLength = length(up);
		for (int i = 0; i < 3; i++) {
			eye[i] /= eyeLength;
			up[i] /= upLength;
		}
	}
	else if (rightClick) {
		eyeDistance -= (lastY - y) * (sphereRadius * 0.00005f);
		if (eyeDistance < 2) eyeDistance = 2;
	}

	lastX = x;
	lastY = y;

	glUseProgram(p);
	glUniform3f(locEye, eyeDistance * eye[0], eyeDistance * eye[1], eyeDistance * eye[2]);

	glutPostRedisplay();
}




//
// Main function
//
int main(int argc, char **argv) {

	glutInit(&argc, argv);

	//
	// Window 1. Transfer function editor
	// 
	init_transferFunction();


	//
	// Window 2. Main volume rendering window
	// 
	
	// init GLUT and create Window
	glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);
	glutInitWindowPosition(100,100);
	glutInitWindowSize(600,600);
	volumeRenderingWindow = glutCreateWindow("Volume Rendering Window");

	// register callbacks
	glutDisplayFunc(renderScene);
	glutReshapeFunc(changeSize);
	glutKeyboardFunc(keyboard);
	glutMouseFunc(mouseClick);
	glutMotionFunc(mouseMove);
	glutIdleFunc(idle);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glewInit();
	if (glewIsSupported("GL_VERSION_3_3")) 	printf("Ready for OpenGL 3.3\n");
	else { printf("OpenGL 3.3 is not supported\n");	exit(1); }

	// Create shader program
	p = createGLSLProgram( "../volumeRendering.vert", NULL, "../volumeRendering.frag" );

	init();

	// enter GLUT event processing cycle
	glutMainLoop();

	return 1;
}