# ext4 shell
Repositório criado para implementação do projeto final da disciplina de Sistemas Operacionais da UTFPR (2026/1).

O objetivo é implementar um shell capaz de manipular uma imagem EXT4, executando comandos de leitura e escrita diretamente sobre a imagem.

Alunas: Caroline Marques Lau, Maria Eduarda Bambini e Victória Stephanie Mottin.

## Dependências (Linux)
- `g++` e `make`
- `libcrypto++-dev` (usado no cálculo dos checksums CRC32C): `sudo apt install libcrypto++-dev`

## Compilação
```
make
```

## Execução
```
./bin/main
```
A imagem a ser aberta deve estar em `images_ext4/`. Ao iniciar, informe o nome do arquivo (ex.: `myext4image1k.img`).

## Comandos
- **Leitura:** `info`, `cat <arquivo>`, `attr <arquivo | dir>`, `cd <caminho>`, `ls`, `testi <inode>`, `testb <bloco>`, `export <origem> <destino>` e `pwd`.
- **Escrita:** `touch <arquivo>`, `mkdir <dir>`, `rm <arquivo>` `rmdir <dir>` e `rename <arquivo> <nome>`.
- `exit`.