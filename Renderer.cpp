#include "precomp.h"

Renderer::Renderer( vector<Primitive *> primitives ) : bvh( BVH( primitives ) )
{
	currentIteration = 1;

	prebuffer = new vec3[SCRWIDTH * SCRHEIGHT];
	depthbuffer = new float[SCRWIDTH * SCRHEIGHT];
	postbuffer = new vec3[SCRWIDTH * SCRHEIGHT];

	for ( unsigned i = 0; i < SCRWIDTH * SCRHEIGHT; i++ )
	{
		prebuffer[i] = vec3( 0.f, 0.f, 0.f );
		depthbuffer[i] = 0.f;
		postbuffer[i] = vec3( 0.f, 0.f, 0.f );
	}

	kernel = new float[3 * 3];

	kernel[0] = 1.f / 9.f;
	kernel[1] = 1.f / 9.f;
	kernel[2] = 1.f / 9.f;

	kernel[3] = 1.f / 9.f;
	kernel[4] = 1.f / 9.f;
	kernel[5] = 1.f / 9.f;

	kernel[6] = 1.f / 9.f;
	kernel[7] = 1.f / 9.f;
	kernel[8] = 1.f / 9.f;

	buffer = new Pixel[SCRWIDTH * SCRHEIGHT];

	for ( unsigned y = 0; y < SCRHEIGHT; y += TILESIZE )
	{
		for ( unsigned x = 0; x < SCRWIDTH; x += TILESIZE )
		{
			tuple<int, int> element = make_pair( x, y );
			tiles.push_back( element );
		}
	}

	this->primitives = primitives;
}

Renderer::~Renderer()
{
	for ( unsigned i = 0; i < primitives.size(); i++ )
	{
		delete primitives[i];
	}

	delete[] prebuffer;
	prebuffer = nullptr;

	delete[] buffer;
	buffer = nullptr;
}

void Renderer::renderFrame( bool bvh_debug )
{
	if ( currentIteration < ITERATIONS )
	{
#pragma omp parallel for
		for ( int i = 0; i < tiles.size(); i++ )
		{
			int x = get<0>( tiles[i] );
			int y = get<1>( tiles[i] );

			for ( unsigned dy = 0; dy < TILESIZE; dy++ )
			{
				for ( unsigned dx = 0; dx < TILESIZE; dx++ )
				{
					if ( ( x + dx ) < SCRWIDTH && ( y + dy ) < SCRHEIGHT )
					{
						prebuffer[( y + dy ) * SCRWIDTH + ( x + dx )] += shootRay( x + dx, y + dy, MAXRAYDEPTH, bvh_debug );
					}
				}
			}
		}
		currentIteration++;
	}
	else
	{
		// Prevent stupidly high framerate
		Sleep( ( 1.f / MAX_IDLE_FPS ) * 1000 );
	}
}

void Renderer::invalidatePrebuffer()
{
	for ( size_t i = 0; i < SCRWIDTH * SCRHEIGHT; i++ )
	{
		prebuffer[i] = vec3();
		depthbuffer[i] = 0.f;
		postbuffer[i] = vec3();
	}

	currentIteration = 1;
}

void Renderer::setCamera( Camera cam )
{
	this->cam = cam;
}

Camera *Renderer::getCamera()
{
	return &cam;
}

void Renderer::moveCam( vec3 vec )
{
	invalidatePrebuffer();
	cam.move( vec );
}

void Renderer::rotateCam( vec3 vec )
{
	invalidatePrebuffer();
	cam.rotate( vec );
}

void Renderer::zoomCam( float deltaZoom )
{
	invalidatePrebuffer();
	cam.zoom( deltaZoom, true );
}

void Renderer::changeAperture( float deltaAperture )
{
	invalidatePrebuffer();
	cam.changeAperture( deltaAperture, true );
}

void Renderer::focusCam()
{
	invalidatePrebuffer();
	Hit h = bvh.intersect( cam.focusRay() );

	cam.focusDistance = h.t;
}

Pixel *Renderer::getOutput() const
{
	// currentSample - 1 because it is increased in the renderFrame() function in preparation of the next frame.
	// Unfortunately, we are getting the current frame, so we get currentSample - 1.
	float importance = 1.f / float( currentIteration - 1 );

#pragma omp parallel for
	for ( int y = 0; y < SCRHEIGHT; y++ )
	{
		for ( int x = 0; x < SCRWIDTH; x++ )
		{
			vec3 value = vec3();
			float depth = depthbuffer[y * SCRHEIGHT + x];
			// Kernel
			for ( int dy = -1; dy < 2; dy++ )
			{
				for ( int dx = -1; dx < 2; dx++ )
				{
					if ( ( y + dy > 0 && y + dy < SCRHEIGHT ) && ( x + dx > 0 && x + dx < SCRWIDTH ) )
					{
						if ( abs( depthbuffer[( y + dy ) * SCRHEIGHT + ( x + dx )] - depth ) < FILTERBIAS )
						{
							value += prebuffer[( y + dy ) * SCRHEIGHT + ( x + dx )] * importance * kernel[( dy + 1 ) * 3 + ( dx + 1 )];
						}
						else
						{
							value += prebuffer[y * SCRHEIGHT + x] * importance * kernel[( dy + 1 ) * 3 + ( dx + 1 )];
						}
					}
				}
			}

			postbuffer[y * SCRHEIGHT + x] = value;
		}
	}

	for ( unsigned i = 0; i < SCRWIDTH * SCRHEIGHT; i++ )
	{
		buffer[i] = rgb( gammaCorrect( postbuffer[i] ) );
	}

	return buffer;
}

vec3 Renderer::shootRay( unsigned x, unsigned y, unsigned depth, bool bvh_debug ) const
{
	Ray r = cam.getRay( x, y );
	return shootRay( x, y, r, depth, bvh_debug );
}

__inline void clampFloat( float &val, float lo, float hi )
{
	if ( val > hi )
	{
		val = hi;
	}
	else if ( val < lo )
	{
		val = lo;
	}
}

vec3 getPointOnHemi()
{
	vec3 point;

	do
	{
		point = 2.f * vec3( Rand( 1.f ), Rand( 1.f ), Rand( 1.f ) ) - vec3( 1.f, 1.f, 1.f );
	} while ( point.sqrLentgh() >= 1 );

	return point;
}

void createLocalCoordinateSystem( const vec3 &N, vec3 &Nt, vec3 &Nb )
{
	// This came out of my head - maybe there is better option
	// However, two other things tested did not work (scratchapixel & gpu blog)
	if ( abs( N.z ) > EPSILON )
	{
		Nt.x = 1.5 * N.x; // arbitary
		Nt.y = 1.5 * N.y; // arbitary
		Nt.z = -( Nt.x * N.x + Nt.y * N.y ) / N.z;
	}
	else if ( abs( N.y ) > EPSILON )
	{
		Nt.x = 1.5 * N.x; // arbitary
		Nt.z = 1.5 * N.z; // arbitary
		Nt.y = -( Nt.x * N.x + Nt.z * N.z ) / N.y;
	}
	else
	{
		Nt.y = 1.5 * N.y; // arbitary
		Nt.z = 1.5 * N.z; // arbitary
		Nt.x = -( Nt.y * N.y + Nt.z * N.z ) / N.x;
	}
	Nt = normalize( Nt );
	Nb = normalize( cross( N, Nt ) );
}

vec3 calculateDiffuseRayDir( const vec3 &N, const vec3 &Nt, const vec3 &Nb )
{
	// Sample the random point on unit hemisphere
	vec3 pointOnHemi = getPointOnHemi();

	// Transform point vector to the local coordinate system of the hit point
	// https://www.scratchapixel.com/lessons/3d-basic-rendering/global-illumination-path-tracing/global-illumination-path-tracing-practical-implementation
	vec3 newdir(
		pointOnHemi.x * Nb.x + pointOnHemi.y * N.x + pointOnHemi.z * Nt.x,
		pointOnHemi.x * Nb.y + pointOnHemi.y * N.y + pointOnHemi.z * Nt.y,
		pointOnHemi.x * Nb.z + pointOnHemi.y * N.z + pointOnHemi.z * Nt.z );

	// Diffused ray with the calculated random direction and origin same as the hit point
	return normalize( newdir );
}

vec3 Renderer::shootRay( const unsigned x, const unsigned y, const Ray &r, unsigned depth, bool bvh_debug ) const
{
	if ( depth > MAXRAYDEPTH ) return vec3( 0.f, 0.f, 0.f );

	if ( bvh_debug )
	{
		return bvh.debug( r );
	}

	Hit closestHit = bvh.intersect( r );

	// A little hacky, but unless the entire structure is changed, this is the best way
	if ( depth == MAXRAYDEPTH )
	{
		depthbuffer[y * SCRWIDTH + x] += closestHit.t;
	}

	// No hit
	if ( closestHit.t == FLT_MAX )
	{
		return vec3( 0.f, 0.f, 0.f );
	}

	// Closest hit is light source
	if ( closestHit.mat.type == EMIT_MAT ) return closestHit.mat.emission;

	// Create the local coordinate system of the hit point
	vec3 Nt, Nb;
	createLocalCoordinateSystem( closestHit.normal, Nt, Nb );
	// Calculate random diffused ray
	Ray diffray;
	diffray.direction = calculateDiffuseRayDir( closestHit.normal, Nt, Nb );
	diffray.origin = closestHit.coordinates + REFLECTIONBIAS * diffray.direction;

	vec3 BRDF = closestHit.mat.albedo * ( 1 / PI );
	vec3 Ei = shootRay( x, y, diffray, depth - 1, bvh_debug ) * dot( closestHit.normal, diffray.direction );

	return PI * 2.0f * BRDF * Ei;
}

Pixel Renderer::rgb( float r, float g, float b ) const
{
	clampFloat( r, 0.f, 1.f );
	clampFloat( g, 0.f, 1.f );
	clampFloat( b, 0.f, 1.f );

	unsigned char cr = r * 255;
	unsigned char cg = g * 255;
	unsigned char cb = b * 255;

	Color c;
	c.c.a = 255; // alpha
	c.c.r = cr;  // red
	c.c.g = cg;  // green
	c.c.b = cb;  // blue

	return c.pixel;
}

Pixel Renderer::rgb( vec3 vec ) const
{
	return rgb( vec.x, vec.y, vec.z );
}

union simdVector {
	__m128 v;   // SSE 4 x float vector
	float a[4]; // scalar array of 4 floats
};

vec3 Renderer::gammaCorrect( vec3 vec ) const
{
	__m128 val = _mm_set_ps( vec.x, vec.y, vec.z, vec.dummy );
	__m128 corrected = _mm_sqrt_ps( val );

	simdVector convert;
	convert.v = corrected;
	vec3 res = vec3( convert.a[3], convert.a[2], convert.a[1] );
	return res;
}
