## Overview

This code renders animations of a Schwarzschild black hole and its accretion disk using ray tracing accelerated with OpenMP.

The code traces light rays (null geodesics) backwards from a virtual camera through the curved spacetime described by the Schwarzschild metric. It numerically integrates the ray paths using the Runge-Kutta 4th order (RK4) method.

When a ray intersects the accretion disk or escapes to infinity, a color is determined based on procedural shaders simulating temperature gradients, relativistic Doppler beaming (approximation), turbulence, and a background starfield. The final image incorporates High Dynamic Range (HDR) rendering with Reinhard tone mapping for better handling of the intense brightness near the event horizon.

## Features

- **OpenMP Acceleration:** Ray tracing is parallelized across CPU cores using OpenMP for significant speedups on multi-core processors.
- **Gravitational Lensing:** Simulates the bending of light according to the Schwarzschild metric (non-rotating black hole).
- **Accretion Disk Simulation:**
  - Flat, planar disk model.
  - Procedural shading based on radial distance (temperature gradient).
  - Approximated Doppler beaming effect (brighter approaching side, dimmer receding side).
  - Tunable turbulence patterns (brightness and color variations).
- **Numerical Ray Tracing:** Uses RK4 integration for accurate path calculation.
- **HDR Rendering:** Calculates high dynamic range colors internally.
- **Tone Mapping:** Uses Reinhard tone mapping to convert HDR results to standard LDR images.
- **Image Output:** Saves frames as PNG or JPG using the `stb_image_write` library.
- **Animation Support:** Can render sequences of frames with a moving camera (example orbit included).
- **Basic Starfield:** Simple procedural background star generation.

## Requirements

- **C++ Compiler with OpenMP Support:** A modern C++ compiler supporting C++17 (required for `<filesystem>`) and OpenMP.
- **(Optional) FFmpeg:** Required only if you want to combine the rendered animation frames into a video file. [Download here](https://ffmpeg.org/download.html) or install via your system's package manager.

## Setup and Compilation

1.  **Clone the Repository (if applicable) or ensure you have the source code.**

    ```bash
    git clone https://github.com/VyoJ/Gargantua.git
    cd Gargantua/OpenMP
    ```

2.  **Compile using your C++ compiler:**
    Open a terminal or command prompt in the directory containing `blackhole_video.cpp`.

    ```bash
    g++ blackhole_video.cpp -o blackhole_animator_omp -std=c++17 -O3 -fopenmp -lm
    ```

    - `blackhole_video.cpp`: Your input source file.
    - `-o blackhole_animator_omp` / `/Fe:blackhole_animator_omp.exe`: The name of the output executable file.
    - `-std=c++17` / `/std:c++17`: Enables C++17 features (needed for `<filesystem>`).
    - `-O3` / `/O2`: Enables optimizations.
    - `-fopenmp` / `/openmp`: Enables OpenMP support.
    - `-lm` (GCC/Clang): Links the math library.
    - `/EHsc` (MSVC): Specifies the exception handling model.

## Running the Code

1.  **Execute the Compiled Program:**

    - On Linux/macOS: `./blackhole_animator_omp`
    - On Windows: `.\blackhole_animator_omp.exe`

2.  **Output:**
    - The program will print status information to the console, including the number of OpenMP threads used.
    - It will create a folder (default: `blackhole_frames_omp`) in the current directory.
    - Rendered PNG images (`frame_0000.png`, `frame_0001.png`, ...) will be saved inside this folder.

## Generating Animations

1.  **Set Animation Parameters:** Modify the constants near the top of the `main` function in the `blackhole_video.cpp` file:

    - `NUM_FRAMES`: Total number of frames to render.
    - `OUTPUT_FOLDER`: Name of the directory to save frames.
    - `orbit_radius`, `orbit_elevation`, `start_angle_deg`, `end_angle_deg`: Control the camera path (currently a simple circular orbit).
    - `width`, `height`: Resolution of the output frames.

2.  **Compile and Run:** As described above. This will generate the sequence of image frames.

3.  **Combine Frames into Video (using FFmpeg):**
    - Open a terminal or command prompt.
    - Navigate to the directory _containing_ the `OUTPUT_FOLDER` (e.g., `Gargantua/OpenMP`).
    - Run the FFmpeg command (adjust parameters as needed):
      ```bash
      # -framerate: Output video FPS
      # -i: Input files (%04d means 4-digit zero-padded numbers)
      # -c:v: Video codec (libx264 is common)
      # -pix_fmt: Pixel format for compatibility
      # -crf: Quality setting (lower = higher quality, larger file; 18-28 is a common range)
      # output.mp4: Output video filename
      ffmpeg -framerate 24 -i blackhole_frames_omp/frame_%04d.png -c:v libx264 -pix_fmt yuv420p -crf 20 blackhole_orbit_omp.mp4
      ```