#include "ext4.hpp"
#include "cores.h"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <cstring>

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

string Ext4::SuperBlock::getLastMounted()
{
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

string Ext4::SuperBlock::getUUID()
{
    char hex_chars[] = "0123456789abcdef";
    string uuid_str;
    for (int i = 0; i < 16; i++)
    {
        uint8_t byte = this->s_uuid[i];
        char letra1 = hex_chars[byte / 16];
        char letra2 = hex_chars[byte % 16];
        uuid_str += letra1;
        uuid_str += letra2;
        if (i == 3 || i == 5 || i == 7 || i == 9)
        {
            uuid_str += "-";
        }
    }
    return uuid_str;
}

string Ext4::SuperBlock::getCreatorOS()
{
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

string Ext4::SuperBlock::getErrorBehavior()
{
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
    return 1024 << this-> s_log_block_size; //mesmo que 2^(10 +  sb.s_log_block_size).
}

uint32_t Ext4::SuperBlock::getBlocksPerGroup()
{
    return this->s_blocks_per_group;
}

uint32_t Ext4::SuperBlock::getInodesPerGroup()
{
    return this->s_inodes_per_group;
}

uint32_t Ext4::SuperBlock::getBlockGroupsCount()
{
    uint32_t blocksCount = this->s_blocks_count_lo;
    uint32_t blocksPerGroup = this->s_blocks_per_group;
    uint32_t firstDataBlock = this->s_first_data_block;

    if (blocksPerGroup == 0)
        return 0;

    return (blocksCount - firstDataBlock + blocksPerGroup - 1) / blocksPerGroup;
}

uint32_t Ext4::SuperBlock::getFirstDataBlock()
{
    return this->s_first_data_block;
}

string Ext4::SuperBlock::getMagic()
{
    switch (this->s_magic)
    {
    case 0xEF53:
        return "0xEF53 (Ext4)";
    default:
        return to_string(s_magic) + " (Desconhecido)";
    }
}

uint32_t Ext4::SuperBlock::getInodeSize(){
    return this-> s_inode_size;
}

Inode Ext4::FileSystemManager::readInode(uint32_t inode){
    if (inode == 0 || inode > this-> sb.getBlockGroupsCount() * this-> sb.getInodesPerGroup()) {
        cout << vermelho << "Erro: Inode fora dos limites." << reset << endl;
        return Inode{};
    }

    uint32_t inodesPerGroup = this-> sb.getInodesPerGroup();
    uint32_t blockGroup = (inode - 1) / inodesPerGroup;
    uint32_t localIndex = (inode - 1) % inodesPerGroup;

    GroupDescriptor desc = this-> group_descriptors[blockGroup];
    uint32_t inodeTableBlock = desc.bg_inode_table_lo;
    uint32_t blockSize = this-> sb.getBlockSize();
    uint32_t inodeSize = this-> sb.getInodeSize();
    uint64_t tableOffset = static_cast<uint64_t>(inodeTableBlock) * blockSize + localIndex * inodeSize;

    Inode node{};
    this-> image_file.clear(); 
    this-> image_file.seekg(tableOffset);
    this-> image_file.read(reinterpret_cast<char*>(&node), sizeof(Inode));

    return node;
}

void Ext4::SuperBlock::superBlockStats()
{
    uint32_t block_size = this->getBlockSize();
    double total_mb = (static_cast<double>(this->s_blocks_count_lo) * block_size) / (1024.0 * 1024.0);
    double livre_mb = (static_cast<double>(this->s_free_blocks_count_lo) * block_size) / (1024.0 * 1024.0);

    uint32_t inodes_em_uso = this->s_inodes_count - this->s_free_inodes_count;

    cout << endl;
    cout << ciano negrito << "Informações do sistema de arquivos:" << reset << endl;
    cout << "Nome do volume do sistema de arquivos:         " << rosa negrito << this->getVolumeName() << reset << endl;
    cout << "Último local de montagem:                      " << azul negrito << this->getLastMounted() << reset << endl;
    cout << "UUID do sistema de arquivos:                   " << this->getUUID() << endl;
    cout << "Número mágico do sistema de arquivos:          " << this->getMagic() << endl;
    cout << "Comportamento em caso de erro:                 " << this->getErrorBehavior() << endl;
    cout << "Tipo de SO criador:                            " << this->getCreatorOS() << endl;
    cout << endl;
    cout << ciano negrito << "Informações dos blocos:" << reset << endl;
    cout << "Blocos totais:                                 " << this->s_blocks_count_lo << " (" << total_mb << " MB)" << endl;
    cout << "Tamanho do bloco:                              " << block_size << " bytes" << endl;
    cout << "Blocos livres:                                 " << this->s_free_blocks_count_lo << " (" << livre_mb << " MB)" << endl;
    cout << "Blocos reservados:                             " << this->s_r_blocks_count_lo << endl;
    cout << "Blocos por grupo:                              " << this->s_blocks_per_group << endl;
    cout << endl;
    cout << ciano negrito << "Informações dos inodes:" << reset << endl;
    cout << "Inodes totais:                                 " << this->s_inodes_count << " (" << inodes_em_uso << " em uso)" << endl;
    cout << "Tamanho do inode:                              " << this->s_inode_size << " bytes" << endl;
    cout << "Inodes livres:                                 " << this->s_free_inodes_count << endl;
    cout << "Inodes por grupo:                              " << this->s_inodes_per_group << endl;
    cout << endl;
}

bool Ext4::FileSystemManager::setImage(string fileName)
{
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

    uint32_t blockSize = this->sb.getBlockSize();

    // block group descriptor table.
    uint32_t bgdtStartBlock = (blockSize == 1024) ? 2 : 1;

    uint64_t bgdtOffset = bgdtStartBlock * blockSize;
    file.seekg(bgdtOffset);
    uint32_t blockGroupsCount = this->sb.getBlockGroupsCount();
    this->group_descriptors.resize(blockGroupsCount);
    file.read(reinterpret_cast<char *>(this->group_descriptors.data()), sizeof(GroupDescriptor) * blockGroupsCount);
    this->current_inode = 2;
    this->image_file = move(file);
    return true;
}

bool Ext4::FileSystemManager::info()
{
    this->sb.superBlockStats();
    return true;
}

void Ext4::FileSystemManager::testi(uint32_t inode)
{
    if (inode == 0 || inode > this->sb.getBlockGroupsCount() * this->sb.getInodesPerGroup())
    {
        cout << vermelho << "Erro: Inode fora dos limites." << reset << endl;
        return;
    }

    uint32_t inodesPerGroup = this->sb.getInodesPerGroup();
    uint32_t blockGroup = (inode - 1) / inodesPerGroup;
    uint32_t localIndex = (inode - 1) % inodesPerGroup;

    GroupDescriptor desc = this->group_descriptors[blockGroup];
    uint32_t bitmapBlock = desc.bg_inode_bitmap_lo;
    uint32_t blockSize = this->sb.getBlockSize();
    uint64_t bitmapOffset = static_cast<uint64_t>(bitmapBlock) * blockSize;
    uint32_t byteOffset = localIndex / 8;
    uint32_t bitOffset = localIndex % 8;

    this->image_file.clear();
    this->image_file.seekg(bitmapOffset + byteOffset);

    uint8_t byte;
    this->image_file.read(reinterpret_cast<char *>(&byte), 1);

    bool isUsed = (byte & (1 << bitOffset)) != 0;
    if (isUsed)
    {
        cout << "Inode " << inode << " está " << vermelho << "OCUPADO" << reset << endl;
    }
    else
    {
        cout << "Inode " << inode << " está " << verde << "LIVRE" << reset << endl;
    }
}

void Ext4::FileSystemManager::testb(uint32_t block){
    if (block == 0 || block > this-> sb.getBlockGroupsCount() * this-> sb.getBlocksPerGroup()) {
        cout << vermelho << "Erro: Bloco fora dos limites." << reset << endl;
        return;
    }

    uint32_t firstDataBlock = this->sb.getFirstDataBlock();
    uint32_t blocksPerGroup = this->sb.getBlocksPerGroup();

    uint32_t blockGroup = (block - firstDataBlock) / blocksPerGroup;
    uint32_t localIndex = (block - firstDataBlock) % blocksPerGroup;
    
    GroupDescriptor desc = this-> group_descriptors[blockGroup];
    uint32_t bitmapBlock = desc.bg_block_bitmap_lo;
    uint32_t blockSize = this->sb.getBlockSize();
    uint64_t bitmapOffset = static_cast<uint64_t>(bitmapBlock) * blockSize;
    uint32_t byteOffset = localIndex / 8;
    uint32_t bitOffset = localIndex % 8;

    this->image_file.clear();
    this->image_file.seekg(bitmapOffset + byteOffset);

    uint8_t byte;
    this->image_file.read(reinterpret_cast<char *>(&byte), 1);

    bool isUsed = (byte & (1 << bitOffset)) != 0;
    if (isUsed) {
        cout << "Bloco " << block << " está " << vermelho << "OCUPADO" << reset << endl;
    } else {
        cout << "Bloco " << block << " está " << verde << "LIVRE" << reset << endl;
    }
}

void Ext4::FileSystemManager::ls()
{
    uint32_t block_size = this->sb.getBlockSize();
    uint32_t data_block = this->getInodeDataBlock(this->current_inode);

    if (data_block == 0)
    {
        cout << vermelho << "Erro: diretório atual não possui bloco de dados válido." << reset << endl;
        return;
    }

    uint64_t block_in_disk = static_cast<uint64_t>(data_block) * block_size;
    this->image_file.clear();
    this->image_file.seekg(block_in_disk);
    uint16_t bytes_read = 0;
    DirEntry dir_entry;
    while (bytes_read < block_size)
    {
        this->image_file.seekg(block_in_disk + bytes_read);
        this->image_file.read(reinterpret_cast<char *>(&dir_entry.inode), sizeof(uint32_t));
        this->image_file.read(reinterpret_cast<char *>(&dir_entry.rec_len), sizeof(uint16_t));
        this->image_file.read(reinterpret_cast<char *>(&dir_entry.name_len), sizeof(uint8_t));
        this->image_file.read(reinterpret_cast<char *>(&dir_entry.file_type), sizeof(uint8_t));

        if (dir_entry.rec_len < 8 || bytes_read + dir_entry.rec_len > block_size)
        {
            break;
        }

        if (dir_entry.inode != 0 && dir_entry.name_len > 0 && dir_entry.name_len <= (dir_entry.rec_len - 8))
        {
            string name(dir_entry.name_len, '\0');
            this->image_file.read(&name[0], dir_entry.name_len);

            cout << name << "   ";
        }

        bytes_read += dir_entry.rec_len;
    }
    cout << endl;
}

uint64_t Ext4::FileSystemManager::getInodeOffset(uint32_t inode_num)
{
    uint32_t block_size = this->sb.getBlockSize();
    uint32_t block_group = (inode_num - 1) / this->sb.getInodesPerGroup();
    uint32_t index = (inode_num - 1) % this->sb.getInodesPerGroup();

    GroupDescriptor desc = this->group_descriptors[block_group];
    uint64_t inode_table_bytes = static_cast<uint64_t>(desc.bg_inode_table_lo) * block_size;

    return inode_table_bytes + (index * this->sb.getInodeSize());
}

uint32_t Ext4::FileSystemManager::getInodeDataBlock(uint32_t inode_num)
{
    uint64_t offset = this->getInodeOffset(inode_num);
    this->image_file.clear();
    this->image_file.seekg(offset);

    Inode inode;
    this->image_file.read(reinterpret_cast<char *>(&inode), sizeof(Inode));

    ExtentHeader extent_header;
    memcpy(&extent_header, &inode.i_block[0], sizeof(ExtentHeader));
    if (extent_header.eh_magic != 0xF30A || extent_header.eh_depth != 0)
    {
        return 0;
    }
    Extent extent_leaf;
    memcpy(&extent_leaf, &inode.i_block[3], sizeof(Extent));

    return extent_leaf.ee_start_lo;
}