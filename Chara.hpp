#include <iostream>
#include <cstring>
//using std::ostream;
using namespace std;
class chara
{
public:
    string hfmCode;
    char ch;
    long int charCount;
    chara()
    {
        ch = 0;
        charCount = 0;
        hfmCode = "";
    }
    chara(char c,long int n);
    chara operator+(chara rt);
    bool operator>(chara rt);
    bool operator<(chara rt);
    ostream & operator<<(ostream& out)
    {
        if(int(ch)==0)
        {
            out << " mid " << charCount << " ";
        }else{
            out<<" "<< char(ch) << " " << charCount << " ";
        }
        return out;
    }
    ~chara();
};
chara chara::operator+(chara rt)
{
    chara outp(0, (this->charCount + rt.charCount));

    return outp;
}
chara::chara(char c, long int n)
{
    ch = c;
    charCount = n;
}
bool chara::operator>(chara rt)
{
    return (this->charCount > rt.charCount);
}
bool chara::operator<(chara rt)
{
    return (this->charCount < rt.charCount);
}
    chara::~chara()
{
}
ostream &operator<<(ostream &out, chara &c)
{
    c << out;
    return out;
}
