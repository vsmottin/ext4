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

    uint32_t block_size = this->sb.getBlockSize();

    // block group descriptor table.
    uint32_t bgdtStartBlock = (block_size == 1024) ? 2 : 1;

    uint64_t bgdtOffset = bgdtStartBlock * block_size;
    file.seekg(bgdtOffset);
    uint32_t block_groupsCount = this->sb.getBlockGroupsCount();
    this->group_descriptors.resize(block_groupsCount);
    file.read(reinterpret_cast<char *>(this->group_descriptors.data()), sizeof(GroupDescriptor) * block_groupsCount);
    this->current_inode = 2;
    this->current_path = "/";
    this->image_file = move(file);
    return true;
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
    uint32_t group = this->getGroupFromInode(this->current_inode);
    GroupDescriptor &desc = this->group_descriptors[group];
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

    this->updateInodeBitmapChecksum(group);

    uint32_t inode_size = this->sb.getInodeSize();
    vector<char> inode_buf(inode_size, 0);
    Inode *inode = reinterpret_cast<Inode *>(inode_buf.data());
    inode->i_mode = 0x81A4;
    inode->i_links_count = 1;
    inode->i_generation = 0;
    inode->i_size_lo = 0;
    inode->i_blocks_lo = 0;
    inode->i_flags = 0x00080000;

    ExtentHeader *eh = reinterpret_cast<ExtentHeader *>(inode->i_block);
    eh->eh_magic = 0xF30A;
    eh->eh_entries = 0;
    eh->eh_max = 4;
    eh->eh_depth = 0;
    eh->eh_generation = 0;

    if (inode_size > 128)
    {
        uint16_t *i_extra_isize = reinterpret_cast<uint16_t *>(&inode_buf[128]);
        *i_extra_isize = 32;
    }

    this->writeInodeWithChecksum(free_inode_index, inode_buf);

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

    this->writeDirBlockWithChecksum(this->current_inode, data_block, data_block_bytes);

    if (desc.bg_free_inodes_count > 0)
        desc.bg_free_inodes_count--;

    desc.bg_itable_unused_lo = 0;
    if (this->sb.getDescriptorSize() > 32)
    {
        desc.bg_itable_unused_hi = 0;
    }
    this->updateGroupDescriptorChecksum(group);
    this->sb.decrementFreeInodesCount();

    vector<char> sb_buffer(1024);
    this->image_file.clear();
    this->image_file.seekg(1024);
    this->image_file.read(sb_buffer.data(), 1024);
    uint32_t *disk_free_inodes = reinterpret_cast<uint32_t *>(&sb_buffer[16]);
    if (*disk_free_inodes > 0)
    {
        (*disk_free_inodes)--;
    }
    this->writeSuperBlockWithChecksum(sb_buffer);
    this->image_file.flush();
    cout << verde << "Arquivo '" << path << "' criado com sucesso!" << reset << endl;
}

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

            this->updateInodeBitmapChecksum(group);

            if (desc.bg_free_inodes_count > 0)
            {
                desc.bg_free_inodes_count--;
            }

            this->updateGroupDescriptorChecksum(group);

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

                this->updateInodeBitmapChecksum(group);
                if (desc.bg_free_blocks_count_lo > 0)
                    desc.bg_free_blocks_count_lo--;
                this->updateGroupDescriptorChecksum(group);
                return first_data_block + group * blocks_per_group + i;
            }
        }
    }
    return 0;
}

// ajustar checksum
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

    uint32_t group = this->getGroupFromInode(this->current_inode);

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

    uint32_t new_group = this->getGroupFromInode(new_inode_num);
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

void Ext4::FileSystemManager::updateBlockBitmapChecksum(uint32_t group)
{
    uint32_t block_size = this->sb.getBlockSize();
    GroupDescriptor &desc = this->group_descriptors[group];

    vector<char> bitmap(block_size);
    this->image_file.clear();
    this->image_file.seekg(static_cast<uint64_t>(desc.bg_block_bitmap_lo) * block_size);
    this->image_file.read(bitmap.data(), block_size);

    int size = this->sb.getBlocksPerGroup() / 8;

    uint32_t csum = checksum_bitmap(this->sb.getRawUUID(), bitmap.data(), size);
    desc.bg_block_bitmap_csum_lo = csum & 0xFFFF;
    if (this->sb.getDescriptorSize() > 32)
        desc.bg_block_bitmap_csum_hi = (csum >> 16) & 0xFFFF;
}

void Ext4::FileSystemManager::updateInodeBitmapChecksum(uint32_t group)
{
    uint32_t block_size = this->sb.getBlockSize();
    GroupDescriptor &desc = this->group_descriptors[group];

    vector<char> bitmap(block_size);
    this->image_file.clear();
    this->image_file.seekg(static_cast<uint64_t>(desc.bg_inode_bitmap_lo) * block_size);
    this->image_file.read(bitmap.data(), block_size);

    int size = this->sb.getInodesPerGroup() / 8;

    uint32_t csum = checksum_bitmap(this->sb.getRawUUID(), bitmap.data(), size);
    desc.bg_inode_bitmap_csum_lo = csum & 0xFFFF;
    if (this->sb.getDescriptorSize() > 32)
        desc.bg_inode_bitmap_csum_hi = (csum >> 16) & 0xFFFF;
}

void Ext4::FileSystemManager::updateGroupDescriptorChecksum(uint32_t group)
{
    GroupDescriptor &desc = this->group_descriptors[group];
    desc.bg_checksum = checksum_group(this->sb.getRawUUID(), group, reinterpret_cast<char *>(&desc));

    this->image_file.clear();
    this->image_file.seekp(this->getGroupDescriptorOffset(group));
    this->image_file.write(reinterpret_cast<char *>(&desc), sizeof(GroupDescriptor));
}

void Ext4::FileSystemManager::writeInodeWithChecksum(uint32_t inode_num, vector<char> &inode_buf)
{
    uint32_t inode_gen;
    memcpy(&inode_gen, &inode_buf[0x64], 4);

    uint32_t csum = checksum_inode(this->sb.getRawUUID(), inode_num, inode_gen, inode_buf.data());
    uint16_t lo = csum & 0xFFFF;
    uint16_t hi = (csum >> 16) & 0xFFFF;
    memcpy(&inode_buf[0x7C], &lo, 2);

    if (this->sb.getInodeSize() > 128)
        memcpy(&inode_buf[0x82], &hi, 2);

    this->image_file.clear();
    this->image_file.seekp(this->getInodeOffset(inode_num));
    this->image_file.write(inode_buf.data(), this->sb.getInodeSize());
}

void Ext4::FileSystemManager::writeDirBlockWithChecksum(uint32_t dir_inode_num, uint32_t block_num, vector<char> &buffer)
{
    uint32_t block_size = this->sb.getBlockSize();
    Inode dir_inode = this->readInode(dir_inode_num);

    uint32_t csum = checksum_dir(this->sb.getRawUUID(), dir_inode_num, dir_inode.i_generation, buffer.data(), block_size);
    memcpy(&buffer[block_size - 4], &csum, 4);

    this->image_file.clear();
    this->image_file.seekp(static_cast<uint64_t>(block_num) * block_size);
    this->image_file.write(buffer.data(), block_size);
}

void Ext4::FileSystemManager::rmdir(string name)
{
    if (name.empty() || name == "." || name == "..")
    {
        cout << vermelho << "Erro: nome de diretório inválido." << reset << endl;
        return;
    }

    uint32_t target_inode = this->findInodeInDirectory(this->current_inode, name);
    if (target_inode == 0)
    {
        cout << vermelho << "Erro: \"" << name << "\" não existe." << reset << endl;
        return;
    }

    Inode inode = this->readInode(target_inode);
    if (!this->isDirectory(inode))
    {
        cout << vermelho << "Erro: \"" << name << "\" não é um diretório." << reset << endl;
        return;
    }

    uint32_t block_size = this->sb.getBlockSize();
    vector<uint32_t> target_blocks = this->getInodeDataBlocks(target_inode);

    // verifica se está vazio (apenas . e ..).
    for (uint32_t block : target_blocks)
    {
        vector<char> buffer(block_size);
        this->image_file.clear();
        this->image_file.seekg(static_cast<uint64_t>(block) * block_size);
        this->image_file.read(buffer.data(), block_size);

        uint32_t pos = 0;
        while (pos + 8 <= block_size)
        {
            DirEntry *entry = reinterpret_cast<DirEntry *>(&buffer[pos]);
            if (entry->rec_len < 8 || pos + entry->rec_len > block_size)
                break;

            if (entry->inode != 0 && entry->name_len > 0)
            {
                string entry_name(entry->name, entry->name_len);
                if (entry_name != "." && entry_name != "..")
                {
                    cout << vermelho << "Erro: o diretório \"" << name << "\" não está vazio." << reset << endl;
                    return;
                }
            }

            pos += entry->rec_len;
        }
    }

    bool removed = false;
    vector<uint32_t> parent_blocks = this->getInodeDataBlocks(this->current_inode);
    for (uint32_t block : parent_blocks)
    {
        uint64_t block_offset = static_cast<uint64_t>(block) * block_size;
        vector<char> buffer(block_size);
        this->image_file.clear();
        this->image_file.seekg(block_offset);
        this->image_file.read(buffer.data(), block_size);

        uint32_t pos = 0;
        DirEntry *prev = nullptr;
        while (pos + 8 <= block_size)
        {
            DirEntry *entry = reinterpret_cast<DirEntry *>(&buffer[pos]);
            if (entry->rec_len < 8 || pos + entry->rec_len > block_size)
                break;

            if (entry->inode == target_inode && entry->name_len == name.length() &&
                strncmp(entry->name, name.c_str(), entry->name_len) == 0)
            {

                if (prev != nullptr)
                {
                    prev->rec_len += entry->rec_len;
                }
                else
                {
                    entry->inode = 0;
                }

                this->writeDirBlockWithChecksum(this->current_inode, block, buffer);
                removed = true;
                break;
            }

            prev = entry;
            pos += entry->rec_len;
        }

        if (removed)
            break;
    }

    if (!removed)
    {
        cout << vermelho << "Erro: não foi possível remover a entrada do diretório." << reset << endl;
        return;
    }

    uint32_t first_data_block = this->sb.getFirstDataBlock();
    uint32_t blocks_per_group = this->sb.getBlocksPerGroup();

    // grupos cujos metadados mudaram e precisam ter o checksum recalculado.
    set<uint32_t> affected_groups;

    for (uint32_t block : target_blocks)
    {
        uint32_t g = (block - first_data_block) / blocks_per_group;
        uint32_t local = (block - first_data_block) % blocks_per_group;
        uint64_t bitmap_offset = static_cast<uint64_t>(this->group_descriptors[g].bg_block_bitmap_lo) * block_size;

        uint8_t byte;
        this->image_file.clear();
        this->image_file.seekg(bitmap_offset + local / 8);
        this->image_file.read(reinterpret_cast<char *>(&byte), 1);

        byte &= ~(1 << (local % 8));

        this->image_file.clear();
        this->image_file.seekp(bitmap_offset + local / 8);
        this->image_file.write(reinterpret_cast<char *>(&byte), 1);

        affected_groups.insert(g);
    }

    // recalcula o checksum do bitmap de blocos de cada grupo tocado.
    for (uint32_t g : affected_groups)
        this->updateBlockBitmapChecksum(g);

    uint32_t inodes_per_group = this->sb.getInodesPerGroup();
    uint32_t inode_group = (target_inode - 1) / inodes_per_group;
    uint32_t inode_local = (target_inode - 1) % inodes_per_group;
    uint64_t inode_bitmap_offset = static_cast<uint64_t>(this->group_descriptors[inode_group].bg_inode_bitmap_lo) * block_size;

    uint8_t byte;
    this->image_file.clear();
    this->image_file.seekg(inode_bitmap_offset + inode_local / 8);
    this->image_file.read(reinterpret_cast<char *>(&byte), 1);

    byte &= ~(1 << (inode_local % 8));

    this->image_file.clear();
    this->image_file.seekp(inode_bitmap_offset + inode_local / 8);
    this->image_file.write(reinterpret_cast<char *>(&byte), 1);

    this->updateInodeBitmapChecksum(inode_group);
    affected_groups.insert(inode_group);

    // zera o inode alvo, preservando i_extra_isize e marcando i_dtime.
    uint64_t inode_offset = this->getInodeOffset(target_inode);
    vector<char> inode_buffer(this->sb.getInodeSize());
    this->image_file.clear();
    this->image_file.seekg(inode_offset);
    this->image_file.read(inode_buffer.data(), this->sb.getInodeSize());

    uint16_t extra_isize;
    memcpy(&extra_isize, &inode_buffer[0x80], 2);
    memset(inode_buffer.data(), 0, this->sb.getInodeSize());
    memcpy(&inode_buffer[0x80], &extra_isize, 2);
    uint32_t dtime = static_cast<uint32_t>(time(nullptr));
    memcpy(&inode_buffer[0x14], &dtime, 4);

    this->writeInodeWithChecksum(target_inode, inode_buffer);

    // decrementa i_links_count do pai (o .. do filho apontava para ele).
    uint64_t parent_offset = this->getInodeOffset(this->current_inode);
    vector<char> parent_buffer(this->sb.getInodeSize());
    this->image_file.clear();
    this->image_file.seekg(parent_offset);
    this->image_file.read(parent_buffer.data(), this->sb.getInodeSize());

    uint16_t links;
    memcpy(&links, &parent_buffer[0x1A], 2);
    if (links > 0)
        links -= 1;
    memcpy(&parent_buffer[0x1A], &links, 2);

    this->writeInodeWithChecksum(this->current_inode, parent_buffer);

    // recalcula bg_checksum e grava cada descritor afetado (depois dos bitmaps).
    for (uint32_t g : affected_groups)
        this->updateGroupDescriptorChecksum(g);

    this->image_file.flush();

    cout << verde << "Diretório \"" << name << "\" removido com sucesso." << reset << endl;
}

void Ext4::FileSystemManager::rename(string name, string newName)
{
    if (newName.length() > 255)
    {
        cout << vermelho << "O novo nome não pode possuir mais de 255 caracteres" << reset << endl;
        return;
    }

    uint32_t target_inode = this->findInodeInDirectory(this->current_inode, name);
    if (target_inode == 0)
    {
        cout << vermelho << "O arquivo '" << name << "' não existe no diretório atual" << endl;
        return;
    }

    uint32_t already_exists = this->findInodeInDirectory(this->current_inode, newName);
    if (already_exists)
    {
        cout << vermelho << "Não foi possível renomear o arquivo pois já existe outro arquivo com esse nome" << endl;
        return;
    }

    uint32_t data_blocks = this->getInodeDataBlock(this->current_inode);
    uint32_t block_size = this->sb.getBlockSize();
    vector<char> buffer(block_size);
    this->image_file.seekg(data_blocks * block_size);
    this->image_file.read(buffer.data(), block_size);

    uint16_t bytes_read = 0;

    while (bytes_read < block_size)
    {
        DirEntry *dir_entry = reinterpret_cast<DirEntry *>(&buffer[bytes_read]);

        if (dir_entry->rec_len < 8 || bytes_read + dir_entry->rec_len > block_size)
        {
            break;
        }

        if (dir_entry->inode != 0 && dir_entry->name_len > 0 && dir_entry->name_len <= (dir_entry->rec_len - 8))
        {
            string current_name(dir_entry->name, dir_entry->name_len);
            if (current_name == name)
            {
                // espaço necessário = 8 bytes de cabeçalho + tamanho do nome, arredondando pra cima
                uint16_t space = (8 + newName.length() + 3) & ~3;
                if (space <= dir_entry->rec_len)
                {
                    memset(dir_entry->name, 0, dir_entry->name_len);
                    dir_entry->name_len = newName.length();
                    strncpy(dir_entry->name, newName.c_str(), newName.length());
                    this->writeDirBlockWithChecksum(this->current_inode, data_blocks, buffer);
                    cout << verde << "Nome alterado com sucesso!" << reset << endl;
                    break;
                }
                else
                {
                    // verificação do espaço total
                    uint16_t total_needed = 0;
                    uint16_t check_offset = 0;
                    while (check_offset < block_size)
                    {
                        DirEntry *check_entry = reinterpret_cast<DirEntry *>(&buffer[check_offset]);

                        if (check_entry->rec_len < 8 || check_offset + check_entry->rec_len > block_size)
                        {
                            break;
                        }

                        if (check_entry->inode != 0)
                        {
                            uint16_t name_len_for_this_entry;
                            if (check_offset == bytes_read)
                            {
                                name_len_for_this_entry = newName.length();
                            }
                            else
                            {
                                name_len_for_this_entry = check_entry->name_len;
                            }

                            uint16_t min_size = (8 + name_len_for_this_entry + 3) & ~3;
                            total_needed += min_size;
                        }

                        check_offset += check_entry->rec_len;
                    }

                    if (total_needed > block_size)
                    {
                        cout << vermelho << "Não há espaço suficiente no diretório para renomear o arquivo" << reset << endl;
                        return;
                    }

                    // caso o nome seja maior, é necessário alterar os DirEntry
                    vector<char> new_buffer(block_size, 0);
                    uint16_t new_bytes_written = 0;
                    DirEntry *last_entry = nullptr;

                    uint16_t search_offset = 0;
                    while (search_offset < block_size)
                    {
                        DirEntry *original_entry = reinterpret_cast<DirEntry *>(&buffer[search_offset]);

                        if (original_entry->rec_len < 8 || search_offset + original_entry->rec_len > block_size)
                        {
                            break;
                        }

                        if (original_entry->inode != 0)
                        {
                            DirEntry *new_entry = reinterpret_cast<DirEntry *>(&new_buffer[new_bytes_written]);
                            new_entry->inode = original_entry->inode;
                            new_entry->file_type = original_entry->file_type;

                            string copy_name;
                            if (search_offset == bytes_read)
                            {
                                copy_name = newName;
                            }
                            else
                            {
                                copy_name = string(original_entry->name, original_entry->name_len);
                            }

                            new_entry->name_len = copy_name.length();
                            strncpy(new_entry->name, copy_name.c_str(), copy_name.length());

                            uint16_t minimum_size = (8 + new_entry->name_len + 3) & ~3;
                            new_entry->rec_len = minimum_size;

                            last_entry = new_entry;
                            new_bytes_written += minimum_size;
                        }
                        search_offset += original_entry->rec_len;
                    }

                    if (last_entry != nullptr)
                    {
                        uint16_t remaining_space = block_size - (new_bytes_written - last_entry->rec_len);
                        last_entry->rec_len = remaining_space;
                    }
                    uint16_t tail_offset = block_size - 12;

                    if (last_entry != nullptr)
                    {
                        // O rec_len da última entrada de arquivo real vai até o início do Tail de Checksum, e não até o fim do bloco!
                        uint16_t current_entry_start = reinterpret_cast<char *>(last_entry) - new_buffer.data();
                        last_entry->rec_len = tail_offset - current_entry_start;
                    }

                    DirEntry *tail_entry = reinterpret_cast<DirEntry *>(&new_buffer[tail_offset]);
                    tail_entry->inode = 0;        // Obrigatório ser 0 para Tails
                    tail_entry->rec_len = 12;     // Tamanho exato do cabeçalho de checksum do diretório
                    tail_entry->name_len = 0;     // Não possui nome
                    tail_entry->file_type = 0xDE; // EXT4_FT_DIR_CSUM (Flag mágica que indica metadados de checksum)

                    // atualiza-se o checksum do bloco do diretório
                    this->writeDirBlockWithChecksum(this->current_inode, data_blocks, new_buffer);
                    uint32_t group = this->getGroupFromInode(this->current_inode);
                    this->updateGroupDescriptorChecksum(group);

                    cout << verde << "Nome alterado com sucesso (bloco reorganizado)!" << reset << endl;
                }
                break;
            }
        }

        bytes_read += dir_entry->rec_len;
    }
}

void Ext4::FileSystemManager::writeSuperBlockWithChecksum(vector<char> &buffer)
{
    uint32_t csum = checksum_superblock(buffer.data());
    memcpy(&buffer[1020], &csum, 4);
    this->image_file.clear();
    this->image_file.seekp(1024);
    this->image_file.write(buffer.data(), 1024);
}