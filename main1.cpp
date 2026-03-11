#include <iostream>
#include <vector>
#include <fstream>
#include <cstdint>
#include <iomanip>
#include <bitset>
#include <chrono>
#include <algorithm>
#include "hfmTree.hpp"
using namespace std;
using namespace std::chrono;
const int BLOCKSIZE = 8 * 8 * 1024; // bits per block
const int DATA_BITS = BLOCKSIZE - 32;
const int DATA_BYTES = DATA_BITS / 8; // 8188
struct PackedBlock
{
    uint32_t charCount;
    array<uint8_t, DATA_BYTES> bytes;
};
int main()
{
    int mode = 0;
    cout << "select a mode 1.compression 2.uncompression" << endl;
    cin >> mode;
    string InputfileName;
    string OutputfileName;
    string keyCode;
    switch (mode)
    {
    case 1:
    {
        cout << "please input the name of inputfile." << endl;
        cin >> InputfileName;
        cout << "please input the name of outputfile." << endl;
        cin >> OutputfileName;

        // === 1. 一次性读入整个文件 ===
        auto startofencode = high_resolution_clock::now();
        ifstream fin(InputfileName, ios::binary | ios::ate);
        if (!fin)
        {
            cerr << "Cannot open file.\n";
            return 1;
        }
        auto sz = fin.tellg();
        fin.seekg(0, ios::beg);
        string buffer(sz, '\0');
        fin.read(&buffer[0], sz);
        fin.close();
        size_t totalChars = buffer.size();

        // === 2. 统计频率 ===
        array<uint64_t, 128> freq = {0};
        for (size_t i = 0; i < totalChars; ++i)
            freq[(uint8_t)buffer[i]]++;

        // 特别处理换行符
        freq[10]++;

        // === 3. 建树+编码表 ===
        vector<chara> cts;
        for (int c = 0; c < 128; ++c)
            if (freq[c] > 0)
                cts.emplace_back((char)c, freq[c]);
        hfmTree tree;
        tree.Creatree(cts);
        tree.GnerateCode();

        vector<string> code(128);
        string tmp;
        for (auto &ch : cts)
        {
            tmp.clear();
            tree.inOrder(ch.ch, tmp);
            code[(uint8_t)ch.ch] = tmp;
        }
        string endCode;
        tree.inOrder(char(10), endCode);

        // === 4. 输出 header ===
        ofstream fout(OutputfileName, ios::binary);
        if (!fout)
        {
            cerr << "Error opening output file!" << endl;
            return 1;
        }

        uint64_t dummyBlockCount = 0;
        fout.write(reinterpret_cast<char *>(&dummyBlockCount), sizeof(dummyBlockCount));
        uint64_t totalnumc = (uint64_t)totalChars;
        fout.write(reinterpret_cast<char *>(&totalnumc), sizeof(totalnumc));

        uint8_t kinds = (uint8_t)cts.size();
        fout.write(reinterpret_cast<char *>(&kinds), sizeof(kinds));
        for (auto &ch : cts)
        {
            uint8_t c = (uint8_t)ch.ch;
            uint64_t cnt = ch.charCount;
            fout.write(reinterpret_cast<char *>(&c), sizeof(c));
            fout.write(reinterpret_cast<char *>(&cnt), sizeof(cnt));
        }

        // === 5. 串行块压缩 ===
        vector<PackedBlock> blocks;
        PackedBlock curr{};
        size_t bitCnt = 0;
        size_t charCnt = 0;

        for (size_t i = 0; i < totalChars; ++i)
        {
            auto &bits = code[(uint8_t)buffer[i]];
            if (bitCnt + bits.size() > DATA_BITS)
            {
                curr.charCount = uint32_t(charCnt);
                blocks.push_back(curr);
                curr = PackedBlock{};
                bitCnt = 0;
                charCnt = 0;
            }
            for (char b : bits)
            {
                if (b == '1')
                    curr.bytes[bitCnt / 8] |= (1 << (7 - (bitCnt % 8)));
                ++bitCnt;
            }
            ++charCnt;
        }

        if (bitCnt > 0)
        {
            curr.charCount = uint32_t(charCnt);
            blocks.push_back(curr);
        }

        // === 6. 写入块 ===
        for (auto &pb : blocks)
        {
            fout.write(reinterpret_cast<char *>(&pb.charCount), sizeof(pb.charCount));
            fout.write(reinterpret_cast<char *>(pb.bytes.data()), DATA_BYTES);
        }

        // 回写 blockcount
        fout.seekp(0);
        uint64_t realBlockCount = (uint64_t)blocks.size();
        fout.write(reinterpret_cast<char *>(&realBlockCount), sizeof(realBlockCount));

        fout.close();

        // === 7. 结束 ===
        auto endofencode = high_resolution_clock::now();
        auto durationofencode = duration_cast<milliseconds>(endofencode - startofencode);
        cout << "Compression done: " << realBlockCount << " blocks." << endl;
        cout << "times for encode:" << durationofencode.count() << " ms" << endl;

        break;
    }

    case 2:
    {
        cout << "please input the name of inputfile." << endl;
        string InputfileName, OutputfileName;
        cin >> InputfileName;
        cout << "please input the name of outputfile." << endl;
        cin >> OutputfileName;

        // 1) 打开输入文件并读取头信息
        ifstream input(InputfileName, ios::binary);
        if (!input.is_open())
        {
            cerr << "Error opening input file!" << endl;
            return 1;
        }
        auto startofuncompress = high_resolution_clock::now();

        uint64_t totalBlockCount = 0, totalChars = 0;
        input.read(reinterpret_cast<char *>(&totalBlockCount), sizeof(totalBlockCount));
        input.read(reinterpret_cast<char *>(&totalChars), sizeof(totalChars));

        uint8_t nKinds = 0;
        input.read(reinterpret_cast<char *>(&nKinds), sizeof(nKinds));

        vector<chara> cts;
        cts.reserve(nKinds);
        for (int i = 0; i < nKinds; ++i)
        {
            uint8_t ch;
            uint64_t cnt;
            input.read(reinterpret_cast<char *>(&ch), sizeof(ch));
            input.read(reinterpret_cast<char *>(&cnt), sizeof(cnt));
            cts.emplace_back(static_cast<char>(ch), cnt);
        }

        // 2) 重建哈夫曼树
        hfmTree tree;
        tree.Creatree(cts);
        tree.GnerateCode();

        // 3) 把所有块读到内存（与并行版一致）
        vector<uint32_t> blockCharCount(totalBlockCount);
        vector<vector<uint8_t>> packedBlocks(totalBlockCount, vector<uint8_t>(DATA_BYTES));
        for (uint64_t b = 0; b < totalBlockCount; ++b)
        {
            input.read(reinterpret_cast<char *>(&blockCharCount[b]), sizeof(uint32_t));
            input.read(reinterpret_cast<char *>(packedBlocks[b].data()), DATA_BYTES);
        }
        input.close();

        // 4) 串行解码每个块（和并行版 for-loop 对齐）
        vector<string> decodedBlocks(totalBlockCount);
        for (uint64_t b = 0; b < totalBlockCount; ++b)
        {
            // 4.1 解包成 bit 字符串
            string bits;
            bits.reserve(DATA_BYTES * 8);
            for (int i = 0; i < DATA_BYTES; ++i)
            {
                for (int bit = 7; bit >= 0; --bit)
                {
                    bits.push_back(((packedBlocks[b][i] >> bit) & 1) ? '1' : '0');
                }
            }

            // 4.2 解码出 blockCharCount[b] 个字符
            string &out = decodedBlocks[b];
            out.reserve(blockCharCount[b]);
            string codeBits;
            codeBits.reserve(64);
            uint32_t matched = 0;

            for (char bitChar : bits)
            {
                codeBits.push_back(bitChar);
                bool flag = false;
                char dc = 0;
                tree.outOrder(dc, codeBits, flag);
                if (flag)
                {
                    // 如需处理换行符：可在此转换
                    out.push_back(dc);
                    ++matched;
                    codeBits.clear();
                    if (matched >= blockCharCount[b])
                        break;
                }
            }
        }

        // 5) 顺序写回文件
        ofstream output(OutputfileName, ios::binary);
        if (!output.is_open())
        {
            cerr << "Error opening output file!" << endl;
            return 1;
        }
        for (uint64_t b = 0; b < totalBlockCount; ++b)
        {
            output.write(decodedBlocks[b].data(), decodedBlocks[b].size());
        }
        output.close();

        // 6) 完成并输出耗时
        auto endofuncompress = high_resolution_clock::now();
        auto durationofuncompress =
            duration_cast<milliseconds>(endofuncompress - startofuncompress);
        cout << "times for uncompress: "
             << durationofuncompress.count() << " ms" << endl;
        break;
    }

    default:
        cout << "illeagal input" << endl;
        break;
    }
    system("pause");
}
//g++ main1.cpp - o main1