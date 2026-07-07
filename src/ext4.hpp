#include <stdint.h>
#include <fstream>
#include <vector>
#include <string>
// Definição das classes e variáveis

#pragma once
#pragma pack(push, 1)

#define EXT4_NAME_LEN 255

namespace Ext4
{
    class SuperBlock
    {
    private:
        uint32_t s_inodes_count;           // 0x00 - Total de inodes
        uint32_t s_blocks_count_lo;        // 0X04 - Total de blocos
        uint32_t s_r_blocks_count_lo;      // 0x08 - Blocos reservados
        uint32_t s_free_blocks_count_lo;   // 0x0C - Blocos livres
        uint32_t s_free_inodes_count;      // 0x10 - Inodes livres
        uint32_t s_first_data_block;       // 0x14 - Primeiro bloco de dados (0 se bloco > 1KB, 1 se bloco = 1KB)
        uint32_t s_log_block_size;         // 0x18 - Tamanho do bloco (2 ^ (10 + s_log_block_size))
        uint32_t s_log_cluster_size;       // 0x1C - Tamanho do cluster
        uint32_t s_blocks_per_group;       // 0x20 - Blocos por grupo
        uint32_t s_clusters_per_group;     // 0x24 - Clusters por grupo
        uint32_t s_inodes_per_group;       // 0x28 - Inodes por grupo
        uint32_t s_mtime;                  // 0x2C - Tempo da última montagem
        uint32_t s_wtime;                  // 0x30 - Tempo da última escrita
        uint16_t s_mnt_count;              // 0x34 - Contagem de montagens
        uint16_t s_max_mnt_count;          // 0x36 - Contagem máxima de montagens
        uint16_t s_magic;                  // 0x38 - Assinatura mágica (0xEF53)
        uint16_t s_state;                  // 0x3A - Estado do sistema de arquivos
        uint16_t s_errors;                 // 0x3C - Comportamento em caso de erro
        uint16_t s_minor_rev_level;        // 0x3E - Nível de revisão menor
        uint32_t s_lastcheck;              // 0x40 - Tempo da última verificação
        uint32_t s_checkinterval;          // 0x44 - Intervalo entre verificações
        uint32_t s_creator_os;             // 0x48 - SO criador
        uint32_t s_rev_level;              // 0x4C - Nível de revisão
        uint16_t s_def_resuid;             // 0x50 - UID padrão para blocos reservados
        uint16_t s_def_resgid;             // 0x52 - GID padrão para blocos reservados
        uint32_t s_first_ino;              // 0x54 - Primeiro inode livre
        uint16_t s_inode_size;             // 0x58 - Tamanho do inode
        uint16_t s_block_group_nr;         // 0x5A - Bloco do grupo deste superblock
        uint32_t s_feature_compat;         // 0x5C - Características compatíveis
        uint32_t s_feature_incompat;       // 0x60 - Características incompatíveis
        uint32_t s_feature_ro_compat;      // 0x64 - Características de somente leitura compatíveis
        uint8_t s_uuid[16];                // 0x68 - UUID
        char s_volume_name[16];            // 0x78 - Nome do volume
        char s_last_mounted[64];           // 0x88 - Última montagem
        uint32_t s_algorithm_usage_bitmap; // 0xC8 - Mapa de uso do algoritmo
        uint8_t s_prealloc_blocks;         // 0xCC - Blocos pré-alocados
        uint8_t s_prealloc_dir_blocks;     // 0xCD - Blocos de diretórios pré-alocados
        uint16_t s_reserved_gdt_blocks;    // 0xCE - Blocos reservados para GDTs
        uint8_t s_journal_uuid[16];        // 0xD0 - UUID do journal
        uint32_t s_journal_inum;           // 0xE0 - Número do inode do journal

        uint32_t s_journal_dev;            // 0xE4 - Dispositivo do journal
        uint32_t s_last_orphan;            // 0xE8 - Lista de inodes orfãos
        uint32_t s_hash_seed[4];           // 0xEC - Seed do hash
        uint8_t s_def_hash_version;        // 0xFC - Versão do hash padrão
        uint8_t s_jnl_backup_type;         // 0xFD - Tipo de backup do journal
        uint16_t s_desc_size;              // 0xFE - Tamanho do descritor

        uint8_t padding[768];              // 0x100 - Preenchimento

    public:
        std::string getVolumeName();
        std::string getLastMounted();
        std::string getUUID();
        std::string getMagic();
        std::string getCreatorOS();
        std::string getErrorBehavior();
        void superBlockStats();
        void decrementFreeInodesCount();
        uint32_t getBlockSize();
        uint32_t getBlockGroupsCount();
        uint32_t getInodesPerGroup();
        uint32_t getBlocksPerGroup();
        uint32_t getFirstDataBlock();
        uint32_t getInodeSize();
        uint32_t getFirstFreeInode();
        uint32_t getInodesCount();
        uint32_t getBlocksCount();
        uint32_t getDescriptorSize();
        char *getRawUUID();
    };

    struct GroupDescriptor
    {
        uint32_t bg_block_bitmap_lo;      // 0x00 - Bloco do bitmap de blocos
        uint32_t bg_inode_bitmap_lo;      // 0x04 - Bloco do bitmap de inodes
        uint32_t bg_inode_table_lo;       // 0x08 - Bloco do inode table
        uint16_t bg_free_blocks_count_lo; // 0x0C - Blocos livres
        uint16_t bg_free_inodes_count;    // 0x0E - Inodes livres
        uint16_t bg_used_dirs_count;      // 0x10 - Diretórios usados
        uint16_t bg_flags;                // 0x12 - Flags do grupo
        uint32_t bg_exclude_bitmap_lo;    // 0x14 - Bloco do bitmap de exclusão
        uint16_t bg_block_bitmap_csum_lo; // 0x18 - Checksum do bitmap de blocos
        uint16_t bg_inode_bitmap_csum_lo; // 0x1A - Checksum do bitmap de inodes
        uint16_t bg_itable_unused_lo;     // 0x1C - Inodes não utilizados
        uint16_t bg_checksum;             // 0x1E - Checksum do grupo
        uint32_t bg_block_bitmap_hi;      // 0x20 - Bloco do bitmap de blocos
        uint32_t bg_inode_bitmap_hi;      // 0x24 - Bloco do bitmap de inodes
        uint32_t bg_inode_table_hi;       // 0x28 - Bloco da tabela de inodes
        uint16_t bg_free_blocks_count_hi; // 0x2C - Blocos livres
        uint16_t bg_free_inodes_count_hi; // 0x2E - Inodes livres
        uint16_t bg_used_dirs_count_hi;   // 0x30 - Diretórios usados
        uint16_t bg_itable_unused_hi;     // 0x32 - Inodes não utilizados
        uint32_t bg_exclude_bitmap_hi;    // 0x34 - Bloco do bitmap de exclusão
        uint16_t bg_block_bitmap_csum_hi; // 0x38 - Checksum do bitmap de blocos
        uint16_t bg_inode_bitmap_csum_hi; // 0x3A - Checksum do bitmap de inodes
        uint32_t bg_reserved;             // 0x3C - Padding reservado
    };

    struct Inode
    {
        uint16_t i_mode;         // 0x00 - Tipo e permissões do arquivo
        uint16_t i_uid;          // 0x02 - UID do proprietário
        uint32_t i_size_lo;      // 0x04 - Tamanho do arquivo (parte baixa)
        uint32_t i_atime;        // 0x08 - Tempo do último acesso
        uint32_t i_ctime;        // 0x0C - Tempo da última alteração
        uint32_t i_mtime;        // 0x10 - Tempo da última modificação
        uint32_t i_dtime;        // 0x14 - Tempo da exclusão
        uint16_t i_gid;          // 0x18 - GID (parte baixa)
        uint16_t i_links_count;  // 0x1A - Contagem de links sólidos
        uint32_t i_blocks_lo;    // 0x1C - Número de blocos alocados (parte baixa)
        uint32_t i_flags;        // 0x20 - Flags do inode
        uint32_t i_osd1;         // 0x24 - Sistema operacional específico
        uint32_t i_block[15];    // 0x28 - Ponteiros para blocos de dados
        uint32_t i_generation;   // 0x64 - Número de geração do arquivo
        uint32_t i_file_acl_lo;  // 0x68 - Ponteiro para ACL do arquivo (parte baixa)
        uint32_t i_size_high;    // 0x6C - Tamanho do arquivo (parte alta)
        uint32_t i_obso_faddr;   // 0x70 - Ponteiro obsoleto para o arquivo
        uint8_t i_osd2[12];      // 0x74 - Sistema operacional específico
        uint16_t i_extra_isize;  // 0x80 - Tamanho extra do inode
        uint16_t i_checksum_hi;  // 0x82 - Checksum do inode (parte alta)
        uint32_t i_ctime_extra;  // 0x84 - Tempo extra da última alteração
        uint32_t i_mtime_extra;  // 0x88 - Tempo extra da última modificação
        uint32_t i_atime_extra;  // 0x8C - Tempo extra do último acesso
        uint32_t i_crtime;       // 0x90 - Tempo de criação do inode
        uint32_t i_crtime_extra; // 0x94 - Tempo extra de criação do inode
        uint32_t i_version_hi;   // 0x98 - Versão do inode
        uint32_t i_projid;       // 0x9C - ID do projeto

        std::string getFileType();
    };

    struct ExtentHeader{
        uint16_t eh_magic;
        uint16_t eh_entries;
        uint16_t eh_max;
        uint16_t eh_depth;
        uint32_t eh_generation;
    };

    struct ExtentIdx{
        uint32_t ei_block;
        uint32_t ei_leaf_lo;
        uint16_t ei_leaf_hi;
        uint16_t ei_unused;
    };

    struct Extent{
        uint32_t ee_block;
        uint16_t ee_len;
        uint16_t ee_start_hi;
        uint32_t ee_start_lo;
    };

    struct ExtentTail{
        uint32_t eb_checksum;
    };

    struct DirEntry{
        uint32_t inode;
        uint16_t rec_len;
        uint8_t name_len;
        uint8_t file_type;
        char name[EXT4_NAME_LEN];
    };

    struct DirEntryTail{
        uint32_t det_reserved_zero1;
        uint16_t det_rec_len;
        uint8_t det_reserved_zero2;
        uint8_t det_reserved_ft;
        uint32_t det_checksum;
    };

    class FileSystemManager
    {
    private:
        std::fstream image_file;
        SuperBlock sb;
        std::vector<GroupDescriptor> group_descriptors;
        uint32_t current_inode;
        std::string current_path;
        
        Inode readInode(uint32_t num);
        Inode resolveNameToInode(const std::string& path);
        uint64_t getInodeOffset(uint32_t inode_num);
        uint32_t getInodeDataBlock(uint32_t inode_num);
        std::vector<uint32_t> getInodeDataBlocks(uint32_t inode_num);
        void collectExtentBlocks(uint32_t block_num, std::vector<uint32_t>& blocks);
        uint32_t findInodeInDirectory(uint32_t dir_inode, const std::string& name);
        bool isDirectory(const Inode& inode);
        uint32_t allocateFreeInode(uint32_t group);
        uint32_t allocateFreeBlock(uint32_t preferred_group);
        bool isRegularFile(const Inode& inode);
        uint64_t getInodeSizeBytes(const Inode& inode);
        uint32_t getGroupFromInode(uint32_t inode_num);

        uint64_t getGroupDescriptorOffset(uint32_t group);
        void updateBlockBitmapChecksum(uint32_t group);
        void updateInodeBitmapChecksum(uint32_t group);
        void updateGroupDescriptorChecksum(uint32_t group);
        void writeInodeWithChecksum(uint32_t inode_num, std::vector<char>& inode_buf);
        void writeDirBlockWithChecksum(uint32_t dir_inode_num, uint32_t block_num, std::vector<char>& buffer);
        void writeSuperBlockWithChecksum(std::vector<char> &buffer);

    public:
        bool setImage(std::string fileName);
        bool info();
        void testi(uint32_t inode);
        void testb(uint32_t block);
        void attr(std::string path);
        void rmdir(std::string name);
        void ls();
        void pwd();
        void cd(std::string path);
        void touch(std::string path);
        std::string getCurrentPath();
        void mkdir(std::string name);
        void cat(std::string name);
        void rename(std::string name, std::string newName);
    };
}

#pragma pack(pop)