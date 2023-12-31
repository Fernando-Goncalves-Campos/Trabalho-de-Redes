#include <iostream>
#include <list>
#include <map>
#include <set>

#include <bits/stdc++.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <mutex>

#define MAX_MES 4096
#define MAX_NAME 50
#define MAX_LEN MAX_MES+MAX_NAME

#define MAX_CLIENTS 10
#define DEFAULT_PORT 8003
#define MAX_RT 5

#define DEFAULT_ROOM ""
#define DEFAULT_NAME ""
#define NULL_NAME "#NULL"

using namespace std;

struct client_t{
	int id;
	string name;
    string room;
    bool adm;
    bool muted;
	int socket;
    in_addr IP;
    set<string> invitations;
	thread th;
};

struct clientSend_t{
    client_t* client;
    char* message;
    size_t messageSize;
};

map<string, list<client_t*>> rooms;
list<client_t> clients;
int clientId=0;
mutex cout_mtx, clients_mtx;

bool setName(client_t &reqClient, string name);

void sharedPrint(string str, bool endLine);
void broadcastMessage(string message, int senderId, string room);
void clientSend(client_t &client, char* message, size_t messageSize);
void clientSendThread(clientSend_t* data);

void leaveRoom(client_t &reqClient);
void joinRoom(client_t &reqClient, string roomName);
void handleClient(client_t *client);

bool handleCommands(client_t &reqClient, string str);
void join(client_t &reqClient, string roomName);
void quit(client_t &reqClient);
void nickname(client_t &reqClient, string newName);
void ping(client_t &reqClient);
void kick(client_t &reqClient, string targetName);
void mute(client_t &reqClient, string targetName);
void unmute(client_t &reqClient, string targetName);
void whois(client_t &reqClient, string targetName);
void invite(client_t &reqClient, string targetName);

int main(){
    //Cria o socket do servidor
	int serverSocket;
	if((serverSocket = socket(AF_INET, SOCK_STREAM, 0))==-1){
        perror("socket: ");
        exit(-1);
    }

    //Configura o endereço do servidor
	struct sockaddr_in server;
	server.sin_family=AF_INET;
	server.sin_addr.s_addr=INADDR_ANY;
	server.sin_port=htons(DEFAULT_PORT);
	bzero(&server.sin_zero, 0);
	
    //Associa o endereço ao socket
    if((bind(serverSocket, (struct sockaddr *)&server, sizeof(struct sockaddr_in))) == -1){
        perror("bind error: ");
        exit(-1);
    }

    //Começa a aceitar conexões
	if((listen(serverSocket, MAX_CLIENTS))==-1){
        perror("listen error: ");
        exit(-1);
    }
	
	int clientSocket;
    struct sockaddr_in client;
	unsigned int len = sizeof(sockaddr_in);
	
    //Imrprime que está em uma sala de chat
    cout << "\n\t*********CHAT SERVER*********" << "\n";

    //Cria a sala central do servidor
    rooms.insert(pair<string, list<client_t*>>(DEFAULT_ROOM, list<client_t*>()));

    //Aceita clientes
	while(true){
        //Realiza a conexão com o cliente
		if((clientSocket = accept(serverSocket, (struct sockaddr *)&client, &len)) == -1){
            perror("accept error: ");
            exit(-1);
        }

        //Incrementa os contadores
		++clientId;

        //Garante que as listas não sejam modificadas enquanto está adicionando o cliente
        lock_guard<mutex> guard(clients_mtx);

        //Cria o cliente
        clients.push_back({clientId, string(DEFAULT_NAME), string(DEFAULT_ROOM), false, false, clientSocket, client.sin_addr});
        struct client_t &newClient = clients.back();
        
        //Cria uma thread
        thread t(handleClient, &newClient);
        newClient.th = (move(t));
		
        //Adiciona o cliente à sala inicial
        rooms[DEFAULT_ROOM].push_back(&newClient);
	}

    //Espera as threads finalizarem
    for(list<client_t>::iterator client = clients.begin(); client != clients.end(); ++client){
        if(client->th.joinable()){
            client->th.join();
        }
    }

    //Fecha o socket do servidor
	close(serverSocket);

	return 0;
}

//Broadcast message to all clients except the sender
void broadcastMessage(string message, int senderId, string room){
	//Verifica se está em alguma sala
    if(room == DEFAULT_ROOM){
        return;
    }
    
    //Copia a mensagem para um buffer (a string do c++ não funciona com a função "send")
    char temp[MAX_LEN];
	strcpy(temp, message.c_str());


    list<clientSend_t> dataList;
    list<thread> threadList;
    //Envia a mensagem para todos os clientes que estão na mesma sala (menos para quem enviou)
	for(list<client_t*>::iterator client = rooms[room].begin(); client != rooms[room].end(); ++client){
        if((*client)->id != senderId)
		{
            dataList.push_back(clientSend_t{*client, temp, sizeof(temp)});
            threadList.push_back(thread(clientSendThread,&dataList.back()));
		}
    }

    for(list<thread>::iterator t = threadList.begin(); t != threadList.end(); ++t){
        t->join();
    }
}

//Envia a mensagem para o cliente selecionado
void clientSendThread(clientSend_t* data){
    //Tenta enviar a mensagem até que consiga, ou até que chegue no número limite de tentativas
    size_t bytesSent = 0;
    for(int i=0; i<MAX_RT && bytesSent < data->messageSize; ++i){
        bytesSent = send(data->client->socket,data->message,data->messageSize,0);
    }

    //Caso ele não tenha conseguido enviar, o processo corta a conexão com o cliente
    if(bytesSent < data->messageSize){
        quit(*(data->client));
    }
}

//Envia a mensagem para o cliente selecionado
void clientSend(client_t &client, char* message, size_t messageSize){
    //Tenta enviar a mensagem até que consiga, ou até que chegue no número limite de tentativas
    size_t bytesSent = 0;
    for(int i=0; i<MAX_RT && bytesSent < messageSize; ++i){
        bytesSent = send(client.socket,message,messageSize,0);
    }

    //Caso ele não tenha conseguido enviar, o processo corta a conexão com o cliente
    if(bytesSent < messageSize){
        quit(client);
    }
    
}

//Imprime todas as mensagens de todas as salas no terminal do servidor
void sharedPrint(string str, bool endLine=true){
	//Evita que mensagens sejam impressas incorretamente
    lock_guard<mutex> guard(cout_mtx);

    //Imprime a mensagem
	cout<<str;
	if(endLine){
        cout<<endl;
    }
}

//Define o nome do cliente
bool setName(client_t &reqClient, string name){
    //Impede mais de um cliente de executar comandos ao mesmo tempo
    lock_guard<mutex> guard(clients_mtx);

    //Testa para ver se o tamanho do nome está dentro do permitido
    if(name.length() > MAX_NAME){
        //Apenas tenho saída de um tamanho porque é garantido que o primeiro nome do usuário seja menor que o limite
        char message[MAX_LEN] = "This name is too long";
        clientSend(reqClient, message, sizeof(message));

        return false;
    }

    for(list<client_t>::iterator client = clients.begin(); client != clients.end(); ++client){
        if(client->name == name){
            //Decide como enviar a mensagem dependendo se o cliente já possui nome
            if(reqClient.name == DEFAULT_NAME){
                //Informa ao cliente que o nome não é válido
                char message[MAX_NAME] = "This name already is been used";
                clientSend(reqClient, message, sizeof(message));
            }
            else{
                //Informa ao cliente que o nome não é válido
                char message[MAX_LEN] = "This name already is been used";
                clientSend(reqClient, message, sizeof(message));
            }
            
            //Marca que o nome não é único
            return false;
        }
    }

    //Decide como enviar a mensagem dependendo se o cliente já possui nome
    if(reqClient.name == DEFAULT_NAME){
        char message[MAX_NAME] = "OK";
        clientSend(reqClient, message, sizeof(message));
    }
    else{
        //Envia uma mensagem para todos no servidor informando sobra a mudança de nome
        string message = reqClient.name + string(" changed his name to ") + name;
        broadcastMessage(message, reqClient.id, reqClient.room);
        sharedPrint(message);

        //Envia uma mensagem para o cliente informando sobre a mudança de nome
        char warning[MAX_LEN] = "Successfully changed your name";
        clientSend(reqClient,warning,sizeof(warning));
    }

    //Remove os convites que o cliente havia recebido, já que ele os recebeu em seu antigo nome
    reqClient.invitations.clear();

    //Altera o nome do cliente
    reqClient.name = name;

    //Marca que o nome é válido
    return true;
}

//Sai de uma sala
void leaveRoom(client_t &reqClient){
    for(list<client_t*>::iterator client = rooms[reqClient.room].begin(); client != rooms[reqClient.room].end(); ++client){
        if((*client)->id == reqClient.id){
            //Informa que o cliente saiu da sala em que estava
            if(reqClient.room != DEFAULT_ROOM){
                sharedPrint(reqClient.name + " left room " + reqClient.room);
            }

            //Envia mensagens para todos informando sobre a saída do cliente
            string welcomeMessage = string(reqClient.name) + string(" has left");
            broadcastMessage(welcomeMessage, reqClient.id, reqClient.room);

            //Apaga a sala caso não haja ninguém nela
            if(rooms[reqClient.room].size() == 1){
                //Remove o cliente da sala
                reqClient.adm = false;
                reqClient.muted = false;
                rooms[(*client)->room].erase(client);

                //Apaga a sala
                if(reqClient.room != DEFAULT_ROOM){
                    rooms.erase(reqClient.room);
                    sharedPrint(string("Room ") + string(reqClient.room) + " deleted");
                }

                //Apaga todos os convites para essa sala
                for(auto& client: clients){
                    client.invitations.erase(reqClient.room);
                }
            }

            //Determina o próximo adm
            else if(reqClient.adm){
                //O próximo adm sempre é o primeiro que entrou das pessoas que sobraram
                ++client;
                (*client)->adm = true;

                //Informa o cliente 
                string warning = (*client)->name + string(" became the adm");
                broadcastMessage(warning, reqClient.id, (*client)->room);
                sharedPrint((*client)->name + " is now the adm of room " + (*client)->room);

                //Remove o cliente da sala
                reqClient.adm = false;
                reqClient.muted = false;
                rooms[reqClient.room].erase(--client);
            }

            //Caso não seja nenhum cenário especial, ele apenas remove o cliente
            else{
                reqClient.adm = false;
                reqClient.muted = false;
                rooms[reqClient.room].erase(client);
            }    

            return;
        }
    }
}

//Entra em uma sala
void joinRoom(client_t &reqClient, string roomName){
    reqClient.room = roomName;
    for(map<string, list<client_t*>>::iterator room = rooms.begin(); room != rooms.end(); ++room){
        if(room->first == roomName){
            //Envia mensagens para todos informando sobre a entrada do cliente
            string welcomeMessage = reqClient.name + string(" has joined");
            broadcastMessage(welcomeMessage, reqClient.id, roomName);

            //Adiciona o cliente na sala
            room->second.push_back(&reqClient);

            //Imprime no terminal uma mensagem informando que o cliente entrou em uma sala
            sharedPrint(reqClient.name + " joined room " + roomName);

            return;
        }
    }

    //Cria uma nova sala
    rooms.insert(pair<string, list<client_t*>>(roomName, list<client_t*>()));
    sharedPrint(string("Room ") + roomName + " created");
    
    //Adiciona o cliente como adm dessa sala
    rooms[roomName].push_back(&reqClient);
    reqClient.adm = true;
    sharedPrint(reqClient.name + " is now the adm of room " + roomName);
}

//Verifica qual comando foi executado
//The return value is used to know if the command used was /quit
bool handleCommands(client_t &reqClient, string str){
    size_t commandEnd = str.find(' ');
    string command = str.substr(0, commandEnd);

    ////Comandos livres
    //Comando /join
    if(command == "/join"){
        //Verifica se o comando foi enviado com o nome da sala
        if(commandEnd != string::npos){
            join(reqClient, str.substr(commandEnd+1));
        }
        else{
            char warning[MAX_LEN] = "You need to provide the name of the room";
            clientSend(reqClient,warning,sizeof(warning));
        }
        return false;
    }
    //Comando /quit
    else if(command == "/quit"){
        quit(reqClient);
        return true;
    }
    //Comando /nickname
    else if(command == "/nickname"){
        //Verifica se o comando foi enviado com o novo nickname
        if(commandEnd != string::npos){
            nickname(reqClient, str.substr(commandEnd+1));
        }
        else{
            char warning[MAX_LEN] = "You need to provide a new nickname";
            clientSend(reqClient,warning,sizeof(warning));
        }
        return false;
    }
    //Comando /ping
    else if(command == "/ping"){
        ping(reqClient);
        return false;
    }

    ////Comandos de adm
    //Comando /kick
    else if(command == "/kick"){
        if(reqClient.adm){
            //Verifica se o comando foi enviado com o nome do cliente
            if(commandEnd != string::npos){
                kick(reqClient, str.substr(commandEnd+1));
            }
            else{
                char warning[MAX_LEN] = "You need to provide the name of the client you want to kick";
                clientSend(reqClient,warning,sizeof(warning));
            }
        }
        else{
            char warning[MAX_LEN] = "You don't have permission to execute this command";
            clientSend(reqClient,warning,sizeof(warning));
        }
        return false;
    }
    //Comando /mute
    else if(command == "/mute"){
        if(reqClient.adm){
            //Verifica se o comando foi enviado com o nome do cliente
            if(commandEnd != string::npos){
                mute(reqClient, str.substr(commandEnd+1));
            }
            else{
                char warning[MAX_LEN] = "You need to provide the name of the client you want to mute";
                clientSend(reqClient,warning,sizeof(warning));
            }
        }
        else{
            char warning[MAX_LEN] = "You don't have permission to execute this command";
            clientSend(reqClient,warning,sizeof(warning));
        }
        return false;
    }
    //Comando /ummute
    else if(command == "/unmute"){
        if(reqClient.adm){
            //Verifica se o comando foi enviado com o nome do cliente
            if(commandEnd != string::npos){
                unmute(reqClient, str.substr(commandEnd+1));
            }
            else{
                char warning[MAX_LEN] = "You need to provide the name of the client you want to unmute";
                clientSend(reqClient,warning,sizeof(warning));
            }
        }
        else{
            char warning[MAX_LEN] = "You don't have permission to execute this command";
            clientSend(reqClient,warning,sizeof(warning));
        }
        return false;
    }
    //Comando /whois
    else if(command == "/whois"){
        if(reqClient.adm){
            //Verifica se o comando foi enviado com o nome do cliente
            if(commandEnd != string::npos){
                whois(reqClient, str.substr(commandEnd+1));
            }
            else{
                char warning[MAX_LEN] = "You need to provide the name of the client whose ID you want to know";
                clientSend(reqClient,warning,sizeof(warning));
            }
        }
        else{
            char warning[MAX_LEN] = "You don't have permission to execute this command";
            clientSend(reqClient,warning,sizeof(warning));
        }
        return false;
    }
    //Comando /whois
    else if(command == "/invite"){
        if(reqClient.adm){
            //Verifica se o comando foi enviado com o nome do cliente
            if(commandEnd != string::npos){
                invite(reqClient, str.substr(commandEnd+1));
            }
            else{
                char warning[MAX_LEN] = "You need to provide the name of the client that you want to invite";
                clientSend(reqClient,warning,sizeof(warning));
            }
        }
        else{
            char warning[MAX_LEN] = "You don't have permission to execute this command";
            clientSend(reqClient,warning,sizeof(warning));
        }
        return false;
    }
    ////Mensagem de comando inválido
    else{
        char warning[MAX_LEN] = "Invalid command";
        clientSend(reqClient,warning,sizeof(warning));
        return false;
    }
}

//Entra em uma sala
void join(client_t &reqClient, string roomName){
    //Impede mais de um cliente de executar comandos ao mesmo tempo
    lock_guard<mutex> guard(clients_mtx);

    //Verifica se a sala é válida
    if(roomName == DEFAULT_ROOM){
        char warning[MAX_LEN] = "Invalid room name";
        clientSend(reqClient,warning,sizeof(warning));
    }

    //Verifica se é uma sala diferente
    else if(roomName == reqClient.room){
        char warning[MAX_LEN] = "You're already in this room";
        clientSend(reqClient,warning,sizeof(warning));
    }

    //Entra na sala
    else if(reqClient.invitations.find(roomName) != reqClient.invitations.end() || rooms.find(roomName) == rooms.end()){
        leaveRoom(reqClient);
        joinRoom(reqClient, roomName);
    }

    //Informa que não possui permissão para entrar na sala
    else{
        char warning[MAX_LEN] = "You wasn't invited to join this room";
        clientSend(reqClient,warning,sizeof(warning));
    }
}

//Sai do servidor
void quit(client_t &reqClient){
    //Impede mais de um cliente de executar comandos ao mesmo tempo
    lock_guard<mutex> guard(clients_mtx);

    //Imprime no terminal uma mensagem informando que o cliente saio do servidor
    sharedPrint(reqClient.name + " left the server");
    
    //Interromper a thread do cliente
    reqClient.th.detach();

    //Apaga o cliente da lista de clientes da sala em que ele está
    leaveRoom(reqClient);

    //Interrompe a conexão com o cliente
    close(reqClient.socket);

    //Apaga o cliente da lista de clientes do servidor
    for(list<client_t>::iterator client = clients.begin(); client != clients.end(); ++client){
        if(client->id == reqClient.id){
            clients.erase(client);
            break;
        }
    }
}

//Muda de nome
void nickname(client_t &reqClient, string newName){
    //Verifica se o nome é válido
    if(newName == DEFAULT_NAME){
        char warning[MAX_LEN] = "Invalid nickname";
        clientSend(reqClient,warning,sizeof(warning));
    }

    //Verifica se é um nickname diferente
    else if(newName == reqClient.name){
        char warning[MAX_LEN] = "That already is your nickname";
        clientSend(reqClient,warning,sizeof(warning));
    }

    //Tenta alterar o nickname
    else{
        setName(reqClient, newName);
    }
}

//Ping
void ping(client_t &reqClient){
    char warning[MAX_LEN] = "pong";
    clientSend(reqClient,warning,sizeof(warning));
}

//Expulsa
void kick(client_t &reqClient, string targetName){
    //Informa o adm que ele está falando dele mesmo
    if(targetName == reqClient.name){
        //Informa o adm que o convite foi enviado
        char inform[MAX_LEN] = "That's your name";
        clientSend(reqClient,inform,sizeof(inform));
        return;
    }
    
    //Impede mais de um cliente de executar comandos ao mesmo tempo
    lock_guard<mutex> guard(clients_mtx);

    //Verifica se a pessoa alvo existe
    client_t *targetClient = NULL;
	for(list<client_t*>::iterator client = rooms[reqClient.room].begin(); client != rooms[reqClient.room].end(); ++client){
        if((*client)->name == targetName)
		{
			targetClient = *client;
		}
    }

    //Verifica se o cliente foi encontrado
    if(targetClient){
        //Remove o cliente da sala
        leaveRoom(*targetClient);
        joinRoom(*targetClient, string(DEFAULT_ROOM));

        char warning[MAX_LEN] = "You were kicked by the adm";
        clientSend(*targetClient,warning,sizeof(warning));

        //Imprime no terminal uma mensagem informando que o cliente foi removido da sala em q estava
        sharedPrint(targetName + " was removed from the room " + reqClient.room);

        //Remove o convite que o cliente possuia para essa sala
        targetClient->invitations.erase(reqClient.room);
    }
    else{
        char warning[MAX_LEN] = "Client doesnt exists";
        clientSend(reqClient,warning,sizeof(warning));
    }
}

//Silencia
void mute(client_t &reqClient, string targetName){
    //Informa o adm que ele está falando dele mesmo
    if(targetName == reqClient.name){
        //Informa o adm que o convite foi enviado
        char inform[MAX_LEN] = "That's your name";
        clientSend(reqClient,inform,sizeof(inform));
        return;
    }
    
    //Impede mais de um cliente de executar comandos ao mesmo tempo
    lock_guard<mutex> guard(clients_mtx);

    //Verifica se a pessoa alvo existe
    client_t *targetClient = NULL;
	for(list<client_t*>::iterator client = rooms[reqClient.room].begin(); client != rooms[reqClient.room].end(); ++client){
        if((*client)->name == targetName)
		{
			targetClient = *client;
		}
    }

    //Verifica se o cliente foi encontrado
    if(targetClient){
        if(!targetClient->muted){
            targetClient->muted = true;

            char warning[MAX_LEN] = "You were muted by the adm";
            clientSend(*targetClient,warning,sizeof(warning));

            //Imprime no terminal uma mensagem informando que o cliente não pode mais falar
            sharedPrint(targetName + " can no longer talk in room " + targetClient->room);
        }
    }
    else{
        char warning[MAX_LEN] = "Client doesn't exist";
        clientSend(reqClient,warning,sizeof(warning));
    }
}

//Permite fala
void unmute(client_t &reqClient, string targetName){
    //Impede mais de um cliente de executar comandos ao mesmo tempo
    lock_guard<mutex> guard(clients_mtx);

    //Verifica se a pessoa alvo existe
    client_t *targetClient = NULL;
	for(list<client_t*>::iterator client = rooms[reqClient.room].begin(); client != rooms[reqClient.room].end(); ++client){
        if((*client)->name == targetName)
		{
			targetClient = *client;
		}
    }

    //Verifica se o cliente foi encontrado
    if(targetClient){
        //Verifica se o cliente estava silenciado
        if(targetClient->muted){
            targetClient->muted = false;
        
            char warning[MAX_LEN] = "You were unmuted by the adm";
            clientSend(*targetClient,warning,sizeof(warning));

            //Imprime no terminal uma mensagem informando que o cliente recebeu novamente a permissão para falar
            sharedPrint(targetName + " now can talk again in room " + targetClient->room);
        }
        
    }
    else{
        char warning[MAX_LEN] = "Client doesn't exists";
        clientSend(reqClient,warning,sizeof(warning));
    }
}

//Silencia
void whois(client_t &reqClient, string targetName){
    //Informa o adm que ele está falando dele mesmo
    if(targetName == reqClient.name){
        //Informa o adm que o convite foi enviado
        char inform[MAX_LEN] = "That's your name";
        clientSend(reqClient,inform,sizeof(inform));
        return;
    }
    
    //Impede mais de um cliente de executar comandos ao mesmo tempo
    lock_guard<mutex> guard(clients_mtx);

    //Verifica se a pessoa alvo existe
    client_t *targetClient = NULL;
	for(list<client_t*>::iterator client = rooms[reqClient.room].begin(); client != rooms[reqClient.room].end(); ++client){
        if((*client)->name == targetName)
		{
			targetClient = *client;
		}
    }

    //Verifica se o cliente foi encontrado
    if(targetClient){
        char warning[MAX_LEN];
        inet_ntop(AF_INET, &(targetClient->IP), warning, sizeof(warning));
        clientSend(reqClient,warning,sizeof(warning));

        //Imprime no terminal 
        sharedPrint(targetName + "'s IP address is " + warning);
    }
    else{
        char warning[MAX_LEN] = "Client doesn't exists";
        clientSend(reqClient,warning,sizeof(warning));
    }
}

//Convida para entrar na sala
void invite(client_t &reqClient, string targetName){
    //Informa o adm que ele está falando dele mesmo
    if(targetName == reqClient.name){
        //Informa o adm que o convite foi enviado
        char inform[MAX_LEN] = "That's your name";
        clientSend(reqClient,inform,sizeof(inform));
        return;
    }

    //Impede mais de um cliente de executar comandos ao mesmo tempo
    lock_guard<mutex> guard(clients_mtx);

    //Verifica se a pessoa alvo existe
    bool found = false;
	for(list<client_t>::iterator client = clients.begin(); client != clients.end(); ++client){
        if(client->name == targetName)
		{
            found = true;
			if(client->invitations.find(reqClient.room) != client->invitations.end()){
                //Informa a pessoa que está enviando a mensagem que esse convite já foi enviado
                char warning[MAX_LEN] = "Client already was invited to join this room";
                clientSend(reqClient,warning,sizeof(warning));
            }
            else{
                //Informa o adm que o convite foi enviado
                char inform[MAX_LEN] = "Invitation sent";
                clientSend(reqClient,inform,sizeof(inform));

                //Informa o cliente que foi convidado sobre o convite
                char warning[MAX_LEN] = "You where invited to join room ";
                strcat(warning, reqClient.room.c_str());
                clientSend(*client,warning,sizeof(warning));

                //Envia o convite
                client->invitations.insert(reqClient.room);
            }

            break;
		}
    }

    //Verifica se o cliente foi encontrado
    if(!found){
        char warning[MAX_LEN] = "Client doesn't exists";
        clientSend(reqClient,warning,sizeof(warning));
    }
}

//Função que recebe as mensagens dos clientes
void handleClient(client_t *client){
	char name[MAX_NAME], str[MAX_MES];

    //Repete a leitura do nome até que receba um nome válido
    bool unique = false;
    while(!unique){
        recv(client->socket, name, sizeof(name), 0);
        unique = setName(*client, string(name));
    }

	while(true){
        //Lê a mensagem
		int bytes_received = recv(client->socket, str, sizeof(str), 0);
		if(bytes_received<=0){
			return;
        }

        //Salva tudo que recebeu de cada cliente no terminal
        sharedPrint(string(client->name) + " : " + string(str));

        //Verifica se a mensagem é um comando
		if(str[0] == '/'){
            if(handleCommands(*client, string(str))){
                return;
            }
		}
        else{
            //Verifica se o cliente está mutado
            if(!client->muted){
                //Envia a mensagem para todos os outros clientes
                broadcastMessage(string(client->name)+": "+string(str), client->id, client->room);
            }
            else {
                char warning[MAX_LEN] = "You were muted, no one saw your message\0";
                clientSend(*client,warning,sizeof(warning));
            }
        }
        
	}
}