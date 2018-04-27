#ifndef TEST_MODEL_CORNEL_BOX_H
#define TEST_MODEL_CORNEL_BOX_H

// Defines a simple test model: The Cornel Box
#include <glm/glm.hpp>
#include <vector>

typedef enum {Mirror, Matte, Gloss} Material_t;
using glm::vec3;
using glm::vec4;
using glm::mat3;

class Object {
	public:
		vec3 color;
		Material_t material;

		Object(vec3 color, Material_t material)
			: color(color), material(material){}
		virtual bool intersect(vec4 s,
							   vec4 dir,
							   float &dist,
							   vec4 &position) const = 0;
		virtual void scale(float L) = 0;
		virtual vec4 ComputeNormal(vec4 position) const = 0;
};


// Used to describe a triangular surface:
class Triangle : public Object
{
	public:
		vec4 v0, v1, v2, normal;
		// vec4 normal;
		vec3 color;
		Material_t material;

		Triangle( vec4 v0, vec4 v1, vec4 v2, vec3 color, Material_t material )
			: Object(color, material), v0(v0), v1(v1), v2(v2)
		{}


		bool intersect(vec4 s,
					   vec4 dir,
					   float &t,
					   vec4 &position) const {

			vec3 e1 = vec3(v1.x - v0.x, v1.y - v0.y, v1.z - v0.z);
	        vec3 e2 = vec3(v2.x - v0.x, v2.y - v0.y, v2.z - v0.z);
	        vec3 b  = vec3(s.x  - v0.x, s.y  - v0.y, s.z  - v0.z);
	        vec3 d  = vec3(dir.x, dir.y, dir.z);

	        mat3 A(-d, e1, e2);
	        vec3 x = vec3(-1, -1, -1);

			float detA = glm::determinant(A);
			float det1 = glm::determinant(mat3(b, e1, e2));
			t = det1/detA;
			if (t >= 0) {
				float det2 = glm::determinant(mat3(-d, b , e2));
				float det3 = glm::determinant(mat3(-d, e1, b ));
				float u = det2/detA;
				float v = det3/detA;

				x = vec3(t, u, v);
			}

	        // Check the inequalities that satisfy valid intersection
	        bool valid = (x[0] >= 0) &&
	        (x[1] >= 0) &&
	        (x[2] >= 0) &&
	        ((x[1] + x[2]) <= 1);
			bool found = false;
	        if (valid) {
	            found = true;
                position = v0 + (x[1] * vec4(e1.x,e1.y,e1.z, 0)) + (x[2] * vec4(e2.x,e2.y,e2.z, 0));
	        }
			return found;
	    }


		vec4 ComputeNormal(vec4 intersect) const {
			vec3 e1 = vec3(v1.x-v0.x,v1.y-v0.y,v1.z-v0.z);
			vec3 e2 = vec3(v2.x-v0.x,v2.y-v0.y,v2.z-v0.z);
			vec3 normal3 = glm::normalize( glm::cross( e2, e1 ) );
			vec4 normal;
			normal.x = normal3.x;
			normal.y = normal3.y;
			normal.z = normal3.z;
			normal.w = 1.0;
			return normal;
		}

		void scale(float L) {
			v0 *= 2/L;
			v1 *= 2/L;
			v2 *= 2/L;

			v0 -= vec4(1,1,1,1);
			v1 -= vec4(1,1,1,1);
			v2 -= vec4(1,1,1,1);

			v0.x *= -1;
			v1.x *= -1;
			v2.x *= -1;

			v0.y *= -1;
			v1.y *= -1;
			v2.y *= -1;

			v0.w = 1.0;
			v1.w = 1.0;
			v2.w = 1.0;
		}
};

class Sphere : public Object {
	public:
		float r, r2;
		vec4 center;

		Sphere( vec4 c, float r, vec3 color, Material_t material )
			: Object(color, material), r(r), r2(r*r), center(c)
			{}

		bool SolveQuadratic(const float &a,
							const float &b,
							const float &c,
							float &x0,
							float &x1) const{

		    float discr = b * b - 4 * a * c;
		    if (discr < 0) return false;
		    else if (discr == 0) x0 = x1 = - 0.5 * b / a;
		    else {
		        float q = (b > 0) ? -0.5 * (b + sqrt(discr)) : -0.5 * (b - sqrt(discr));
		        x0 = q / a;
		        x1 = c / q;
		    }
		    return true;
		}

		vec4 v3to4(vec3 v3) const {
			return vec4(v3.x, v3.y, v3.z, 1.f);
		}
		vec3 v4to3(vec4 v4) const {
			return vec3(v4.x, v4.y, v4.z);
		}

		bool intersect(vec4 s,
					   vec4 dir,
					   float &t,
					   vec4 &position) const {

			float t0, t1;
			vec3 s3 	 = this->v4to3(s);
			vec3 center3 = this->v4to3(center);
			vec3 dir3 	 = this->v4to3(dir);
			vec3 L 		 = s3 - center3;

			float a = glm::dot(dir3, dir3),
				  b = 2 * glm::dot(dir3, L) - r2,
				  c = glm::dot(L, L) - r2;

			if (!this->SolveQuadratic(a, b, c, t0, t1)) return false;
			if (t0 > t1) std::swap(t0, t1);
			if (t0 < 0) {
				t0 = t1;
				if (t0 < 0) return false;
			}
			t = t0;
			// vec3 position3 = s3 + (dir3 * t);
			position = this->v3to4(s3 + (dir3 * t));
			return true;
		}


		vec4 ComputeNormal( vec4 intersect) const{
	        vec4 normal = intersect - center;
	        vec3 normal3 = vec3(normal.x, normal.y, normal.z);
	        normal3 = glm::normalize(normal3);
	        normal = vec4(normal3.x, normal3.y, normal3.z, 1.f);
	        return normal;
	    }

		void scale(float L){}
};


// Loads the Cornell Box. It is scaled to fill the volume:
// -1 <= x <= +1
// -1 <= y <= +1
// -1 <= z <= +1
void LoadTestModel( std::vector<Object*>& objects )
{

	Material_t matte = Matte, mir = Mirror, gloss = Gloss;

	// Defines colors:
	vec3 red(    0.75f, 0.15f, 0.15f );
	vec3 yellow( 0.75f, 0.75f, 0.15f );
	vec3 green(  0.15f, 0.75f, 0.15f );
	vec3 cyan(   0.15f, 0.75f, 0.75f );
	vec3 blue(   0.15f, 0.15f, 0.75f );
	vec3 purple( 0.75f, 0.15f, 0.75f );
	vec3 white(  0.75f, 0.75f, 0.75f );

	objects.clear();
	objects.reserve( 5*2*3 );

	// ---------------------------------------------------------------------------
	// Room

	float L = 555;			// Length of Cornell Box side.

	vec4 A(L,0,0,1);
	vec4 B(0,0,0,1);
	vec4 C(L,0,L,1);
	vec4 D(0,0,L,1);

	vec4 E(L,L,0,1);
	vec4 F(0,L,0,1);
	vec4 G(L,L,L,1);
	vec4 H(0,L,L,1);

	objects.push_back( new Sphere(vec4(0.5, -0.6, 0.2 ,1), 0.2, yellow, matte));
	objects.push_back( new Sphere(vec4(-1, 0, -0.7,1), 0.26, white, mir));

	// Floor:
	objects.push_back( new Triangle( C, B, A, white, gloss ) );
	objects.push_back( new Triangle( C, D, B, white, gloss ) );

	// Left wall
	objects.push_back( new Triangle( A, E, C, green, matte ) );
	objects.push_back( new Triangle( C, E, G, green, matte ) );

	// Right wall
	objects.push_back( new Triangle( F, B, D, white, mir ) );
	objects.push_back( new Triangle( H, F, D, white, mir ) );

	// Ceiling
	objects.push_back( new Triangle( E, F, G, white, gloss ) );
	objects.push_back( new Triangle( F, H, G, white, gloss ) );

	// Back wall
	objects.push_back( new Triangle( G, D, C, purple, matte ) );
	objects.push_back( new Triangle( G, H, D, purple, matte ) );

	// ---------------------------------------------------------------------------
	// Short block

	A = vec4(290,0,114,1);
	B = vec4(130,0, 65,1);
	C = vec4(240,0,272,1);
	D = vec4( 82,0,225,1);

	E = vec4(290,165,114,1);
	F = vec4(130,165, 65,1);
	G = vec4(240,165,272,1);
	H = vec4( 82,165,225,1);


	// Front
	objects.push_back( new Triangle(E,B,A,red, matte) );
	objects.push_back( new Triangle(E,F,B,red, matte) );

	// Front
	objects.push_back( new Triangle(F,D,B,red, matte) );
	objects.push_back( new Triangle(F,H,D,red, matte) );

	// BACK
	objects.push_back( new Triangle(H,C,D,red, matte) );
	objects.push_back( new Triangle(H,G,C,red, matte) );

	// LEFT
	objects.push_back( new Triangle(G,E,C,red, matte) );
	objects.push_back( new Triangle(E,A,C,red, matte) );

	// TOP
	objects.push_back( new Triangle(G,F,E,red, matte) );
	objects.push_back( new Triangle(G,H,F,red, matte) );

	// ---------------------------------------------------------------------------
	// Tall block

	A = vec4(423,0,247,1);
	B = vec4(265,0,296,1);
	C = vec4(472,0,406,1);
	D = vec4(314,0,456,1);

	E = vec4(423,330,247,1);
	F = vec4(265,330,296,1);
	G = vec4(472,330,406,1);
	H = vec4(314,330,456,1);

	// Front
	objects.push_back( new Triangle(E,B,A,blue, gloss) );
	objects.push_back( new Triangle(E,F,B,blue, gloss) );

	// Front
	objects.push_back( new Triangle(F,D,B,blue, gloss) );
	objects.push_back( new Triangle(F,H,D,blue, gloss) );

	// BACK
	objects.push_back( new Triangle(H,C,D,blue, gloss) );
	objects.push_back( new Triangle(H,G,C,blue, gloss) );

	// LEFT
	objects.push_back( new Triangle(G,E,C,blue, gloss) );
	objects.push_back( new Triangle(E,A,C,blue, gloss) );

	// TOP
	objects.push_back( new Triangle(G,F,E,blue, gloss) );
	objects.push_back( new Triangle(G,H,F,blue, gloss) );


	// ----------------------------------------------
	// Scale to the volume [-1,1]^3

	for(uint32_t i = 0; i < objects.size(); i++) {
        objects[i]->scale(L);
    }
}

#endif
