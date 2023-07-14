### Descrição

Para este módulo, utilizando parte do que foi feito para o primeiro, deverá ser implementado um modelo de clientes-servidor que corresponda a um chat, de modo que uma mensagem de um cliente deverá ser enviada para todos os clientes passando por uma aplicação servidora.

Cada cliente deverá ter um apelido definido arbitrariamente, que para este módulo possa simplesmente ser um inteiro (index) ou uma string qualquer. As mensagens aparecerão para todos os usuários (inclusive para quem enviou) no formato apelido: mensagem. Cada mensagem será separada por um ' $\backslash n^{\prime}$. De forma semelhante ao primeiro módulo, cada mensagem deverá ser limitada por 4096 caracteres.

Para fechar a sua conexão, um cliente poderá enviar um comando de saída (/quit) ou um sinal de $\mathrm{EOF}(\mathrm{Ctrl}+\mathrm{D})$.

#### Comandos implementados
- / connect - Estabelece a conexão com o servidor;

- /quit - O cliente fecha a conexão e fecha a aplicação;

- / ping - O servidor retorna "pong"assim que receber a mensagem.

#### Pontos importantes
O servidor deverá checar se os clientes receberam as mensagens. Caso eles não tenham recebido a mensagem ela deve ser enviada novamente. Após 5 tentativas falhas o servidor deve fechar a conexão com o cliente.

Não esqueça de tratar deadlocks e possíveis problemas que possam surgir com o uso de threads.

Deve ser necessário lidar com SIGINT (Ctrl + C) no chat, para isso a sugestão é adicionar um handler que ignore o sinal ou imprima alguma mensagem.
