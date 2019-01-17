#include "precomp.h"

Renderer::Renderer( vector<Primitive *> primitives ) : bvh( BVH( primitives ) )
{
	currentSample = 1;

	prebuffer = new vec3[SCRWIDTH * SCRHEIGHT];
	for ( unsigned i = 0; i < SCRWIDTH * SCRHEIGHT; i++ )
	{
		prebuffer[i] = vec3();
	}

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

	for ( unsigned i = 0; i < lights.size(); i++ )
	{
		delete lights[i];
	}

	delete[] prebuffer;
	prebuffer = nullptr;

	delete[] buffer;
	buffer = nullptr;
}

void Renderer::renderFrame()
{
	if ( currentSample < SAMPLES )
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
						prebuffer[( y + dy ) * SCRWIDTH + ( x + dx )] += shootRay( x + dx, y + dy, MAXRAYDEPTH );
					}
				}
			}
		}
		currentSample++;
	}
	else
	{
		// Prevent stupidly high framerate
		Sleep( ( 1.f / MAX_IDLE_FPS ) * 1000 );
	}
}

void Renderer::setLights( vector<Light *> lights )
{
	this->lights = lights;
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
	invalidatePrebuffer();
	cam.move( vec );
}

// As preparation for iterative rendering
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
	float importance = 1.f / float( currentSample - 1 );
	for ( unsigned i = 0; i < SCRWIDTH * SCRHEIGHT; i++ )
	{
		buffer[i] = rgb( gammaCorrect( prebuffer[i] * importance ) );
	}

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

// Code adapted from Scratchapixel
inline void calcFresnel( const vec3 &rayDir, const vec3 &normal, const float &refractiveIndex, float &kr )
{
	float cosI = -1 * normal.dot( rayDir );
	float etaI = 1;
	float etaT = refractiveIndex;
	if ( cosI > 0 )
	{
		swap( etaI, etaT );
	}

	float sinT = etaI / etaT * sqrtf( max( 0.f, 1 - cosI * cosI ) );

	if ( sinT >= 1 )
	{
		// Total internal reflection
		kr = 1;
	}
	else
	{
		float cosT = sqrtf( max( 0.f, 1 - sinT * sinT ) );
		cosI = fabsf( cosI );
		float Rs = ( ( etaT * cosI ) - ( etaI * cosT ) ) / ( ( etaT * cosI ) + ( etaI * cosT ) );
		float Rp = ( ( etaI * cosI ) - ( etaT * cosT ) ) / ( ( etaI * cosI ) + ( etaT * cosT ) );

		kr = ( Rs * Rs + Rp * Rp ) / 2;
	}
}

Ray getReflectedRay( const vec3 &incoming, const vec3 &normal, const vec3 &hitLocation )
{
	Ray r;
	vec3 outgoing = incoming - 2.f * incoming.dot( normal ) * normal;
	r.origin = hitLocation + ( REFLECTIONBIAS * outgoing );
	r.direction = outgoing;

	return r;
}

Ray getRefractedRay( const float &n1, const float &n2, const vec3 &normal, const vec3 &incoming, const vec3 &hitLocation )
{
	float n = n1 / n2;
	// If we hit from inside, flip the normal
	float cosI = -1 * normal.dot( incoming );
	float cosT2 = 1.f - ( n * n ) * ( 1.f - ( cosI * cosI ) );

	Ray r;
	vec3 refractedDirection = ( n * incoming ) + ( n * cosI - sqrtf( cosT2 ) ) * normal;
	refractedDirection.normalize();
	r.origin = hitLocation + ( REFRACTIONBIAS * refractedDirection );
	r.direction = refractedDirection;

	return r;
}

vec3 Renderer::shootRay( const Ray &r, unsigned depth ) const
{
	Hit closestHit;
	closestHit.t = FLT_MAX;
#ifdef LINEAR_TRAVERSE
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
#endif
#ifdef USE_BVH
	closestHit = bvh.intersect( r );
#endif
#ifdef BVH_DEBUG
	return bvh.debug( r );
#endif

	// No hit
	if ( closestHit.t == FLT_MAX )
	{
		return vec3();
	}

	vec3 lightIntensity = vec3( AMBIENTLIGHT, AMBIENTLIGHT, AMBIENTLIGHT );

	// Shadows
	for ( Light *l : lights )
	{
		lightIntensity += shadowRay( closestHit, l );
	}

	vec3 color = vec3();

	// Reflection
	if ( depth > 0 && closestHit.mat.type == MaterialType::MIRROR_MAT )
	{
		Ray refl = getReflectedRay( r.direction, closestHit.normal, closestHit.coordinates );

		vec3 specular = shootRay( refl, depth - 1 );
		color = ( closestHit.mat.getDiffuse( closestHit.u, closestHit.v ) * lightIntensity ) * ( 1.f - closestHit.mat.spec );
		specular *= closestHit.mat.spec;
		color += specular;
	}
	// Refraction
	else if ( depth > 0 && closestHit.mat.type == MaterialType::GLASS_MAT )
	{
		vec3 refracted = vec3();
		vec3 reflected = vec3();
		float kr = 0.f;
		// Not sure why, but the -1.f factor for the normal seems to make the fresnel work better
		calcFresnel( r.direction, -1.f * closestHit.normal * closestHit.hitType, closestHit.mat.refractionIndex, kr );

		// Do we need to refract?
		if ( kr < 1.f )
		{
			Ray refr = getRefractedRay( r.refractionIndex, closestHit.mat.refractionIndex, closestHit.normal * closestHit.hitType, r.direction, closestHit.coordinates );

			vec3 attenuation = vec3( 1.f, 1.f, 1.f );
			if ( closestHit.hitType == -1 )
			{
				// If we hit from inside the object, add in Attenuation and set the refraction index on the Ray
				refr.refractionIndex = closestHit.mat.refractionIndex;

				// Calculate attenuation of the light through Beer's Law
				vec3 absorbance = ( vec3( 1.f, 1.f, 1.f ) - closestHit.mat.color ) * closestHit.mat.attenuation * -1.f * closestHit.t;
				attenuation = vec3( expf( absorbance.x ), expf( absorbance.y ), expf( absorbance.z ) );
			}

			refracted = shootRay( refr, depth - 1 );

			refracted *= attenuation;
		}

		// Do we need to reflect?
		if ( kr > 0.f )
		{
			Ray refl = getReflectedRay( r.direction, closestHit.normal, closestHit.coordinates );

			reflected = shootRay( refl, depth - 1 );
		}

		color = reflected * kr + refracted * ( 1 - kr );
	}
	// Not refractive or reflective, or we've reached the end of the allowed depth
	else
	{
		color = closestHit.mat.getDiffuse( closestHit.u, closestHit.v ) * lightIntensity;
	}

	return color;
}

vec3 Renderer::shadowRay( const Hit &h, const Light *l ) const
{
	Ray shadowRay;
	float dist;
	float intensity;
	float inverseSquare;
	vec3 dir;

	if ( l->type == LightType::DIRECTIONAL_LIGHT )
	{
		dist = FLT_MAX;
		dir = -1.f * l->direction;
		dir.normalize();
		inverseSquare = 1.f;
		intensity = l->intensity;
	}
	else if ( l->type == LightType::POINT_LIGHT )
	{
		dir = l->origin - h.coordinates;
		dist = dir.length();
		dir.normalize();
		inverseSquare = 1 / ( dist * dist );
		intensity = l->intensity;
	}
	else if ( l->type == LightType::SPOT_LIGHT )
	{
		// A spotlight is essentially a point light, but we limit the response to a specific range of incoming angles
		dir = l->origin - h.coordinates;
		dist = dir.length();
		dir.normalize();

		// The dot product of 2 vectors is the cos(theta) of  the angle, theta, between the vectors
		// As we store the fov of the spotlight in degrees, we first calculate the cos of the fov to compare to the dot product
		float angle = cos( l->fov );
		// Make dir and the light direction point in roughly the same direction to calculate the angle
		float dot = l->direction.dot( -dir );

		if ( angle > dot )
		{
			return vec3();
		}
		else
		{
			intensity = l->intensity;
			inverseSquare = 1 / ( dist * dist );
		}
	}

	float dot = h.normal.dot( dir );
	if ( dot > 0 )
	{
		shadowRay.direction = dir;
		shadowRay.origin = h.coordinates + ( SHADOWBIAS * dir );

		Hit shdw;

#ifdef LINEAR_TRAVERSE
		// Find nearest hit
		for ( Primitive *p : primitives )
		{
			Hit tmp = p->hit( shadowRay );
			if ( tmp.hitType != 0 )
			{
				if ( tmp.t < shdw.t )
				{
					shdw = tmp;
				}
			}
		}
#endif
#ifdef USE_BVH
		shdw = bvh.intersect( shadowRay );
#endif

		if ( shdw.hitType != 0 && shdw.t < dist )
		{
			return vec3();
		}
		else
		{
			return l->color * dot * intensity * inverseSquare;
		}
	}
	else
	{
		return vec3();
	}
}

void Renderer::invalidatePrebuffer()
{
	for ( size_t i = 0; i < SCRWIDTH * SCRHEIGHT; i++ )
	{
		prebuffer[i] = vec3();
	}

	currentSample = 1;
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
