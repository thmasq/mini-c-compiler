# Definição do Projeto e Linguagem de Programação Escolhida

## Definição do Projeto

O objetivo deste projeto é desenvolver um compilador para um subconjunto da linguagem de programação C. O compilador receberá um arquivo fonte escrito na linguagem Mini C e produzirá Representação Intermediária LLVM (IR) como saída.

## Linguagem de Programação Escolhida

Como acabou de ser mencionado, se escolheu fazer um compilador baseado na linguagem C, tanto como sua linguagem de implementação como a sua linguagem de origem. Além disso, para a linguagem de destino foi escolhida a representação intermediária LLVM.
Entre os motivos explicando essas escolhas estão:
* C é uma linguagem amplamente utilizada e conhecida, o que facilita a compreensão e a adoção do compilador por parte dos desenvolvedores.
* A linguagem C possui uma sintaxe relativamente simples, porém um conjunto razoavelmente robusto de recursos, o que facilita a implementação do compilador, enquanto ao mesmo tempo permite explorar conceitos importantes de compilação e a usabilidade do compilador para a potencial produção de programas úteis e poderosos.
* Por um outro lado, justamente por ser uma linguagem tão robusta e vasta, o C também apresenta desafios notáveis para a implementação do compilador, como a gestão de memória, ponteiros e tipos de dados complexos. Inclusive, o escolhido foco em um subconjunto da linguagem C geral ajuda a mitigar esses desafios dados os limites de tempo e recursos do projeto impostos na matéria de Compiladores 1.
* A Representação Intermediária (RI/IR) LLVM, no geral, é uma escolha popular para compiladores devido à sua flexibilidade, portabilidade e capacidade de otimização. Utilizá-la como linguagem de destino em vez de mirar uma arquitetura de hardware específica permitirá que o compilador aproveite as otimizações, os recursos, a portabilidade e a flexibilidade oferecidos pela infraestrutura LLVM, resultando em código de máquina (final) eficiente.
