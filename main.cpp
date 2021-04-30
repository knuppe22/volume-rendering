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
#define FILE_NAME "../CThead_512_512_452.raw"//tooth_100_90_160.raw"
#define W 512
#define H 512
#define D 452

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

float histogram[256];
float transferFunction[256*4];
bool transferFunctionChanged = false;

// trackball
float distance = 1;
float eyeX = 0, eyeY = 0, eyeZ = 1;
float upX = 0, upY = 1, upZ = 0;

// mouse input
bool leftClick, rightClick;
float mouseX, mouseY;
float clickedX, clickedY;

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
}


//
// when the window size is changed
//
void changeSize(int w, int h) {

	// Prevent a divide by zero, when window is too short
	// (you cant make a window of zero width).
	if(h == 0) h = 1;
	float ratio = 1.0f * (float) w / (float)h;

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

	glutPostRedisplay();
}


//
// Main volume rendering code
//
void renderScene(void) 
{
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glLoadIdentity();
	glFrustum(-1, 1, -1, 1, 1, 20);
	//gluLookAt(eyeX, eyeY, eyeZ, 0, 0, 0, upX, upY, upZ);
	gluLookAt(5, 5, 5, 0, 0, 0, 0, 1, 0);

	//glUseProgram(p);
	
	glBindTexture(GL_TEXTURE_1D, transferFunctionTex);
	glTexSubImage1D(GL_TEXTURE_1D, 0, 0, 256, GL_RGBA, GL_FLOAT, transferFunction);

	// draw a quad for 3D volume rendering
	// x=1
	glColor3f(1, 0, 0);
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
	glColor3f(0, 1, 0);
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
	glColor3f(0, 0, 1);
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
			clickedX = x;
			clickedY = y;
		}
		else if (state == GLUT_UP) {
			leftClick = false;
		}
	}
	else if (button == GLUT_RIGHT_BUTTON)
	{
		if (state == GLUT_DOWN) {
			rightClick = true;
			clickedX = x;
			clickedY = y;
		}
		else if (state == GLUT_UP) {
			rightClick = false;
		}
	}

	mouseX = x;
	mouseY = y;

	glutPostRedisplay();
}

void mouseMove(int x, int y) 
{
	mouseX = x;
	mouseY = y;

	if (leftClick) {

	}
	else if (rightClick) {

	}

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