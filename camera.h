#ifndef CAMERA_H
#define CAMERA_H
#include "hittable.h"
#include "material.h"
#include <thread>
#include <mutex>
#include <vector>
#include <chrono>
#include <iostream>
#include <SDL.h>

class camera
{
public:
	double aspect_ratio = 1.00;
	int image_width = 100;
	int samples_per_pixel = 10; // Count of random samples for each pixel
	int max_depth = 10;			// Maximum number of ray bounces into scene
	color  background;               // Scene background color
	double vfov = 90;  // Vertical view angle (field of view)
	point3 lookfrom = point3(0, 0, 0);
	point3 lookat = point3(0, 0, -1);
	vec3 vup = vec3(0, 1, 0);

	double defocus_angle = 0;  // Variation angle of rays through each pixel
	double focus_dist = 10;    // Distance from camera lookfrom point to plane of perfect focus


	/*void render(const hittable& world) {
		initialize();

		std::cout << "P3\n" << image_width << ' ' << image_height << "\n255\n";

		
		for (int j = 0; j < image_height; j++) 
		{
			std::clog << "\rScanlines remaining: " << (image_height - j) << ' ' << std::flush;
			for (int i = 0; i < image_width; i++) 
			{
				for (int i = 0; i < image_width; ++i) {
					color pixel_color(0, 0, 0);
					for (int s_j = 0; s_j < sqrt_spp; s_j++) {
						for (int s_i = 0; s_i < sqrt_spp; s_i++) {
							ray r = get_ray(i, j, s_i, s_j);
							pixel_color += ray_color(r, max_depth, world);
						}
					}
					write_color(std::cout, pixel_samples_scale * pixel_color);
				}
			}
		}	
		std::clog << "\rDone.                 \n";
	}*/

	void render(const hittable& world) {

		initialize();
		std::mutex print_mutex;

		// --- SDL2 setup ---
		if (SDL_Init(SDL_INIT_VIDEO) < 0) {
			std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << "\n";
			return;
		}

		int FIXED_WIDTH = 1280;
		int FIXED_HEIGHT = 720;

		SDL_Window* window = SDL_CreateWindow("Ray Tracer",
			SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
			FIXED_WIDTH, FIXED_HEIGHT, 0);

		SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

		// Use ARGB8888 for better color consistency
		SDL_Texture* texture = SDL_CreateTexture(renderer,
			SDL_PIXELFORMAT_ARGB8888,
			SDL_TEXTUREACCESS_STREAMING,
			image_width, image_height);

		// Calculate destination rect with preserved aspect ratio
		double renderAspect = double(image_width) / double(image_height);
		double windowAspect = double(FIXED_WIDTH) / double(FIXED_HEIGHT);

		SDL_Rect dst;
		if (renderAspect > windowAspect) {
			// Image is wider than window, fit to width
			dst.w = FIXED_WIDTH;
			dst.h = int(FIXED_WIDTH / renderAspect);
			dst.x = 0;
			dst.y = (FIXED_HEIGHT - dst.h) / 2;  // center vertically
		}
		else {
			// Image taller than window, fit to height
			dst.h = FIXED_HEIGHT;
			dst.w = int(FIXED_HEIGHT * renderAspect);
			dst.x = (FIXED_WIDTH - dst.w) / 2;   // center horizontally
			dst.y = 0;
		}

		std::vector<uint32_t> framebuffer(image_width * image_height, 0);
		std::mutex sdl_mutex;

		std::atomic<int> next_line = 0;
		std::atomic<int> lines_completed = 0;
		std::atomic<int> last_reported_percent = -1;

		int num_threads = std::thread::hardware_concurrency();
		num_threads = std::max(1, num_threads);

		auto start_time = std::chrono::high_resolution_clock::now();

		auto gamma_correct = [](double c) { return sqrt(c); }; // gamma 2.0

		// --- Worker threads ---
		auto worker = [&](int thread_id) {
			while (true) {
				int j = next_line.fetch_add(1);
				if (j >= image_height) break;

				for (int i = 0; i < image_width; ++i) {
					color pixel_color(0, 0, 0);
					for (int sj = 0; sj < sqrt_spp; ++sj) {
						for (int si = 0; si < sqrt_spp; ++si) {
							ray r = get_ray(i, j, si, sj);
							pixel_color += ray_color(r, max_depth, world);
						}
					}
					pixel_color *= pixel_samples_scale;

					// Apply gamma correction and convert to ARGB8888
					double r = std::clamp(gamma_correct(pixel_color.x()), 0.0, 0.999);
					double g = std::clamp(gamma_correct(pixel_color.y()), 0.0, 0.999);
					double b = std::clamp(gamma_correct(pixel_color.z()), 0.0, 0.999);

					uint32_t R = uint32_t(255.999 * r);
					uint32_t G = uint32_t(255.999 * g);
					uint32_t B = uint32_t(255.999 * b);

					framebuffer[j * image_width + i] = (255 << 24) | (R << 16) | (G << 8) | B;
				}

				// Update SDL texture
				{
					std::lock_guard<std::mutex> lock(sdl_mutex);
					SDL_UpdateTexture(texture, nullptr, framebuffer.data(), image_width * sizeof(uint32_t));
					SDL_RenderClear(renderer);
					SDL_RenderCopy(renderer, texture, nullptr, &dst);
					SDL_RenderPresent(renderer);
				}

				// Progress reporting
				int completed = lines_completed.fetch_add(1) + 1;
				int percent_done = (100 * completed) / image_height;
				int last = last_reported_percent.load();
				if (percent_done > last &&
					last_reported_percent.compare_exchange_strong(last, percent_done)) {
					std::lock_guard<std::mutex> lock(print_mutex);
					std::clog << "\rProgress: " << percent_done << "%" << std::flush;
				}
			}
			};

		std::vector<std::thread> threads;
		for (int i = 0; i < num_threads; ++i)
			threads.emplace_back(worker, i);

		// --- Main thread: SDL event loop while rendering ---
		bool running = true;
		while (lines_completed.load() < image_height && running) {
			SDL_Event event;
			while (SDL_PollEvent(&event)) {
				if (event.type == SDL_QUIT) running = false;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}

		for (auto& t : threads) t.join();

		auto end_time = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double> elapsed = end_time - start_time;
		std::clog << "\rProgress: 100% completed" << std::endl;
		std::clog << "Done." << std::endl;
		std::clog << "Render time: " << elapsed.count() << " seconds" << std::endl;

		// --- Keep SDL window open until user closes ---
		while (running) {
			SDL_Event event;
			while (SDL_PollEvent(&event)) {
				if (event.type == SDL_QUIT) running = false;
			}
			SDL_RenderClear(renderer);
			SDL_RenderCopy(renderer, texture, nullptr, &dst);
			SDL_RenderPresent(renderer);
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}

		SDL_DestroyTexture(texture);
		SDL_DestroyRenderer(renderer);
		SDL_DestroyWindow(window);
		SDL_Quit();
	}



private:

	int image_height;			// Rendered image height
	double pixel_samples_scale; // Color scale factor for a sum of pixel samples
	int    sqrt_spp;             // Square root of number of samples per pixel
	point3 center;				// Camera center
	point3 pixel00_loc;			// Location of the pixel 0, 0
	vec3 pixel_delta_u;			// Offset to pixel to the right
	vec3 pixel_delta_v;			// Offset to pixel below

	double recip_sqrt_spp;       // 1 / sqrt_spp
	vec3 u, v, w;				// Camera frame basis vectors
	vec3   defocus_disk_u;       // Defocus disk horizontal radius
	vec3   defocus_disk_v;       // Defocus disk vertical radius

	void initialize()
	{
		image_height = (int)(image_width / aspect_ratio);
		image_height = (image_height < 1) ? 1 : image_height; // Calculate the image height, and ensure that it's at least 1.


		sqrt_spp = int(std::sqrt(samples_per_pixel));
		pixel_samples_scale = 1.0 / (sqrt_spp * sqrt_spp);
		recip_sqrt_spp = 1.0 / sqrt_spp;

		center = lookfrom;

		// Determine viewport dimensions
		auto theta = degrees_to_radians(vfov);
		auto h = std::tan(theta / 2);
		auto viewport_height = 2 * h * focus_dist;
		auto viewport_width = viewport_height * (double(image_width) / image_height);


		// Calculate the u,v,w unit basis vectors for the camera coordinate frame.
		w = unit_vector(lookfrom - lookat);
		u = unit_vector(cross(vup, w));
		v = cross(w, u);

		// Calculate the vectors across the horizontal and down the vertical viewport edges.
		auto viewport_u = viewport_width * u; // Vector across viewport horizontal edge
		auto viewport_v = viewport_height * -v; // Vector down viewport vertical edge

		// Calculate the horizontal and vertical delta vectors from pixel to pixel.
		pixel_delta_u = viewport_u / image_width;
		pixel_delta_v = viewport_v / image_height;

		// Calculate the location of the upper left pixel.
		auto viewport_upper_left = center - (focus_dist * w) - viewport_u / 2 - viewport_v / 2;
		pixel00_loc = viewport_upper_left + 0.5 * (pixel_delta_u + pixel_delta_v);

		// Calculate the camera defocus disk basis vectors.
		auto defocus_radius = focus_dist * std::tan(degrees_to_radians(defocus_angle / 2));
		defocus_disk_u = u * defocus_radius;
		defocus_disk_v = v * defocus_radius;
	}

	point3 defocus_disk_sample() const {
		// Returns a random point in the camera defocus disk.
		auto p = random_in_unit_disk();
		return center + (p[0] * defocus_disk_u) + (p[1] * defocus_disk_v);
	}

	//vec3 sample_square() const {
	//	// Returns the vector to a random point in the [-.5,-.5]-[+.5,+.5] unit square.
	//	return vec3(random_double() - 0.5, random_double() - 0.5, 0);
	//}

	ray get_ray(int i, int j, int s_i, int s_j) const {
		// Construct a camera ray originating from the defocus disk and directed at a randomly
		// sampled point around the pixel location i, j for stratified sample square s_i, s_j.

		auto offset = sample_square_stratified(s_i, s_j);

		auto pixel_sample = pixel00_loc
			+ ((i + offset.x()) * pixel_delta_u)
			+ ((j + offset.y()) * pixel_delta_v);

		auto ray_origin = (defocus_angle <= 0) ? center : defocus_disk_sample();
		auto ray_direction = pixel_sample - ray_origin;
		auto ray_time = random_double();

		return ray(ray_origin, ray_direction, ray_time);
	}

	vec3 sample_square_stratified(int s_i, int s_j) const {
		// Returns the vector to a random point in the square sub-pixel specified by grid
		// indices s_i and s_j, for an idealized unit square pixel [-.5,-.5] to [+.5,+.5].

		auto px = ((s_i + random_double()) * recip_sqrt_spp) - 0.5;
		auto py = ((s_j + random_double()) * recip_sqrt_spp) - 0.5;

		return vec3(px, py, 0);
	}


	//color ray_color(const ray& r, int depth, const hittable& world) const 
	//{
	//	// If we have exceeded the ray bounce limit, no more light is gathered.
	//	if (depth <= 0)
	//	{
	//		return color(0, 0, 0);
	//	}
	//	hit_record rec;

	//	if (world.hit(r, interval(0.001, infinity), rec))
	//	{
	//		ray scattered;
	//		color attenuation;
	//		if (rec.mat->scatter(r, rec, attenuation, scattered))
	//		{
	//			return attenuation * ray_color(scattered, depth - 1, world);
	//		}
	//		return color(0, 0, 0);
	//	}

	//	vec3 unit_direction = unit_vector(r.direction());
	//	auto a = 0.5 * (unit_direction.y() + 1.0);
	//	return (1.0 - a) * color(1.0, 1.0, 1.0) + a * color(0.5, 0.7, 1.0);
	//}

	color ray_color(const ray& r, int depth, const hittable& world) const {
		// If we've exceeded the ray bounce limit, no more light is gathered.
		if (depth <= 0)
			return color(0, 0, 0);

		hit_record rec;

		// If the ray hits nothing, return the background color.
		if (!world.hit(r, interval(0.001, infinity), rec))
			return background;

		ray scattered;
		color attenuation;
		color color_from_emission = rec.mat->emitted(rec.u, rec.v, rec.p);

		if (!rec.mat->scatter(r, rec, attenuation, scattered))
			return color_from_emission;

		color color_from_scatter = attenuation * ray_color(scattered, depth - 1, world);

		return color_from_emission + color_from_scatter;
	}
};

#endif
