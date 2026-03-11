#include <cstdio>
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

static string shellQuote(const string &raw)
{
    string out = "'";
    for (char c : raw)
    {
        if (c == '\'')
            out += "'\\''";
        else
            out.push_back(c);
    }
    out += "'";
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

    string cmd = shellQuote(targetExe) + " < " + shellQuote(tempInput);
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
