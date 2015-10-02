/*
* A quick and dirty example "game" created for the November 2014 TasLUG
* (Tasmanian Linux User Group) talk on creating a simple game from scratch
* using SDL2 and OpenGL.
* 
* Copyright Josh "Cheeseness" Bush 2014
* 
* Licenced under Creative Commons: By Attribution 3.0
* http://creativecommons.org/licenses/by/3.0/
*/


//Yay, windows.
#ifdef __MINGW64__
	#include <cstdlib>
	#include <math.h>
	#include <windows.h>
	#include <fstream>
#endif
 
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_ttf.h>
#include <list>
#include <string>
#include <vector>

using namespace std;

//Certain operating systems are dumb and use the wrong slash to separate paths.
//Thanks, Windows.
const string pathSeparator = 
#ifdef __MINGW64__
	"\\";
#else
	"/";
#endif


//The size of the window we're going to generate
int screenWidth = 1440;
int screenHeight = 900;

//Some windowing and OpenGL variables
SDL_Window* win = NULL;
SDL_GLContext glContext;

//Whether or not we want to continue playing
bool running = true;

//HUD text variables
SDL_Colour textColour = {255,255,255};
int hudSize = 32;
TTF_Font *hudFont = NULL;

//Mouselook variables
int limitY = 60;
bool invertY = false;
float mouseSensX = 0.1f;
float mouseSensY = 0.1f;
GLfloat rotY = 0;
GLfloat rotX = 0;

//Vehicle position/orientation variables
int carSteer = 0;
bool carAccel = false;
bool carBrake = false;
float carSpeed = 0;
float carDirection = 180;
float carX = 0;
float carY = 0;

//Audio  variables
int mixChannelFans = -1;
int mixChannelHover = -1;
Mix_Chunk* sampleHover;
Mix_Chunk* sampleFans;
Mix_Music* sampleMusic;

//A structure representing a 3D model in the game
struct GameObject
{
	string name;

	//Position
	float x;
	float y;
	float rz;

	//Material
	SDL_Colour colour;

	//Geometry
	vector <GLfloat> vertexList;
	vector <GLfloat> normalList;
	vector <GLushort> faceList;
};

//Lists of the 3D models that appear in the game
list<GameObject> sceneryObjects;
list<GameObject> vehicleObjects;

//Declarations for all the functions we'll be using
//(this is only necessary when functions are being used that are written later in the file than they're being called, but it's a nice overview)
bool init();
bool initGL();
void handleMouseMotion(int xrel, int yrel);
void handleMouseClick(SDL_MouseButtonEvent button);
void handleKeys(SDL_KeyboardEvent key);
void updateSim();
void rotateCamera();
void updateLighting();
void updateSound();
void renderScenery();
void renderCar();
void renderHUD();
void renderText(TTF_Font *font, float x, float y, int width, string text);
GameObject loadObj(string objectName, SDL_Colour foo);
void loadAssets();
void close();


/*
* Intitialises the SDL, SDL_TTF and SDL_Mixer, creates our window and OpenGL context, and loads our fonts.
* Returns true if all actions are successful and false if there have been any errors.
*/
bool init()
{
	//Keep track of whether we've encountered any errors
	bool errors = false;

	//This will initialise SDL and SDL_Mixer
	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0)
	{
		printf("Error whilst initialising SDL: %s\n", SDL_GetError());
		errors = true;
	}
	else
	{
		//Use OpenGL 2.1 for compatibility
		//(A better way to do this is to use SDL_GL_GetAttribute to get the current supported OpenGL version and use conditional statements to control what sorts of stuff is and isn't done.
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

		//Create a SDL window. We're not giving the window a defailt position, so it'll always spawn in the middle of the primary display
		win = SDL_CreateWindow("Hover Drive - A Simple Example Game Demonstrating SDL2 and OpenGL", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, screenWidth, screenHeight, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);

		//If we got null back, then we couldn't create our window. If we didn't, then let's get on with initialisng stuff
		if(win == NULL)
		{
			printf("Error whilst creating SDL window: %s\n", SDL_GetError());
			errors = true;
		}
		else
		{
			//Initialise SDL_TTF stuff
			if(TTF_Init() == -1)
			{
				printf("Error whilst initialising SDL_ttf: %s\n", TTF_GetError());
				errors = true;
			}

			//Load the font that we're going to be using
			hudFont = TTF_OpenFont(("resources" + pathSeparator + "fonts" + pathSeparator + "SciFly-Sans.ttf").c_str(), hudSize);
			if(hudFont == NULL)
			{
				printf("Error whilst loading description font face: %s\n", TTF_GetError());
				errors = true;
			}
			
			//Initialise SDL_Mixer stuff
			if (Mix_Init(MIX_INIT_OGG) == -1)
			{
				printf("Error whilst initialising SDL_Mixer: %s\n", Mix_GetError());
				errors = true;
			}

			//Test opening audio so that if there are problems, we know about it now
			if(Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0)
			{
				printf("Error whilst initialising SDL_mixer: %s\n", Mix_GetError());
				errors = false;
			}

			//Create the GL context
			glContext = SDL_GL_CreateContext(win);
			if(glContext == NULL)
			{
				printf("Error whilst creating GL context: %s\n", SDL_GetError());
				errors = true;
			}
			else
			{
				//FIXME: This is meant to enforce vsync, but I still get tearing \o/
				if(SDL_GL_SetSwapInterval(1) < 0)
				{
					printf("No vsync? : %s\n", SDL_GetError());
				}

				//Call our function that tests a bunch of OpenGL initialisaton
				if(!initGL())
				{
					printf("We couldn't get the OpenGLs to work for us. We'll have to bail :(!\n");
					errors = true;
				}
			}
		}
	}

	return !errors;
}


/*
* Tests that a bunch of OpenGL stuff works properly
* Returns true if all actions are successful and false if there have been any errors.
*/
bool initGL()
{
	//Keep track of whether we've encountered any errors
	bool errors = false;
	GLenum e = GL_NO_ERROR;

	//Test Projection Matrix
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	//Set up the camera's frustrum by defining the horizontal FOV (75 here), the aspect ratio from which the vertical FOV can be derived, and the near and far clipping planes
	gluPerspective(75, (float)screenWidth / (float)screenHeight, 0.2f, 2000);
	
	//Check for errors
	e = glGetError();
	if (e != GL_NO_ERROR)
	{
		printf("Error whilst initialising OpenGL: %s\n", gluErrorString(e));
		errors = true;
	}

	//Test Modelview Matrix
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	//Check for errors
	e = glGetError();
	if (e != GL_NO_ERROR)
	{
		printf("Error whilst initialising OpenGL: %s\n", gluErrorString(e));
		errors = true;
	}
	
	//Test setting a background colour
	glClearColor(0.f, 0.f, 0.0f, 1.f);
	
	//Check for errors
	e = glGetError();
	if (e != GL_NO_ERROR)
	{
		printf("Error whilst initialising OpenGL: %s\n", gluErrorString(e));
		errors = true;
	}
	
	//Hide backfaces and specify the winding order for faces (the direction that they face based on whether the verticies are defined in clockwise or anticlockwise order
	glEnable(GL_CULL_FACE);
	glFrontFace(GL_CW);
	
	//Enable texturing - we use this for our font rendering
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	
	//Initialise GLEW (OpenGL Extension Wrangler) and check for errors
	e = glewInit();
	if (e != GLEW_OK)
	{
		printf("Error whilst initialising OpenGL: %s\n", gluErrorString(e));
		errors = true;
	}

	return !errors;
}



/*
* Updates our camera orientation variables based on the relative X and Y mouse movement.
* Returns nothing.
*/
void handleMouseMotion(int xrel, int yrel)
{
	//If we've moved in the X axis
	if (xrel != 0)
	{
		//Decrease rotX by the amount of movement adjusted for mouse sensitivity
		rotX += xrel * mouseSensX;

		//If rotX ends up outside of 0 to 360, bring it back within those bounds
		if (rotX > 360)
		{
			rotX -= 360;
		}
		else if (rotX < 0)
		{
			rotX += 360;
		}
		//printf("Mouse ?X: %f\t", rotX);
	}

	//If we've moved in the Y axis
	if (yrel != 0)
	{
		//This temporary variable is used to check that the new value is appropriate before assining it to rotY
		float tempY = 0;
		
		//If the Y axis is inverted (which is the more natural way for things to be), add the relative movement to rotY otherwise subtract it
		if (invertY)
		{
			//If the temporary variable is within the view limits specified by limitY (so that we can't look too far up or down), assign it to rotY
			tempY = rotY - (yrel * mouseSensY);			
			if ((tempY > -limitY) && (tempY < limitY))
			{
				rotY = tempY;
			}
		}
		else
		{
			//If the temporary variable is within the view limits specified by limitY (so that we can't look too far up or down), assign it to rotY
			tempY = rotY + (yrel * mouseSensY);
			if ((tempY > -limitY) && (tempY < limitY))
			{
				rotY = tempY;
			}
		}
		//printf("Mouse Y: %f\n", rotY);
	}
	
}


/*
* Performs appropriate actions based on any mouse click events (currently we don't do anything).
* Returns nothing.
*/
void handleMouseClick(SDL_MouseButtonEvent button)
{
	//Keep track of whether this is a mouse up or mouse down event
	bool press = true;
	if (button.state == SDL_RELEASED)
	{
		press = false;
	}

	//Perform the appropriate action depending on which button it is that has been pressed
	switch (button.button)
	{
		case SDL_BUTTON_LEFT:
			if (press)
			{
			
			}
			else
			{
			
			}
			break;

		case SDL_BUTTON_RIGHT:
			if (press)
			{
			
			}
			else
			{
			
			}
			break;

		case SDL_BUTTON_MIDDLE:
			if (press)
			{
			
			}
			else
			{
			
			}
			break;

		default:
			//printf("Unknown button %d", button.button);
			break;

	}
}


/*
* Performs appropriate actions based on any keyboard events.
* Returns nothing.
*/
void handleKeys(SDL_KeyboardEvent key)
{
	//Keep track of whether this is a mouse up or mouse down event
	bool press = true;
	if (key.type == SDL_KEYUP)
	{
		press = false;
	}

	//Perform the appropriate action depending on which button it is that has been pressed
	switch (key.keysym.sym)
	{
		case SDLK_d:
		case SDLK_RIGHT:
			if (press)
			{
				//Steer right
				carSteer = -1;
			}
			else
			{
				//Straighten back up
				carSteer = 0;
			}
			break;

		case SDLK_a:
		case SDLK_LEFT:
			if (press)
			{
				//Steer left
				carSteer = 1;
			}
			else
			{
				//Straighten back up
				carSteer = 0;
			}
			break;

		case SDLK_w:
		case SDLK_UP:
			if (press)
			{
				//Make go faster
				carAccel = true;
			}
			else
			{
				carAccel = false;
			}
			break;

		case SDLK_s:
		case SDLK_DOWN:
			if (press)
			{
				//Brake
				carBrake = true;
			}
			else
			{
				carBrake = false;
			}
			break;

		case SDLK_g:
			if (press)
			{
				//Let go of the mouse when G is pressed
				SDL_SetRelativeMouseMode((SDL_bool)!SDL_GetRelativeMouseMode());
			}
			break;

		case SDLK_ESCAPE:
		case SDLK_q:
			//Quit the game when Q is pressed
			running = false;
			break;
	}
}


/*
* Handles the loose simulation that the game runs on. Moving these calculations outside of input events and rendering calls is important.
* Longer term, it would make sense to bring the time since last frame into the equation so that movement becomes independent of frame rate (you could also go multithreaded).
* Returns nothing.
*/
void updateSim()
{
	//Steering will be a negative number for right, positive number for left. If we're going straight, then carSteer will be zero and there'll be no change
	carDirection += 5.0f * carSteer;
	
	//Adjust orientation to fit within 0 - 360 degrees
	if (carDirection > 360)
	{
		carDirection = fmod(carDirection, 360.0f);
	}
	else if (carDirection < 0)
	{
		carDirection = 360.0f - carDirection;
	}

	//If we're braking, slow us down nice and quick
	if (carBrake)
	{
		carSpeed -= 0.01f;
		if (carSpeed < 0)
		{
			carSpeed = 0.0f;
		}	
	}
	//If we're accelerating, speed us up quickfast (node that this increases our speed over time rather than jumping instantly to top speed)
	else if (carAccel)
	{
		carSpeed += 0.01f;
		if (carSpeed > 0.25f)
		{
			carSpeed = 0.25f;
		}
	}
	//If we're not braking or accelerating, let's slow down by a tiny amount to give the impression of drag
	else if (carSpeed > 0.02f)
	{
		carSpeed -= 0.0025f;
		
		//This if is unnecessary, but if we ever applied drag when carSpeed was below 0.0025, we'd end up moving backwards, so we'd need this check
		if (carSpeed < 0)
		{
			carSpeed = 0.0f;
		}
	}

	//Yay, trigonometry! What we're calculating is the distance that the vehicle has traveled from 0,0 (world origin) along each axis since the last frame
	//cos() and sin() take stuff in radians, so we're doing the conversion from degrees inline
	float dy = cos((M_PI * carDirection) / 180) * carSpeed;
	float dx = sin((M_PI * carDirection) / 180) * carSpeed;

	//Add these new distances to the car's current position
	carX += dx;	
	carY += dy;	
}


/*
* Reorient the world based on our camera rotation variables. This makes it look like the camera has moved.
* Returns nothing.
*/
void rotateCamera()
{
	//Make sure we're in the modelview matrix mode
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	//Rotate the scene according to our current orientation
	//The last three parameters of glRotate are multipliers for how much the rotation will effect each axis
        glRotatef(rotY, 1, 0, 0);
        glRotatef(rotX, 0, 1, 0);
        
        //Correct up axis for roll/gimbal lock by resetting Z rotation to 0;
	glRotatef(0, 0, 0, 1);
}


/*
* Updates the camera position (in case the camera has moved).
* If we were doing any neat effects (such as clouds moving past the sun), we could adjust the light's other properties here.
* Returns nothing.
*/
void updateLighting()
{
	//Let's use flat shading instead of smooth (totally optional)
	glShadeModel(GL_FLAT);

	//Make sure that lighting and the light that we care about are both enabled.
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);

	//Set the specular values for this light
	GLfloat specular[] = {0.2f, 0.2f, 0.2f, 1.0f};
	glLightfv(GL_LIGHT0, GL_SPECULAR, specular);

	//Set the ambient values for this light
	GLfloat ambient[] = {0.8f, 0.8f, 0.8, 1.0f};
	glLightfv(GL_LIGHT0, GL_SPECULAR, ambient);

	//Set the difuse values for this light
	GLfloat diffuse[] = {0.5f, 0.5f, 0.5f, 1.0f};
	glLightfv(GL_LIGHT0, GL_SPECULAR, diffuse);

	//Set the position for this light
	//FIXME: There's something fishy about the position required to get this light behaving nicely - it's probably indicating weird normals?
	GLfloat position[] = {0.0f, 10.0f, 200.0f, 1.0f};
	glLightfv(GL_LIGHT0, GL_POSITION, position);

	//Make the ambient light level nice and bright
	GLfloat gambient[] = {0.8f, 0.8f, 0.8f, 1.0f};
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, gambient);
}


/*
* Updates the sound position and attenuation based on vehicle and camera movement.
* Returns nothing.
*/
void updateSound()
{
	//Do some more trigonometry to work out the vehicle's distance and angle from 0,0 (world origin)
	float distance = sqrt(pow(carX,2.0) + pow(carY, 2.0));
	float bearing = atan2(carX, carY) * (180 / M_PI);
	
	//If the distance is greater than the maximum distance (we multiply this by 4 and 128 is the max), clip it
	if (distance > 63.5)
	{
		distance = 63.5;
	}
	
	//Set the hover sound position
	if(!Mix_SetPosition(mixChannelHover, bearing + rotX, distance * 4))
	{
		printf("Error setting initial position for hover sound: %s\n", Mix_GetError());
	}

	//Adjust the fan sound's volume so that it's louder when we're going faster
	//carSpeed will never be more than 0.25, so the fan volume will never be more than MIX_MAX_VOLUME / 4
	Mix_VolumeChunk(sampleFans, MIX_MAX_VOLUME * carSpeed);

	//Set the fan sound position
	if(!Mix_SetPosition(mixChannelFans, bearing + rotX, distance * 4))
	{
		printf("Error setting initial position for fan sound: %s\n", Mix_GetError());
	}
}


/*
* Draws the geometry for a given object, translating and rotating it as required.
* Returns nothing.
*/
void renderObject(GameObject o)
{
	//Push the current matrix onto the stack (just in case it's not stored there - we want to make sure we can come back to it)
	glPushMatrix();

	//Translate (move) and rotate based on the object's x and y properties
	glTranslatef(o.x, 0.0f, o.y);
	glRotatef(o.rz, 0.0f, 1.0f, 0.0f);		

	//Enable ambient and diffuse colouring
	glEnable(GL_COLOR_MATERIAL);
	glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);

	//Set the rendering colour based on the scenery object's colour
	glColor3ub(o.colour.r, o.colour.g, o.colour.b);

	//Enable the use of vertex and normal arrays
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);

	//Give OpenGL pointers to our vertex and normal arrays
	glVertexPointer(3, GL_FLOAT, sizeof(GLfloat) * 3, o.vertexList.data());
	glNormalPointer   (GL_FLOAT, sizeof(GLfloat) * 3, o.normalList.data());

	//Ask OpenGL to draw polygons based on the indices contained in
	//the faces array, which correspond to the vertexes from the
	//normal array
	glDrawElements(GL_TRIANGLES, o.faceList.size()/3, GL_UNSIGNED_SHORT, o.faceList.data());

	//Disable the use of vertex and normal arrays (since we might not
	//need this for other rendering)
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);

	//Pop the last stored matrix off the top of the stack so that we go back to the state we were in at the start of the loop		
	glPopMatrix();
}



/*
* Loop through all of our scenery objects and call renderObject() for each.
* Returns nothing.
*/
void renderScenery()
{
	//Loop through our list of scenery objects and render them
	list<GameObject>::iterator x;
	for(x = sceneryObjects.begin(); x != sceneryObjects.end(); ++x)
	{
		renderObject(*x);
	}
}


/*
* Loop through all of our vehicle objects and call renderObject() for each.
* Returns nothing.
*/
void renderCar()
{
	//Push the current matrix onto the stack (just in case it's not stored there - we want to make sure we can come back to it)
	glPushMatrix();

	//Translate to the vehicle's new position
	glTranslatef(carX, -1.8f, carY);
	glRotatef(carDirection, 0.0f, 1.0f, 0.0f);
	
	//Loop through our list of scenery objects and render them
	list<GameObject>::iterator x;
	for(x = vehicleObjects.begin(); x != vehicleObjects.end(); ++x)
	{
		renderObject(*x);
	}

	//Pop the vehicle's position off the matrix stack
	glPopMatrix();
}


/*
* Switches to orthogonal rendering and draws some HUD elements.
* Returns nothing.
*/
void renderHUD()
{
	//Jiggle our matrices around a bit
	//By doing this pushing and loading, we set it up so that we can draw orthogonally to the screen in pixel coordinates
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0.0, screenWidth, screenHeight, 0.0, -1.0, 10.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	//If we don't disable lighting, then our HUD ends up black because it generally won't be receiving any light
	glDisable(GL_LIGHTING);
	
	//Set the colour for our HUD text
	glColor3ub(textColour.r, textColour.g, textColour.b);

	//Make a character array for our HUD text
	char hudtext[20];
	
	//Fill the HUD text with the current speed, and then render it towards the bottom right of the screen
	sprintf(hudtext, "Speed: %.0f", carSpeed * 100);
	renderText(hudFont, screenWidth - 300, screenHeight - hudSize * 5, 300, hudtext);

	//Fill the HUD text with the implied state of the left fan, and then draw it towards the bottom left of the screen
	if ((carSteer < 0) || (carSteer == 0 && !carAccel))
	{
		sprintf(hudtext, "Left Fan: Off");
	}
	else
	{
		sprintf(hudtext, "Left Fan: On");
	}
	renderText(hudFont, 100, screenHeight - hudSize * 5, 300, hudtext);

	//Fill the HUD text with the implied state of the right fan, and then draw it towards the bottom left of the screen
	if ((carSteer > 0) || (carSteer == 0 && !carAccel))
	{
		sprintf(hudtext, "Right Fan: Off");
	}
	else
	{
		sprintf(hudtext, "Right Fan: On");
	}
	renderText(hudFont, 100, screenHeight - hudSize * 4, 300, hudtext);
	
	//Reset the matrix state that we set at the beginning of the function
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	
	//Re-enable lighting for any future rendering calls that require it
	glEnable(GL_LIGHTING);
}


/*
* Renders a given string in a given font at given coordinates, wrapping to a given width.
* Returns nothing.
*/
void renderText(TTF_Font *font, float x, float y, int width, string text)
{
	//Create a new SDL_Surface and write the string to it - we'll be generating our OpenGL texture from this
	//It's worth noting that the colour parameter is more or less ignored, as the glColour call overrides
	SDL_Surface *surf = TTF_RenderText_Blended_Wrapped(font, text.c_str(), textColour, width);
	if(surf == NULL)
	{
		printf("Error whilst rendering text: %s\n", TTF_GetError());
	}
 
 	//Make a new OpenGL texture and bind our current rendering actions to it
	unsigned tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
 
 	//Set some properties for this texture. These are to do with how scaling is handled when the texture is rendered above or below its native pixel resolution
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	//Copy the pixels from our SDL_Surface into the new texture
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surf->w, surf->h, 0, GL_BGRA, GL_UNSIGNED_BYTE, surf->pixels);
 
	//Draw this texture on a quad with the given coordinates.
	glBegin(GL_QUADS);
		glTexCoord2d(0, 0); glVertex3d(x, y, 1.0);
		glTexCoord2d(1, 0); glVertex3d(x + surf->w, y, 1.0);
		glTexCoord2d(1, 1); glVertex3d(x + surf->w, y + surf->h, 1.0);
		glTexCoord2d(0, 1); glVertex3d(x, y + surf->h, 1.0);
	glEnd();
 
	//Clean up the texture and surface since we won't be using them anymore
	glDeleteTextures(1, &tex);
	SDL_FreeSurface(surf);
}


/*
* Reads a specified .obj model and creates a GameObject instance representing the 3D model and its position/rotation in 3D space.
* Returns a GameObject.
*/
GameObject loadObj(string objFile, SDL_Colour foo, float posX, float posY, float rotZ)
{
	//Create a new GameObject and set its properties based on what's been passed in
	GameObject newObject;
	newObject.name = objFile;
	newObject.x = posX;
	newObject.y = posY;
	newObject.rz = rotZ;

	//TODO: This stuff should be parsed from whatever mtrl files the OBJ says it uses
	//Set the colour for the object
	newObject.colour.r = foo.r;
	newObject.colour.g = foo.g;
	newObject.colour.b = foo.b;

	//Declare some temporary variables that we'll be using
	GLfloat x = 0;
	GLfloat y = 0;
	GLfloat z = 0;

	//Open the .obj file
	string fileName = "resources" + pathSeparator + "models" + pathSeparator + objFile;
	FILE * currentFile = fopen(fileName.c_str(), "r");

    // vectors for the vertices and their normals
    vector<GLfloat> normalVertices;
    vector<GLfloat> vertices;

	//If the file exists and can be opened
	if(currentFile != NULL)
	{
		printf("  Attempting to parse obj file %s\n", fileName.c_str());
		
		//Loop through forever (we'll break out if we detect the end of the file)
		while(1)
		{
			//A character array that we'll store the first part of the line that we're reading in
			char lineType[256];

			//TODO: Longer term, it'd probably be worth just reading the entire line here and then do our conditionals based on  the first two characters, instead of just reading the first word.
			//Grab everything on the line before the first space (in the Wavefront OBJ format, the first few characters indicate the type of data stored on that line)
			int result = fscanf(currentFile, "%s ", lineType);

			//printf("   %s\n", lineType);

			//If we've hit the end of the file, drop out of the loop
			if (result == EOF)
			{
				break;
			}

			//TODO: There are other line types in the OBJ spec such as vt, usemtrl and mtrlib that we're ignoring for now
			//If the line represents a vertex
			if (strcmp(lineType, "v") == 0)
			{
				//Read the three float values and store them in the vertex list
				fscanf(currentFile, "%f %f %f\n", &x, &y, &z);
				vertices.push_back(x);
				vertices.push_back(y);
				vertices.push_back(z);
			}
			// If the line represents a normal vertex
			else if (strcmp(lineType, "vn") == 0)
			{
				// Read the values and store them in our temporary list of normal vertices
				fscanf(currentFile, "%f %f %f\n", &x, &y, &z);
				normalVertices.push_back(x);
				normalVertices.push_back(y);
				normalVertices.push_back(z);
			}
			//If the line represents a face
			else if (strcmp(lineType, "f") == 0)
			{
				//Declare some temporary arrays to store the values we're reading out
				unsigned int faceDefs[3];
				unsigned int normalDefs[3];
				
				//TODO: This doesn't account for other styles of face definitions (with texture verts)
				//Read the vertex and normal values out of the file and put them into the temporary arrays
				int pcount = fscanf(currentFile, "%d//%d %d//%d %d//%d\n", &faceDefs[0], &normalDefs[0], &faceDefs[1], &normalDefs[1], &faceDefs[2], &normalDefs[2]);
				//If we've gotten the right number of pattern matches when reading from the file
				if (pcount == 6)
				{
					for(unsigned i = 0; i < 3; i++) {
						// push the next indices into the list of faces
						newObject.faceList.push_back(newObject.vertexList.size()+2);
						newObject.faceList.push_back(newObject.vertexList.size()+1);
						newObject.faceList.push_back(newObject.vertexList.size()+0);
                        
						//These indices aren't 0 based, so let's
						//subtract one to get the right index into
						//vertices[] and normalVertices[] and push the
						//vertices into their respective lists
						newObject.vertexList.push_back(vertices[(faceDefs[i]-1) * 3 + 0]);
						newObject.vertexList.push_back(vertices[(faceDefs[i]-1) * 3 + 1]);
						newObject.vertexList.push_back(vertices[(faceDefs[i]-1) * 3 + 2]);
						newObject.normalList.push_back(normalVertices[(normalDefs[i]-1) * 3 + 0]);
						newObject.normalList.push_back(normalVertices[(normalDefs[i]-1) * 3 + 1]);
						newObject.normalList.push_back(normalVertices[(normalDefs[i]-1) * 3 + 2]);
					}
				}
				//Else something has gone wrong. We'll output some information and try to make do with what we have
				else
				{
					for(unsigned i = 0; i < 3; i++) {
						newObject.faceList.push_back(newObject.vertexList.size()+2);
                        newObject.faceList.push_back(newObject.vertexList.size()+1);
                        newObject.faceList.push_back(newObject.vertexList.size()+0);
                        
                        newObject.vertexList.push_back(vertices[(faceDefs[i]-1) * 3 + 0]);
                        newObject.vertexList.push_back(vertices[(faceDefs[i]-1) * 3 + 1]);
                        newObject.vertexList.push_back(vertices[(faceDefs[i]-1) * 3 + 2]);
                        newObject.normalList.push_back(normalVertices[(normalDefs[i]-1) * 3 + 0]);
                        newObject.normalList.push_back(normalVertices[(normalDefs[i]-1) * 3 + 1]);
                        newObject.normalList.push_back(normalVertices[(normalDefs[i]-1) * 3 + 2]);
                    }
					printf("Our obj parser is bad and we should feel bad. We couldn't parse the face defs >_<");
					printf("%d  x: %+d  y: %+d  z: %+d\n", pcount, faceDefs[0], faceDefs[1], faceDefs[2]);
					break;
				}
			}
		}
		//printf("  Finished parsing obj file: %d verts, %d face indices\n", (int)newObject.vertexList.size(), (int)newObject.faceList.size());

		//Close the .obj file
		fclose(currentFile);
	}
	else
	{
		printf("Couldn't open %s\n", fileName.c_str());
	}

	return newObject;
}


/*
* Specifies which assets should be loaded and calls appropriate functions to load models and sounds.
* Returns nothing.
*/
void loadAssets()
{
	//Define a temporary colour.
	//Longer term, we'd look at reading colours from .mtrl files listed in the .obj files we're loading, but for now we'll declare our colours here
	SDL_Colour temp = {128,128,128};

	//Load the object we're using for the ground. Its coordinates in the file are already positioned below the camera, so we don't need to lower it
	sceneryObjects.push_back(loadObj("ground.obj", temp, 0, 0, 0));

	//Load the object we're using for the builings. Their coordinates in the file are already positioned below the camera, so we don't need to lower it
	sceneryObjects.push_back(loadObj("buildings.obj", temp, 0, 0, 0));

	//Load the object we're using for the hill. Its coordinates in the file are already positioned below the camera, so we don't need to lower it
	sceneryObjects.push_back(loadObj("hill.obj", temp, 0, 0, 0));

	//Make a stack of trees to line the north side of the environment! :D
	temp = {60,128,60};
	sceneryObjects.push_back(loadObj("tree.obj", temp, 0, -100.0f, 0));
	sceneryObjects.push_back(loadObj("tree.obj", temp, -10.0f, -100.0f, 0));
	sceneryObjects.push_back(loadObj("tree.obj", temp, -20.0f, -100.0f, 0));
	sceneryObjects.push_back(loadObj("tree.obj", temp, -30.0f, -100.0f, 0));
	sceneryObjects.push_back(loadObj("tree.obj", temp, -40.0f, -100.0f, 0));
	sceneryObjects.push_back(loadObj("tree.obj", temp, -50.0f, -100.0f, 0));
	sceneryObjects.push_back(loadObj("tree.obj", temp, -60.0f, -100.0f, 0));
	sceneryObjects.push_back(loadObj("tree.obj", temp, -70.0f, -100.0f, 0));
	sceneryObjects.push_back(loadObj("tree.obj", temp, -80.0f, -100.0f, 0));
	sceneryObjects.push_back(loadObj("tree.obj", temp, -90.0f, -100.0f, 0));
	sceneryObjects.push_back(loadObj("tree.obj", temp, 10.0f, -100.0f, 0));
	sceneryObjects.push_back(loadObj("tree.obj", temp, 20.0f, -100.0f, 0));
	sceneryObjects.push_back(loadObj("tree.obj", temp, 30.0f, -100.0f, 0));
	sceneryObjects.push_back(loadObj("tree.obj", temp, 40.0f, -100.0f, 0));
	sceneryObjects.push_back(loadObj("tree.obj", temp, 50.0f, -100.0f, 0));
	sceneryObjects.push_back(loadObj("tree.obj", temp, 60.0f, -100.0f, 0));
	sceneryObjects.push_back(loadObj("tree.obj", temp, 70.0f, -100.0f, 0));
	sceneryObjects.push_back(loadObj("tree.obj", temp, 80.0f, -100.0f, 0));
	sceneryObjects.push_back(loadObj("tree.obj", temp, 90.0f, -100.0f, 0));
	
	//And another bunch of trees for the south side
	sceneryObjects.push_back(loadObj("tree.obj", temp, 0, 100.0f, 0));
	sceneryObjects.push_back(loadObj("tree.obj", temp, -10.0f, 100.0f, 0));
	sceneryObjects.push_back(loadObj("tree.obj", temp, -20.0f, 100.0f, 0));
	sceneryObjects.push_back(loadObj("tree.obj", temp, -30.0f, 100.0f, 0));
	sceneryObjects.push_back(loadObj("tree.obj", temp, -40.0f, 100.0f, 0));
	sceneryObjects.push_back(loadObj("tree.obj", temp, -50.0f, 100.0f, 0));
	sceneryObjects.push_back(loadObj("tree.obj", temp, -60.0f, 100.0f, 0));
	sceneryObjects.push_back(loadObj("tree.obj", temp, -70.0f, 100.0f, 0));
	sceneryObjects.push_back(loadObj("tree.obj", temp, -80.0f, 100.0f, 0));
	sceneryObjects.push_back(loadObj("tree.obj", temp, -90.0f, 100.0f, 0));
	sceneryObjects.push_back(loadObj("tree.obj", temp, 10.0f, 100.0f, 0));
	sceneryObjects.push_back(loadObj("tree.obj", temp, 20.0f, 100.0f, 0));
	sceneryObjects.push_back(loadObj("tree.obj", temp, 30.0f, 100.0f, 0));
	sceneryObjects.push_back(loadObj("tree.obj", temp, 40.0f, 100.0f, 0));
	sceneryObjects.push_back(loadObj("tree.obj", temp, 50.0f, 100.0f, 0));
	sceneryObjects.push_back(loadObj("tree.obj", temp, 60.0f, 100.0f, 0));
	sceneryObjects.push_back(loadObj("tree.obj", temp, 70.0f, 100.0f, 0));
	sceneryObjects.push_back(loadObj("tree.obj", temp, 80.0f, 100.0f, 0));
	sceneryObjects.push_back(loadObj("tree.obj", temp, 90.0f, 100.0f, 0));

	//Load the components that make up the vehicle
	temp = {30, 30, 30};
	vehicleObjects.push_back(loadObj("bladder.obj", temp, 0, 0, 0));
	temp = {255, 255, 0};
	vehicleObjects.push_back(loadObj("chasis.obj", temp, 0, 0, 0));
	vehicleObjects.push_back(loadObj("fans.obj", temp, 0, 0, 0));

	//Set the initial direction and location of the vehicle so that it'll be visible on screen when the game starts
	carDirection = 180.0f;
	carY = -4.0f;


	//Load a sound that will represent the hover fan's constant sound
	sampleHover = Mix_LoadWAV(("resources" + pathSeparator + "sounds" + pathSeparator + "hovercraft.ogg").c_str());
	if(sampleHover == NULL)
	{
		fprintf(stderr, "Unable to load audio file hovercraft.ogg: %s\n", Mix_GetError());
	}
	
	//Adjust the volume of the hover sound so that it's a bit more subtle
	Mix_VolumeChunk(sampleHover, MIX_MAX_VOLUME / 8);

	//Fade in the hover sound over the first half a second of the game
	mixChannelHover = Mix_FadeInChannel(mixChannelHover, sampleHover, -1, 500);
	if(mixChannelHover == -1)
	{
		fprintf(stderr, "Unable to play audio file hovercraft.ogg: %s\n", Mix_GetError());
	}

	//Set the position of the hover sound to match the angle and distance of the vehicle
	if(!Mix_SetPosition(mixChannelHover, 0,  sqrt(pow(carX,2.0) + pow(carY, 2.0))))
	{
		printf("Error setting initial position for hover sound: %s\n", Mix_GetError());
	}


	
	//Load a sound that will represent the movement fans' sound
	sampleFans = Mix_LoadWAV(("resources" + pathSeparator + "sounds" + pathSeparator + "fan.ogg").c_str());
	if(sampleFans == NULL)
	{
		fprintf(stderr, "Unable to load audio file fan.ogg: %s\n", Mix_GetError());
	}

	//Adjust the volume of the fan sound so that it's quiet (we'll make that louder as speed increases, but for now, the vehicle is stopped)
	Mix_VolumeChunk(sampleFans, 0);


	//Fade in the hover sound over the first half a second of the game
	mixChannelFans = Mix_FadeInChannel(mixChannelFans, sampleFans, -1, 500);
	if(mixChannelFans == -1)
	{
		fprintf(stderr, "Unable to play audio file fan.ogg: %s\n", Mix_GetError());
	}

	//Set the position of the fans' sound to match the angle and distance of the vehicle
	if(!Mix_SetPosition(mixChannelFans, 0,  sqrt(pow(carX,2.0) + pow(carY, 2.0))))
	{
		printf("Error setting initial position for fan sound: %s\n", Mix_GetError());
	}
	
	//Load a repeating sound that will be played as a music track
	sampleMusic = Mix_LoadMUS(("resources" + pathSeparator + "sounds" + pathSeparator + "Funk_Game_Loop.ogg").c_str());
	if(sampleMusic == NULL)
	{
		fprintf(stderr, "Unable to play audio file Funk_Game_Loop.ogg: %s\n", Mix_GetError());
	}

	//Set the music volume so that the sound effects can be heard around it
	Mix_VolumeMusic(MIX_MAX_VOLUME / 4);

	//Set the music playing
	if(Mix_PlayMusic(sampleMusic, -1) != 0)
	{
		printf("Error playing music: %s\n", Mix_GetError());
	}
}


/*
* Shuts down the SDL subsystems.
* Returns nothing.
*/
void close()
{
	//TODO: Is there anything else we need to do here? Should we kill the GL context, etc.?

	printf("Time to quit now \\o/\n");
	SDL_DestroyWindow(win);
	win = NULL;

	Mix_Quit();
	TTF_Quit();
	SDL_Quit();
}


/*
* Calls our initialisation and asset loading functions, and then runs the main game loop.
* Returns an integer representing the quit state (zero means things exited normally).
*/
int main(int argc, char* args[])
{
	//If we have problems during the initialisation, print a message and skip running the game
	if(!init())
	{
		printf("Failed to initialize!\n");
	}
	//Else, we can get to work
	else
	{
		//Load in all our models and sounds
		loadAssets();

		//Declare an SDL_Event object that we'll use for polling for mouse and keyboard input
		SDL_Event e;

		//Turn on mouse grab
		SDL_SetRelativeMouseMode((SDL_bool)true);

		//While we want the game to continue
		while(running)
		{
			//Update the vehicle simulation to reflect the changes that should've happened since the last iteration of this loop
			updateSim();

			//While we have input events to handle (this allows us to deal with multiple events per loop iteration)
			while(SDL_PollEvent(&e) != 0)
			{
				//Switch on the type of input event we've gotten
				switch (e.type)
				{

					case SDL_MOUSEMOTION:
						//Update the camera orientation based on the relative x and y mouse movement
						handleMouseMotion(e.motion.xrel, e.motion.yrel);
						break;

					case SDL_KEYDOWN:
					case SDL_KEYUP:
						//Perform any state changes (like steering, acceleration or quitting) that result from keyboard input events
						handleKeys(e.key);
						break;

					case SDL_MOUSEBUTTONDOWN:
					case SDL_MOUSEBUTTONUP:
						//Perform any state changes or actions that result from mouse click events (this is a placeholder)
						handleMouseClick(e.button);
						break;

					case SDL_QUIT:
						//Set the running flag to false so that we'll fall out of the game loop
						running = false;
						break;

					default:
						break;
				}
			}
			
			//Bind our rendering to the primary framebuffer and tell OpenGL to render to the entire window
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glViewport(0, 0, screenWidth, screenHeight);
			
			//Clear the buffer with our background colour (a nice blue)
			glClearColor(0.5f, 0.5f, 1.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			
			//Enable depth testing - this allows a polygon's position in 3D space to determine whether it's visible or occluded (otherwise everything will render based on the order in which we're drawing things)
			glEnable(GL_DEPTH_TEST);

			//Rotate the camera to match the current camera orientation
			rotateCamera();

			//Push this matrix onto the stack so that it becomes the "default" matrix for anything afterward (this allows the scene to be rendered as though the camera has moved whilst still using normal x, y coordinates. 
			glPushMatrix();

			//Set the lighting colour and position
			updateLighting();

			//Set the position of any positional audio we have
			updateSound();

			//Render the scenery models
			renderScenery();

			//Render the vehicle models
			renderCar();

			//Render the speed and fan states as HUD elements
			renderHUD();

			//Pop the matrix so that we don't have anything lefton the stack.
			glPopMatrix();

			//Draw our freshly rendered frame to the window
			SDL_GL_SwapWindow(win);
		}
	}

	//Call our close function
	close();
	
	//Return zero since we exited normally
	return 0;
}

//Oh Windows, you scallywag.
#ifdef __MINGW64__
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	char* argv[0];
	return main(0, argv);
	
}
#endif
