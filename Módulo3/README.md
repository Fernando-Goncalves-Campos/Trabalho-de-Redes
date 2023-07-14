### Descrição
Nesta etapa deverá ser feita a implementação de múltiplos canais e a função de administradores de canais.

Ao abrir a aplicação, o usuário deverá, por meio do comando join, especificar em qual canal ele quer se conectar. Caso este canal não exista ele deverá ser criado e o primeiro usuário a se conectar se torna o administrador do canal.

O nome de um canal deverá seguir as restrições apresentadas no RFC-1459. Um administrador de um canal tem a permissão de usar os comandos Kick, Mute e Whois em usuários.

Ao abrir a aplicação pela primeira vez um usuário deverá definir um apelido por meio do comando nickname, limitando o nome do usuário a 50 caracteres ASCII.

#### Comandos implementados
Além dos comandos apresentados no módulo anterior, devem ser implementados os seguintes comandos:

- /join nomeCanal - Entra no canal;
- /nickname apelidoDesejado - O cliente passa a ser reconhecido pelo apelido especificado;

- /ping - O servidor retorna "pong"assim que receber a mensagem.

Comandos apenas para administradores de canais:

- /kick nomeU surio - Fecha a conexão de um usuário especificado
- /mute nomeUsurio - Faz com que um usuário não possa enviar mensagens neste canal
- /unmute nomeU surio - Retira o mute de um usuário.
- /whois nomeU surio - Retorna o endereço IP do usuário apenas para o administrador
