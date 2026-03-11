#include <iostream>
#include <vector>
#include <fstream>
#include <cstdint>
#include <cstring>
#include <chrono>
#include <cuda_runtime.h>
#include "hfmTree.hpp"
using namespace std;
using namespace std::chrono;

constexpr int BLOCKSIZE = 8 * 8 * 1024;
constexpr int DATA_BITS = BLOCKSIZE - 32;
constexpr int DATA_BYTES = DATA_BITS / 8; // 8188

struct PackedBlock
{
    uint32_t charCount;
    uint8_t bytes[DATA_BYTES];
};

// CUDA 核函数：统计输入数据中各字符的出现频率
__global__ void countFreqKernel(const char *data, size_t dataSize, uint32_t *freqTable)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < dataSize)
    {
        atomicAdd(&freqTable[(uint8_t)data[idx]], 1);
    }
}

//  声明共享内存，用于编码时的临时缓冲区
extern __shared__ uint8_t s_bytes[];
// 对每个逻辑块执行编码
__global__ void encodeBlocksKernel(const char *data,
                                   const size_t *blockStarts,
                                   size_t numBlocks,
                                   uint32_t *charCounts,
                                   uint8_t *outputBytes,
                                   const char **d_codeTable,
                                   const int *d_codeLens)
{
    // 仅线程 0 执行编码逻辑，其余线程立即返回
    if (threadIdx.x != 0)
        return;
    size_t b = blockIdx.x;
    if (b >= numBlocks)
        return;

    size_t start = blockStarts[b];
    size_t end = blockStarts[b + 1];
    int byteIdx = 0, bitIdx = 0;
    memset(s_bytes, 0, DATA_BYTES);

    for (size_t i = start; i < end; ++i)
    {
        uint8_t ch = (uint8_t)data[i];
        const char *code = d_codeTable[ch];
        int len = d_codeLens[ch];
        for (int j = 0; j < len; ++j)
        {
            if (code[j] == '1')
            {
                s_bytes[byteIdx] |= (1 << (7 - bitIdx));
            }
            if (++bitIdx == 8)
            {
                byteIdx++;
                bitIdx = 0;
                if (byteIdx >= DATA_BYTES)
                    break;
            }
        }
        if (byteIdx >= DATA_BYTES)
            break;
    }

    size_t outOffset = b * DATA_BYTES;
    for (int i = 0; i < DATA_BYTES; ++i)
    {
        outputBytes[outOffset + i] = s_bytes[i];
    }
    charCounts[b] = uint32_t(end - start);
}

// 解码每个压缩块
__global__ void decodeBlocksKernel(const uint8_t *bytes,
                                   const uint32_t *charCount,
                                   int numBlocks,
                                   const char **codeTable,
                                   const int *codeLens,
                                   char *outData,
                                   const uint64_t *prefixCounts)
{
    size_t b = blockIdx.x;
    if (b >= numBlocks)
        return;
    size_t bitsBase = b * DATA_BITS;
    size_t outOffset = prefixCounts[b];
    uint32_t toDecode = charCount[b];
    size_t bitPtr = bitsBase;
    for (uint32_t cnt = 0; cnt < toDecode; ++cnt)
    {
        // accumulate bits until a code matches
        int maxLen = 0;
        for (int c = 0; c < 128; ++c)
            if (codeLens[c] > maxLen)
                maxLen = codeLens[c];
        // local buffer to store bits
        extern __shared__ uint8_t sbuf[];
        int depth = 0;
        while (true)
        {
            // read next bit
            size_t byteIdx = bitPtr / 8;
            int bitIdx = 7 - (bitPtr & 7);
            uint8_t bit = (bytes[byteIdx] >> bitIdx) & 1;
            sbuf[depth++] = bit;
            bitPtr++;
            // try matching any code of current length
            for (int c = 0; c < 128; ++c)
            {
                if (codeLens[c] == depth)
                {
                    bool match = true;
                    char *code = (char *)codeTable[c];
                    for (int k = 0; k < depth; ++k)
                    {
                        if ((code[k] - '0') != sbuf[k])
                        {
                            match = false;
                            break;
                        }
                    }
                    if (match)
                    {
                        outData[outOffset++] = char(c);
                        depth = 0;
                        goto NEXT_CHAR;
                    }
                }
            }
        }
    NEXT_CHAR:;
    }
}

int main()
{
    int mode;
    cout << "select a mode 1.compression 2.uncompression" << endl;
    cin >> mode;
    if (mode == 1)
    {
        string inName, outName;
        cout << "input file:";
        cin >> inName;
        cout << "output file:";
        cin >> outName;

        ifstream fin(inName, ios::binary | ios::ate);
        if (!fin)
        {
            cerr << "Cannot open input file\n";
            return 1;
        }
        auto startofencode = high_resolution_clock::now();
        auto sz = fin.tellg();
        fin.seekg(0, ios::beg);
        vector<char> buffer(sz);
        fin.read(buffer.data(), sz);
        fin.close();
        size_t totalChars = buffer.size();
        // 将数据传输到 GPU
        char *d_data;
        cudaMalloc(&d_data, totalChars);
        cudaMemcpy(d_data, buffer.data(), totalChars, cudaMemcpyHostToDevice);
        uint32_t *d_freq;
        cudaMalloc(&d_freq, 128 * sizeof(uint32_t));
        cudaMemset(d_freq, 0, 128 * sizeof(uint32_t));

        int threads = 256;
        int blocks = (totalChars + threads - 1) / threads;
        countFreqKernel<<<blocks, threads>>>(d_data, totalChars, d_freq);
        cudaDeviceSynchronize();
        // 启动核函数统计字符频率
        uint64_t freq[128] = {0};
        uint32_t h_freq[128];
        cudaMemcpy(h_freq, d_freq, 128 * sizeof(uint32_t), cudaMemcpyDeviceToHost);
        for (int i = 0; i < 128; ++i)
            freq[i] = h_freq[i];
        cudaFree(d_freq);

        vector<chara> cts;
        for (int c = 0; c < 128; ++c)
            if (freq[c] > 0)
                cts.emplace_back((char)c, freq[c]);
        hfmTree tree;
        tree.Creatree(cts);
        tree.GnerateCode();
        vector<string> code(128);
        for (auto &ch : cts)
        {
            string tmp;
            tree.inOrder(ch.ch, tmp);
            code[(uint8_t)ch.ch] = tmp;
        }
        string endCode;
        tree.inOrder(char(10), endCode);

        // 分块处理数据，确保每块不超过 DATA_BITS 位
        vector<size_t> blockStarts{0};
        size_t bitRem = DATA_BITS, pos = 0;
        while (pos < totalChars)
        {
            auto &cd = code[(uint8_t)buffer[pos]];
            if (cd.size() <= bitRem)
            {
                bitRem -= cd.size();
                pos++;
            }
            else
            {
                // if (endCode.size() <= bitRem)
                //     bitRem -= endCode.size();
                blockStarts.push_back(pos);
                bitRem = DATA_BITS;
            }
        }
        blockStarts.push_back(totalChars);
        size_t numBlocks = blockStarts.size() - 1;

        // 将分块信息上传到 GPU
        size_t *d_blockStarts;
        cudaMalloc(&d_blockStarts, blockStarts.size() * sizeof(size_t));
        cudaMemcpy(d_blockStarts, blockStarts.data(), blockStarts.size() * sizeof(size_t), cudaMemcpyHostToDevice);

        char **h_codePtrs = new char *[128];
        char **d_codeTable;
        cudaMalloc(&d_codeTable, 128 * sizeof(char *));
        int h_codeLens[128] = {0};
        for (int i = 0; i < 128; ++i)
        {
            h_codeLens[i] = code[i].size();
            if (h_codeLens[i] > 0)
            {
                cudaMalloc(&h_codePtrs[i], h_codeLens[i]);
                cudaMemcpy(h_codePtrs[i], code[i].data(), h_codeLens[i], cudaMemcpyHostToDevice);
            }
            else
                h_codePtrs[i] = nullptr;
        }
        cudaMemcpy(d_codeTable, h_codePtrs, 128 * sizeof(char *), cudaMemcpyHostToDevice);
        int *d_codeLens;
        cudaMalloc(&d_codeLens, 128 * sizeof(int));
        cudaMemcpy(d_codeLens, h_codeLens, 128 * sizeof(int), cudaMemcpyHostToDevice);

        uint8_t *d_output;
        cudaMalloc(&d_output, numBlocks * DATA_BYTES);
        uint32_t *d_charCounts;
        cudaMalloc(&d_charCounts, numBlocks * sizeof(uint32_t));

        encodeBlocksKernel<<<numBlocks, 1, DATA_BYTES>>>(d_data, d_blockStarts, numBlocks, d_charCounts, d_output, (const char **)d_codeTable, d_codeLens);
        cudaDeviceSynchronize();

        vector<PackedBlock> blocksVec(numBlocks);
        uint32_t *tmpChar = new uint32_t[numBlocks];
        uint8_t *tmpBytes = new uint8_t[numBlocks * DATA_BYTES];
        cudaMemcpy(tmpChar, d_charCounts, numBlocks * sizeof(uint32_t), cudaMemcpyDeviceToHost);
        cudaMemcpy(tmpBytes, d_output, numBlocks * DATA_BYTES, cudaMemcpyDeviceToHost);
        for (size_t i = 0; i < numBlocks; ++i)
        {
            blocksVec[i].charCount = tmpChar[i];
            memcpy(blocksVec[i].bytes, tmpBytes + i * DATA_BYTES, DATA_BYTES);
        }

        ofstream fout(outName, ios::binary);
        uint64_t bc = numBlocks, tc = totalChars;
        fout.write((char *)&bc, sizeof(bc));
        fout.write((char *)&tc, sizeof(tc));
        uint8_t kinds = cts.size();
        fout.write((char *)&kinds, sizeof(kinds));
        for (auto &ch : cts)
        {
            uint8_t c = (uint8_t)ch.ch;
            uint64_t cnt = ch.charCount;
            fout.write((char *)&c, sizeof(c));
            fout.write((char *)&cnt, sizeof(cnt));
        }
        for (auto &pb : blocksVec)
        {
            fout.write((char *)&pb.charCount, sizeof(pb.charCount));
            fout.write((char *)pb.bytes, DATA_BYTES);
        }
        fout.close();

        cudaFree(d_data);
        cudaFree(d_blockStarts);
        cudaFree(d_output);
        cudaFree(d_charCounts);
        cudaFree(d_codeTable);
        cudaFree(d_codeLens);
        for (int i = 0; i < 128; ++i)
            if (h_codePtrs[i])
                cudaFree(h_codePtrs[i]);
        delete[] h_codePtrs;
        delete[] tmpChar;
        delete[] tmpBytes;

        // cout << "Compression done: " << numBlocks << " blocks." << endl;
        auto endofencode = high_resolution_clock::now();
        auto durationofencode = duration_cast<milliseconds>(endofencode - startofencode);
        cout << "times for encode:" << durationofencode.count() << " ms" << endl;
    }
    else if (mode == 2)
    {
        cout << "please input the name of inputfile." << endl;
        string InputfileName;
        cin >> InputfileName;
        cout << "please input the name of outputfile." << endl;
        string OutputfileName;
        cin >> OutputfileName;

        ifstream fin(InputfileName, ios::binary);
        if (!fin.is_open())
        {
            cerr << "Error opening input file!" << endl;
            return 1;
        }
        auto startofuncompress = high_resolution_clock::now();
        uint64_t numBlocks = 0, totalChars = 0;
        fin.read(reinterpret_cast<char *>(&numBlocks), sizeof(numBlocks));
        fin.read(reinterpret_cast<char *>(&totalChars), sizeof(totalChars));
        uint8_t kinds = 0;
        fin.read(reinterpret_cast<char *>(&kinds), sizeof(kinds));

        vector<chara> cts;
        for (int i = 0; i < kinds; ++i)
        {
            uint8_t c;
            uint64_t cnt;
            fin.read(reinterpret_cast<char *>(&c), sizeof(c));
            fin.read(reinterpret_cast<char *>(&cnt), sizeof(cnt));
            cts.emplace_back(static_cast<char>(c), cnt);
        }

        hfmTree tree;
        tree.Creatree(cts);
        tree.GnerateCode();

        vector<string> code(128);
        for (auto &ch : cts)
        {
            string tmp;
            tree.inOrder(ch.ch, tmp);
            code[(uint8_t)ch.ch] = tmp;
        }

        char **h_codePtrs = new char *[128];
        int h_codeLens[128] = {0};
        char **d_codeTable;
        cudaMalloc(&d_codeTable, 128 * sizeof(char *));
        for (int i = 0; i < 128; ++i)
        {
            h_codeLens[i] = code[i].size();
            if (h_codeLens[i] > 0)
            {
                cudaMalloc(&h_codePtrs[i], h_codeLens[i]);
                cudaMemcpy(h_codePtrs[i], code[i].data(), h_codeLens[i], cudaMemcpyHostToDevice);
            }
            else
                h_codePtrs[i] = nullptr;
        }
        cudaMemcpy(d_codeTable, h_codePtrs, 128 * sizeof(char *), cudaMemcpyHostToDevice);

        int *d_codeLens;
        cudaMalloc(&d_codeLens, 128 * sizeof(int));
        cudaMemcpy(d_codeLens, h_codeLens, 128 * sizeof(int), cudaMemcpyHostToDevice);

        // 读取压缩块的内容
        vector<uint32_t> h_charCount(numBlocks);
        vector<uint8_t> h_bytes(numBlocks * DATA_BYTES);
        for (size_t b = 0; b < numBlocks; ++b)
        {
            fin.read(reinterpret_cast<char *>(&h_charCount[b]), sizeof(uint32_t));
            fin.read(reinterpret_cast<char *>(&h_bytes[b * DATA_BYTES]), DATA_BYTES);
        }
        fin.close();

        uint8_t *d_bytes;
        uint32_t *d_charCount;
        char *d_out;
        cudaMalloc(&d_bytes, numBlocks * DATA_BYTES);
        cudaMalloc(&d_charCount, numBlocks * sizeof(uint32_t));
        cudaMalloc(&d_out, totalChars);
        cudaMemcpy(d_bytes, h_bytes.data(), numBlocks * DATA_BYTES, cudaMemcpyHostToDevice);
        cudaMemcpy(d_charCount, h_charCount.data(), numBlocks * sizeof(uint32_t), cudaMemcpyHostToDevice);

        vector<uint64_t> h_prefix(numBlocks);
        uint64_t acc = 0;
        for (size_t i = 0; i < numBlocks; ++i)
        {
            h_prefix[i] = acc;
            acc += h_charCount[i];
        }
        uint64_t *d_prefix;
        cudaMalloc(&d_prefix, numBlocks * sizeof(uint64_t));
        cudaMemcpy(d_prefix, h_prefix.data(), numBlocks * sizeof(uint64_t), cudaMemcpyHostToDevice);

        int threads = 1;
        decodeBlocksKernel<<<numBlocks, threads, sizeof(uint8_t) * 1024>>>(
            d_bytes, d_charCount, numBlocks,
            (const char **)d_codeTable, d_codeLens,
            d_out, d_prefix);
        cudaDeviceSynchronize();

        vector<char> h_out(totalChars);
        cudaMemcpy(h_out.data(), d_out, totalChars, cudaMemcpyDeviceToHost);
        ofstream fout(OutputfileName, ios::binary);
        fout.write(h_out.data(), totalChars);
        fout.close();

        cudaFree(d_bytes);
        cudaFree(d_charCount);
        cudaFree(d_codeTable);
        cudaFree(d_codeLens);
        cudaFree(d_out);
        cudaFree(d_prefix);
        for (int i = 0; i < 128; ++i)
            if (h_codePtrs[i])
                cudaFree(h_codePtrs[i]);
        delete[] h_codePtrs;

        // cout << "Decompression done: " << totalChars << " characters." << endl;
        auto endofuncompress = high_resolution_clock::now();
        auto durationofuncompress = duration_cast<milliseconds>(endofuncompress - startofuncompress);
        cout << "times for uncompress:" << durationofuncompress.count() << " ms" << endl;
    }
    else
    {
        cout << "illegal input" << endl;
    }
    return 0;
}
// nvcc main3.cu -o main3