#include "precomp.h"

Renderer::Renderer( vector<Primitive *> primitives ) : bvh( BVH( primitives ) )
{
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

	delete[] buffer;
	buffer = nullptr;
}

void Renderer::renderFrame()
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
					vec3 color = shootRay( x + dx, y + dy, MAXRAYDEPTH );
					buffer[( y + dy ) * SCRWIDTH + ( x + dx )] = rgb( color );
				}
			}
		}
	}
}

void Renderer::setCamera( Camera cam )
{
	this->cam = cam;
}

Camera *Renderer::getCamera()
{
	return &cam;
}

// As preparation for iterative rendering
void Renderer::moveCam( vec3 vec )
{
	cam.move( vec );
}

// As preparation for iterative rendering
void Renderer::rotateCam( vec3 vec )
{
	cam.rotate( vec );
}

Pixel *Renderer::getOutput() const
{
	return buffer;
}

vec3 Renderer::shootRay( unsigned x, unsigned y, unsigned depth ) const
{
	Ray r = cam.getRay( x, y );
	return shootRay( r, depth );
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

// FROM Ray-tracer:
//Ray getReflectedRay( const vec3 &incoming, const vec3 &normal, const vec3 &hitLocation )
//{
//	Ray r;
//	vec3 outgoing = incoming - 2.f * incoming.dot( normal ) * normal;
//	r.origin = hitLocation + ( REFLECTIONBIAS * outgoing );
//	r.direction = outgoing;
//
//	return r;
//}

// Random generator - needs to move somewhere else (?)
std::random_device rd;
std::mt19937 mt( rd() );
std::uniform_real_distribution<float> uniform_dist( 0.f, 1.f );

vec3 getPointOnHemi()
{
	float r1 = uniform_dist( mt );
	float r2 = uniform_dist( mt );

	Sample s;
	// vec3 point = s.cosineSampleHemisphere( r1, r2 ); // Does not work as expected!
	vec3 point = s.uniformSampleHemisphere( r1, r2 );

	return point;
}

void createLocalCoordinateSystem( const vec3 &N, vec3 &Nt, vec3 &Nb )
{
	// This came out of my head - maybe there is better option
	// However, two other things tested did not work (scratchapixel & gpu blog)
	if ( abs(N.z) > EPSILON )
	{
		Nt.x = 1.5 * N.x; // arbitary
		Nt.y = 1.5 * N.y; // arbitary
		Nt.z = -( Nt.x * N.x + Nt.y * N.y ) / N.z;
	}
	else if ( abs(N.y) > EPSILON )
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
	Nb = normalize(cross( N, Nt ));
}

vec3 Renderer::shootRay( const Ray &r, unsigned depth ) const
{
	vec3 directDiffuse = vec3(0.f, 0.f, 0.f);


	Hit closestHit;
	closestHit.t = FLT_MAX;
	// Find nearest hit
	for ( Primitive *p : primitives )
	{
		Hit tmp = p->hit( r );
		if ( tmp.hitType != 0 )
		{
			if ( tmp.t < closestHit.t )
			{
				closestHit = tmp;
			}
		}
	}

	// No hit
	if ( closestHit.t == FLT_MAX )
	{
		return vec3( 0.f, 0.f, 0.f );
	}

	// Closest hit is light source
	if ( closestHit.mat.type == EMIT_MAT ) return closestHit.mat.albedo;

	// Create the local coordinate system of the hit point
	vec3 Nt, Nb;
	createLocalCoordinateSystem( closestHit.normal, Nt, Nb );
	for ( int i = 0; i < SAMPLES; ++i )
	{
		// Sample the random point on unit hemisphere
		vec3 pointOnHemi = getPointOnHemi();

		// Transform point vector to the local coordinate system of the hit point
		// https://www.scratchapixel.com/lessons/3d-basic-rendering/global-illumination-path-tracing/global-illumination-path-tracing-practical-implementation
		vec3 newdir(
			pointOnHemi.x * Nb.x + pointOnHemi.y * closestHit.normal.x + pointOnHemi.z * Nt.x,
			pointOnHemi.x * Nb.y + pointOnHemi.y * closestHit.normal.y + pointOnHemi.z * Nt.y,
			pointOnHemi.x * Nb.z + pointOnHemi.y * closestHit.normal.z + pointOnHemi.z * Nt.z );

		// Diffused ray with the calculated random direction and origin same as the hit point
		Ray diffray;
		diffray.direction = normalize( newdir );
		diffray.origin = closestHit.coordinates;

		// Cast the random ray and find new intersection
		Hit newHit;
		newHit.t = FLT_MAX;

		for ( Primitive *p : primitives )
		{
			Hit tmp = p->hit( diffray );
			if ( tmp.hitType != 0 )
			{
				if ( tmp.t < newHit.t )
				{
					newHit = tmp;
				}
			}
		}

		// No hit for the diffused ray
		if ( newHit.t == FLT_MAX )
		{
			return vec3( 0.f, 0.f, 0.f );
		}

		// Does diffused ray hit a light source?
		if ( newHit.mat.type == EMIT_MAT )
		{
			vec3 BRDF = closestHit.mat.albedo * ( 1 / PI );
			vec3 cos_i = dot( diffray.direction, closestHit.normal );
			directDiffuse = BRDF * newHit.mat.emission * cos_i;
		}
	}
	
	
	return directDiffuse * 2 * (PI / SAMPLES);
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
