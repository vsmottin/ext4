#include "ext4.hpp"
#include "cores.h"
#include "utils.hpp"
#include <iostream>
#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;

int main(){
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    string fileName;
    Ext4::FileSystemManager fs;
    bool img_chosen = false;
    while (!img_chosen){
        cout << amarelo << "ext4shell " << reset;
        cin >> fileName;
        img_chosen = fs.setImage(trim(fileName));
    }

    fileName = trim(fileName);
    string comand;
    while (true)
    {
        cout << amarelo << "ext4shell:" << reset azul << "[" << fileName << fs.getCurrentPath() << "]" << reset << "$ ";
        cin >> comand;
        comand = trim(comand);

        if (comand == "info"){
            fs.info();

        } else if (comand == "attr"){
            string alvo;
            cin >> alvo;
            fs.attr(trim(alvo));

        } else if (comand == "ls"){
            fs.ls();
        
        } else if (comand == "testi"){
            uint32_t inode;
            cin >> inode;
            fs.testi(inode);
        
        } else if (comand == "testb"){
            uint32_t block;
            cin >> block;
            fs.testb(block);
        } else if (comand == "pwd")
        {
            fs.pwd();
        }
        else if (comand == "touch")
        {
            string path;
            getline(cin >> ws, path);
            if (path.length() == 0)
            {
                cout << vermelho << "Caminho inválido" << reset << endl;
                continue;
            }
            fs.touch(path);
        }
        else if (comand == "cd")
        {
            string path;
            cin >> path;
            fs.cd(path);
        }
        else if (comand == "mkdir")
        {
            string name;
            getline(cin >> ws, name);
            if (name.empty())
                {
                    cout << vermelho << "Nome inválido" << reset << endl;
                    continue;
                }
            fs.mkdir(name);
        }
        else if (comand == "exit")
        {
            break;
        
        } else {
            cout << vermelho << "Comando inválido" << reset << endl;
        }
    }

    return 0;
}