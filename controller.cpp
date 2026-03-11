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
    cout << "Select implementation: 1.Serial 2.OpenMP 3.CUDA" << endl;
    int impl = 0;
    cin >> impl;

    string targetExe = exeNameByChoice(impl);
    if (targetExe.empty())
    {
        cerr << "Invalid implementation choice." << endl;
        return 1;
    }

    cout << "Select operation: 1.Compress 2.Decompress" << endl;
    int mode = 0;
    cin >> mode;
    if (mode != 1 && mode != 2)
    {
        cerr << "Invalid operation choice." << endl;
        return 1;
    }

    string inputPath;
    string outputPath;
    cout << "Input file path:" << endl;
    cin >> inputPath;
    cout << "Output file path:" << endl;
    cin >> outputPath;

    const string tempInput = ".controller_stdin.tmp";
    ofstream fout(tempInput, ios::trunc);
    if (!fout)
    {
        cerr << "Failed to create temporary input file." << endl;
        return 1;
    }

    fout << mode << '\n'
         << inputPath << '\n'
         << outputPath << '\n';
    fout.close();

    string cmd = targetExe + " < " + tempInput;
    cout << "Running: " << targetExe << endl;
    int rc = system(cmd.c_str());

    remove(tempInput.c_str());

    if (rc != 0)
    {
        cerr << "Child process failed. Please ensure target executable is built." << endl;
        return 1;
    }

    cout << "Controller finished." << endl;
    return 0;
}
