# Huffman_compression_inParallel_summary

这里是你本地原本写的 README 内容

## 1. 项目简介

本项目实现了哈夫曼（Huffman）压缩算法的三个版本：

1. **串行（Serial）版本**
2. **OpenMP 并行（OpenMP）版本**
3. **CUDA 并行（CUDA）版本**

三个版本的操作逻辑完全一致，均支持对指定文件进行哈夫曼压缩（Compress）或解压缩（Decompress）。

---

## 2. 文件结构

假设当前目录为 `Huffman_compression_summary/`，其下主要文件/文件夹结构如下：

```
Huffman_compression_summary/
├── Chara.hpp           # 通用：字符处理相关头文件
├── Node.hpp            # 通用：哈夫曼树节点定义头文件
├── hfmTree.hpp         # 通用：哈夫曼树构建及操作头文件
│
├── main1.cpp           # 串行版本（Serial）源码
├── main2.cpp           # OpenMP 并行版本（OpenMP）源码
├── main3.cu            # CUDA 并行版本（CUDA）源码
│
└── (可选的测试输入输出文件夹，例如)
    ├── input/
    │   └── example.txt     # 示例输入文件
    └── output/
        └── example.hfm     # 示例压缩后输出（或解压后输出）
```

- **通用头文件（共用）**
  - `Chara.hpp`
  - `Node.hpp`
  - `hfmTree.hpp`
    这三份头文件在三个版本中都需要包含，定义了哈夫曼树节点、字符频率统计、哈夫曼树构建与遍历等基础功能。

- **串行版本源码**
  - `main1.cpp`
    - 实现哈夫曼压缩与解压缩的串行逻辑，直接使用 C++ 标准库完成所有操作。

- **OpenMP 并行版本源码**
  - `main2.cpp`
    - 基于 `main1.cpp`，在统计字符频率、构建哈夫曼树或编码/解码过程中适当插入 OpenMP 并行指令，以加速多核 CPU 上的计算。

- **CUDA 并行版本源码**
  - `main3.cu`
    - 使用 CUDA 在 GPU 上并行化计算，例如字符频率统计、优先队列并行化、编码/解码并行化等（具体实现请参见源码注释）。
    - 需要 NVIDIA GPU 及 CUDA Toolkit 环境（至少 CUDA 10.x 或更高版本）。

---

## 3. 环境与依赖

- **硬件要求**
  - 串行 & OpenMP 版本：普通 x86_64 CPU 即可
  - CUDA 版本：需要配备 NVIDIA GPU，且已安装对应驱动

- **软件依赖**
  - **编译 Flags**
    - OpenMP 版本必须开启 `-fopenmp`
    - CUDA 版本直接调用 `nvcc`

  - **C++ 标准**
    - 建议至少支持 C++11（若有新语法，可根据源码修改）

---

## 4. 编译方法

### 4.1 串行版本（Serial）

```bash
# 使用 g++ 编译 main1.cpp
g++ main1.cpp -o main1
```

### 4.2 OpenMP 并行版本（OpenMP）

```bash
# 使用 g++ 并开启 OpenMP 支持
g++ -fopenmp main2.cpp -o main2
```

- `-fopenmp` 标志用于启用 OpenMP 并行化

### 4.3 CUDA 并行版本（CUDA）

```bash
# 使用 nvcc 编译 main3.cu
nvcc main3.cu -o main3
```

- 若需链接其他库或设置更高的优化，可根据实际 CUDA Toolkit 文档进行调整。

---

## 5. 运行方法

三个版本的**运行逻辑完全一致**，差别仅在于可执行文件名称：`main1`、`main2`、`main3`。以下统一以 `./mainX`（X=1、2、3）来表示：

### 5.1 程序交互流程

1. **启动程序**
   运行对应可执行文件后，程序会首先打印欢迎信息并提示选择操作模式。

2. **选择操作模式**
   - **输入数字**：
     - `1` —— 进行 **压缩（Compress）**
     - `2` —— 进行 **解压缩（Decompress）**

3. **输入文件路径**
   - 程序会提示输入两个路径：
     1. **待处理的输入文件路径**（原始文件或哈夫曼压缩文件）
     2. **输出文件路径**（压缩后文件或解压后文件）

4. **等待结果**

---

## 注意事项

- 输入文件仅接受全英文，输入文件可用给出的随机文件生成程序进行生成。
