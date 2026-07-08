/**
 * @file inode.cpp
 * @author Caroline Lau, Maria Bambini e Victória Mottin
 * @brief Implementa a estrutura Inode e métodos auxiliares para interpretação de seus campos (modo, tipo de arquivo, tamanho, timestamps, etc.).
 * @date 2026-07-08
 * 
 */

#include "ext4.hpp"
#include <iostream>
#include <string>

using namespace std;

string Ext4::Inode::getFileType()
{
    switch (this->i_mode & 0xF000)
    {
    case 0x1000:
        return "FIFO";
    case 0x2000:
        return "Dispositivo de caractere";
    case 0x4000:
        return "Diretório";
    case 0x6000:
        return "Dispositivo de bloco";
    case 0x8000:
        return "Arquivo regular";
    case 0xA000:
        return "Link simbólico";
    case 0xC000:
        return "Socket";
    default:
        return "Desconhecido";
    }
}