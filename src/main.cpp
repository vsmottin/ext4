#include "ext4.hpp"
#include "cores.h"
#include "utils.hpp"
#include <iostream>

using namespace std;

int main()
{
    string fileName;
    Ext4::FileSystemManager fs;
    bool img_chosen = false;
    while (!img_chosen)
    {
        cout << amarelo << "ext4shell " << reset;
        cin >> fileName;
        img_chosen = fs.setImage(trim(fileName));
    }
    fileName = trim(fileName);
    string comand;
    while (true)
    {
        cout << amarelo << "ext4shell:" << reset azul << "[" << fileName << "/]" << reset << "$ ";
        cin >> comand;
        comand = trim(comand);
        if (comand == "info")
        {
            fs.info();
        }
        else if (comand == "ls")
        {
            fs.ls();
        }
        else if (comand == "pwd")
        {
            fs.pwd();
        }
        else if (comand == "touch"){
            string path;
            getline(cin >> ws, path);
            if (path.length() == 0)
            {
                cout << vermelho << "Caminho invalido" << reset << endl;
                continue;
            }
            fs.touch(path);
        }
        else if (comand == "exit")
        {
            break;
        }
        else
        {
            cout << vermelho << "Comando invalido" << reset << endl;
        }
    }

    return 0;
}