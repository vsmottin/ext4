#include "ext4.hpp"
#include "cores.h"
#include "checksum/ext4checksum.h"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <ctime>
#include <vector>
#include <set>

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

uint32_t Ext4::SuperBlock::getBlockSize()
{
    return 1024 << this->s_log_block_size; // mesmo que 2^(10 +  sb.s_log_block_size).
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

char *Ext4::SuperBlock::getRawUUID()
{
    return reinterpret_cast<char *>(this->s_uuid);
}

uint32_t Ext4::SuperBlock::getDescriptorSize()
{
    return this->s_desc_size;
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

uint32_t Ext4::SuperBlock::getInodeSize()
{
    return this->s_inode_size;
}

uint32_t Ext4::SuperBlock::getInodesCount()
{
    return this->s_inodes_count;
}

uint32_t Ext4::SuperBlock::getBlocksCount()
{
    return this->s_blocks_count_lo;
}

uint32_t Ext4::SuperBlock::getFirstFreeInode()
{
    return this->s_first_ino;
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

void Ext4::SuperBlock::decrementFreeInodesCount(){
    if (this->s_free_inodes_count > 0) {
        this->s_free_inodes_count--;
    }
}

void Ext4::SuperBlock::incrementFreeBlocksCount(){
    this->s_free_blocks_count_lo++;
}

void Ext4::SuperBlock::adjustFreeBlocksCount(int32_t delta)
{
    this->s_free_blocks_count_lo = static_cast<uint32_t>(
        static_cast<int64_t>(this->s_free_blocks_count_lo) + delta);
}

void Ext4::SuperBlock::adjustFreeInodesCount(int32_t delta)
{
    this->s_free_inodes_count = static_cast<uint32_t>(
        static_cast<int64_t>(this->s_free_inodes_count) + delta);
}