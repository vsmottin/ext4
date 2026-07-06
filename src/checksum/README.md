Como usar ext4checksum.h e ext4checksum.cc

O arquivo test_crc.cc contém os exemplos de como realizar as chamadas dos checksums.

-- Instalação (LINUX):

# sudo apt install libcrypto++-dev

-- Compilação

# g++ -c ext4checksum.cc
# g++ test_crc.cc ext4checksum.o -o test_crc -lcryptopp

-- Execução
#./test_crc




