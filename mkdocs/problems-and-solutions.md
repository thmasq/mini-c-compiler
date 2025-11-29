# Problemas Encontrados e Soluções Adotadas

Este documento detalha os principais desafios enfrentados durante o desenvolvimento do compilador, além das estratégias adotadas para (se) resolvê-los.

## Problema: Diagnóstico de *Bugs* de Memória

### Contexto

Uma eventual preocupação central no projeto, a segurança de memória é crítica em compiladores (como os de) C devido ao uso intensivo de alocação dinâmica para estruturas como AST (Abstract Syntax Tree), tabelas de símbolos e gerenciamento de escopos, além de compilações paralelizadas – em múltiplas *threads* simultâneas –. Erros de memória não apenas comprometem a estabilidade do compilador, mas podem levar a:

**Vazamentos de memória**:
Áreas de memória nunca liberadas após uso, gerando chamados *ponteiros soltos* que, criticamente, degradam o desempenho e podem levar à exaustão de memória em compilações de arquivos/projetos grandes.

***Double free***:
Tentativas de liberar memória já desalocada, causando falhas de segmentação imediatas.

**Uso após *free* (*use-after-free*)**:
Acesso a memória já liberada, produzindo comportamento indefinido segundo o padrão C e podendo resultar em corrupção de dados ou vulnerabilidades exploráveis (*exploits*).

### Solução Adotada

#### Ferramentas de Análise

Para se abordar essas e outras questões, utilizamos o **Valgrind**, um *software* livre que auxilia o trabalho de depuração de programas, como analisador dinâmico principal, executando o compilador com as seguintes opções:

```bash
valgrind --leak-check=full \
         --show-leak-kinds=definite,possible \
         --track-origins=yes \
         --error-exitcode=99 \
         ./minicc tests/sample.c
```

O Valgrind possui ferramentas que detectam erros decorrentes do uso incorreto da memória dinâmica, como por exemplo os vazamentos de memória, alocação e desalocação incorretas e acessos a áreas inválidas.

**Parâmetros utilizados:**
- `--leak-check=full`: Detecção completa de vazamentos
- `--show-leak-kinds=definite,possible`: Reporta vazamentos confirmados e prováveis
- `--track-origins=yes`: Rastreia origem de valores não inicializados
- `--error-exitcode=99`: Retorna código de erro em caso de problemas de memória

#### Metodologia

1. **Bateria de Testes**: É executado o compilador sob Valgrind com:
    - Suíte de testes léxicos e sintáticos (30+ casos)
    - Geração de LLVM IR para programas básicos e avançados
    - Testes de estresse (recursão profunda, *arrays* grandes, múltiplos escopos)
    - Programas com erros semânticos intencionais

2. **Ciclo de Correção**:
   - Análise de relatórios do Valgrind
   - Identificação de padrões de erro
   - Correção no código-fonte
   - Re-validação com Valgrind

3. **Estratégia de *Ownership***:
   - Cada nó AST é liberado por `free_ast()` recursivamente
   - Símbolos são liberados automaticamente ao sair do escopo via `exit_scope()`
   - *Strings* duplicadas sempre via `string_duplicate()` e liberadas em `free_symbol()`
   - *Deep copy* de `type_info_t` via `deep_copy_type_info()` para evitar *double-free*

#### Resultados

| Métrica | Antes | Depois |
| -------- | ------- | -------- |
| Vazamentos definitivos | ~1.2 KB | **0 bytes** ✅ |
| Vazamentos possíveis | ~3.4 KB | **0 bytes** ✅ |
| Acessos inválidos | 12 casos | **0 casos** ✅ |
| Uso de memória não inicializada | 8 casos | **0 casos** ✅ |

**Exemplo de saída final do Valgrind:**
```
HEAP SUMMARY:
    in use at exit: 0 bytes in 0 blocks
  total heap usage: 1,847 allocs, 1,847 frees, 156,890 bytes allocated

All heap blocks were freed -- no leaks are possible
```

#### Integração Contínua

Os testes de memória foram integrados ao *pipeline* CI/CD (GitHub Actions) (`.github/workflows/main.yml`), executando o Valgrind automaticamente em cada *commit*:

```yaml
- name: Run tests with Valgrind
  run: |
    BIN="valgrind $VALGRIND_OPTS ./minicc" bash tests/lex/run.sh
    BIN="valgrind $VALGRIND_OPTS ./minicc" bash tests/syntax/run.sh
```

*Logs* do Valgrind são preservados como artefatos para auditoria(s).

### Estado Atual

O compilador está **completamente livre de vazamentos e erros de memória** detectáveis pelo Valgrind em todos os cenários de teste. A validação contínua garante que novas funcionalidades não introduzam regressões de memória.
