#include "ext4.hpp"
#include "cores.h"
#include <string>
#include <fstream>
#include <iostream>

using namespace std;

string Ext4::SuperBlock::getVolumeName()
{
    string volumeName(this->s_volume_name, 16);
    size_t nullPos = volumeName.find('\0');
    if (nullPos != string::npos)
    {
        volumeName.resize(nullPos);
    }
    if (volumeName.empty())
    {
        return "Não definido";
    }
    
    return volumeName;
}

string Ext4::SuperBlock::getLastMounted(){
    string lastMounted(this->s_last_mounted, 64);
    size_t nullPos = lastMounted.find('\0');
    if (nullPos != string::npos)
    {
        lastMounted.resize(nullPos);
    }
    if (lastMounted.empty())
    {
        return "Não definido";
    }
    
    return lastMounted;
}

string Ext4::SuperBlock::getUUID(){
    char hex_chars[] = "0123456789abcdef";
    string uuid_str;
    for (int i = 0; i < 16; i++)
    {
        uint8_t byte = this->s_uuid[i];
        char letra1 = hex_chars[byte/16];
        char letra2 = hex_chars[byte%16];
        uuid_str += letra1;
        uuid_str += letra2;
        if (i == 3 || i == 5 || i == 7 || i == 9)
        {
            uuid_str += "-";
        }
    }
    return uuid_str;
}

string Ext4::SuperBlock::getCreatorOS(){
    switch (this->s_creator_os)
    {
    case 0:
        return "Linux";
    case 1:
        return "Hurd";
    case 2:
        return "Masix";
    case 3:
        return "FreeBSD";
    case 4:
        return "Lites";
    default:
        return "Desconhecido";
    }
}

string Ext4::SuperBlock::getErrorBehavior(){
    switch (this->s_errors)
    {
    case 0:
        return "Ignorar";
    case 1:
        return "Remontar somente leitura";
    case 2:
        return "Panico";
    default:
        return "Desconhecido";
    }
}

uint32_t Ext4::SuperBlock::getBlockSize(){
    return 1024 << this-> s_log_block_size;
}

uint32_t Ext4::SuperBlock::getBlocksPerGroup(){
    return this-> s_blocks_per_group;
}

uint32_t Ext4::SuperBlock::getInodesPerGroup(){
    return this-> s_inodes_per_group;
}

uint32_t Ext4::SuperBlock::getBlockGroupsCount(){
    uint32_t blocksCount = this-> s_blocks_count_lo;
    uint32_t blocksPerGroup = this-> s_blocks_per_group;

    if(blocksPerGroup == 0) return 0;

    return (blocksCount + blocksPerGroup - 1) / blocksPerGroup;
}

uint32_t Ext4::SuperBlock::getFirstDataBlock(){
    return this-> s_first_data_block;
}

string Ext4::SuperBlock::getMagic(){
    switch (this->s_magic)
    {
    case 0xEF53:
        return "Ext4";
    default:
        return "Desconhecido";
    }
}

void Ext4::SuperBlock::superBlockStats()
{
    cout << endl;
    cout << "Nome do volume do sistema de arquivos:         " << rosa negrito << this->getVolumeName() << reset << endl;
    cout << "Último local de montagem:                      " << azul negrito << this->getLastMounted() << reset << endl;
    cout << "UUID do sistema de arquivos:                   " << this->getUUID() << endl;
    cout << "Número mágico do sistema de arquivos:          " << this->getMagic() << endl;
    cout << "Nível de revisão do sistema de arquivos:       " << this->s_rev_level << endl;
    cout << "Recursos do sistema de arquivos:               " << this->s_feature_compat << endl;
    cout << "Comportamento em caso de erro:                 " << this->getErrorBehavior() << endl;
    cout << "Tipo de SO criador:                            " << this->getCreatorOS() << endl;
    cout << endl;
}

bool Ext4::FileSystemManager::setImage(string fileName){
    fileName.insert(0, "./images_ext4/");
    ifstream file(fileName, ios::binary);
    if (!file)
    {
        cout << vermelho << "Erro ao abrir o arquivo, tente novamente" << reset << endl;
        return false;
    }
    cout << verde << "Arquivo aberto com sucesso" << reset << endl;

    
    file.seekg(1024);
    file.read(reinterpret_cast<char *>(&this->sb), sizeof(SuperBlock));

    uint32_t blockSize = this-> sb.getBlockSize();

    //block group descriptor table.
    uint32_t bgdtStartBlock = (blockSize == 1024) ? 2 : 1; 

    uint64_t bgdtOffset = bgdtStartBlock * blockSize;
    file.seekg(bgdtOffset);

    uint32_t blockGroupsCount = this-> sb.getBlockGroupsCount();
    this-> group_descriptors.resize(blockGroupsCount);
    file.read(reinterpret_cast<char *>(this-> group_descriptors.data()), sizeof(GroupDescriptor) * blockGroupsCount);

    this->image_file = move(file);
    return true;
}

bool Ext4::FileSystemManager::info(){
    this->sb.superBlockStats();
    return true;
}

void Ext4::FileSystemManager::testi(uint32_t inode){
    if (inode == 0 || inode > this-> sb.getBlockGroupsCount() * this-> sb.getInodesPerGroup()) {
        cout << vermelho << "Erro: Inode fora dos limites." << reset << endl;
        return;
    }

    uint32_t inodesPerGroup = this-> sb.getInodesPerGroup();
    uint32_t blockGroup = (inode - 1) / inodesPerGroup;
    uint32_t localIndex = (inode - 1) % inodesPerGroup;

    GroupDescriptor desc = this-> group_descriptors[blockGroup];
    uint32_t bitmapBlock = desc.bg_inode_bitmap_lo;
    uint32_t blockSize = this-> sb.getBlockSize();
    uint64_t bitmapOffset = static_cast<uint64_t>(bitmapBlock) * blockSize;
    uint32_t byteOffset = localIndex / 8;
    uint32_t bitOffset = localIndex % 8;

    this-> image_file.clear(); 
    this-> image_file.seekg(bitmapOffset + byteOffset);
    
    uint8_t byte;
    this->image_file.read(reinterpret_cast<char*>(&byte), 1);

    bool isUsed = (byte & (1 << bitOffset)) != 0;
    if (isUsed) {
        cout << "Inode " << inode << " está " << vermelho << "OCUPADO" << reset << endl;
    } else {
        cout << "Inode " << inode << " está " << verde << "LIVRE" << reset << endl;
    }
}

void Ext4::FileSystemManager::testb(uint32_t bloco){
    if (bloco == 0 || bloco > this-> sb.getBlockGroupsCount() * this-> sb.getBlocksPerGroup()) {
        cout << vermelho << "Erro: Bloco fora dos limites." << reset << endl;
        return;
    }

    uint32_t firstDataBlock = this-> sb.getFirstDataBlock();
    uint32_t blocksPerGroup = this-> sb.getBlocksPerGroup();

    uint32_t blockGroup = (bloco - firstDataBlock) / blocksPerGroup;
    uint32_t localIndex = (bloco - firstDataBlock) % blocksPerGroup;
    
    GroupDescriptor desc = this-> group_descriptors[blockGroup];
    uint32_t bitmapBlock = desc.bg_block_bitmap_lo;
    uint32_t blockSize = this-> sb.getBlockSize();
    uint64_t bitmapOffset = static_cast<uint64_t>(bitmapBlock) * blockSize;
    uint32_t byteOffset = localIndex / 8;
    uint32_t bitOffset = localIndex % 8;

    this-> image_file.clear(); 
    this-> image_file.seekg(bitmapOffset + byteOffset);
    
    uint8_t byte;
    this->image_file.read(reinterpret_cast<char*>(&byte), 1);

    bool isUsed = (byte & (1 << bitOffset)) != 0;
    if (isUsed) {
        cout << "Bloco " << bloco << " está " << vermelho << "OCUPADO" << reset << endl;
    } else {
        cout << "Bloco " << bloco << " está " << verde << "LIVRE" << reset << endl;
    }
}