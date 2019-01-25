#include "precomp.h" // include (only) this in every .cpp file

Renderer *renderer;
int noPrim;
int noLight;

// -----------------------------------------------------------
// Initialize the application
// -----------------------------------------------------------
void Game::Init()
{
#if defined MONKEY_SCENE
	Camera cam = Camera( vec3( 0.f, -0.75f, -3.f ), vec3( 0.f, -0.75f, 0.f ), vec3( 0.f, 1.f, 0.f ), PI / 4, ( (float)SCRWIDTH / (float)SCRHEIGHT ), 0.25f, 0.5f, 3.f );

	Material monkeyMat;
	monkeyMat.type = MaterialType::LAMBERTIAN_MAT;
	monkeyMat.albedo = vec3( 0.25f, 0.25f, 0.25f );

	Material eyesRedMat;
	eyesRedMat.type = MaterialType::EMIT_MAT;
	eyesRedMat.albedo = vec3( 1.f, 0.05f, 0.05f );
	eyesRedMat.emission = eyesRedMat.albedo * 100.f;

	Material eyesGreenMat;
	eyesGreenMat.type = MaterialType::EMIT_MAT;
	eyesGreenMat.albedo = vec3( 0.05f, 1.f, 0.05f );
	eyesGreenMat.emission = eyesGreenMat.albedo * 100.f;

	Material eyesBlueMat;
	eyesBlueMat.type = MaterialType::EMIT_MAT;
	eyesBlueMat.albedo = vec3( 0.05f, 0.05f, 1.f );
	eyesBlueMat.emission = eyesBlueMat.albedo * 100.f;

	Material personMat;
	personMat.type = MaterialType::LAMBERTIAN_MAT;
	personMat.roughness = 0.f;
	personMat.albedo = vec3( 1.f, 1.f, 1.f );

	Material cillinderMat;
	cillinderMat.type = MaterialType::SPECULAR_MAT;
	cillinderMat.roughness = 0.0f;
	cillinderMat.albedo = vec3( 0.75f, 0.75f, 0.75f );

	vector<Primitive *> monkeys = loadOBJ( "assets/final/Monkeys.obj", monkeyMat );
	vector<Primitive *> cillinder = loadOBJ( "assets/final/Cillinder.obj", cillinderMat );
	vector<Primitive *> redEyes = loadOBJ( "assets/final/MonkeyEyesRed.obj", eyesRedMat );
	vector<Primitive *> blueEyes = loadOBJ( "assets/final/MonkeyEyesGreen.obj", eyesGreenMat );
	vector<Primitive *> greenEyes = loadOBJ( "assets/final/MonkeyEyesBlue.obj", eyesBlueMat );
	vector<Primitive *> person = loadOBJ( "assets/final/Person.obj", personMat );

	monkeys.insert( monkeys.end(), cillinder.begin(), cillinder.end() );
	monkeys.insert( monkeys.end(), redEyes.begin(), redEyes.end() );
	monkeys.insert( monkeys.end(), blueEyes.begin(), blueEyes.end() );
	monkeys.insert( monkeys.end(), greenEyes.begin(), greenEyes.end() );
	monkeys.insert( monkeys.end(), person.begin(), person.end() );

	Material basePlaneMat;
	basePlaneMat.type = MaterialType::LAMBERTIAN_MAT;
	basePlaneMat.albedo = vec3( 0.5f, 0.5f, 0.5f );
	monkeys.push_back( new Sphere( vec3( 0.f, 2500.f, 0.f ), 2500.f, basePlaneMat ) );

	Material glassMat;
	glassMat.type = MaterialType::DIELECTRIC_MAT;
	glassMat.albedo = vec3( 1.f );
	glassMat.attenuation = vec3();
	glassMat.ior = 1.51f;
	glassMat.roughness = 0.f;
	monkeys.push_back( new Sphere( vec3( 0.55f, -0.4f, 0.f ), 0.25f, glassMat ) );
	monkeys.push_back( new Sphere( vec3( -0.55f, -0.4f, 0.f ), 0.25f, glassMat ) );

	renderer = new Renderer( monkeys );
	noPrim = monkeys.size();
	noLight = 6;
	renderer->setCamera( cam );
#else
	Camera cam = Camera( vec3( 0.f, 0.f, -2.f ), vec3( 0.f, 0.f, 0.f ), vec3( 0.f, 1.f, 0.f ), PI / 4, ( (float)SCRWIDTH / (float)SCRHEIGHT ), 0.f, 0.5f, 1.f );

	Material mat;
	mat.type = MaterialType::SPECULAR_MAT;
	mat.roughness = 0.f;
	mat.albedo = vec3( 0.75f, 0.75f, 0.25f );
	mat.emission = vec3( 0.f, 0.f, 0.f );

	vector<Primitive *> scene;
	vector<Primitive *> person = loadOBJ( "assets/final/Person.obj", mat );

	for ( Primitive *p : person )
	{
		p->translate( vec3( 0.f, 5.f, 15.f ) );
	}

	// Light
	mat.albedo = vec3( 1.f, 1.f, 1.f );
	mat.emission = vec3( 10.f, 10.f, 10.f );
	mat.type = MaterialType::EMIT_MAT;
	scene.push_back( new Sphere( vec3( 0.f, -10.f, 15.f ), 3.f, mat ) );

	// Spheres
	mat.type = MaterialType::LAMBERTIAN_MAT;
	mat.albedo = vec3( 0.25f, 0.25f, 0.25f );
	mat.emission = vec3( 0.f, 0.f, 0.f );
	scene.push_back( new Sphere( vec3( 0.f, 1e5f - 10.f, 15.f ), 1e5f, mat ) );

	mat.albedo = vec3( 0.75f, 0.25f, 0.25f );
	mat.emission = vec3( 0.f, 0.f, 0.f );
	scene.push_back( new Sphere( vec3( 0.f, 1e5f + 5.f, 15.f ), 1e5f, mat ) );

	mat.albedo = vec3( 0.25f, 0.25f, 0.75f );
	mat.emission = vec3( 0.f, 0.f, 0.f );
	scene.push_back( new Sphere( vec3( 0.f, 0.f, 1e5f + 20.f ), 1e5f, mat ) );

	mat.albedo = vec3( 0.25f, 0.75f, 0.25f );
	mat.emission = vec3( 0.f, 0.f, 0.f );
	scene.push_back( new Sphere( vec3( -3.f, 0.f, 12.f ), 2.f, mat ) );

	mat.albedo = vec3( 0.1f, 0.3f, 0.6f );
	mat.emission = vec3( 0.f, 0.f, 0.f );
	scene.push_back( new Sphere( vec3( 4.f, -2.5f, 12.f ), 2.f, mat ) );

	mat.type = MaterialType::DIELECTRIC_MAT;
	mat.albedo = vec3( 1.f, 1.f, 1.f );
	mat.emission = vec3( 0.f, 0.f, 0.f );
	mat.roughness = 0.f;
	mat.ior = 1.51f;
	mat.attenuation = vec3();
	//scene.push_back( new Sphere( vec3( 0.f, 2.f, 15.f ), 2.f, mat ) );

	scene.insert( scene.end(), person.begin(), person.end() );

	renderer = new Renderer( scene );
	noPrim = scene.size();
	noLight = 1;
	renderer->setCamera( cam );
#endif
}

// -----------------------------------------------------------
// Close down application
// -----------------------------------------------------------
void Game::Shutdown()
{
	delete renderer;
}

bool showHelp = false;
bool BVH_DEBUG = false;

bool moveLeft = false;
bool moveRight = false;
bool moveUp = false;
bool moveDown = false;
bool moveForward = false;
bool moveBackward = false;

bool rotLeft = false;
bool rotRight = false;
bool rotUp = false;
bool rotDown = false;
bool rotCW = false;
bool rotCCW = false;

bool focusCam = false;
bool zoomIn = false;
bool zoomOut = false;
bool apertureUp = false;
bool apertureDown = false;

// -----------------------------------------------------------
// Main application tick function
// -----------------------------------------------------------
void Game::Tick( float deltaTime )
{
	// Handle input
	if ( moveLeft )
	{
		renderer->moveCam( vec3( -0.25f, 0.f, 0.f ) );
	}

	if ( moveRight )
	{
		renderer->moveCam( vec3( 0.25f, 0.f, 0.f ) );
	}

	if ( moveUp )
	{
		renderer->moveCam( vec3( 0.f, -0.25f, 0.f ) );
	}

	if ( moveDown )
	{
		renderer->moveCam( vec3( 0.f, 0.25f, 0.f ) );
	}

	if ( moveForward )
	{
		renderer->moveCam( vec3( 0.f, 0.f, 0.25f ) );
	}

	if ( moveBackward )
	{
		renderer->moveCam( vec3( 0.f, 0.f, -0.25f ) );
	}

	if ( rotLeft )
	{
		renderer->rotateCam( vec3( 0.05f, 0.f, 0.f ) );
	}

	if ( rotRight )
	{
		renderer->rotateCam( vec3( -0.05f, 0.f, 0.f ) );
	}

	if ( rotUp )
	{
		renderer->rotateCam( vec3( 0.f, -0.05f, 0.f ) );
	}

	if ( rotDown )
	{
		renderer->rotateCam( vec3( 0.f, 0.05f, 0.f ) );
	}

	if ( rotCW )
	{
		renderer->rotateCam( vec3( 0.f, 0.f, -0.05f ) );
	}

	if ( rotCCW )
	{
		renderer->rotateCam( vec3( 0.f, 0.f, 0.05f ) );
	}

	if ( focusCam )
	{
		renderer->focusCam();
	}

	if ( zoomIn )
	{
		renderer->zoomCam( 0.05f );
	}

	if ( zoomOut )
	{
		renderer->zoomCam( -0.05f );
	}

	if ( apertureUp )
	{
		renderer->changeAperture( 0.05f );
	}

	if ( apertureDown )
	{
		renderer->changeAperture( -0.05f );
	}

	// clear the graphics window
	screen->Clear( 0 ); /// I COMMENTED THAT OUT

	// Render the frame
	timer t = timer();
	renderer->renderFrame( BVH_DEBUG );
	float elapsed = t.elapsed();
	float fps = 1 / ( elapsed / 1000 );

	// Display
	screen->SetBuffer( renderer->getOutput() );
	if ( !showHelp )
	{
		screen->Print( ( "FPS: " + to_string( fps ) ).c_str(), 2, 2, 0xFFFFFF );
		screen->Print( "Press \"h\" for controls", 2, 10, 0xFFFFFF );
	}
	else
	{
		screen->Print( ( "FPS: " + to_string( fps ) + " " + to_string( noPrim ) + " Primitives, " + to_string( noLight ) + " Lights" ).c_str(), 2, 2, 0xFFFFFF );
		screen->Print( "W - Move forward\n", 2, 10, 0xFFFFFF );
		screen->Print( "S - Move back\n", 2, 18, 0xFFFFFF );
		screen->Print( "A - Move left\n", 2, 26, 0xFFFFFF );
		screen->Print( "D - Move right\n", 2, 34, 0xFFFFFF );
		screen->Print( "Q - Rotate counter clock wise\n", 2, 42, 0xFFFFFF );
		screen->Print( "E - Rotate clock wise\n", 2, 50, 0xFFFFFF );
		screen->Print( "Space - Move up\n", 2, 58, 0xFFFFFF );
		screen->Print( "Left Ctrl - Move down\n", 2, 66, 0xFFFFFF );
		screen->Print( "Move mouse or use the arrow keys to rotate camera\n", 2, 74, 0xFFFFFF );
		screen->Print( "F - Focus on center\n", 2, 82, 0xFFFFFF );
		screen->Print( "T - Zoom in\n", 2, 90, 0xFFFFFF );
		screen->Print( "G - Zoom out\n", 2, 98, 0xFFFFFF );
		screen->Print( "Z - Aperture increase\n", 2, 106, 0xFFFFFF );
		screen->Print( "X - Aperture decrease\n", 2, 114, 0xFFFFFF );
		screen->Print( "B - Show BVH\n", 2, 122, 0xFFFFFF );
		screen->Print( "X", SCRWIDTH / 2, SCRHEIGHT / 2, 0xFFFFFF );
		screen->Print( ( "Aperture: " + to_string( renderer->getCamera()->aperture ) ).c_str(), 2, SCRHEIGHT - 24, 0xFFFFFF );
		screen->Print( ( "Focal Length: " + to_string( renderer->getCamera()->focalLength ) ).c_str(), 2, SCRHEIGHT - 16, 0xFFFFFF );
		screen->Print( ( "Focus Distance: " + to_string( renderer->getCamera()->focusDistance ) ).c_str(), 2, SCRHEIGHT - 8, 0xFFFFFF );
	}
}

constexpr float rot_speed = 0.005f;

void Tmpl8::Game::MouseMove( int x, int y )
{
	renderer->rotateCam( vec3( -x * rot_speed, y * rot_speed, 0.f ) );
}

void Tmpl8::Game::KeyUp( int key )
{
	switch ( key )
	{
	case SDL_SCANCODE_D:
		moveRight = false;
		break;
	case SDL_SCANCODE_A:
		moveLeft = false;
		break;
	case SDL_SCANCODE_W:
		moveForward = false;
		break;
	case SDL_SCANCODE_S:
		moveBackward = false;
		break;
	case SDL_SCANCODE_SPACE:
		moveUp = false;
		break;
	case SDL_SCANCODE_LCTRL:
		moveDown = false;
		break;
	case SDL_SCANCODE_LEFT:
		rotLeft = false;
		break;
	case SDL_SCANCODE_RIGHT:
		rotRight = false;
		break;
	case SDL_SCANCODE_UP:
		rotUp = false;
		break;
	case SDL_SCANCODE_DOWN:
		rotDown = false;
		break;
	case SDL_SCANCODE_Q:
		rotCCW = false;
		break;
	case SDL_SCANCODE_E:
		rotCW = false;
		break;
	case SDL_SCANCODE_F:
		focusCam = false;
		break;
	case SDL_SCANCODE_T:
		zoomIn = false;
		break;
	case SDL_SCANCODE_G:
		zoomOut = false;
		break;
	case SDL_SCANCODE_Z:
		apertureUp = false;
		break;
	case SDL_SCANCODE_X:
		apertureDown = false;
		break;
	default:
		break;
	}
}

void Tmpl8::Game::KeyDown( int key )
{
	switch ( key )
	{
	case SDL_SCANCODE_D:
		moveRight = true;
		break;
	case SDL_SCANCODE_A:
		moveLeft = true;
		break;
	case SDL_SCANCODE_W:
		moveForward = true;
		break;
	case SDL_SCANCODE_S:
		moveBackward = true;
		break;
	case SDL_SCANCODE_SPACE:
		moveUp = true;
		break;
	case SDL_SCANCODE_LCTRL:
		moveDown = true;
		break;
	case SDL_SCANCODE_H:
		showHelp = !showHelp;
		break;
	case SDL_SCANCODE_B:
		renderer->invalidatePrebuffer();
		BVH_DEBUG = !BVH_DEBUG;
		break;
	case SDL_SCANCODE_LEFT:
		rotLeft = true;
		break;
	case SDL_SCANCODE_RIGHT:
		rotRight = true;
		break;
	case SDL_SCANCODE_UP:
		rotUp = true;
		break;
	case SDL_SCANCODE_DOWN:
		rotDown = true;
		break;
	case SDL_SCANCODE_Q:
		rotCCW = true;
		break;
	case SDL_SCANCODE_E:
		rotCW = true;
		break;
	case SDL_SCANCODE_F:
		focusCam = true;
		break;
	case SDL_SCANCODE_T:
		zoomIn = true;
		break;
	case SDL_SCANCODE_G:
		zoomOut = true;
		break;
	case SDL_SCANCODE_Z:
		apertureUp = true;
		break;
	case SDL_SCANCODE_X:
		apertureDown = true;
		break;
	case SDL_SCANCODE_R:
		renderer->report();
	default:
		break;
	}
}