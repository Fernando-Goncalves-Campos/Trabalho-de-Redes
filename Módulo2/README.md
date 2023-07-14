### Descrição

Para este módulo, utilizando parte do que foi feito para o primeiro, deverá ser implementado um modelo de clientes-servidor que corresponda a um chat, de modo que uma mensagem de um cliente deverá ser enviada para todos os clientes passando por uma aplicação servidora.

Cada cliente deverá ter um apelido definido arbitrariamente, que para este módulo possa simplesmente ser um inteiro (index) ou uma string qualquer. As mensagens aparecerão para todos os usuários (inclusive para quem enviou) no formato apelido: mensagem. Cada mensagem será separada por um ' $\backslash n^{\prime}$. De forma semelhante ao primeiro módulo, cada mensagem deverá ser limitada por 4096 caracteres.

Para fechar a sua conexão, um cliente poderá enviar um comando de saída (/quit) ou um sinal de $\mathrm{EOF}(\mathrm{Ctrl}+\mathrm{D})$.
