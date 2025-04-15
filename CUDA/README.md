## Overview

The code traces light rays (null geodesics) backwards from a virtual camera through the curved spacetime described by the Schwarzschild metric. It numerically integrates the ray paths using the Runge-Kutta 4th order (RK4) method.

When a ray intersects the accretion disk or escapes to infinity, a color is determined based on procedural shaders simulating temperature gradients, relativistic Doppler beaming (approximation), turbulence, and a background starfield. The final image incorporates High Dynamic Range (HDR) rendering with Reinhard tone mapping for better handling of the intense brightness near the event horizon.

## Features

- **CUDA Acceleration:** Ray tracing is parallelized across the GPU for significant speedups.
- **Gravitational Lensing:** Simulates the bending of light according to the Schwarzschild metric (non-rotating black hole).
- **Accretion Disk Simulation:**
  - Flat, planar disk model.
  - Procedural shading based on radial distance (temperature gradient).
  - Approximated Doppler beaming effect (brighter approaching side, dimmer receding side).
  - Tunable turbulence patterns (brightness and color variations).
- **Numerical Ray Tracing:** Uses RK4 integration for accurate path calculation.
- **HDR Rendering:** Calculates high dynamic range colors internally.
- **Tone Mapping:** Uses Reinhard tone mapping to convert HDR results to standard LDR images.
- **Image Output:** Saves frames as PNG, JPG, BMP, or TGA using the `stb_image_write` library.
- **Animation Support:** Can render sequences of frames with a moving camera (example orbit included).
- **Basic Starfield:** Simple procedural background star generation.

## Requirements

- **NVIDIA GPU:** Must support CUDA.
- **CUDA Toolkit:** Version 10.x or later recommended (includes the `nvcc` compiler). [Download here](https://developer.nvidia.com/cuda-downloads)
- **C++ Compiler:** A modern C++ compiler supporting C++17 (required for `<filesystem>`).
  - GCC or Clang on Linux/macOS.
  - Visual Studio (with C++ workload and CUDA integration) on Windows.
- **`stb_image_write.h`:** A single-header library for saving images. Download it from the [stb repository](https://github.com/nothings/stb) and place it in the same directory as the `.cu` source file.
- **(Optional) FFmpeg:** Required only if you want to combine the rendered animation frames into a video file. [Download here](https://ffmpeg.org/download.html) or install via your system's package manager.

## Setup and Compilation

1.  **Clone the Repository:**

    ```bash
    git clone https://github.com/VyoJ/Gargantua.git
    cd Gargantua
    ```

2.  **Compile using `nvcc`:**
    Open a terminal or command prompt in the repository directory.

    ```bash
    nvcc blackhole_video.cu -o blackhole_animator -std=c++17 -O3 -use_fast_math --ftz=true -lcudadevrt -lcurand
    ```

    **Explanation of Compile Flags:**

    - `blackhole.cu`: Your input source file.
    - `-o blackhole_animator`: The name of the output executable file.
    - `-std=c++17`: Enables C++17 features (needed for `<filesystem>`). Use `/std:c++17` if using MSVC compiler directly with nvcc.
    - `-O3`: Enables optimizations.
    - `-use_fast_math --ftz=true`: Enables faster, potentially less precise math operations (often fine for graphics).
    - `-lcudadevrt`: Links the CUDA device runtime library (needed for device functions like curand).
    - `-lcurand`: Links the CURAND library for random number generation on the GPU.

## Running the Code

1.  **Execute the Compiled Program:**

    - On Linux/macOS: `./blackhole_animator`
    - On Windows: `.\blackhole_animator.exe`

2.  **Output:**
    - The program will print status information to the console.
    - It will create a folder (default: `blackhole_frames`) in the current directory.
    - Rendered PNG images (`frame_0000.png`, `frame_0001.png`, ...) will be saved inside this folder.

## Generating Animations

1.  **Set Animation Parameters:** Modify the constants near the top of the `main` function in the `.cu` file:

    - `NUM_FRAMES`: Total number of frames to render.
    - `OUTPUT_FOLDER`: Name of the directory to save frames.
    - `orbit_radius`, `orbit_elevation`, `start_angle_deg`, `end_angle_deg`: Control the camera path (currently a simple circular orbit).

2.  **Compile and Run:** As described above. This will generate the sequence of image frames.

3.  **Combine Frames into Video (using FFmpeg):**
    - Open a terminal or command prompt.
    - Navigate to the directory _containing_ the `OUTPUT_FOLDER`.
    - Run the FFmpeg command (adjust parameters as needed):
      ```bash
      # -framerate: Output video FPS
      # -i: Input files (%04d means 4-digit zero-padded numbers)
      # -c:v: Video codec (libx264 is common)
      # -pix_fmt: Pixel format for compatibility
      # -crf: Quality setting (lower = higher quality, larger file; 18-28 is a common range)
      # output.mp4: Output video filename
      ffmpeg -framerate 30 -i blackhole_frames/frame_%04d.png -c:v libx264 -pix_fmt yuv420p -crf 20 blackhole_orbit.mp4
      ```
