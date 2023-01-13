#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

#define IMAGE_WIDTH 240
#define IMAGE_HEIGHT 180
#define SAMPLE_COUNT 8192
#define BOUNCE_COUNT 4

typedef struct {
	float x, y, z;
} Vector3;

typedef struct {
	uint8_t r, g, b;
} Color8;

typedef struct {
	Vector3 color;
	Vector3 center;
	float radius;
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
	
	if (Delta >= 0.0)
		result = - halfB - sqrt(halfB * halfB - C);
	else
		result = -1.0;
	
	return result;
}

float D_GGX(float NoH, float a) {
    float a2 = a * a;
    float f = (NoH * a2 - NoH) * NoH + 1.0;
    return a2 / (M_PI * f * f);
}

float V_SmithGGXCorrelatedFast(float NoV, float NoL, float roughness) {
    float a = roughness;
    float GGXV = NoL * (NoV * (1.0 - a) + a);
    float GGXL = NoV * (NoL * (1.0 - a) + a);
    return 0.5 / (GGXV + GGXL);
}

Vector3 F_Schlick(float u, Vector3 f0) {
	return vector3_add(f0, vector3_scale(vector3_subtract(vector3_all(1.0), f0), vector3_all(pow(1.0 - u, 5.0))));
}

Vector3 reflectance_function(
	Vector3 incoming, 
	Vector3 outgoing, 
	Vector3 normal, 
	Vector3 baseColor, 
	float metallic,
	float perceptualRoughness) 
{
	Vector3 result;

	Vector3 halfway = vector3_normalized(vector3_add(incoming, outgoing));
	float NdotH = vector3_dot_product(normal, halfway);
	float NdotI = vector3_dot_product(normal, incoming);
	float NdotR = vector3_dot_product(normal, outgoing);
	float HdotR = vector3_dot_product(halfway, outgoing);

	// NdotH = 0.0 < NdotH ? NdotH < 1.0 ? NdotH : 1.0 : 0.0;
	// NdotI = 0.0 < NdotI ? NdotI < 1.0 ? NdotI : 1.0 : 0.0;
	// NdotR = 0.0 < NdotR ? NdotR < 1.0 ? NdotR : 1.0 : 0.0;
	// HdotR = 0.0 < HdotR ? HdotR < 1.0 ? HdotR : 1.0 : 0.0;

	// NdotH = 0.0 < NdotH ? NdotH : 0.0;
	// NdotI = 0.0 < NdotI ? NdotI : 0.0;
	// NdotR = 0.0 < NdotR ? NdotR : 0.0;
	// HdotR = 0.0 < HdotR ? HdotR : 0.0;


	NdotH = 0.0 < NdotH ? NdotH : -NdotH;
	NdotI = 0.0 < NdotI ? NdotI : -NdotI;
	NdotR = 0.0 < NdotR ? NdotR : -NdotR;
	HdotR = 0.0 < HdotR ? HdotR : -HdotR;

	float roughness = perceptualRoughness * perceptualRoughness;
	Vector3 f0 = vector3_add(vector3_all(0.04 * (1.0 - metallic)), vector3_scale(baseColor, vector3_all(metallic)));

	float D = D_GGX(NdotH, roughness);
	float V = V_SmithGGXCorrelatedFast(NdotR, NdotI, roughness);
	Vector3 F = F_Schlick(HdotR, f0);

	Vector3 cookTorrance = vector3_scale(F, vector3_all((D * V) / (M_PI * NdotI * NdotR)));
	Vector3 lambertian = vector3_scale(baseColor, vector3_all((1.0 - metallic) * M_1_PI));

	result = vector3_add(lambertian, cookTorrance);

	return result;
}


#define red (Vector3){1.0, 0.0, 0.0}
#define green (Vector3){0.0, 1.0, 0.0}
#define blue (Vector3){0.0, 0.0, 1.0}
#define white (Vector3){1.0, 1.0, 1.0}
#define black (Vector3){0.0, 0.0, 0.0}
#define skyblue (Vector3){0.529412, 0.807843, 0.921569}

#define SPHERE_COUNT 2
Sphere spheres[SPHERE_COUNT] = {{red, (Vector3){0.0, 1.0, 0.0}, 1.0}, {green, (Vector3){0.0, -10.0, 0.0}, 10.0}};


Vector3 ray_trace(Line ray) {
	Vector3 color = {1.0, 1.0, 1.0};
	// incident
	Vector3 incomingRay;
	// reflected
	Vector3 outgoingRay;
	// surface
	Vector3 surfaceNormal;
	Vector3 surfacePoint;
	Vector3 surfaceColor;
	float surfaceRoughness;
	float surfaceMetallic;
	float surfaceIOR;

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
			color = vector3_scale(color, skyblue);
			break;
		}

		float cosTheta;
		Vector3 brdf;

		surfacePoint = vector3_add(ray.origin, vector3_scale(ray.direction, vector3_all(distance)));
		surfaceNormal = vector3_normalized(vector3_subtract(surfacePoint, spheres[hit].center));
		surfaceColor = spheres[hit].color;
		surfaceMetallic = 1.0;
		surfaceRoughness = 0.2;

		outgoingRay = vector3_scale(ray.direction, vector3_all(-1.0));

		incomingRay = vector3_random_unit_vector();
		if (vector3_dot_product(incomingRay, surfaceNormal) < 0.0)
			incomingRay = vector3_scale(incomingRay, vector3_all(-1.0));

		cosTheta = vector3_dot_product(incomingRay, surfaceNormal);
		brdf = reflectance_function(
			incomingRay, outgoingRay, surfaceNormal, surfaceColor, surfaceMetallic, surfaceRoughness
		);

		ray.origin = surfacePoint;
		ray.direction = incomingRay;

		// brdf * light * cosTheta;
		color  = vector3_scale(vector3_scale(color, brdf), vector3_all(cosTheta));
		// color  = brdf;
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
			ray.direction.x = (float)(x - (IMAGE_WIDTH / 2)) / IMAGE_HEIGHT;
			ray.direction.y = (float)((IMAGE_HEIGHT / 2) - y) / IMAGE_HEIGHT;
			ray.direction.z = -1.0;
			ray.direction = vector3_normalized(ray.direction);
			
			Vector3 color = {0.0, 0.0, 0.0};
			for (int i = 0; i < SAMPLE_COUNT; ++i) {
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

	stbi_write_png("image/image.png", IMAGE_WIDTH, IMAGE_HEIGHT, 3, image, 0);
	return 0;
}
