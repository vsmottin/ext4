#include "ext4.hpp"
#include "cores.h"
#include <string>
#include <fstream>
#include <iostream>
#include <cstring>
#include <vector>

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
    return 1024 << this->s_log_block_size;
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

uint32_t Ext4::SuperBlock::getInodeSize()
{
    return this->s_inode_size;
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

uint32_t Ext4::SuperBlock::getFirstFreeInode()
{
    return this->s_first_ino;
}

bool Ext4::FileSystemManager::setImage(string fileName)
{
    fileName.insert(0, "./images_ext4/");
    fstream file(fileName, ios::in | ios::out | ios::binary);
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
    this->current_path = "/";
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

void Ext4::FileSystemManager::testb(uint32_t bloco)
{
    if (bloco == 0 || bloco > this->sb.getBlockGroupsCount() * this->sb.getBlocksPerGroup())
    {
        cout << vermelho << "Erro: Bloco fora dos limites." << reset << endl;
        return;
    }

    uint32_t firstDataBlock = this->sb.getFirstDataBlock();
    uint32_t blocksPerGroup = this->sb.getBlocksPerGroup();

    uint32_t blockGroup = (bloco - firstDataBlock) / blocksPerGroup;
    uint32_t localIndex = (bloco - firstDataBlock) % blocksPerGroup;

    GroupDescriptor desc = this->group_descriptors[blockGroup];
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
    if (isUsed)
    {
        cout << "Bloco " << bloco << " está " << vermelho << "OCUPADO" << reset << endl;
    }
    else
    {
        cout << "Bloco " << bloco << " está " << verde << "LIVRE" << reset << endl;
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

bool Ext4::FileSystemManager::isDirectory(const Inode& inode)
{
    return (inode.i_mode & 0xF000) == 0x4000;
}

void Ext4::FileSystemManager::collectExtentBlocks(uint32_t block_num, std::vector<uint32_t>& blocks)
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

void Ext4::FileSystemManager::touch(string path)
{
    if (path.length() > 255)
    {
        cout << vermelho << "Erro: O nome do arquivo é longo demais! O limite do Ext4 é 255 caracteres." << reset << endl;
        return;
    }
    uint32_t block_size = this->sb.getBlockSize();

    uint32_t data_block = getInodeDataBlock(this->current_inode);
    uint64_t data_block_offset = (static_cast<uint64_t>(data_block) * block_size);

    this->image_file.seekg(data_block_offset);
    vector<char> data_block_bytes(block_size);
    this->image_file.read(data_block_bytes.data(), block_size);

    uint32_t current_offset = 0;
    DirEntry *last_dir_entry = nullptr;
    uint32_t last_offset = 0;

    while (current_offset < block_size)
    {
        if (current_offset + 8 > block_size)
        {
            cout << vermelho << "Erro: Bloco de diretório corrompido (extrapolou o limite do bloco)." << reset << endl;
            return;
        }

        DirEntry *attempt_dir_entry = reinterpret_cast<DirEntry *>(&data_block_bytes[current_offset]);
        if (attempt_dir_entry->rec_len < 12 || (attempt_dir_entry->rec_len % 4) != 0)
        {
            cout << vermelho << "Erro: Estrutura Ext4 inválida detectada (rec_len inválido: "
                 << attempt_dir_entry->rec_len << ")." << reset << endl;
            return;
        }
        if (current_offset + attempt_dir_entry->rec_len > block_size)
        {
            cout << vermelho << "Erro: Entrada de diretório aponta para fora do bloco ("
                 << current_offset + attempt_dir_entry->rec_len << " > " << block_size << ")." << reset << endl;
            return;
        }
        if (attempt_dir_entry->inode != 0)
        {
            last_dir_entry = attempt_dir_entry;
            last_offset = current_offset;
        }
        current_offset += attempt_dir_entry->rec_len;
    }

    if (last_dir_entry == nullptr)
    {
        cout << vermelho << "Erro: Nenhum registro válido de diretório foi encontrado no bloco." << reset << endl;
        return;
    }

    uint32_t new_rec_len = ((last_dir_entry->name_len + 8 + 3) / 4) * 4;
    uint32_t space_left = last_dir_entry->rec_len - new_rec_len;
    uint32_t new_file_required_size = ((path.length() + 8 + 3) / 4) * 4;

    if (space_left < new_file_required_size)
    {
        cout << vermelho << "Erro: O diretório atual não tem espaço para este arquivo!" << reset << endl;
        return;
    }
    uint32_t group = (this->current_inode - 1) / this->sb.getInodesPerGroup();
    GroupDescriptor desc = this->group_descriptors[group];
    uint32_t inode_bitmap = desc.bg_inode_bitmap_lo;
    uint64_t inode_bitmap_offset = static_cast<uint64_t>(inode_bitmap) * block_size;

    this->image_file.clear();
    this->image_file.seekg(inode_bitmap_offset);
    vector<char> bytes(block_size);
    this->image_file.read(bytes.data(), block_size);

    uint32_t free_inode_index;
    bool found = false;
    for (uint32_t i = 0; i < block_size && !found; i++)
    {
        for (uint8_t j = 0; j < 8; j++)
        {
            if ((bytes[i] & (1 << j)) == 0)
            {
                uint32_t inode_number = (group * this->sb.getInodesPerGroup()) + (i * 8) + j + 1;
                if (inode_number >= this->sb.getFirstFreeInode())
                {
                    free_inode_index = inode_number;
                    found = true;
                    bytes[i] |= (1 << j);
                    break;
                }
            }
        }
    }

    if (!found)
    {
        cout << vermelho << "Erro: o sistema de arquivos está cheio! Não há inodes livres" << reset << endl;
        return;
    }

    this->image_file.seekp(inode_bitmap_offset);
    this->image_file.write(bytes.data(), block_size);

    uint32_t local_index_inode = (free_inode_index - 1) % this->sb.getInodesPerGroup();
    uint64_t inode_offset = (static_cast<uint64_t>(desc.bg_inode_table_lo) * block_size) +
                            static_cast<uint64_t>(local_index_inode) * this->sb.getInodeSize();
    this->image_file.seekp(inode_offset);
    Inode inode{};
    inode.i_mode = 0x81A4;
    inode.i_links_count = 1;
    this->image_file.write(reinterpret_cast<char *>(&inode), sizeof(Inode));

    uint32_t padding_size = this->sb.getInodeSize() - sizeof(Inode);
    if (padding_size > 0)
    {
        vector<char> padding(padding_size, 0);
        this->image_file.write(padding.data(), padding_size);
    }
    last_dir_entry->rec_len = new_rec_len;
    uint32_t new_entry_offset = last_offset + new_rec_len;
    DirEntry *new_entry = reinterpret_cast<DirEntry *>(&data_block_bytes[new_entry_offset]);
    new_entry->inode = free_inode_index;
    new_entry->name_len = path.length();
    new_entry->file_type = 1;
    new_entry->rec_len = space_left;
    uint32_t padding_bytes_to_clear = new_file_required_size - 8 - path.length();
    if (padding_bytes_to_clear > 0)
    {
        memset(new_entry->name + path.length(), 0, padding_bytes_to_clear);
    }
    memcpy(new_entry->name, path.c_str(), path.length());

    this->image_file.seekp(data_block_offset);
    this->image_file.write(data_block_bytes.data(), block_size);
}

// As funções abaixo (allocateFreeInode, allocateFreeBlock) e as operações de
// escrita em touch()/mkdir() modificam bitmaps de blocos/inodes, inodes e
// entradas de diretório diretamente no disco, mas não recalculam os checksums
// exigidos pela feature "metadata_csum" do ext4 (CRC32C sobre bitmaps, group
// descriptors, inodes e DirEntryTail).

// Também não são atualizados os contadores agregados de blocos/inodes livres
// no superblock (s_free_blocks_count_lo, s_free_inodes_count) nem nos group
// descriptors (bg_free_blocks_count_lo, bg_free_inodes_count, bg_used_dirs_count).
// Os bitmaps (fonte real da informação de alocação) ficam corretos; apenas os
// contadores "de cache" ficam desatualizados.
uint32_t Ext4::FileSystemManager::allocateFreeInode(uint32_t group)
{
    uint32_t block_size = this->sb.getBlockSize();
    GroupDescriptor &desc = this->group_descriptors[group];
    uint64_t bitmap_offset = static_cast<uint64_t>(desc.bg_inode_bitmap_lo) * block_size;

    vector<char> bitmap(block_size);
    this->image_file.clear();
    this->image_file.seekg(bitmap_offset);
    this->image_file.read(bitmap.data(), block_size);

    uint32_t inodes_per_group = this->sb.getInodesPerGroup();
    for (uint32_t i = 0; i < inodes_per_group; i++)
    {
        uint32_t byte_index = i / 8;
        uint8_t bit_index = i % 8;

        if ((bitmap[byte_index] & (1 << bit_index)) == 0)
        {
            uint32_t inode_number = group * inodes_per_group + i + 1;
            if (inode_number < this->sb.getFirstFreeInode())
                continue;

            bitmap[byte_index] |= (1 << bit_index);
            this->image_file.clear();
            this->image_file.seekp(bitmap_offset);
            this->image_file.write(bitmap.data(), block_size);
            return inode_number;
        }
    }
    return 0;
}

uint32_t Ext4::FileSystemManager::allocateFreeBlock(uint32_t preferred_group)
{
    uint32_t block_size = this->sb.getBlockSize();
    uint32_t blocks_per_group = this->sb.getBlocksPerGroup();
    uint32_t first_data_block = this->sb.getFirstDataBlock();
    uint32_t group_count = this->sb.getBlockGroupsCount();

    for (uint32_t g = 0; g < group_count; g++)
    {
        uint32_t group = (preferred_group + g) % group_count;
        GroupDescriptor &desc = this->group_descriptors[group];
        uint64_t bitmap_offset = static_cast<uint64_t>(desc.bg_block_bitmap_lo) * block_size;

        vector<char> bitmap(block_size);
        this->image_file.clear();
        this->image_file.seekg(bitmap_offset);
        this->image_file.read(bitmap.data(), block_size);

        for (uint32_t i = 0; i < blocks_per_group; i++)
        {
            uint32_t byte_index = i / 8;
            uint8_t bit_index = i % 8;
            if (byte_index >= block_size)
                break;

            if ((bitmap[byte_index] & (1 << bit_index)) == 0)
            {
                bitmap[byte_index] |= (1 << bit_index);
                this->image_file.clear();
                this->image_file.seekp(bitmap_offset);
                this->image_file.write(bitmap.data(), block_size);
                return first_data_block + group * blocks_per_group + i;
            }
        }
    }
    return 0;
}

void Ext4::FileSystemManager::mkdir(string name)
{
    if (name.empty() || name.length() > 255)
    {
        cout << vermelho << "Erro: nome de diretório inválido." << reset << endl;
        return;
    }

    if (this->findInodeInDirectory(this->current_inode, name) != 0)
    {
        cout << vermelho << "Erro: já existe um arquivo ou diretório com esse nome." << reset << endl;
        return;
    }

    uint32_t block_size = this->sb.getBlockSize();
    uint32_t data_block = this->getInodeDataBlock(this->current_inode);
    uint64_t data_block_offset = static_cast<uint64_t>(data_block) * block_size;

    vector<char> data_block_bytes(block_size);
    this->image_file.clear();
    this->image_file.seekg(data_block_offset);
    this->image_file.read(data_block_bytes.data(), block_size);

    uint32_t current_offset = 0;
    DirEntry *last_dir_entry = nullptr;
    uint32_t last_offset = 0;

    while (current_offset < block_size)
    {
        DirEntry *entry = reinterpret_cast<DirEntry *>(&data_block_bytes[current_offset]);
        if (entry->rec_len < 12 || (entry->rec_len % 4) != 0 || current_offset + entry->rec_len > block_size)
        {
            cout << vermelho << "Erro: estrutura de diretório inválida." << reset << endl;
            return;
        }
        if (entry->inode != 0)
        {
            last_dir_entry = entry;
            last_offset = current_offset;
        }
        current_offset += entry->rec_len;
    }

    if (last_dir_entry == nullptr)
    {
        cout << vermelho << "Erro: diretório atual corrompido." << reset << endl;
        return;
    }

    uint32_t used_rec_len = ((last_dir_entry->name_len + 8 + 3) / 4) * 4;
    uint32_t space_left = last_dir_entry->rec_len - used_rec_len;
    uint32_t required = ((name.length() + 8 + 3) / 4) * 4;

    if (space_left < required)
    {
        cout << vermelho << "Erro: o diretório atual não tem espaço para essa entrada." << reset << endl;
        return;
    }

    uint32_t group = (this->current_inode - 1) / this->sb.getInodesPerGroup();

    uint32_t new_inode_num = this->allocateFreeInode(group);
    if (new_inode_num == 0)
    {
        cout << vermelho << "Erro: sem inodes livres." << reset << endl;
        return;
    }

    uint32_t new_block_num = this->allocateFreeBlock(group);
    if (new_block_num == 0)
    {
        cout << vermelho << "Erro: sem blocos livres." << reset << endl;
        return;
    }

    vector<char> new_dir_block(block_size, 0);

    DirEntry *dot = reinterpret_cast<DirEntry *>(&new_dir_block[0]);
    dot->inode = new_inode_num;
    dot->rec_len = 12;
    dot->name_len = 1;
    dot->file_type = 2;
    dot->name[0] = '.';

    DirEntry *dotdot = reinterpret_cast<DirEntry *>(&new_dir_block[12]);
    dotdot->inode = this->current_inode;
    dotdot->rec_len = block_size - 12;
    dotdot->name_len = 2;
    dotdot->file_type = 2;
    dotdot->name[0] = '.';
    dotdot->name[1] = '.';

    this->image_file.clear();
    this->image_file.seekp(static_cast<uint64_t>(new_block_num) * block_size);
    this->image_file.write(new_dir_block.data(), block_size);

    Inode new_inode{};
    new_inode.i_mode = 0x41ED;
    new_inode.i_links_count = 2;
    new_inode.i_size_lo = block_size;

    ExtentHeader header{};
    header.eh_magic = 0xF30A;
    header.eh_entries = 1;
    header.eh_max = 4;
    header.eh_depth = 0;
    memcpy(&new_inode.i_block[0], &header, sizeof(ExtentHeader));

    Extent extent{};
    extent.ee_block = 0;
    extent.ee_len = 1;
    extent.ee_start_hi = 0;
    extent.ee_start_lo = new_block_num;
    memcpy(&new_inode.i_block[3], &extent, sizeof(Extent));

    uint32_t new_group = (new_inode_num - 1) / this->sb.getInodesPerGroup();
    uint32_t local_index = (new_inode_num - 1) % this->sb.getInodesPerGroup();
    uint64_t inode_offset = static_cast<uint64_t>(this->group_descriptors[new_group].bg_inode_table_lo) * block_size +
                             static_cast<uint64_t>(local_index) * this->sb.getInodeSize();

    this->image_file.clear();
    this->image_file.seekp(inode_offset);
    this->image_file.write(reinterpret_cast<char *>(&new_inode), sizeof(Inode));

    uint32_t padding_size = this->sb.getInodeSize() - sizeof(Inode);
    if (padding_size > 0)
    {
        vector<char> padding(padding_size, 0);
        this->image_file.write(padding.data(), padding_size);
    }

    last_dir_entry->rec_len = used_rec_len;
    uint32_t new_entry_offset = last_offset + used_rec_len;
    DirEntry *new_entry = reinterpret_cast<DirEntry *>(&data_block_bytes[new_entry_offset]);
    new_entry->inode = new_inode_num;
    new_entry->name_len = name.length();
    new_entry->file_type = 2;
    new_entry->rec_len = space_left;
    memset(new_entry->name, 0, required - 8);
    memcpy(new_entry->name, name.c_str(), name.length());

    this->image_file.clear();
    this->image_file.seekp(data_block_offset);
    this->image_file.write(data_block_bytes.data(), block_size);

    Inode parent_inode = this->readInode(this->current_inode);
    parent_inode.i_links_count += 1;
    uint64_t parent_offset = this->getInodeOffset(this->current_inode);
    this->image_file.clear();
    this->image_file.seekp(parent_offset);
    this->image_file.write(reinterpret_cast<char *>(&parent_inode), sizeof(Inode));

    cout << verde << "Diretório \"" << name << "\" criado com sucesso (inode " << new_inode_num << ")." << reset << endl;
}

bool Ext4::FileSystemManager::isRegularFile(const Inode& inode)
{
    return (inode.i_mode & 0xF000) == 0x8000;
}

uint64_t Ext4::FileSystemManager::getInodeSizeBytes(const Inode& inode)
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