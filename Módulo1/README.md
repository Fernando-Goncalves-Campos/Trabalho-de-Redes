### Descrição

Imagine uma aplicação online como um chat ou um jogo. Como a comunicação dela é feita?

Neste módulo será desenvolvido uma aplicação para a comunicação entre Clientes na linguagem $\mathrm{C}$ ou $\mathrm{C}++$, sem o uso de bibliotecas externas. Para isso, deve ser implementado um socket, que define um mecanismo de troca de dados entre dois ou mais processos distintos, podendo estes estar em execução na mesma máquina ou em máquinas diferentes, porém ligadas através da rede. Uma vez estabelecida a ligação entre dois processos, eles devem poder enviar e receber mensagens um do outro.

$\mathrm{Na}$ aplicação a ser entregue devem ser implementados sockets TCP que permitam a comunicação entre duas aplicações, isso de modo que o usuário da aplicação 2 possa ler e enviar mensagens para o usuário da aplicação 1 e vice-versa.

O limite para o tamanho de cada mensagem deve ser de 4096 caracteres. Caso um usuário envie uma mensagem maior do que isso ela deverá ser dividida em múltiplas mensagens automaticamente.
