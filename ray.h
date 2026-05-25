#ifndef RAY_H
#define RAY_H
#include "vec3.h"
class ray
{
private:
	point3 org;
	vec3 dir;
	double tm;
public:
	ray() {}
	ray(const point3& origin, const vec3& direction, double time) : org(origin), dir(direction), tm(time) {}
	ray(const point3& origin, const vec3& direction)
		: ray(origin, direction, 0) {
	}
	const point3& origin() const { return org; }
	const vec3& direction() const { return dir; }

	double time() const { return tm; }

	point3 at(double t) const
	{
		return org +  t * dir;
	}
};

#endif