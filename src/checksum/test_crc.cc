 /**
 * @file test_crc.cc
 * @author Rodrigo Campiolo
 * @brief Testa o calculo do CRC32C em arquivos de dump do ext4
 * @version 1.0
 * @date 2023-10-11, 2023-10-23 (atualizado)
 *
 * 
 * @copyright Copyright (c) 2023
 * 
 * Compilar: g++ test_crc.cc ext4checksum.o -o test_crc -lcryptopp
 */

#include <iostream>
#include <fstream>
#include <stdio.h>
#include <cstring>

#include "ext4checksum.h"

#define N 1020

void print_str_to_hex (char *str, int size) {
    for (int i=0; i<size; i++) {
        printf("%.2X", (uint8_t)str[i]);
    }
}


int main (int argc, char* argv[])
{
    char* superbloco = new char[N];
    unsigned char* checksum = new unsigned char[4];
  
    memset(superbloco,0,N);
    
    std::ifstream input("resources/superblock.out", std::ifstream::binary);
    input.read(superbloco, N);
    input.read((char *)checksum, 4);

    if (input) std::cout << "Arquivo lido com sucesso." << std::endl;

    std::cout << "print superblock:" << std::endl;
    print_str_to_hex (superbloco, 1024);
    std::cout << std::endl;

    std::cout << "print checksum:" << std::endl;
    std::cout << bytearray_to_int32_le(checksum);
    std::cout << std::endl;

    /* usado em varios checksums */
    uint8_t sb_uuid[16] = {0xE0, 0xF7, 0x78, 0x66, 0xF8, 0x5F, 0x4C, 0xBD, 0xBF, 0x5F, 0x0C, 0x23, 0x78, 0x47, 0x01, 0x5F};
    std::cout << "uuid:" << std::endl;
    print_str_to_hex((char *) sb_uuid, 16);
    std::cout << std::endl;

    uint32_t csum_super = checksum_superblock(superbloco);
    
    std::cout << "Checksum do superbloco: " << csum_super << std::endl;
   
/* Checksum do descritor de grupo */   

    char *group = new char[64];
    uint32_t  group_number = 0;

    std::ifstream inputgroup("resources/blockgroup.out", std::ifstream::binary);
    inputgroup.read(group, 64);

    print_str_to_hex(group, 64);
    std::cout << std::endl;   
    std::cout << "print checksum (hex):" << std::endl;
    print_str_to_hex(&group[30],2);
    std::cout << std::endl;   

    uint32_t csum_group = checksum_group((char *)sb_uuid, group_number, group);
    std::cout << "Checksum do descritor de grupos: " << csum_group << std::endl;

/* Checksum do mapa de bits dos blocos livres/ocupados */
    char blockbitmap[1024];
    std::ifstream inputblock("resources/blockbitmap.out", std::ifstream::binary);
    inputblock.read(blockbitmap, 1024);

    print_str_to_hex(blockbitmap, 1024);
    std::cout << std::endl;   

    uint32_t csum_blockbitmap = checksum_bitmap((char *)sb_uuid, blockbitmap, 1024);
    std::cout << "Checksum de bitmap de blocos: " << csum_blockbitmap << std::endl;


/** Checksum do mapa de bits de inodes */
    char inodebitmap[64];
    std::ifstream inputinode("resources/nodebitmap.out", std::ifstream::binary);
    inputinode.read(inodebitmap, 64);

    print_str_to_hex(inodebitmap, 64);
    std::cout << std::endl;   

    uint32_t csum_inodebitmap = checksum_bitmap((char *)sb_uuid, blockbitmap, 1024);
    std::cout << "Checksum de bitmap de inodes: " << csum_inodebitmap << std::endl;


/** Checksum do inode */ 
    char inode[256];
    uint32_t inumber = 2;
    uint32_t igen;
    memcpy(&igen, &inode[100], 4);

    std::ifstream inputi("resources/inode2.out", std::ifstream::binary);
    inputi.read(inode, 256);

    print_str_to_hex(inode, 256);
    std::cout << std::endl;   

    uint32_t csum_inode = checksum_inode((char *)sb_uuid, inumber, igen, inode);
    std::cout << "Checksum do inode: " << csum_inode << std::endl;


/** Checksum do diretório */
    char dir[1024];
    std::ifstream inputdir("resources/dir.out", std::ifstream::binary);
    inputdir.read(dir, 1024);

    print_str_to_hex(dir, 1024);
    std::cout << std::endl;   

    uint32_t csum_dir = checksum_dir((char *)sb_uuid, inumber, igen, dir, 1024);  
    std::cout << "Checksum do diretorio: " << csum_dir << std::endl;

/** Checksum do extent */

    char extent[1024];
    inumber = 24578;
    igen = 2897741026;
    std::ifstream inputex("resources/extent.out", std::ifstream::binary);
    inputex.read(extent, 1024);

    print_str_to_hex(extent, 1024);
    std::cout << std::endl;   

    uint32_t csum_extent = checksum_extent((char *)sb_uuid, inumber, igen, extent, 1024);  
    std::cout << "Checksum do extent: " << csum_extent << std::endl;

    return 0; 
}
