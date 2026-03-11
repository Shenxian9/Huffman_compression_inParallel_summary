#include <iostream>
#include <vector>
#include <fstream>
#include <cstdint>
#include <iomanip>
#include <bitset>
#include <algorithm>
#include <omp.h>
#include <chrono>
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
        string inName, outName;
        cout << "please input the name of inputfile." << endl;
        cin >> inName;
        cout << "please input the name of outputfile." << endl;
        cin >> outName;

        ifstream fin(inName, ios::binary | ios::ate);
        if (!fin)
        {
            cerr << "Cannot open file.\n";
            return 1;
        }
        auto startofencode = high_resolution_clock::now();
        auto sz = fin.tellg();
        fin.seekg(0, ios::beg);
        string buffer(sz, '\0');
        fin.read(&buffer[0], sz);
        fin.close();
        size_t totalChars = buffer.size();

        // === 1. 并行统计频率 ===
        array<uint64_t, 128> freq = {0};
#pragma omp parallel
        {
            array<uint64_t, 128> local = {0};
            int tid = omp_get_thread_num(), nth = omp_get_num_threads();
            size_t blk = totalChars / nth;
            size_t lo = tid * blk,
                   hi = (tid + 1 == nth ? totalChars : lo + blk);
            for (size_t i = lo; i < hi; ++i)
                local[(uint8_t)buffer[i]]++;
#pragma omp critical
            for (int c = 0; c < 128; ++c)
                freq[c] += local[c];
        }

        // === 2. 构建哈夫曼树并生成编码表 ===
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

        // === 3. 并行分段压缩，各自生成块（修正 charCount） ===
        int T = omp_get_max_threads();
        vector<vector<PackedBlock>> allBlocks(T);

#pragma omp parallel
        {
            int tid = omp_get_thread_num(), nth = omp_get_num_threads();
            size_t blk = totalChars / nth;
            size_t lo = tid * blk,
                   hi = (tid + 1 == nth ? totalChars : lo + blk);

            vector<PackedBlock> localBlocks;
            //localBlocks.reserve((hi - lo) / 4);

            PackedBlock curr{};
            size_t bitCnt = 0;
            size_t charCnt = 0; // 实际字符计数

            // 对 [lo, hi) 范围内的每个字符进行哈夫曼编码并打包
            for (size_t i = lo; i < hi; ++i)
            {
                auto &bits = code[(uint8_t)buffer[i]];
                // 如果下一个字符的编码放不下，就先输出当前块
                if (bitCnt + bits.size() > DATA_BITS)
                {
                    curr.charCount = uint32_t(charCnt);
                    localBlocks.push_back(curr);
                    curr = PackedBlock{};
                    bitCnt = 0;
                    charCnt = 0;
                }
                // 写入当前字符的所有 bits
                for (char b : bits)
                {
                    if (b == '1')
                        curr.bytes[bitCnt / 8] |= (1 << (7 - (bitCnt % 8)));
                    ++bitCnt;
                }
                ++charCnt; // 一个字符处理完，计数加一
            }

            // 推入最后一块（如果有未满 DATA_BITS 的残余）
            if (bitCnt > 0)
            {
                curr.charCount = uint32_t(charCnt);
                localBlocks.push_back(curr);
            }

            allBlocks[tid] = std::move(localBlocks);
        }

        // === 4. 简单拼接所有线程的块 ===
        vector<PackedBlock> blocks;
        for (int t = 0; t < T; ++t)
        {
            blocks.insert(blocks.end(),
                          allBlocks[t].begin(),
                          allBlocks[t].end());
        }
        size_t numBlocks = blocks.size();

        // === 5. 写入输出文件 ===
        ofstream fout(outName, ios::binary);
        uint64_t bc = numBlocks, tc = totalChars;
        fout.write((char *)&bc, sizeof(bc));
        fout.write((char *)&tc, sizeof(tc));

        uint8_t kinds = (uint8_t)cts.size();
        fout.write((char *)&kinds, sizeof(kinds));
        for (auto &ch : cts)
        {
            uint8_t c = (uint8_t)ch.ch;
            uint64_t cnt = ch.charCount;
            fout.write((char *)&c, sizeof(c));
            fout.write((char *)&cnt, sizeof(cnt));
        }
        for (auto &pb : blocks)
        {
            fout.write((char *)&pb.charCount, sizeof(pb.charCount));
            fout.write((char *)pb.bytes.data(), DATA_BYTES);
        }
        fout.close();

        cout << "Compression done: " << numBlocks << " blocks." << endl;
        auto endofencode = high_resolution_clock::now();
        auto durationofencode =
            duration_cast<milliseconds>(endofencode - startofencode);
        cout << "times for encode:" << durationofencode.count()
             << " ms" << endl;
        break;
    }

    case 2:
    {
        cout << "please input the name of inputfile." << endl;
        string InputfileName, OutputfileName;
        cin >> InputfileName;
        cout << "please input the name of outputfile." << endl;
        cin >> OutputfileName;

        ifstream input(InputfileName, ios::binary);
        if (!input.is_open())
        {
            cerr << "Error opening input file!" << endl;
            return 1;
        }
        auto startofuncompress = high_resolution_clock::now();
        // 1) 读头信息
        uint64_t totalBlockCount = 0, totalChars = 0;
        input.read(reinterpret_cast<char *>(&totalBlockCount), sizeof(totalBlockCount));
        input.read(reinterpret_cast<char *>(&totalChars), sizeof(totalChars));

        uint8_t nKinds = 0;
        input.read(reinterpret_cast<char *>(&nKinds), sizeof(nKinds));

        vector<chara> cts;
        for (int i = 0; i < nKinds; ++i)
        {
            uint8_t ch;
            uint64_t cnt;
            input.read(reinterpret_cast<char *>(&ch), sizeof(ch));
            input.read(reinterpret_cast<char *>(&cnt), sizeof(cnt));
            cts.emplace_back(static_cast<char>(ch), cnt);
        }

        // 重建哈夫曼树
        hfmTree atree;
        atree.Creatree(cts);
        atree.GnerateCode();

        // 2) 先把所有块读到内存
        vector<uint32_t> blockCharCount(totalBlockCount);
        vector<vector<uint8_t>> packedBlocks(totalBlockCount, vector<uint8_t>(DATA_BYTES));
        for (uint64_t b = 0; b < totalBlockCount; ++b)
        {
            input.read(reinterpret_cast<char *>(&blockCharCount[b]), sizeof(uint32_t));
            input.read(reinterpret_cast<char *>(packedBlocks[b].data()), DATA_BYTES);
        }
        input.close();

        // 3) 并行解码每个块
        vector<string> decodedBlocks(totalBlockCount);
#pragma omp parallel for schedule(dynamic)
        for (int b = 0; b < (int)totalBlockCount; ++b)
        {
            // 3.1 解包成 bit 字符串
            string bits;
            bits.reserve(DATA_BYTES * 8);
            for (int i = 0; i < DATA_BYTES; ++i)
            {
                for (int bit = 7; bit >= 0; --bit)
                {
                    bits.push_back(((packedBlocks[b][i] >> bit) & 1) ? '1' : '0');
                }
            }

            // 3.2 解码出 blockCharCount[b] 个字符
            string &out = decodedBlocks[b];
            out.reserve(blockCharCount[b] + 1);

            string code;
            code.reserve(64);
            uint32_t matched = 0;

            for (char bitChar : bits)
            {
                code.push_back(bitChar);
                bool flag = false;
                char dc = 0;
                atree.outOrder(dc, code, flag);
                if (flag)
                {
                    // 将换行符转为 "\r\n" 或者只输出 '\r' 看需求
                    if (dc == '\n')
                        out.push_back('\r');
                    out.push_back(dc);
                    ++matched;
                    code.clear();
                    if (matched >= blockCharCount[b])
                        break;
                }
            }
        }

        // 4) 顺序写回文件（不基于 totalChars 中断）
        ofstream output(OutputfileName, ios::binary);
        if (!output.is_open())
        {
            cerr << "Error opening output file!" << endl;
            return 1;
        }
        // 直接写完所有块
        for (uint64_t b = 0; b < totalBlockCount; ++b)
        {
            output.write(decodedBlocks[b].data(),
                         decodedBlocks[b].size());
        }
        output.close();
        auto endofuncompress = high_resolution_clock::now();
        auto durationofuncompress = duration_cast<milliseconds>(endofuncompress - startofuncompress);
        cout << "times for uncompress:" << durationofuncompress.count() << " ms" << endl;
        break;
    }

    default:
        cout << "illeagal input" << endl;
        break;
    }
    system("pause");
}
// g++ -fopenmp main2.cpp -o main2