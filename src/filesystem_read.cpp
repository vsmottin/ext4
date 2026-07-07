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

uint32_t Ext4::FileSystemManager::getGroupFromInode(uint32_t inode_num)
{
    return (inode_num - 1) / this->sb.getInodesPerGroup();
}

Ext4::Inode Ext4::FileSystemManager::resolveNameToInode(const string &path)
{
    uint32_t block_size = this->sb.getBlockSize();
    uint32_t dataBlock = this->getInodeDataBlock(this->current_inode);

    if (dataBlock == 0)
    {
        cout << vermelho << "Erro: diretório atual não possui bloco de dados válido." << reset << endl;
        return Inode{};
    }

    uint64_t blockInDisk = static_cast<uint64_t>(dataBlock) * block_size;
    this->image_file.clear();

    uint16_t bytesRead = 0;
    DirEntry dirEntry;

    while (bytesRead < block_size)
    {
        this->image_file.seekg(blockInDisk + bytesRead);
        this->image_file.read(reinterpret_cast<char *>(&dirEntry.inode), sizeof(uint32_t));
        this->image_file.read(reinterpret_cast<char *>(&dirEntry.rec_len), sizeof(uint16_t));
        this->image_file.read(reinterpret_cast<char *>(&dirEntry.name_len), sizeof(uint8_t));
        this->image_file.read(reinterpret_cast<char *>(&dirEntry.file_type), sizeof(uint8_t));

        if (dirEntry.rec_len < 8 || bytesRead + dirEntry.rec_len > block_size)
        {
            break;
        }

        if (dirEntry.inode != 0 && dirEntry.name_len > 0 && dirEntry.name_len <= (dirEntry.rec_len - 8))
        {
            string name(dirEntry.name_len, '\0');
            this->image_file.read(&name[0], dirEntry.name_len);

            if (name == path)
            {
                return this->readInode(dirEntry.inode);
            }
        }

        bytesRead += dirEntry.rec_len;
    }

    return Inode{};
}

bool Ext4::FileSystemManager::info()
{
    this->sb.superBlockStats();
    return true;
}

void Ext4::FileSystemManager::testi(uint32_t inode)
{
    if (inode == 0 || inode > this->sb.getInodesCount())
    {
        cout << vermelho << "Erro: Inode fora dos limites." << reset << endl;
        return;
    }

    uint32_t inodesPerGroup = this->sb.getInodesPerGroup();
    uint32_t block_group = (inode - 1) / inodesPerGroup;
    uint32_t local_index = (inode - 1) % inodesPerGroup;

    GroupDescriptor desc = this->group_descriptors[block_group];
    uint32_t bitmapBlock = desc.bg_inode_bitmap_lo;
    uint32_t block_size = this->sb.getBlockSize();
    uint64_t bitmapOffset = static_cast<uint64_t>(bitmapBlock) * block_size;
    uint32_t byteOffset = local_index / 8;
    uint32_t bitOffset = local_index % 8;

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

void Ext4::FileSystemManager::testb(uint32_t block)
{
    if (block < this->sb.getFirstDataBlock() || block >= this->sb.getBlocksCount())
    {
        cout << vermelho << "Erro: Bloco fora dos limites." << reset << endl;
        return;
    }

    uint32_t firstDataBlock = this->sb.getFirstDataBlock();
    uint32_t blocksPerGroup = this->sb.getBlocksPerGroup();

    uint32_t block_group = (block - firstDataBlock) / blocksPerGroup;
    uint32_t local_index = (block - firstDataBlock) % blocksPerGroup;

    GroupDescriptor desc = this->group_descriptors[block_group];
    uint32_t bitmapBlock = desc.bg_block_bitmap_lo;
    uint32_t block_size = this->sb.getBlockSize();
    uint64_t bitmapOffset = static_cast<uint64_t>(bitmapBlock) * block_size;
    uint32_t byteOffset = local_index / 8;
    uint32_t bitOffset = local_index % 8;

    this->image_file.clear();
    this->image_file.seekg(bitmapOffset + byteOffset);

    uint8_t byte;
    this->image_file.read(reinterpret_cast<char *>(&byte), 1);

    bool isUsed = (byte & (1 << bitOffset)) != 0;
    if (isUsed)
    {
        cout << "Bloco " << block << " está " << vermelho << "OCUPADO" << reset << endl;
    }
    else
    {
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
    // Varre o bloco do diretório iterando pelo tamanho dinâmico (rec_len) de cada entrada
    while (bytes_read < block_size)
    {
        // Lê os campos fixos do cabeçalho da DirEntry (Inodo, Tamanho do Registro, Tamanho do Nome e Tipo)
        this->image_file.seekg(block_in_disk + bytes_read);
        this->image_file.read(reinterpret_cast<char *>(&dir_entry.inode), sizeof(uint32_t));
        this->image_file.read(reinterpret_cast<char *>(&dir_entry.rec_len), sizeof(uint16_t));
        this->image_file.read(reinterpret_cast<char *>(&dir_entry.name_len), sizeof(uint8_t));
        this->image_file.read(reinterpret_cast<char *>(&dir_entry.file_type), sizeof(uint8_t));

        if (dir_entry.rec_len < 8 || bytes_read + dir_entry.rec_len > block_size)
        {
            break;
        }

        // Se a entrada for válida (Inodo não zerado), lê a string do nome diretamente do disco e a exibe
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
    uint32_t block_group = this->getGroupFromInode(inode_num);
    uint32_t index = (inode_num - 1) % this->sb.getInodesPerGroup();

    // Localiza a tabela de inodes do grupo de blocos correspondente e calcula o offset exato do inode em bytes
    GroupDescriptor& desc = this->group_descriptors[block_group];
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

    // Valida a assinatura mágica e a profundidade da árvore de Extents no cabeçalho interno do Inode
    ExtentHeader extent_header;
    memcpy(&extent_header, &inode.i_block[0], sizeof(ExtentHeader));
    if (extent_header.eh_magic != 0xF30A || extent_header.eh_depth != 0)
    {
        return 0;
    }
    // Extrai o primeiro nó folha (Extent) e retorna o endereço físico do bloco de dados inicial
    Extent extent_leaf;
    memcpy(&extent_leaf, &inode.i_block[3], sizeof(Extent));

    return extent_leaf.ee_start_lo;
}

void Ext4::FileSystemManager::attr(string path)
{
    Inode inode = this->resolveNameToInode(path);

    if (inode.i_mode == 0)
    {
        cout << vermelho << "attr: '" << path << "' não encontrado no diretório atual" << reset << endl;
        return;
    }

    const char letters[3] = {'r', 'w', 'x'};
    string permissions;
    for (int i = 8; i >= 0; i--)
    {
        bool on = (inode.i_mode & (1 << i)) != 0;
        permissions += on ? letters[2 - (i % 3)] : '-';
    }

    uint64_t size = (static_cast<uint64_t>(inode.i_size_high) << 32) | inode.i_size_lo;

    time_t t_access = inode.i_atime;
    time_t t_modif = inode.i_mtime;
    time_t t_meta = inode.i_ctime;
    time_t t_create = inode.i_crtime;

    cout << endl;
    cout << ciano << "Atributos de " << negrito << path << reset << endl;
    cout << "Tipo:              " << inode.getFileType() << endl;
    cout << "Permissões:        " << permissions << " (" << oct << (inode.i_mode & 0x1FF) << dec << ")" << endl;
    cout << "Tamanho:           " << size << " bytes" << endl;
    cout << "Dono (UID):        " << inode.i_uid << endl;
    cout << "Grupo (GID):       " << inode.i_gid << endl;
    cout << "Links:             " << inode.i_links_count << endl;
    cout << "Blocos (512B):     " << inode.i_blocks_lo << endl;
    cout << "Ultimo acesso:     " << ctime(&t_access);
    cout << "Modificação:       " << ctime(&t_modif);
    cout << "Alteração (meta):  " << ctime(&t_meta);
    cout << "Criação:           " << ctime(&t_create);
}

void Ext4::FileSystemManager::pwd()
{
    cout << this->current_path << endl;
}

Ext4::Inode Ext4::FileSystemManager::readInode(uint32_t inode_num)
{
    Inode inode{};
    uint64_t offset = this->getInodeOffset(inode_num);
    this->image_file.clear();
    this->image_file.seekg(offset);
    this->image_file.read(reinterpret_cast<char *>(&inode), sizeof(Inode));
    return inode;
}

bool Ext4::FileSystemManager::isDirectory(const Inode &inode)
{
    return (inode.i_mode & 0xF000) == 0x4000;
}

void Ext4::FileSystemManager::collectExtentBlocks(uint32_t block_num, std::vector<uint32_t> &blocks)
{
    ExtentHeader header;
    uint32_t block_size = this->sb.getBlockSize();

    this->image_file.clear();
    this->image_file.seekg(static_cast<uint64_t>(block_num) * block_size);
    this->image_file.read(reinterpret_cast<char *>(&header), sizeof(ExtentHeader));

    if (header.eh_magic != 0xF30A)
        return;

    if (header.eh_depth == 0)
    {
        for (int i = 0; i < header.eh_entries; i++)
        {
            Extent extent;
            this->image_file.read(reinterpret_cast<char *>(&extent), sizeof(Extent));
            for (uint16_t j = 0; j < extent.ee_len; j++)
                blocks.push_back(extent.ee_start_lo + j);
        }
    }
    else
    {
        for (int i = 0; i < header.eh_entries; i++)
        {
            ExtentIdx idx;
            this->image_file.read(reinterpret_cast<char *>(&idx), sizeof(ExtentIdx));
            this->collectExtentBlocks(idx.ei_leaf_lo, blocks);
        }
    }
}

std::vector<uint32_t> Ext4::FileSystemManager::getInodeDataBlocks(uint32_t inode_num)
{
    std::vector<uint32_t> blocks;
    Inode inode = this->readInode(inode_num);

    ExtentHeader header;
    memcpy(&header, &inode.i_block[0], sizeof(ExtentHeader));
    if (header.eh_magic != 0xF30A)
        return blocks;

    if (header.eh_depth == 0)
    {
        for (int i = 0; i < header.eh_entries; i++)
        {
            Extent extent;
            memcpy(&extent, &inode.i_block[3 + i * 3], sizeof(Extent));
            for (uint16_t j = 0; j < extent.ee_len; j++)
                blocks.push_back(extent.ee_start_lo + j);
        }
    }
    else
    {
        for (int i = 0; i < header.eh_entries; i++)
        {
            ExtentIdx idx;
            memcpy(&idx, &inode.i_block[3 + i * 3], sizeof(ExtentIdx));
            this->collectExtentBlocks(idx.ei_leaf_lo, blocks);
        }
    }

    return blocks;
}

uint32_t Ext4::FileSystemManager::findInodeInDirectory(uint32_t dir_inode, const std::string &name)
{
    uint32_t block_size = this->sb.getBlockSize();
    std::vector<uint32_t> blocks = this->getInodeDataBlocks(dir_inode);

    for (uint32_t block : blocks)
    {
        std::vector<char> buffer(block_size);
        this->image_file.clear();
        this->image_file.seekg(static_cast<uint64_t>(block) * block_size);
        this->image_file.read(buffer.data(), block_size);

        uint32_t pos = 0;
        while (pos + 8 <= block_size)
        {
            DirEntry *entry = reinterpret_cast<DirEntry *>(&buffer[pos]);
            if (entry->rec_len < 8 || pos + entry->rec_len > block_size)
                break;

            if (entry->inode != 0 &&
                entry->name_len == name.length() &&
                strncmp(entry->name, name.c_str(), entry->name_len) == 0)
            {
                return entry->inode;
            }

            pos += entry->rec_len;
        }
    }

    return 0;
}

static std::vector<std::string> tokenizePath(const std::string &path)
{
    std::vector<std::string> tokens;
    std::string component;
    for (size_t i = 0; i <= path.size(); i++)
    {
        if (i == path.size() || path[i] == '/')
        {
            if (!component.empty() && component != ".")
                tokens.push_back(component);
            component.clear();
        }
        else
        {
            component += path[i];
        }
    }
    return tokens;
}

void Ext4::FileSystemManager::cd(std::string path)
{
    if (path.empty())
        return;

    uint32_t root_inode;
    std::vector<std::string> path_stack;

    if (path[0] == '/')
    {
        root_inode = 2;
    }
    else
    {
        root_inode = this->current_inode;
        path_stack = tokenizePath(this->current_path);
    }

    std::vector<std::string> components = tokenizePath(path);

    for (const std::string &component : components)
    {
        if (component == "..")
        {
            uint32_t parent_inode = this->findInodeInDirectory(root_inode, "..");
            if (parent_inode == 0)
            {
                cout << vermelho << "Erro: não foi possível localizar o diretório pai." << reset << endl;
                return;
            }
            root_inode = parent_inode;
            if (!path_stack.empty())
                path_stack.pop_back();
        }
        else
        {
            uint32_t found_inode = this->findInodeInDirectory(root_inode, component);
            if (found_inode == 0)
            {
                cout << vermelho << "Erro: \"" << component << "\" não existe." << reset << endl;
                return;
            }

            Inode inode = this->readInode(found_inode);
            if (!this->isDirectory(inode))
            {
                cout << vermelho << "Erro: \"" << component << "\" não é um diretório." << reset << endl;
                return;
            }

            root_inode = found_inode;
            path_stack.push_back(component);
        }
    }

    this->current_inode = root_inode;
    this->current_path = "/";
    for (size_t i = 0; i < path_stack.size(); i++)
    {
        this->current_path += path_stack[i];
        if (i + 1 < path_stack.size())
            this->current_path += "/";
    }
}

string Ext4::FileSystemManager::getCurrentPath()
{
    return this->current_path;
}

bool Ext4::FileSystemManager::isRegularFile(const Inode &inode)
{
    return (inode.i_mode & 0xF000) == 0x8000;
}

uint64_t Ext4::FileSystemManager::getInodeSizeBytes(const Inode &inode)
{
    return (static_cast<uint64_t>(inode.i_size_high) << 32) | inode.i_size_lo;
}

void Ext4::FileSystemManager::cat(string name)
{
    if (name.empty())
    {
        cout << vermelho << "Erro: nome de arquivo inválido." << reset << endl;
        return;
    }

    uint32_t file_inode_num = this->findInodeInDirectory(this->current_inode, name);
    if (file_inode_num == 0)
    {
        cout << vermelho << "Erro: \"" << name << "\" não existe." << reset << endl;
        return;
    }

    Inode file_inode = this->readInode(file_inode_num);

    if (!this->isRegularFile(file_inode))
    {
        cout << vermelho << "Erro: \"" << name << "\" não é um arquivo regular." << reset << endl;
        return;
    }

    uint64_t file_size = this->getInodeSizeBytes(file_inode);
    if (file_size == 0)
    {
        return;
    }

    uint32_t block_size = this->sb.getBlockSize();
    vector<uint32_t> blocks = this->getInodeDataBlocks(file_inode_num);

    if (blocks.empty())
    {
        cout << vermelho << "Erro: arquivo não possui blocos de dados válidos." << reset << endl;
        return;
    }

    uint64_t bytes_remaining = file_size;
    vector<char> buffer(block_size);

    for (uint32_t block : blocks)
    {
        if (bytes_remaining == 0)
            break;

        uint32_t bytes_to_read = (bytes_remaining < block_size) ? static_cast<uint32_t>(bytes_remaining) : block_size;

        this->image_file.clear();
        this->image_file.seekg(static_cast<uint64_t>(block) * block_size);
        this->image_file.read(buffer.data(), bytes_to_read);

        cout.write(buffer.data(), bytes_to_read);

        bytes_remaining -= bytes_to_read;
    }

    cout << endl;
}

uint64_t Ext4::FileSystemManager::getGroupDescriptorOffset(uint32_t group)
{
    uint32_t block_size = this->sb.getBlockSize();
    uint32_t gdt_start_block = (block_size == 1024) ? 2 : 1;
    return static_cast<uint64_t>(gdt_start_block) * block_size + static_cast<uint64_t>(group) * sizeof(GroupDescriptor);
}