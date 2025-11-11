# Problemas Encontrados e Soluções Adotadas

Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.

## Problema: Diagnóstico de *Bugs* de Memória

Uma eventual preocupação central no projeto foi com a segurança de memória do próprio compilador. Dado que geralmente compilações da linguagem C tipicamente exigem muitos recursos de memória e processamento, especialmente quando paralelizadas em múltiplas *threads* simultâneas (dado que é efetivamente uma instância inteira do compilador rodando para cada uma delas), uma boa parte de garantir que os recursos estejam sendo usados o mais inteligentemente e, especialmente, com a maior segurança possível, recai sobre definir (muito) bem quando a memória dinamicamente alocada é liberada, isto é, a típica prevenção de falhas como:
* Vazamentos de memória: Quando uma área de memória nunca é liberada após se terminar de usá-la, produzindo um chamado *ponteiro solto*, e podendo prejudicar severamente o desempenho do programa (como já aludido previamente, uma situação previsivelmente crítica numa compilação), e até mesmo levar à sua parada quando faltar memória;
* _Double free_: Quando se tenta liberar uma área (um endereço) de memória que já foi liberado, o que geralmente para o programa imediatamente (falta de segmentação);
* Uso após _free_: Quando se tenta acessar uma área de memória já liberada. O próprio sistema costuma parar o programa quando ele encontra uma tentativa de acessar uma memória não alocada, mas pela linguagem C esse comportamento por definição/regra é indefinido, podendo inclusive levar a falhas de segurança como corrupção de dados ou vulnerabilidades exploráveis (*exploits*).

Para se abordar essas e outras questões foi usado o analisador dinâmico Valgrind, um *software* livre que auxilia o trabalho de depuração de programas.

O Valgrind possui ferramentas que detectam erros decorrentes do uso incorreto da memória dinâmica, como por exemplo os vazamentos de memória, alocação e desalocação incorretas e acessos a áreas inválidas. E quando eles são encontrados, o código é atualizado visando-se eliminá-los, reiterando-se o ciclo de análise e atualizações até não serem mais identificados erros de memória no programa.
