#include <stdint.h>
#include <fstream>
// Definição das classes e variáveis

#pragma once
#pragma pack(push, 1)
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

        uint8_t padding[796]; // 0xE4 - Preenchimento

    public:
        std::string getVolumeName();
        std::string getLastMounted();
        std::string getUUID();
        std::string getMagic();
        std::string getCreatorOS();
        std::string getErrorBehavior();
        void superBlockStats();
    };
    // class GroupDescriptor {
    //     uint32_t bg_block_bitmap_lo;           // 0x00 - Bloco do bitmap de blocos
    //     uint32_t bg_inode_bitmap_lo;           // 0x04 - Bloco do bitmap de inodes
    //     uint32_t bg_inode_table_lo;            // 0x08 - Bloco do inode table
    //     uint16_t bg_free_blocks_count_lo;      // 0x0C - Blocos livres
    //     uint16_t bg_free_inodes_count;         // 0x0E - Inodes livres
    //     uint16_t bg_used_dirs_count;           // 0x10 - Diretórios usados

    // };
    class FileSystemManager
    {
    private:
        std::ifstream image_file;
        SuperBlock sb;
    public:
        bool setImage(std::string fileName);
        bool info();
    };
}
#pragma pack(pop)