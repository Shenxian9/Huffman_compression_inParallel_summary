#include <iostream>
#include <fstream>
#include <cstdlib>
#include <ctime>

void generateRandomTextFile(const std::string &filename, std::size_t sizeInMB)
{
    std::ofstream outFile(filename, std::ios::binary);
    if (!outFile)
    {
        std::cerr << "无法创建文件: " << filename << std::endl;
        return;
    }

    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    std::size_t sizeInBytes = sizeInMB * 1024 * 1024;
    std::size_t bytesWritten = 0;

    while (bytesWritten < sizeInBytes)
    {
        char ch;
        int r = std::rand() % 100;

        if (r < 5)
        {
            ch = '\n'; // 5% 概率插入换行符
        }
        else
        {
            ch = static_cast<char>(32 + (std::rand() % (126 - 32 + 1))); // 可见字符
        }

        outFile.put(ch);
        ++bytesWritten;
    }

    outFile.close();
    std::cout << "文件生成完成：" << filename << "（大小 " << sizeInMB << " MB）" << std::endl;
}

int main()
{
    std::string filename;
    std::size_t sizeMB;

    std::cout << "请输入要生成的文件名：";
    std::cin >> filename;
    std::cout << "请输入文件大小（单位：MB）：";
    std::cin >> sizeMB;

    generateRandomTextFile(filename, sizeMB);
    return 0;
}
