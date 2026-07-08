#include "ext4.hpp"
#include "cores.h"
#include "utils.hpp"
#include "checksum/ext4checksum.h"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <ctime>
#include <vector>
#include <set>
#include <map>

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

    // Salta o padding inicial de 1024 bytes e carrega o Superbloco para a memória
    file.seekg(1024);
    file.read(reinterpret_cast<char *>(&this->sb), sizeof(SuperBlock));

    uint32_t block_size = this->sb.getBlockSize();

    // Determina o bloco inicial da BGDT (Block Group Descriptor Table) com base no tamanho do bloco
    uint32_t bgdtStartBlock = (block_size == 1024) ? 2 : 1;

    // Calcula o offset em bytes e lê a tabela de descritores de todos os grupos de blocos
    uint64_t bgdtOffset = bgdtStartBlock * block_size;
    file.seekg(bgdtOffset);
    uint32_t block_groupsCount = this->sb.getBlockGroupsCount();
    this->group_descriptors.resize(block_groupsCount);
    file.read(reinterpret_cast<char *>(this->group_descriptors.data()), sizeof(GroupDescriptor) * block_groupsCount);

    // Define o diretório raiz (Inode 2) como o ponto de partida inicial do sistema
    this->current_inode = 2;
    this->current_path = "/";
    this->image_file = move(file);
    return true;
}

/**
 * @brief Cria um novo arquivo vazio no sistema de arquivos (comando touch).
 * @param path Nome ou caminho do arquivo a ser criado.
 * * Localiza o diretório pai, aloca um i-node livre no bitmap e inicializa
 * seus metadados na tabela de i-nodes. Por fim, insere a nova entrada (DirEntry) 
 * no bloco de dados do diretório pai e atualiza os checksums.
 */
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

    // Varre as entradas existentes no bloco de diretório para encontrar o último registro válido
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

    // Calcula o espaço necessário e verifica se a última entrada pode ser encolhida para abrir espaço para o novo arquivo
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

    // Varre o bitmap de inodes bit a bit para encontrar e marcar a primeira posição livre disponível
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

    // Salva o bitmap de inodes atualizado e recalcula o seu respectivo checksum
    this->image_file.seekp(inode_bitmap_offset);
    this->image_file.write(bytes.data(), block_size);

    this->updateInodeBitmapChecksum(group);

    // Inicializa a nova estrutura de Inode em memória com as flags de arquivo regular e a árvore de Extents vazia
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

    // Persiste o novo Inode diretamente na tabela de inodes do disco
    this->writeInodeWithChecksum(free_inode_index, inode_buf);

    // Modifica a entrada de diretório antiga e insere fisicamente a nova DirEntry no espaço liberado
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

    // Grava o bloco do diretório atualizado no disco atualizando seus metadados de validação
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

    // Atualiza os contadores globais de inodes livres no Superbloco e sincroniza tudo com a imagem em disco
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

/**
 * @brief Aloca o primeiro inode livre disponível dentro de um grupo de blocos específico, 
 * varrendo bit a bit o bitmap de inodes do grupo até encontrar uma posição livre.
 * @param group: número do grupo de blocos onde o inode deve ser alocado
 * @returns número do inode alocado (1-indexed), ou 0 caso não haja inodes livres disponíveis nesse grupo
 */
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

            uint32_t unused_start = inodes_per_group - desc.bg_itable_unused_lo;
            if (i >= unused_start)
            {
                uint16_t new_unused = static_cast<uint16_t>(inodes_per_group - (i + 1));
                if (new_unused < desc.bg_itable_unused_lo)
                    desc.bg_itable_unused_lo = new_unused;
            }

            this->updateGroupDescriptorChecksum(group);

            return inode_number;
        }
    }
    return 0;
}

/**
 * @brief Aloca o primeiro bloco de dados livre disponível, priorizando o grupo indicado em preferred_group e, 
 * caso esse grupo esteja cheio, buscando nos demais grupos em sequência circular. 
 * @param preferred_group: grupo de blocos onde a busca deve começar
 * @returns número absoluto do bloco alocado, ou 0 caso não haja blocos livres disponíveis em nenhum grupo do sistema de arquivos
 */
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

                this->updateBlockBitmapChecksum(group);
                if (desc.bg_free_blocks_count_lo > 0)
                    desc.bg_free_blocks_count_lo--;
                this->updateGroupDescriptorChecksum(group);
                return first_data_block + group * blocks_per_group + i;
            }
        }
    }
    return 0;
}

/**
 * @brief Implementa o comando mkdir, criando um novo diretório vazio dentro do diretório atual. 
 * @note Limitação conhecida: assume que o diretório atual (pai) possui todo o
 *       seu conteúdo em um único bloco/extent; diretórios que já ocupem mais
 *       de um bloco não são suportados por esta implementação.
 * @param name: nome do novo diretório a ser criado, relativo ao diretório atual
 * @returns void (cria o diretório na imagem; em caso de erro, retorna sem alterar a imagem)
 */
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

    uint32_t inode_size = this->sb.getInodeSize();
    vector<char> new_inode_buffer(inode_size, 0);

    uint16_t i_mode = 0x41ED;
    memcpy(&new_inode_buffer[0x00], &i_mode, 2);

    uint32_t i_size_lo = block_size;
    memcpy(&new_inode_buffer[0x04], &i_size_lo, 4);

    uint16_t i_links_count = 2;
    memcpy(&new_inode_buffer[0x1A], &i_links_count, 2);

    uint32_t i_blocks_lo = block_size / 512;
    memcpy(&new_inode_buffer[0x1C], &i_blocks_lo, 4);

    uint32_t i_flags = 0x80000;
    memcpy(&new_inode_buffer[0x20], &i_flags, 4);

    ExtentHeader header{};
    header.eh_magic = 0xF30A;
    header.eh_entries = 1;
    header.eh_max = 4;
    header.eh_depth = 0;
    memcpy(&new_inode_buffer[0x28], &header, sizeof(ExtentHeader));

    Extent extent{};
    extent.ee_block = 0;
    extent.ee_len = 1;
    extent.ee_start_hi = 0;
    extent.ee_start_lo = new_block_num;
    memcpy(&new_inode_buffer[0x28 + sizeof(ExtentHeader)], &extent, sizeof(Extent));

    if (inode_size > 128)
    {
        uint16_t extra_isize = 32;
        memcpy(&new_inode_buffer[0x80], &extra_isize, 2);
    }
    this->writeInodeWithChecksum(new_inode_num, new_inode_buffer);

    vector<char> new_dir_block(block_size, 0);

    DirEntry *dot = reinterpret_cast<DirEntry *>(&new_dir_block[0]);
    dot->inode = new_inode_num;
    dot->rec_len = 12;
    dot->name_len = 1;
    dot->file_type = 2;
    dot->name[0] = '.';

    DirEntry *dotdot = reinterpret_cast<DirEntry *>(&new_dir_block[12]);
    dotdot->inode = this->current_inode;
    dotdot->rec_len = block_size - 12 - 12;
    dotdot->name_len = 2;
    dotdot->file_type = 2;
    dotdot->name[0] = '.';
    dotdot->name[1] = '.';

    uint32_t tail_offset = block_size - 12;
    uint32_t tail_inode = 0;
    uint16_t tail_rec_len = 12;
    uint8_t tail_name_len = 0;
    uint8_t tail_file_type = 0xDE;

    memcpy(&new_dir_block[tail_offset], &tail_inode, 4);
    memcpy(&new_dir_block[tail_offset + 4], &tail_rec_len, 2);
    new_dir_block[tail_offset + 6] = static_cast<char>(tail_name_len);
    new_dir_block[tail_offset + 7] = static_cast<char>(tail_file_type);

    this->writeDirBlockWithChecksum(new_inode_num, new_block_num, new_dir_block);

    last_dir_entry->rec_len = used_rec_len;
    uint32_t new_entry_offset = last_offset + used_rec_len;
    DirEntry *new_entry = reinterpret_cast<DirEntry *>(&data_block_bytes[new_entry_offset]);
    new_entry->inode = new_inode_num;
    new_entry->name_len = name.length();
    new_entry->file_type = 2;
    new_entry->rec_len = space_left;
    memset(new_entry->name, 0, required - 8);
    memcpy(new_entry->name, name.c_str(), name.length());

    this->writeDirBlockWithChecksum(this->current_inode, data_block, data_block_bytes);

    uint64_t parent_offset = this->getInodeOffset(this->current_inode);
    vector<char> parent_buffer(inode_size);
    this->image_file.clear();
    this->image_file.seekg(parent_offset);
    this->image_file.read(parent_buffer.data(), inode_size);

    uint16_t parent_links;
    memcpy(&parent_links, &parent_buffer[0x1A], 2);
    parent_links += 1;
    memcpy(&parent_buffer[0x1A], &parent_links, 2);

    this->writeInodeWithChecksum(this->current_inode, parent_buffer);

    uint32_t new_group = this->getGroupFromInode(new_inode_num);
    this->group_descriptors[new_group].bg_used_dirs_count += 1;
    this->updateGroupDescriptorChecksum(new_group);

    this->sb.adjustFreeInodesCount(-1);
    this->sb.adjustFreeBlocksCount(-1);
    this->writeSuperBlockToDisk();

    this->image_file.flush();

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

    uint32_t desc_size = this->sb.getDescriptorSize();

    this->image_file.clear();
    this->image_file.seekp(this->getGroupDescriptorOffset(group));
    this->image_file.write(reinterpret_cast<char *>(&desc), desc_size);
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

/**
 * Remove um diretório, se estiver vazio.
 * @param name: nome do diretório a ser removido
 */
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
                    prev->rec_len += entry->rec_len; //mescla o rec_len do anterior com o removido.
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

    map<uint32_t, int32_t> blocks_freed_per_group;

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
        blocks_freed_per_group[g]++;
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

    for (auto &[g, count] : blocks_freed_per_group)
        this->adjustFreeCounters(count, 0, 0, g);

    this->adjustFreeCounters(0, 1, 1, inode_group);

    for (uint32_t g : affected_groups)
        this->updateGroupDescriptorChecksum(g);

    this->image_file.flush();

    cout << verde << "Diretório \"" << name << "\" removido com sucesso." << reset << endl;
}

/**
 * @brief Altera o nome de um arquivo ou diretório existente (comando rename).
 * @param name Nome atual do arquivo.
 * @param newName Novo nome a ser atribuído.
 * * Varre o bloco de dados do diretório pai para localizar a entrada (DirEntry) 
 * correspondente ao nome atual. Ao encontrá-la, sobrescreve o campo nominal com o 
 * novo nome e ajusta o tamanho do nome (name_len), atualizando o checksum do bloco.
 */
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
                // Espaço necessário = 8 bytes de cabeçalho + tamanho do nome, arredondando pra cima
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
                    // Caso o novo nome precise de mais espaço, calcula o tamanho mínimo que todas as entradas válidas somadas vão ocupar
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

                    // Se a soma do tamanho comprimido de todas as entradas estourar o bloco, aborta pois não cabe reorganização
                    if (total_needed > block_size)
                    {
                        cout << vermelho << "Não há espaço suficiente no diretório para renomear o arquivo" << reset << endl;
                        return;
                    }

                    // Reconstrói o bloco do diretório do zero (compactando o espaço fragmentado) em um novo buffer temporário
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

                    // Cria a estrutura de rodapé (DirEntryTail) no final do bloco para suportar metadados de metachecksum do Ext4
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

/**
 * @brief Remove um arquivo regular do sistema de arquivos (comando rm).
 * @param name Nome do arquivo a ser removido.
 * * Localiza o i-node do arquivo varrendo linearmente as entradas do diretório pai. 
 * Após a identificação, remove a respectiva DirEntry do bloco do pai, marca o 
 * i-node e os blocos de dados contíguos (extents) como livres nos seus mapas de 
 * bits (bitmaps) e atualiza os checksums do grupo.
 */
void Ext4::FileSystemManager::rm(string name)
{
    vector<string> path = tokenizePath(name);
    if (path.size() > 2)
    {
        cout << vermelho << "Esse comando não lida com múltiplos diretórios" << reset << endl;
        return;
    }

    string file_name = path[0];
    uint32_t inode_dir = this->current_inode;
    // Caso exista um diretório no caminho inserido
    if (path.size() > 1)
    {
        inode_dir = this->findInodeInDirectory(this->current_inode, path[0]);
        file_name = path[1];
    }

    uint32_t inode_index = this->findInodeInDirectory(inode_dir, file_name);
    if (inode_index == 0)
    {
        cout << vermelho << "Arquivo não encontrado!" << endl;
        return;
    }

    Inode inode = this->readInode(inode_index);
    if (isDirectory(inode))
    {
        cout << vermelho << "Use rmdir para deletar diretórios!" << reset << endl;
        return;
    }
    uint32_t group = this->getGroupFromInode(inode_index);
    uint32_t dir_block = this->getInodeDataBlock(inode_dir);
    uint32_t block_size = this->sb.getBlockSize();

    vector<char> dir_buffer(block_size);
    this->image_file.seekg(static_cast<uint64_t>(dir_block) * block_size);
    this->image_file.read(dir_buffer.data(), block_size);

    uint32_t current_offset = 0;
    DirEntry *prev_entry = nullptr;
    bool dir_updated = false;

    // O limite máximo é block_size - 12 por conta do DirEntryTail de checksum
    while (current_offset < (block_size - 12))
    {
        DirEntry *current_entry = reinterpret_cast<DirEntry *>(&dir_buffer[current_offset]);

        if (current_entry->rec_len < 8 || current_offset + current_entry->rec_len > block_size)
            break;

        if (current_entry->inode == inode_index)
        {
            if (prev_entry != nullptr)
            {
                // O anterior toma o espaço do atual
                prev_entry->rec_len += current_entry->rec_len;
            }
            else
            {
                // Se for o primeiríssimo arquivo do bloco, o inode é zerado
                current_entry->inode = 0;
            }
            dir_updated = true;
            break;
        }

        prev_entry = current_entry;
        current_offset += current_entry->rec_len;
    }

    if (!dir_updated)
    {
        cout << vermelho << "Erro ao atualizar a estrutura de diretórios." << reset << endl;
        return;
    }

    this->writeDirBlockWithChecksum(inode_dir, dir_block, dir_buffer);

    vector<uint32_t> data_blocks = this->getInodeDataBlocks(inode_index);

    // Atualiza o inode
    inode.i_links_count = 0;
    inode.i_mode = 0;
    inode.i_dtime = time(nullptr);
    memset(inode.i_block, 0, sizeof(inode.i_block));
    std::vector<char> inode_buffer(sizeof(Inode));
    memcpy(inode_buffer.data(), &inode, sizeof(Inode));

    this->writeInodeWithChecksum(inode_index, inode_buffer);

    for (uint32_t block_num : data_blocks)
    {
        if (block_num == 0)
            continue;

        // Descobre a qual grupo esse bloco pertencia
        uint32_t block_group = (block_num - this->sb.getFirstDataBlock()) / this->sb.getBlocksPerGroup();
        GroupDescriptor &b_desc = this->group_descriptors[block_group];
        uint64_t block_bitmap_offset = static_cast<uint64_t>(b_desc.bg_block_bitmap_lo) * block_size;

        vector<char> block_bitmap(block_size);
        this->image_file.clear();
        this->image_file.seekg(block_bitmap_offset);
        this->image_file.read(block_bitmap.data(), block_size);

        uint32_t local_block_idx = (block_num - this->sb.getFirstDataBlock()) % this->sb.getBlocksPerGroup();
        uint32_t b_byte_idx = local_block_idx / 8;
        uint32_t b_bit_idx = local_block_idx % 8;

        // Desmarca o bloco no bitmap (muda para 0)
        block_bitmap[b_byte_idx] &= ~(1 << b_bit_idx);

        // Salva o bitmap de blocos modificado
        this->image_file.clear();
        this->image_file.seekp(block_bitmap_offset);
        this->image_file.write(block_bitmap.data(), block_size);
        this->updateBlockBitmapChecksum(block_group);

        // Atualiza o contador de blocos livres do grupo dono daquele bloco
        b_desc.bg_free_blocks_count_lo++;
        this->updateGroupDescriptorChecksum(block_group);

        // Sincroniza os contadores de bloco também na struct interna do Superbloco
        this->sb.incrementFreeBlocksCount();
    }
    GroupDescriptor &desc = this->group_descriptors[group];
    // Atualiza contador de inodes livres do grupo
    desc.bg_free_inodes_count++;

    // Atualiza o bitmap de inodes
    uint32_t inode_bitmap = desc.bg_inode_bitmap_lo;
    uint64_t inode_bitmap_offset = static_cast<uint64_t>(inode_bitmap) * block_size;
    this->image_file.clear();
    this->image_file.seekg(inode_bitmap_offset);
    vector<char> bytes(block_size);
    this->image_file.read(bytes.data(), block_size);
    uint32_t local_inode_index = (inode_index - 1) % this->sb.getInodesPerGroup();
    uint16_t byte_offset = local_inode_index / 8;
    uint16_t bit_offset = local_inode_index % 8;
    bytes[byte_offset] &= ~(1 << bit_offset);

    this->image_file.seekp(inode_bitmap_offset);
    this->image_file.write(bytes.data(), block_size);

    this->updateInodeBitmapChecksum(group);
    this->updateGroupDescriptorChecksum(group);

    this->writeGroupDescriptors();

    // Atualiza o Superbloco
    // Lemos os 1024 bytes originais do disco
    vector<char> sb_buffer(1024);
    this->image_file.clear();
    this->image_file.seekg(1024);
    this->image_file.read(sb_buffer.data(), 1024);

    // Incrementa o s_free_inodes_count (offset 16) no buffer do disco
    uint32_t *disk_free_inodes = reinterpret_cast<uint32_t *>(&sb_buffer[16]);
    (*disk_free_inodes)++;

    if (!data_blocks.empty())
    {
        uint32_t *disk_free_blocks = reinterpret_cast<uint32_t *>(&sb_buffer[12]);
        (*disk_free_blocks) += data_blocks.size();
    }
    this->writeSuperBlockWithChecksum(sb_buffer);
    this->image_file.flush();

    cout << verde << "Arquivo '" << name << "' deletado com sucesso!" << reset << endl;
}

void Ext4::FileSystemManager::writeGroupDescriptors()
{
    uint32_t block_size = this->sb.getBlockSize();

    // O offset depende do tamanho do bloco no Ext4
    uint64_t gd_offset = (block_size == 1024) ? 2048 : block_size;

    this->image_file.clear();
    this->image_file.seekp(gd_offset);

    // Escreve todo o vetor de descriptors de volta para o disco
    this->image_file.write(
        reinterpret_cast<const char *>(this->group_descriptors.data()),
        this->group_descriptors.size() * this->sb.getDescriptorSize());
}

void Ext4::FileSystemManager::writeSuperBlockToDisk()
{
    uint32_t csum = checksum_superblock(reinterpret_cast<char *>(&this->sb));
    char *sb_bytes = reinterpret_cast<char *>(&this->sb);
    memcpy(sb_bytes + 1020, &csum, sizeof(uint32_t));
    this->image_file.clear();
    this->image_file.seekp(1024);
    this->image_file.write(sb_bytes, sizeof(SuperBlock));
}

void Ext4::FileSystemManager::adjustFreeCounters(int32_t blocks_delta, int32_t inodes_delta,
                                                  int32_t dirs_delta, uint32_t affected_group)
{
    GroupDescriptor &desc = this->group_descriptors[affected_group];

    uint32_t free_blocks = desc.bg_free_blocks_count_lo | (static_cast<uint32_t>(desc.bg_free_blocks_count_hi) << 16);
    free_blocks = static_cast<uint32_t>(static_cast<int64_t>(free_blocks) + blocks_delta);
    desc.bg_free_blocks_count_lo = free_blocks & 0xFFFF;
    desc.bg_free_blocks_count_hi = (free_blocks >> 16) & 0xFFFF;

    uint16_t free_inodes = static_cast<uint16_t>(desc.bg_free_inodes_count + inodes_delta);
    desc.bg_free_inodes_count = free_inodes;

    uint16_t used_dirs = static_cast<uint16_t>(desc.bg_used_dirs_count - dirs_delta);
    desc.bg_used_dirs_count = used_dirs;

    this->sb.adjustFreeBlocksCount(blocks_delta);
    this->sb.adjustFreeInodesCount(inodes_delta);

    this->updateGroupDescriptorChecksum(affected_group);
    this->writeSuperBlockToDisk();
}