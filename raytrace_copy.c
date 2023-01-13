#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

#define IMAGE_WIDTH 240
#define IMAGE_HEIGHT 180
#define SAMPLE_COUNT 256
#define BOUNCE_COUNT 8

typedef struct {
	float x, y, z;
} Vector3;

typedef struct {
	uint8_t r, g, b;
} Color8;

typedef struct {
	Vector3 center;
	float radius;
	Vector3 color;
	float metallic;
	float roughness;
	float refractiveIndex ;
	float transmission;
} Sphere;

typedef struct {
	Vector3 origin;
	Vector3 direction;
} Line;

int inSafeRange(float x) {
	float y = x < 0 ? -x : x;
	return 1e-5 < y && y < 1e5;
}

float random_float() {
	float result;
	result = ((float)rand())/RAND_MAX;
	return result;
}

Vector3 vector3_all(float a) {
	Vector3 result;
	result.x = a;
	result.y = a;
	result.z = a;
	return result;
}

Vector3 vector3_add(Vector3 a, Vector3 b) {
	Vector3 result;
	result.x = a.x + b.x;
	result.y = a.y + b.y;
	result.z = a.z + b.z;
	return result;
}

Vector3 vector3_subtract(Vector3 a, Vector3 b) {
	Vector3 result;
	result.x = a.x - b.x;
	result.y = a.y - b.y;
	result.z = a.z - b.z;
	return result;
}

Vector3 vector3_scale(Vector3 a, Vector3 b) {
	Vector3 result;
	result.x = a.x * b.x;
	result.y = a.y * b.y;
	result.z = a.z * b.z;
	return result;
}

float vector3_dot_product(Vector3 a, Vector3 b) {
	float result;
	result = a.x * b.x + a.y * b.y + a.z * b.z;
	return result;
}

Vector3 vector3_cross_product(Vector3 a, Vector3 b) {
	Vector3 result;
	result.x = a.y * b.z - a.z * b.y;
	result.y = a.z * b.x - a.x * b.z;
	result.z = a.x * b.y - a.y * b.x;
	return result;
}

float vector3_length(Vector3 a) {
	float result;
	result = sqrt(vector3_dot_product(a, a));
	return result;
}

Vector3 vector3_normalized(Vector3 a) {
	Vector3 result = {1.0, 0.0, 0.0};
	if (!(a.x == 0.0 && a.y == 0.0 && a.z == 0.0))
		result = vector3_scale(a, vector3_all(1.0 / vector3_length(a)));
	return result;
}

Vector3 vector3_reflect(Vector3 i, Vector3 n) {
	Vector3 result;
	result = vector3_subtract(i, vector3_scale(vector3_all(2.0 * vector3_dot_product(i, n)), n));
	return result;
}

Vector3 vector3_mix(Vector3 a, Vector3 b, Vector3 t){
	Vector3 result;
	result = vector3_add(a, vector3_scale(vector3_subtract(b, a), t));
	return result;
}

Vector3 vector3_random_unit_vector() {
	Vector3 result;
	result.x = 2.0 * random_float() - 1.0;
	result.y = 2.0 * random_float() - 1.0;
	result.z = 2.0 * random_float() - 1.0;
	result = vector3_normalized(result);
	return result;
}

float line_sphere_intersect(Line line, Sphere sphere) {
	float result;
	
	Vector3 d = vector3_subtract(line.origin, sphere.center);
	
	float halfB = vector3_dot_product(line.direction, d);
	float C = vector3_dot_product(d, d) - sphere.radius * sphere.radius;
	float Delta = halfB * halfB - C;
	// return the cloest intersection
	if (Delta >= 0.0)
		result = - halfB - sqrt(Delta);
	else
		result = -1.0;
	
	return result;
}

#define red (Vector3){1.0, 0.0, 0.0}
#define green (Vector3){0.0, 1.0, 0.0}
#define blue (Vector3){0.0, 0.0, 1.0}
#define white (Vector3){1.0, 1.0, 1.0}
#define black (Vector3){0.0, 0.0, 0.0}
#define skyblue (Vector3){0.529412, 0.807843, 0.921569}

#define SPHERE_COUNT 4
Sphere spheres[SPHERE_COUNT] = {
//   position                            radius  color  metallic roughness  refractive index  transmission    
	{(Vector3){   2.0,    1.0,    0.0},    1.0,    red,   1.0,     0.0,     1.45,             0.0}, 
	{(Vector3){   0.0,    1.0,    0.0},    1.0,   blue,   0.5,     0.0,     1.45,             0.0}, 
	{(Vector3){  -2.0,    1.0,    0.0},    1.0,  green,   0.0,     0.0,     1.45,             0.0}, 
	{(Vector3){   0.0, -100.0,    0.0},  100.0,  white,   0.0,     1.0,     1.45,             0.0}
};

Vector3 F_Schlick(float u, Vector3 f0) {
	return vector3_add(f0, vector3_scale(vector3_subtract(vector3_all(1.0), f0), vector3_all(pow(1.0 - u, 5.0))));
}

// incoming is toward the light
// outgoing is toward our eye
Vector3 ray_trace(Line ray) {
	Vector3 color = {1.0, 1.0, 1.0};

	for (int bounce = 0; bounce < BOUNCE_COUNT; ++bounce) {

		float distance = 100000.0;
		int hit = -1;
		for (int i = 0; i < SPHERE_COUNT; ++i) {
			float d  = line_sphere_intersect(ray, spheres[i]);
			if (0.000001 < d && d < distance) {
				distance = d;
				hit = i;
			}
		}

		if (hit < 0) {
			if (bounce == BOUNCE_COUNT - 1)
				color = vector3_scale(color, black);
			else
				color = vector3_scale(color, white);
			break;
		}

		Vector3 point = vector3_add(ray.origin, vector3_scale(ray.direction, vector3_all(distance)));
		Vector3 normal = vector3_normalized(vector3_subtract(point, spheres[hit].center));
		Vector3 baseColor = spheres[hit].color;
		float metallic = spheres[hit].metallic;
		float roughness = spheres[hit].roughness;

		float refractiveIndex = spheres[hit].refractiveIndex;

		float refractance = (1.0 - refractiveIndex) / (1.0 + refractiveIndex);
		refractance = refractance * refractance;

		Vector3 incoming = vector3_random_unit_vector();
		if (vector3_dot_product(incoming, normal) < 0.0)
			incoming = vector3_scale(incoming, vector3_all(-1.0));

		Vector3 outgoing = vector3_scale(ray.direction, vector3_all(-1.0));
		
		// TODO : make rough diffuse surface better (glitter surface)

		Vector3 reflect = vector3_reflect(ray.direction, normal);
		reflect = vector3_add(reflect, vector3_scale(vector3_random_unit_vector(), vector3_all(roughness)));
		reflect = vector3_normalized(reflect);

		// 1:pi is just an arbitrary specular:diffuse ratio I choose for plastic  
		if (random_float() < (M_1_PI + (1 - M_1_PI) * metallic))
			incoming = reflect;

		Vector3 halfway = vector3_normalized(vector3_add(incoming, outgoing));
		float LoH = vector3_dot_product(incoming, halfway);

		Vector3 specular = F_Schlick(LoH, vector3_mix(vector3_all(refractance), baseColor, vector3_all(metallic)));
		Vector3 diffuse = vector3_scale(baseColor, vector3_all((1.0 - metallic)));

		ray.origin = point;
		ray.direction = incoming;

		color  = vector3_scale(color, vector3_add(specular, diffuse));
	}

	return color;
}

Color8 image[IMAGE_WIDTH * IMAGE_HEIGHT];
int main() {

	for (int y = 0; y < IMAGE_HEIGHT; ++y) {
 		for (int x = 0; x < IMAGE_WIDTH; ++x) {
			
			Line ray;
			ray.origin.x = 0.0;
			ray.origin.y = 1.0;
			ray.origin.z = 5.0;
			
			Vector3 color = {0.0, 0.0, 0.0};
			for (int i = 0; i < SAMPLE_COUNT; ++i) {
				ray.direction.x = (random_float() - (IMAGE_WIDTH  / 2) + x) / IMAGE_HEIGHT;
				ray.direction.y = (random_float() + (IMAGE_HEIGHT / 2) - y) / IMAGE_HEIGHT;
				ray.direction.z = -1.0;
				ray.direction = vector3_normalized(ray.direction);

				color = vector3_add(color, vector3_scale(ray_trace(ray), vector3_all(1.0 / (float)SAMPLE_COUNT)));
			}

			color.x = color.x / (color.x + 1.0);
			color.y = color.y / (color.y + 1.0);
			color.z = color.z / (color.z + 1.0);

			color.x = pow(color.x, 1.0 / 2.2);
			color.y = pow(color.y, 1.0 / 2.2);
			color.z = pow(color.z, 1.0 / 2.2);

			Color8 pixel;
			pixel.r = 0.0 < color.x ? color.x < 1.0 ? (uint8_t)(255.0 * color.x) : 255 : 0;
			pixel.g = 0.0 < color.y ? color.y < 1.0 ? (uint8_t)(255.0 * color.y) : 255 : 0;
			pixel.b = 0.0 < color.z ? color.z < 1.0 ? (uint8_t)(255.0 * color.z) : 255 : 0;

			image[y * IMAGE_WIDTH + x] = pixel;
		}
	}

	stbi_write_png("image/image1.png", IMAGE_WIDTH, IMAGE_HEIGHT, 3, image, 0);
	return 0;
}
