#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

using namespace std;

static string exeNameByChoice(int choice)
{
    switch (choice)
    {
    case 1:
        return "./main1";
    case 2:
        return "./main2";
    case 3:
        return "./main3";
    default:
        return "";
    }
}

int main()
{
    cout << "请选择实现版本：1.串行 2.OpenMP 3.CUDA" << endl;
    int impl = 0;
    cin >> impl;

    string targetExe = exeNameByChoice(impl);
    if (targetExe.empty())
    {
        cerr << "实现版本输入非法。" << endl;
        return 1;
    }

    cout << "请选择操作：1.压缩 2.解压" << endl;
    int mode = 0;
    cin >> mode;
    if (mode != 1 && mode != 2)
    {
        cerr << "操作模式输入非法。" << endl;
        return 1;
    }

    string inputPath;
    string outputPath;
    cout << "请输入输入文件路径：" << endl;
    cin >> inputPath;
    cout << "请输入输出文件路径：" << endl;
    cin >> outputPath;

    const string tempInput = ".controller_stdin.tmp";
    ofstream fout(tempInput, ios::trunc);
    if (!fout)
    {
        cerr << "无法创建临时输入文件。" << endl;
        return 1;
    }

    fout << mode << '\n'
         << inputPath << '\n'
         << outputPath << '\n';
    fout.close();

    string cmd = targetExe + " < " + tempInput;
    cout << "即将执行：" << targetExe << endl;
    int rc = system(cmd.c_str());

    remove(tempInput.c_str());

    if (rc != 0)
    {
        cerr << "子程序执行失败，请先确认对应可执行文件已正确编译。" << endl;
        return 1;
    }

    cout << "控制程序执行完成。" << endl;
    return 0;
}
