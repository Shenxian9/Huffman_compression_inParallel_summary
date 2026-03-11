#include <iostream>
#include "Chara.hpp"
using namespace std;
class Node
{
private:
    
    
    
public:
    Node * left;
    Node * right;
    chara c;
    Node()
    {
        left = NULL;
        right = NULL;
    }
    Node(chara chrt, Node *l, Node *r) : c(chrt),left(l),right(r){}
};