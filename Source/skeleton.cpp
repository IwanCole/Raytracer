#include <iostream>
#include <glm/glm.hpp>
#include <SDL.h>
#include "SDLauxiliary.h"
#include "TestModelH.h"
#include <stdint.h>
#include <limits.h>
#include <math.h>
#include <glm/gtc/random.hpp>
#include <omp.h>


using namespace std;
using glm::vec3;
using glm::mat3;
using glm::vec4;
using glm::mat4;


#define PI 3.14159265
#define SCREEN_WIDTH 1300    // 320
#define SCREEN_HEIGHT 1300   // 256
#define FULLSCREEN_MODE false
#define L_SCATTER 0.08f


// Structs and global variables
struct Intersection {
    vec4 position;
    float distance;
    int objectIndex;
    int shadowCount;
    vec3 colourBleed;
    float colourBleedAmount;
};

struct Camera {
    mat4 R;
    float F;
    vec4 position;
};

struct Light {
    vec3 colour;
    vec4 position;
};

vec4 light_origin;
Camera camera;
int softN    = 1;
bool smthF   = false;
bool darkF   = false;
bool bleed   = false;
bool mirrorF = false;
float yaw    = 0.0;
float rad    = PI / 32.f;
int LIGHT_SAMPLES = 70;


/* ----------------------------------------------------------------------------*/
/* FUNCTIONS                                                                   */
void Update( vector<Light>& light_points );

void Draw( screen* screen,
           const vector<Object*>& objects,
           const vector<Light>& light_points);

bool ClosestIntersection( const vec4 s,
                          const vec4 dir,
                          const vector<Object*>& objects,
                          Intersection& intersection,
                          const int global_lum);

vec3 DirectLight( const Intersection& intersection,
                  const vector<Object*>& objects,
                  const vector<Light>& light_points);

void GenerateLight( vector<Light>& light_points );

vec4 reflekt(const vec4 incident, const vec4 normal);

vec4 refract( const vec4 dir,
              const vector<Object*>& objects,
              Intersection& intersection);



int main( int argc, char* argv[] ) {
    // Parse runtime flags (Basic, no error/duplicate/confliction checking)
    if (argc > 1) {
        for (int i = 1; i < argc; i++) {
            if (std::string(argv[i]) == "--soft4")  softN   = 5;
            if (std::string(argv[i]) == "--soft8")  softN   = 9;
            if (std::string(argv[i]) == "--smooth") smthF   = true;
            if (std::string(argv[i]) == "--dark")   darkF   = true;
            if (std::string(argv[i]) == "--mirror") mirrorF = true;
            if (std::string(argv[i]) == "--bleed")  bleed   = true;
            if (std::string(argv[i]) == "--all-flags") {
                smthF   = true;
                darkF   = true;
                mirrorF = true;
                softN   = 9;
                bleed   = true;
            };
        }
    }

    vector<Object*> objects;
    LoadTestModel(objects);
    camera.F        = SCREEN_WIDTH;
    camera.position = vec4( 0.0, 0.0, -3.0, 1.0);

    if (!smthF) { LIGHT_SAMPLES = 1; }
    // light_origin = vec4(0, -0.5, -0.7, 1.0);
    light_origin = vec4(0.8, 0.4, -0.7, 1.0);
    vector<Light> light_points;
    GenerateLight(light_points);

    screen *screen = InitializeSDL( SCREEN_WIDTH, SCREEN_HEIGHT, FULLSCREEN_MODE );
    while( NoQuitMessageSDL() ) {
        Update(light_points);
        Draw(screen, objects, light_points);
        SDL_Renderframe(screen);
    }

    SDL_SaveImage( screen, "screenshot.bmp" );
    KillSDL(screen);
    return 0;
}


// Generate a random set of light points around a given origin
void GenerateLight( vector<Light>& light_points ) {
    light_points.clear();
    vec3 colour = 14.f * vec3(1, 1, 1);
    Light firstLight = {.colour = colour, .position = light_origin};
    light_points.push_back(firstLight);
    for (int i = 1; i < LIGHT_SAMPLES; i++) {
        float x = glm::linearRand(-L_SCATTER, L_SCATTER) + light_origin.x;
        float y = glm::linearRand(-L_SCATTER, L_SCATTER) + light_origin.y;
        float z = glm::linearRand(-L_SCATTER, L_SCATTER) + light_origin.z;
        vec4 pos = vec4(x, y, z, 1.0);
        Light newLight = {.colour = colour, .position = pos} ;
        light_points.push_back(newLight);
    }
}


// Calculate direct lighting and depth of shadows
vec3 DirectLight( const Intersection& intersection,
                  const vector<Object*>& objects,
                  const vector<Light>& light_points ) {

    // Object tri = objects[intersection.objectIndex];
    vec4 normal = objects[intersection.objectIndex]->ComputeNormal(intersection.position);
    vec3 totalColur = vec3(0, 0, 0);

    for (int i = 0; i < LIGHT_SAMPLES; i++) {
        vec4 r = normalize(light_points[i].position - intersection.position);
        vec3 colour = light_points[i].colour;
        float length_v = glm::length(light_points[i].position - intersection.position);

        Intersection shadowIntersection;
        if (ClosestIntersection(intersection.position + 0.000001f*r, r, objects, shadowIntersection, -1)) {
            if (shadowIntersection.distance <= length_v) {
                colour = vec3(0, 0, 0);
                if (darkF && shadowIntersection.shadowCount > 2) colour = vec3(-6, -6, -6);
            }
        }

        float A = (4.f * PI * length_v * length_v);
        vec3  B = colour / A;
        float C = glm::dot(r, normal);
        vec3 D = B * C;
        totalColur += D;
    }

    totalColur /= LIGHT_SAMPLES;
    return totalColur;
}


// Find the closest intersection between a ray and a triangle
// Take in start s, direction dir, and all the triangles
// Return true if intersection found, and the intersection
bool ClosestIntersection( const vec4 s,
                          const vec4 dir,
                          const vector<Object*>& objects,
                          Intersection& intersection,
                          const int global_lum) {

    intersection.distance = std::numeric_limits<float>::max();
    intersection.shadowCount = 0;
    vec4 position = vec4(0.f, 0.f, 0.f, 1.f);
    bool found = false;
    float t = intersection.distance;

    for (uint32_t i = 0; i < objects.size(); i++) {
        if (objects[i]->intersect(s, dir, t, position)) {
            if (t < intersection.distance) {
                found = true;
                intersection.shadowCount += 1;
                intersection.objectIndex = i;
                intersection.distance = t;
                intersection.position = position;
            }
        }
    }

    intersection.colourBleed = vec3(0, 0, 0);
    intersection.colourBleedAmount = 0;
    if (bleed && global_lum > 0 && global_lum < 3) {
        if (Gloss == objects[intersection.objectIndex]->material) {
            vec4 reflektor = reflekt(dir, objects[intersection.objectIndex]->ComputeNormal(intersection.position));
            Intersection bounced;
            bool found_bounced = ClosestIntersection(intersection.position + (0.000001f * reflektor), reflektor, objects, bounced, global_lum + 1);

            if (found_bounced) {
                float dist = glm::length(bounced.position - intersection.position);
                if (dist < 0.5) {
                    intersection.colourBleedAmount = 0.2f - ((dist / 0.5f) * 0.2f);
                    intersection.colourBleed = objects[bounced.objectIndex]->color;
                }
            }
        }
    }
    return found;
}


// Return the reflected angle of an incident ray onto a surface with a given normal
vec4 reflekt(const vec4 incident, const vec4 normal) {
    return incident - (2 * (glm::dot(incident, normal)) * normal );
}


// Draw the image to the screen. For each pixel, find the nearest triangle
// that the pixel's ray intersects with, and draw it
void Draw( screen* screen,
           const vector<Object*>& objects,
           const vector<Light>& light_points ) {

    memset(screen->buffer, 0, screen->height*screen->width*sizeof(uint32_t));
    const float delta_x[9] = {0, 0,    0.25,  0,   -0.25, 0.1,  0.1, -0.1, -0.1}; // SSAA change in ray X direction
    const float delta_y[9] = {0, 0.25, 0,    -0.25, 0,    0.1, -0.1, -0.1,  0.1}; // SSAA change in ray Y direction

    #pragma omp parallel for
    for (int x = 0; x < SCREEN_WIDTH; x++) {
        for (int y = 0; y < SCREEN_HEIGHT; y++) {
            vector<Intersection>intersections;
            vector<bool>founds;
            int reflektorCount = 0;

            // Generate all directions and intersections
            // If no Anti-Aliasing, then softN = 1
            // bool through_glass = false;
            for (int i = 0; i < softN; i++) {
                vec4 dir = vec4(x - SCREEN_WIDTH/2 +delta_x[i], y - SCREEN_HEIGHT/2 +delta_y[i], camera.F, 1.0);
                Intersection intersection;
                bool found = ClosestIntersection(camera.position, camera.R * dir, objects, intersection, 1);

                vec4 incident  = camera.R * dir;
                while (found && mirrorF && objects[intersection.objectIndex]->material == Mirror && reflektorCount < 3) {
                    vec4 normal    = objects[intersection.objectIndex]->ComputeNormal(intersection.position);
                    vec4 reflektor = reflekt(incident, normal);
                    vec4 oldStart  = intersection.position + (0.000001f * normal);

                    found = ClosestIntersection(oldStart, reflektor, objects, intersection, 1);
                    incident = reflektor;
                    if (i == 0) reflektorCount += 1;
                }

                // if (found && triangles[intersection.objectIndex].material == Glass) {
                //     vec4 T = refract(dir, triangles, intersection);
                //     // through_glass = true;
                //     Intersection temp;
                //     ClosestIntersection(intersection.position + (0.0001f * T), T, triangles, temp);
                //     ClosestIntersection(temp.position + (0.0001f * T), T, triangles, intersection);
                // }

                intersections.push_back(intersection);
                founds.push_back(found);
            }

            // For all found intersections, average the colour values
            vec3 colour = vec3(0, 0, 0);
            float N = 0.f;
            for (int i = 0; i < softN; i++) {
                if (founds[i]) {
                    float normalColourAmount = 1 - intersections[i].colourBleedAmount;
                    colour += objects[intersections[i].objectIndex]->color * normalColourAmount;
                    colour += intersections[i].colourBleed * intersections[i].colourBleedAmount;
                    N += 1;
                }
            }
            colour /= N;

            // Draw the pixel values on the screen
            if (founds[0]) {
                vec3 totalLight = DirectLight(intersections[0], objects, light_points) + (0.5f*vec3(1,1,1));
                colour *= totalLight;
                colour *= (1 - (0.15 * reflektorCount));
                // if (through_glass) colour *= 0.8;
                PutPixelSDL(screen, x, y, colour);
            } else {
                PutPixelSDL(screen, x, y, vec3(0.0, 0.0, 0.0));
            }
        }
    }
}


void Update( vector<Light>& light_points ) {
    static int t = SDL_GetTicks();
    int t2 = SDL_GetTicks();
    float dt = float(t2-t);
    t = t2;

    std::cout << "Render time: " << dt << " ms." << std::endl;

    bool rot = false;
    const Uint8* keystate = SDL_GetKeyboardState(NULL);
    if (keystate[SDL_SCANCODE_UP]) {
        cout << "UP\n";
        camera.position.z += 0.1;
    } else if (keystate[SDL_SCANCODE_DOWN]) {
        cout << "DOWN\n";
        camera.position.z -= 0.1;
    } else if (keystate[SDL_SCANCODE_N]) {
        cout << "MOV LEFT\n";
        camera.position.x -= 0.1;
    } else if (keystate[SDL_SCANCODE_M]) {
        cout << "MOV RIGHT\n";
        camera.position.x += 0.1;
    } else if (keystate[SDL_SCANCODE_LEFT]) {
        cout << "LEFT\n";
        yaw += rad;
        rot = true;
    } else if (keystate[SDL_SCANCODE_RIGHT]) {
        cout << "RIGHT\n";
        yaw -= rad;
        rot = true;
    } else if (keystate[SDL_SCANCODE_W]) {
        cout << "LIGHT UP\n";
        light_origin.y -= 0.1;
        GenerateLight(light_points);
    } else if (keystate[SDL_SCANCODE_S]) {
        cout << "LIGHT DOWN\n";
        light_origin.y += 0.1;
        GenerateLight(light_points);
    } else if (keystate[SDL_SCANCODE_A]) {
        cout << "LIGHT LEFT\n";
        light_origin.x -= 0.1;
        GenerateLight(light_points);
    } else if (keystate[SDL_SCANCODE_D]) {
        cout << "LIGHT RIGHT\n";
        light_origin.x += 0.1;
        GenerateLight(light_points);
    }

    if (rot) {
        camera.R = mat4(cos(yaw), 0, sin(yaw), 0,
                               0, 1, 0       , 0,
                       -sin(yaw), 0, cos(yaw), 0,
                               0, 0, 0       , 1);
    }
}


// Calculate if light is reflected or refracted in a material made of glass.
// Couldn't fully implement therefore this function is not called.
// vec4 refract(const vec4 dir, const vector<Object*>& objects, Intersection& intersection) {
//     vec4 normal = objects[intersection.objectIndex]->ComputeNormal(intersection.position);
//     float ref_air = 1.f, ref_glass = 1.5f;
//     float cosi = glm::clamp(-1.f, 1.f, glm::dot(dir, normal));
//     if (cosi < 0) {
//         cosi = -cosi;
//     } else {
//         swap(ref_air, ref_glass);
//         normal *= -1.f;
//     }
//     float ref_i = ref_air / ref_glass;
//     float k = 1.f - ref_i * ref_i * (1.f - cosi * cosi);
//     vec4 T;
//     if (k < 0) {
//         return vec4(0,0,0,1);
//     }
//     else {
//         return ref_i * dir + (ref_i * cosi - sqrt(k)) * normal;
//     }
// }
