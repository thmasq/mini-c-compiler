# Mini C Compiler — minicc (C → LLVM IR)

Este repositório contém o compilador *minicc, que traduz um subconjunto da linguagem C para código intermediário **LLVM IR*.

O projeto foi escrito em C e implementa as principais fases de um compilador:

- Análise léxica
- Análise sintática
- Análise semântica
- Geração de código LLVM IR

---

# Dependências
```
cmake, flex, bison, clang, gcc
```

## Sistema Operacional
- Linux (testado em Ubuntu e derivados)

## Ferramentas de Build

Instale as ferramentas básicas de compilação:

```bash
sudo apt update
sudo apt install build-essential
sudo apt install llvm clang
sudo apt install flex bison
sudo apt install valgrind
```
# Como compilar
Na raiz do projeto, rode
```
make clean && make
```

# Como usar 
Exemplo:
```
# Editor de texto minimalista
./minicc -c examples/text_editor.c -o text_editor
# Se quiser o código intermediário apenas:
./minicc -S examples/text_editor.c -o text_editor.ll

./text_editor examples/text_editor.c
./text_editor text_editor.ll
```

```
./minicc -c examples/test_error.c -o error
```
