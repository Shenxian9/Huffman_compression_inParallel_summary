#include <iostream>
#include <deque>
#include <algorithm>
#include "Node.hpp"
class hfmTree
{
private:
    deque<Node *> forest;

public:
    Node *root;
    hfmTree();
    //
    void Creatree(vector<chara> Crs);
    //
    void preOrder();
    void inOrder(const char c,string & otps);
    void inOrder(Node *n, const char c,string &otps);
    void postOrder();
    void outOrder(char &c, const string otps, bool & flag);
    void outOrder(Node *n, char &c, const string otps, bool & flag);
    // chu
    void GnerateCode();
    void GnerateCode(Node *pnode, string cd);
    ~hfmTree();
};
hfmTree::hfmTree(/* args */)
{
}

hfmTree::~hfmTree()
{
}
void hfmTree::Creatree(vector<chara> Crs)
{
    int cnum = Crs.size();
    for (int i = 0; i < cnum; i++)
    {
        Node *ptr = new Node(Crs[i], NULL, NULL);
        forest.push_back(ptr);
    }

    for (int i = 0; i < cnum - 1; i++)
    {
        sort(forest.begin(), forest.end(), [](Node *a, Node *b)
             { return a->c < b->c; });
        Node *nnode = new Node(forest[0]->c + forest[1]->c, forest[0], forest[1]);
        forest.push_back(nnode);
        forest.pop_front();
        forest.pop_front();
    }
    root = forest.front();
    forest.clear();
}

void hfmTree::GnerateCode(Node *tNode, string code)
{
    string s = code;
    if (tNode != nullptr)
    {

        if (tNode->c.ch != 0)
        {
            if (tNode->c.ch == 10)
            {
                //cout << "chara: enter";
                // cout << tNode->c;
                //cout << " hfmCode is " << s << " " << endl;
                tNode->c.hfmCode = s;
            }
            else if (tNode->c.ch == 32)
            {
                //cout << "chara: space";
                // cout << tNode->c;
                //cout << " hfmCode is " << s << " " << endl;
                tNode->c.hfmCode = s;
            }
            else
            {
                //cout << "chara:";
                //cout << tNode->c.ch;
                tNode->c.hfmCode = s;
                //cout << " hfmCode is " << tNode->c.hfmCode << " " << endl;
            }
        }
    }
    if (tNode->left != nullptr)
    {
        // cout << "left node" << tNode->left->c<<endl;
        string sl = s + "0";
        GnerateCode(tNode->left, sl);
    }
    else
    {
        // cout << "no left node"<<endl;
    }
    if (tNode->right != nullptr)
    {
        // cout << "right node" << tNode->right->c << endl;
        string sr = s + "1";
        GnerateCode(tNode->right, sr);
    }
    else
    {
        // cout << "no right node"<<endl;
    }
}
void hfmTree::GnerateCode()
{
    GnerateCode(root, "");
}
void hfmTree::inOrder(Node * tNode,const char c,string & otps)
{
    if (tNode == nullptr)
    {
        return;
    }
    if ((tNode->left == nullptr) && (tNode->right == nullptr))
    {
        if(c==tNode->c.ch)
        {
            otps += tNode->c.hfmCode;
        }
    }else{
        if (tNode->left != nullptr)
        {
            inOrder(tNode->left, c, otps);
        }
        if (tNode->right != nullptr)
        {
            inOrder(tNode->right, c, otps);
        }
    }
}
void hfmTree::inOrder(const char c,string & otps)
{
    return inOrder(root, c,otps);
}
void hfmTree::outOrder(Node *tNode, char &c, const string otps, bool &flag)
{
    if (flag)
    {
        return; 
    }
    if ((tNode->left == nullptr) && (tNode->right == nullptr))
    {
        if (otps == tNode->c.hfmCode)
        {
            flag = true;
            c = tNode->c.ch;
        }
    }
    else
    {
        if (tNode->left != nullptr)
        {
            outOrder(tNode->left, c, otps,flag);
        }
        if (!flag && tNode->right != nullptr)
        {
            outOrder(tNode->right, c, otps,flag);
        }
    }
}
void hfmTree::outOrder(char & c,const string otps,bool & flag)
{
    return outOrder(root, c, otps,flag);
}