#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <string>
#include <stdexcept>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <chrono>
#include <omp.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

struct vec3
{
    float x, y, z;
    vec3(float _x = 0.f, float _y = 0.f, float _z = 0.f) : x(_x), y(_y), z(_z) {}
    float length() const { return sqrtf(x * x + y * y + z * z); }
    float lengthSq() const { return x * x + y * y + z * z; }
    vec3 normalize() const
    {
        float l = length();
        if (l == 0.f)
            return vec3(0, 0, 0);
        float inv_l = 1.0f / l;
        return vec3(x * inv_l, y * inv_l, z * inv_l);
    }
    vec3 operator+(const vec3 &v) const { return vec3(x + v.x, y + v.y, z + v.z); }
    vec3 operator-(const vec3 &v) const { return vec3(x - v.x, y - v.y, z - v.z); }
    vec3 operator*(float s) const { return vec3(x * s, y * s, z * s); }
    vec3 operator/(float s) const
    {
        if (fabsf(s) < 1e-9f)
            return vec3(0, 0, 0);
        float inv_s = 1.0f / s;
        return vec3(x * inv_s, y * inv_s, z * inv_s);
    }
    vec3 &operator+=(const vec3 &v)
    {
        x += v.x;
        y += v.y;
        z += v.z;
        return *this;
    }
    vec3 &operator*=(float s)
    {
        x *= s;
        y *= s;
        z *= s;
        return *this;
    }
    vec3 operator-() const { return vec3(-x, -y, -z); }
    vec3 cross(const vec3 &v) const { return vec3(y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x); }
    float dot(const vec3 &v) const { return x * v.x + y * v.y + z * v.z; }
};

inline vec3 operator*(float s, const vec3 &v) { return v * s; }

#define PI 3.141592653589793f
#define M_BH 1.0f
#define RS (2.0f * M_BH)
#define RS_SQ (RS * RS)
#define MAX_STEPS 2000
#define DT 0.05f
#define MAX_DIST 100.0f
#define HIT_DIST_SQ (RS_SQ * 1.01f)
#define DISK_INNER_R (3.0f * RS)
#define DISK_OUTER_R (15.0f * RS)
#define DISK_INNER_R_SQ (DISK_INNER_R * DISK_INNER_R)
#define DISK_OUTER_R_SQ (DISK_OUTER_R * DISK_OUTER_R)

vec3 getDiskEmissionColor(const vec3 &pos, const vec3 &view_dir_at_disk)
{
    float r = sqrtf(pos.x * pos.x + pos.y * pos.y);
    float norm_r = (r - DISK_INNER_R) / (DISK_OUTER_R - DISK_INNER_R);
    norm_r = fmaxf(0.0f, fminf(1.0f, norm_r));
    float temp_brightness = powf(1.0f - norm_r, 3.5f) + 0.03f;

    vec3 hot_color = vec3(1.0f, 1.0f, 0.8f);
    vec3 cool_color = vec3(0.8f, 0.2f, 0.0f);
    vec3 temp_color = hot_color * (1.0f - norm_r) + cool_color * norm_r;

    float doppler_modulator = (r > 1e-5f) ? (pos.x / r) : 0.0f;
    float doppler_strength = 0.75f;
    float doppler_brightness = 1.0f + doppler_modulator * doppler_strength;
    float color_shift_strength = 0.1f;
    vec3 doppler_color_shift = vec3(-0.05f, 0.05f, 0.10f) * doppler_modulator * color_shift_strength;

    float turbulence_brightness = 1.0f;

    float inner_edge_boost = 0.0f;
    float edge_sharpness = 1200.0f;
    inner_edge_boost = expf(-norm_r * edge_sharpness) * 8.0f;

    float cos_theta = fabsf(view_dir_at_disk.z);
    float grazing_boost = 1.0f + 0.5f * powf(1.0f - cos_theta, 4.0f);

    vec3 base_color = temp_color + doppler_color_shift;
    base_color.x = fmaxf(0.0f, base_color.x);
    base_color.y = fmaxf(0.0f, base_color.y);
    base_color.z = fmaxf(0.0f, base_color.z);

    float combined_brightness = temp_brightness * doppler_brightness * turbulence_brightness * grazing_boost;
    vec3 ring_color = hot_color * inner_edge_boost;
    float global_scale = 2.0f;
    return (base_color * combined_brightness + ring_color) * global_scale;
}

vec3 backgroundStars(const vec3 &dir)
{
    float hash_val = sinf(dir.x * 500.0f) * cosf(dir.y * 500.0f) * sinf(dir.z * 500.0f);
    hash_val = hash_val * hash_val;
    if (hash_val > 0.99f)
    {
        float brightness = (hash_val - 0.99f) / 0.01f;
        brightness = powf(brightness, 2.0f);
        return vec3(1.0f, 1.0f, 1.0f) * brightness * 0.8f;
    }
    return vec3(0.0f, 0.0f, 0.0f);
}

vec3 geodesic_acceleration(const vec3 &pos, const vec3 &dir)
{
    float r_sq = pos.lengthSq();
    float r = sqrtf(r_sq);
    if (r < RS * 1.001f)
        return vec3(0, 0, 0);
    vec3 grav_accel = -pos * (M_BH / (r_sq * r));
    float gr_factor = 1.0f + 1.5f * RS_SQ / r_sq;
    vec3 total_accel = grav_accel * gr_factor;
    vec3 accel_perp = total_accel - dir * dir.dot(total_accel);
    return accel_perp;
}

void integrateRK4(vec3 &pos, vec3 &dir, float dt)
{
    vec3 k1_pos = dir;
    vec3 k1_dir = geodesic_acceleration(pos, dir);

    vec3 p2 = pos + 0.5f * dt * k1_pos;
    vec3 d2 = (dir + 0.5f * dt * k1_dir).normalize();
    vec3 k2_dir = geodesic_acceleration(p2, d2);
    vec3 k2_pos = d2;

    vec3 p3 = pos + 0.5f * dt * k2_pos;
    vec3 d3 = (dir + 0.5f * dt * k2_dir).normalize();
    vec3 k3_dir = geodesic_acceleration(p3, d3);
    vec3 k3_pos = d3;

    vec3 p4 = pos + dt * k3_pos;
    vec3 d4 = (dir + dt * k3_dir).normalize();
    vec3 k4_dir = geodesic_acceleration(p4, d4);
    vec3 k4_pos = d4;

    pos += (dt / 6.0f) * (k1_pos + 2.0f * k2_pos + 2.0f * k3_pos + k4_pos);
    dir += (dt / 6.0f) * (k1_dir + 2.0f * k2_dir + 2.0f * k3_dir + k4_dir);
    dir = dir.normalize();
}

bool intersectDisk(const vec3 &p1, const vec3 &p2, vec3 &intersection_point)
{
    if (p1.z * p2.z >= 0)
        return false;

    float dz = p2.z - p1.z;
    if (fabsf(dz) < 1e-6f)
        return false;

    float t = -p1.z / dz;

    if (t < 0.0f || t > 1.0f)
        return false;

    intersection_point = p1 + t * (p2 - p1);

    float r_sq = intersection_point.x * intersection_point.x + intersection_point.y * intersection_point.y;
    return (r_sq >= DISK_INNER_R_SQ && r_sq <= DISK_OUTER_R_SQ);
}

vec3 traceRay(vec3 ray_pos, vec3 ray_dir)
{
    vec3 current_pos = ray_pos;
    vec3 current_dir = ray_dir.normalize();

    for (int step = 0; step < MAX_STEPS; ++step)
    {
        vec3 prev_pos = current_pos;
        vec3 prev_dir = current_dir;
        integrateRK4(current_pos, current_dir, DT);

        float r_sq = current_pos.lengthSq();
        if (r_sq < HIT_DIST_SQ)
            return vec3(0.0f, 0.0f, 0.0f);

        vec3 intersection_pt;
        if (intersectDisk(prev_pos, current_pos, intersection_pt))
        {
            vec3 view_direction_at_disk = -prev_dir.normalize();
            return getDiskEmissionColor(intersection_pt, view_direction_at_disk);
        }

        if (r_sq > MAX_DIST * MAX_DIST)
            return backgroundStars(current_dir);
    }

    return backgroundStars(current_dir);
}

void renderKernel(std::vector<vec3> &output_buffer, int width, int height, vec3 cam_pos, vec3 cam_look_at, vec3 cam_up, float fov)
{
    float aspect_ratio = (float)width / (float)height;
    float half_fov_tan = tanf(fov * PI / 180.0f * 0.5f);

    vec3 cam_forward = (cam_look_at - cam_pos).normalize();
    vec3 cam_right = cam_forward.cross(cam_up).normalize();
    vec3 cam_up_actual = cam_right.cross(cam_forward).normalize();

#pragma omp parallel for collapse(2) schedule(dynamic, 1)
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            float u = (2.0f * (x + 0.5f) / (float)width - 1.0f) * aspect_ratio * half_fov_tan;
            float v = (1.0f - 2.0f * (y + 0.5f) / (float)height) * half_fov_tan;

            vec3 ray_dir = (cam_forward + u * cam_right + v * cam_up_actual).normalize();
            vec3 color = traceRay(cam_pos, ray_dir);
            output_buffer[y * width + x] = color;
        }
    }
}

void saveImage(const std::string &filename, const std::vector<vec3> &buffer, int width, int height, int jpg_quality = 95)
{
    std::string ext = "";
    size_t dot_pos = filename.find_last_of('.');
    if (dot_pos != std::string::npos)
    {
        ext = filename.substr(dot_pos);
        for (char &c : ext)
        {
            c = tolower(c);
        }
    }

    const int num_components = 3;
    std::vector<unsigned char> image_data(width * height * num_components);

    const float gamma = 1.0f / 2.2f;
#pragma omp parallel for
    for (int i = 0; i < width * height; ++i)
    {
        float r = buffer[i].x;
        float g = buffer[i].y;
        float b = buffer[i].z;

        r = r / (r + 1.0f + 1e-6f);
        g = g / (g + 1.0f + 1e-6f);
        b = b / (b + 1.0f + 1e-6f);

        r = fmaxf(0.0f, fminf(1.0f, r));
        g = fmaxf(0.0f, fminf(1.0f, g));
        b = fmaxf(0.0f, fminf(1.0f, b));

        r = powf(r, gamma);
        g = powf(g, gamma);
        b = powf(b, gamma);

        image_data[i * num_components + 0] = static_cast<unsigned char>(r * 255.99f);
        image_data[i * num_components + 1] = static_cast<unsigned char>(g * 255.99f);
        image_data[i * num_components + 2] = static_cast<unsigned char>(b * 255.99f);
    }

    int success = 0;
    int stride_in_bytes = width * num_components;
    std::cout << "Attempting to save image as " << filename << "..." << std::endl;

    if (ext == ".png")
        success = stbi_write_png(filename.c_str(), width, height, num_components, image_data.data(), stride_in_bytes);
    else if (ext == ".jpg" || ext == ".jpeg")
        success = stbi_write_jpg(filename.c_str(), width, height, num_components, image_data.data(), jpg_quality);
    else
    {
        std::string fallback_filename = filename;
        if (dot_pos == std::string::npos)
            fallback_filename += ".png";
        else
            fallback_filename = filename.substr(0, dot_pos) + ".png";
        std::cerr << "Warning: Unsupported or missing image format extension. Saving as PNG: " << fallback_filename << std::endl;
        success = stbi_write_png(fallback_filename.c_str(), width, height, num_components, image_data.data(), stride_in_bytes);
        if (!success)
        {
            std::cerr << "Error: Fallback PNG save failed for " << fallback_filename << "." << std::endl;
            success = stbi_write_png("output_ultimate_fallback.png", width, height, num_components, image_data.data(), stride_in_bytes);
            if (!success)
                std::cerr << "Error: Ultimate fallback PNG save failed." << std::endl;
            return;
        }
    }

    if (success)
        std::cout << "Image saved successfully." << std::endl;
    else
        std::cerr << "Error: Failed to save image " << filename << "." << std::endl;
}

int main()
{
    const int width = 1280;
    const int height = 720;
    const float fov = 75.0f;

    const int NUM_FRAMES = 24;
    const std::string OUTPUT_FOLDER = "blackhole_frames_omp";
    const float orbit_radius = 20.0f;
    const float orbit_elevation = 4.0f;
    const float start_angle_deg = 0.0f;
    const float end_angle_deg = 360.0f;

    std::cout << "Initializing Black Hole Animation Renderer (OpenMP Version)..." << std::endl;
    std::cout << " Resolution: " << width << "x" << height << std::endl;
    std::cout << " Total Frames: " << NUM_FRAMES << std::endl;
    std::cout << " Output Folder: " << OUTPUT_FOLDER << std::endl;
    std::cout << " Using OpenMP with up to " << omp_get_max_threads() << " threads." << std::endl;

    try
    {
        if (!std::filesystem::exists(OUTPUT_FOLDER))
        {
            if (std::filesystem::create_directory(OUTPUT_FOLDER))
            {
                std::cout << "Created output directory: " << OUTPUT_FOLDER << std::endl;
            }
            else
            {
                std::cerr << "Error: Failed to create output directory: " << OUTPUT_FOLDER << std::endl;
                return 1;
            }
        }
        else
        {
            std::cout << "Output directory already exists: " << OUTPUT_FOLDER << std::endl;
        }
    }
    catch (const std::filesystem::filesystem_error &e)
    {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
        return 1;
    }

    std::vector<vec3> h_buffer(width * height);

    std::cout << "Starting frame rendering loop..." << std::endl;
    auto total_start_time = std::chrono::high_resolution_clock::now();

    for (int frame = 0; frame < NUM_FRAMES; ++frame)
    {
        std::cout << "\n--- Rendering Frame " << frame + 1 << "/" << NUM_FRAMES << " ---" << std::endl;
        auto frame_start_time = std::chrono::high_resolution_clock::now();

        float t = (NUM_FRAMES <= 1) ? 0.0f : static_cast<float>(frame) / (NUM_FRAMES - 1);
        float current_angle_rad = (start_angle_deg + t * (end_angle_deg - start_angle_deg)) * PI / 180.0f;

        vec3 cam_pos(orbit_radius * cosf(current_angle_rad),
                     orbit_radius * sinf(current_angle_rad),
                     orbit_elevation);
        vec3 cam_look_at(0.0f, 0.0f, 0.0f);
        vec3 cam_up(0.0f, 0.0f, 1.0f);

        std::cout << " Camera Pos: (" << std::fixed << std::setprecision(2) << cam_pos.x << ", " << cam_pos.y << ", " << cam_pos.z << ")" << std::endl;

        std::ostringstream filename_stream;
        filename_stream << OUTPUT_FOLDER << "/frame_"
                        << std::setw(4) << std::setfill('0') << frame
                        << ".png";
        std::string output_filename = filename_stream.str();

        renderKernel(h_buffer, width, height, cam_pos, cam_look_at, cam_up, fov);

        auto frame_end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> frame_duration = frame_end_time - frame_start_time;
        std::cout << " Frame Render Time: " << std::fixed << std::setprecision(3) << frame_duration.count() << " seconds" << std::endl;

        try
        {
            saveImage(output_filename, h_buffer, width, height);
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error during image saving for frame " << frame << ": " << e.what() << std::endl;
        }
    }

    auto total_end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> total_duration = total_end_time - total_start_time;
    std::cout << "\n--- Rendering Finished ---" << std::endl;
    std::cout << "Total rendering time for " << NUM_FRAMES << " frames: "
              << std::fixed << std::setprecision(3) << total_duration.count() << " seconds." << std::endl;
    if (NUM_FRAMES > 0)
    {
        std::cout << "Average time per frame: "
                  << std::fixed << std::setprecision(3) << total_duration.count() / NUM_FRAMES << " seconds." << std::endl;
    }

    std::cout << "Program finished successfully." << std::endl;

    return 0;
}