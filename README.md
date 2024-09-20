#### Gradient mesh simplification
This is a tool to test gradient mesh simplification driven by pixel-based (image) metrics.

#### Building
This application requires C++17 and OpenGL 4.3, as well as some external libraries. In the root directory, create a folder called `libs`. Within this folder, create separate folders for each library: 
- `glad`: for [Glad](https://glad.dav1d.de/)
- `glm`: for the [OpenGL Mathematics library](https://github.com/g-truc/glm/tree/master/glm)
- `imgui`: for the [Dear ImGui library](https://github.com/ocornut/imgui), as well as [imgui-filebrowser](https://github.com/AirGuanZ/imgui-filebrowser)
- `stb`: for the [stb_image_write header file](https://github.com/nothings/stb/blob/master/stb_image_write.h)

#### UI controls

|  | Controls |
| ----------- | ----------- |
| Zoom in/out      | Mouse scroll forward/backward or +/- keys       |
| Translate x/y      | Move mouse while holding down the middle mouse button |