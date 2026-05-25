#ifndef HITTABLE_LIST_H
#define HITTABLE_LIST_H
#include "aabb.h"
#include "hittable.h"
#include <vector>

class hittable_list : public hittable
{
public:
	std::vector<shared_ptr<hittable>> objects;

	hittable_list() : bbox(aabb()), hit_count(0) {}
	hittable_list(shared_ptr<hittable> object) : bbox(aabb()), hit_count(0) { add(object); }

	void clear()
	{
		objects.clear();
		bbox = aabb();
		hit_count = 0;
	}

	void add(shared_ptr<hittable> object)
	{
		objects.push_back(object);
		bbox = aabb(bbox, object->bounding_box());
	}

	bool hit(const ray& r, interval ray_t, hit_record& rec) const override
	{
		hit_record temp_rec;
		bool hit_anything = false;
		auto closest_so_far = ray_t.max;

		for (const auto& object : objects)
		{
			// This loop is O(n) for n objects
			if (object->hit(r, interval(ray_t.min, closest_so_far), temp_rec)) 
			{
				hit_anything = true;
				closest_so_far = temp_rec.t;
				rec = temp_rec;
			}
		}
		return hit_anything;
	}

	aabb bounding_box() const override { return bbox; }
	const int getHitCount() { return hit_count; }
private:
	aabb bbox;
	mutable int hit_count;
};
#endif
