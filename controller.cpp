#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

using namespace std;

static string exeNameByChoice(int choice)
{
#ifdef _WIN32
    switch (choice)
    {
    case 1:
        return ".\\main1.exe";
    case 2:
        return ".\\main2.exe";
    case 3:
        return ".\\main3.exe";
    default:
        return "";
    }
#else
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
#endif
}


static string fallbackExeNameByChoice(int choice)
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

static string buildHintByChoice(int choice)
{
#ifdef _WIN32
    switch (choice)
    {
    case 1:
        return "g++ main1.cpp -o main1.exe";
    case 2:
        return "g++ -fopenmp main2.cpp -o main2.exe";
    case 3:
        return "nvcc main3.cu -o main3.exe";
    default:
        return "";
    }
#else
    switch (choice)
    {
    case 1:
        return "g++ main1.cpp -o main1";
    case 2:
        return "g++ -fopenmp main2.cpp -o main2";
    case 3:
        return "nvcc main3.cu -o main3";
    default:
        return "";
    }
#endif
}

static bool fileExists(const string &path)
{
#ifdef _WIN32
    return _access(path.c_str(), 0) == 0;
#else
    return access(path.c_str(), F_OK) == 0;
#endif
}

#ifndef _WIN32
static bool fileExecutable(const string &path)
{
    return access(path.c_str(), X_OK) == 0;
}
#endif

static string cmdQuote(const string &raw)
{
    string out = "\"";
    for (char c : raw)
    {
        if (c == '"')
            out += "\\\"";
        else
            out.push_back(c);
    }
    out += "\"";
    return out;
}

int main()
{
    cout << "Select implementation: 1.Serial 2.OpenMP 3.CUDA" << endl;
    int impl = 0;
    if (!(cin >> impl))
    {
        cerr << "Invalid implementation input." << endl;
        return 1;
    }

    string targetExe = exeNameByChoice(impl);
    if (targetExe.empty())
    {
        cerr << "Invalid implementation choice." << endl;
        return 1;
    }

#ifdef _WIN32
    if (!fileExists(targetExe))
    {
        string fallbackExe = fallbackExeNameByChoice(impl);
        if (!fallbackExe.empty() && fileExists(fallbackExe))
        {
            targetExe = fallbackExe;
        }
    }
#endif

    if (!fileExists(targetExe))
    {
        cerr << "Target executable not found: " << targetExe << endl;
        cerr << "Please build it first, e.g.: " << buildHintByChoice(impl) << endl;
        return 1;
    }

#ifndef _WIN32
    if (!fileExecutable(targetExe))
    {
        cerr << "Target exists but is not executable: " << targetExe << endl;
        cerr << "Try: chmod +x " << targetExe << endl;
        return 1;
    }
#endif

    cout << "Select operation: 1.Compress 2.Decompress" << endl;
    int mode = 0;
    if (!(cin >> mode))
    {
        cerr << "Invalid operation input." << endl;
        return 1;
    }
    if (mode != 1 && mode != 2)
    {
        cerr << "Invalid operation choice." << endl;
        return 1;
    }

    string inputPath;
    string outputPath;
    cout << "Input file path:" << endl;
    if (!(cin >> inputPath))
    {
        cerr << "Invalid input file path." << endl;
        return 1;
    }
    cout << "Output file path:" << endl;
    if (!(cin >> outputPath))
    {
        cerr << "Invalid output file path." << endl;
        return 1;
    }

    const string tempInput = ".controller_stdin.tmp";
    ofstream fout(tempInput, ios::trunc);
    if (!fout)
    {
        cerr << "Failed to create temporary input file: " << tempInput << endl;
        return 1;
    }

    fout << mode << '\n'
         << inputPath << '\n'
         << outputPath << '\n';
    fout.close();

    ifstream verify(tempInput, ios::binary);
    if (!verify)
    {
        cerr << "Temporary input file disappeared unexpectedly: " << tempInput << endl;
        return 1;
    }
    verify.close();

    string cmd = cmdQuote(targetExe) + " < " + cmdQuote(tempInput);
    cout << "Running: " << targetExe << endl;
    cout << "Command: " << cmd << endl;
    int rc = system(cmd.c_str());

    const char *keepTemp = getenv("CONTROLLER_KEEP_TEMP");
    if (!(keepTemp && string(keepTemp) == "1"))
    {
        remove(tempInput.c_str());
    }
    else
    {
        cout << "Temp file kept for debugging: " << tempInput << endl;
    }

    if (rc != 0)
    {
        cerr << "Child process failed. Please ensure target executable is built and runnable." << endl;
        return 1;
    }

    cout << "Controller finished." << endl;
    return 0;
}
