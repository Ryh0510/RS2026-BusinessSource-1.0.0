#include <StringToFile/StringToFile.h>
#include <iostream>

int main()
{
    const char* filename = "test.bin";

    // ================= Write file =================
    {
        std::ofstream ofs(filename, std::ios::binary);
        if (!ofs) {
            std::cout << "Open file for write failed\n";
            return -1;
        }

        std::string s1 = "Hello OpenGL";
        std::string s2 = "String serialization demo";
        std::string s3 = "1234567890";

        StringToFile::save_string(ofs, s1);
        StringToFile::save_string(ofs, s2);
        StringToFile::save_string(ofs, s3);

        std::cout << "Write done.\n";
    }

    // ================= Read file (normal path) =================
    {
        std::ifstream ifs(filename, std::ios::binary);
        if (!ifs) {
            std::cout << "Open file for read failed\n";
            return -1;
        }

        std::string a, b, c;

        StringToFile::load_string(ifs, a);
        StringToFile::load_string(ifs, b);
        StringToFile::load_string(ifs, c);

        std::cout << "Read strings:\n";
        std::cout << "A = " << a << "\n";
        std::cout << "B = " << b << "\n";
        std::cout << "C = " << c << "\n";
    }

    // ================= Safe read example =================
    {
        std::ifstream ifs(filename, std::ios::binary);
        if (!ifs) {
            std::cout << "Open file failed\n";
            return -1;
        }

        std::string str;
        unsigned int maxLen = 8;  // Intentionally small.

        if (StringToFile::load_string(ifs, str, maxLen)) {
            std::cout << "Safe read success: " << str << "\n";
        }
        else {
            std::cout << "Safe read blocked (string too long)\n";
        }
    }

    return 0;
}
