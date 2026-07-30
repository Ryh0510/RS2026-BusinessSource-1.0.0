#include <iostream>
#include <string>

// stb header (do not define IMPLEMENTATION here).
#include <stb_image.h>

#include <data_path.h>

int main()
{
    std::string data_path = DATA_PATH;
    std::string filename = data_path + "/G1.jpg";  // Place an image in the same directory as the executable.

    int width = 0;
    int height = 0;
    int channels = 0;

    // The fourth parameter, 0, keeps the original channel count.
    unsigned char* data = stbi_load(filename.c_str(), &width, &height, &channels, 0);

    if (!data)
    {
        std::cout << "Failed to load image: " << filename << "\n";
        std::cout << "Reason: " << stbi_failure_reason() << "\n";
        return -1;
    }

    std::cout << "Image loaded successfully!\n";
    std::cout << "Width: " << width << "\n";
    std::cout << "Height: " << height << "\n";
    std::cout << "Channels: " << channels << "\n";

    // Example: access the first few pixels.
    int pixel_count = width * height;
    std::cout << "First pixel values: ";
    for (int i = 0; i < channels; ++i)
        std::cout << (int)data[i] << " ";
    std::cout << "\n";

    // Free the image memory (required).
    stbi_image_free(data);

    return 0;
}
