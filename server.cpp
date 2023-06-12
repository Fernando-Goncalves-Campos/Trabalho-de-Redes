#include <iostream>
#include <list>
#include <map>

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

#define MAX_LEN 4096
#define MAX_NAME 50
#define MAX_CLIENTS 10
#define DEF_PORT 8000

using namespace std;

struct client_t{
	int id;
	string name;
    string room;
    bool adm;
    bool muted;
	int socket;
	thread th;
};

map<string, list<client_t*>> rooms;
list<client_t> clients;
int clientId=0;
mutex cout_mtx, clients_mtx;

bool setName(client_t &reqClient, string name);
void sharedPrint(string str, bool endLine);
void broadcastMessage(string message, int senderId, string room, int messageLen);
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
	server.sin_port=htons(DEF_PORT);
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
    rooms.insert(pair<string, list<client_t*>>("", list<client_t*>()));

    //Aceita clientes
	while(true){
        //Realiza a conexão com o cliente
		if((clientSocket = accept(serverSocket, (struct sockaddr *)&client, &len)) == -1){
            perror("accept error: ");
            exit(-1);
        }

        //Incrementa os contadores
		++clientId;

        lock_guard<mutex> guard(clients_mtx);
        clients.push_back({clientId, string(""), string(""), false, false, clientSocket, thread()});
        struct client_t &newClient = clients.back();
        
        //Cria uma thread
        thread t(handleClient, &newClient);
        newClient.th = (move(t));
		
        //Adiciona o cliente às listas
        rooms[""].push_back(&newClient);
	}

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
void broadcastMessage(string message, int senderId, string room, int messageLen = MAX_LEN){
	//Verifica se está em alguma sala
    if(room == ""){
        return;
    }
    
    //Copia a mensagem para um buffer (a string do c++ não funciona com a função "send")
    char temp[messageLen];
	strcpy(temp, message.c_str());

    //Envia a mensagem para todos os clientes que estão na mesma sala (menos para quem enviou)
	for(list<client_t*>::iterator client = rooms[room].begin(); client != rooms[room].end(); ++client){
        if((*client)->id != senderId)
		{
			send((*client)->socket,temp,sizeof(temp),0);
		}
    }
}

//Imprime todas as mensagens de todas as salas no terminal do servidor
void sharedPrint(string str, bool endLine=true){
	lock_guard<mutex> guard(cout_mtx);
	cout<<str;
	if(endLine){
        cout<<endl;
    }
}

//Define o nome do cliente
bool setName(client_t &reqClient, string name){
    for(list<client_t>::iterator client = clients.begin(); client != clients.end(); ++client){
        if(client->name == name){
            //Decide como enviar a mensagem dependendo se o cliente já possui nome
            if(reqClient.name == ""){
                //Informa ao cliente que o nome não é válido
                char message[MAX_NAME] = "This name already is been used";
                send(reqClient.socket, message, sizeof(message), 0);
            }
            else{
                //Informa ao cliente que o nome não é válido
                char nullName[MAX_NAME] = "#NULL";
                char message[MAX_LEN] = "This name already is been used";
                send(reqClient.socket, nullName, sizeof(nullName), 0);
                send(reqClient.socket, message, sizeof(message), 0);
            }
            
            //Marca que o nome não é único
            return false;
        }
    }

    //Decide como enviar a mensagem dependendo se o cliente já possui nome
    if(reqClient.name == ""){
        char message[MAX_NAME] = "OK";
        send(reqClient.socket, message, sizeof(message), 0);
    }
    else{
        //Envia uma mensagem para todos no servidor informando sobra a mudança de nome
        string message = reqClient.name + string(" changed his name to ") + name;
        broadcastMessage("#NULL", reqClient.id, reqClient.room, MAX_NAME);
        broadcastMessage(message, reqClient.id, reqClient.room);
        sharedPrint(message);

        //Envia uma mensagem para o cliente informando sobre a mudança de nome
        char nullName[MAX_NAME] = "#NULL";
        char warning[MAX_LEN] = "Successfully changed your name";
        send(reqClient.socket,nullName,sizeof(nullName),0);
        send(reqClient.socket,warning,sizeof(warning),0);
    }
    
    
    //Altera o nome do cliente
    reqClient.name = name;

    //Marca que o nome é válido
    return true;
}

//Sai de uma sala
void leaveRoom(client_t &reqClient){
    for(list<client_t*>::iterator client = rooms[reqClient.room].begin(); client != rooms[reqClient.room].end(); ++client){
        if((*client)->id == reqClient.id){
            //Retorna os valores do cliente para os estados iniciais
            bool adm = reqClient.adm;
            reqClient.adm = false;
            reqClient.muted = false;

            //Remove o cliente da sala
            rooms[(*client)->room].erase(client);

            //Envia mensagens para todos informando sobre a saída do cliente
            string welcomeMessage = reqClient.name + string(" has left");
            broadcastMessage("#NULL", reqClient.id, reqClient.room, NAME_MAX);
            broadcastMessage(welcomeMessage, reqClient.id, reqClient.room);

            //Apaga a sala caso não haja ninguém nela
            if(rooms[reqClient.room].size() == 0 && reqClient.room != ""){
                rooms.erase(reqClient.room);
                sharedPrint(string("Room ") + string(reqClient.room) + " deleted");
            }
            //Determina o próximo adm
            else if(adm){
                ++client;
                (*client)->adm = true;

                //Informa o cliente 
                string warning = (*client)->name + string(" became the adm");
                broadcastMessage("#NULL", -1, (*client)->room, NAME_MAX);
                broadcastMessage(warning, -1, (*client)->room);
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
            room->second.push_back(&reqClient);

            //Envia mensagens para todos informando sobre a entrada do cliente
            string welcomeMessage = reqClient.name + string(" has joined");
            broadcastMessage("#NULL", reqClient.id, roomName, NAME_MAX);
            broadcastMessage(welcomeMessage, reqClient.id, roomName);
            return;
        }
    }

    rooms.insert(pair<string, list<client_t*>>(roomName, list<client_t*>()));
    sharedPrint(string("Room ") + roomName + " created");
    rooms[roomName].push_back(&reqClient);
    reqClient.adm = true;
}

//Verifica qual comando foi executado
//The return value is used to know if the command used was /quit
bool handleCommands(client_t &reqClient, string str){
    size_t commandEnd = str.find(' ');
    string command= str.substr(0, commandEnd);

    ////Comandos livres
    //Comando /join
    if(command == "/join"){
        join(reqClient, str.substr(commandEnd+1));
        return false;
    }
    //Comando /quit
    else if(command == "/quit"){
        quit(reqClient);
        return true;
    }
    //Comando /nickname
    else if(command == "/nickname"){
        nickname(reqClient, str.substr(commandEnd+1));
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
            kick(reqClient, str.substr(commandEnd+1));
        }
        else{
            char nullName[MAX_NAME] = "#NULL";
            char warning[MAX_LEN] = "You don't have permission to execute this command";
            send(reqClient.socket,nullName,sizeof(nullName),0);
            send(reqClient.socket,warning,sizeof(warning),0);
        }
        return false;
    }
    //Comando /mute
    else if(command == "/mute"){
        if(reqClient.adm){
            mute(reqClient, str.substr(commandEnd+1));
        }
        else{
            char nullName[MAX_NAME] = "#NULL";
            char warning[MAX_LEN] = "You don't have permission to execute this command";
            send(reqClient.socket,nullName,sizeof(nullName),0);
            send(reqClient.socket,warning,sizeof(warning),0);
        }
        return false;
    }
    //Comando /ummute
    else if(command == "/unmute"){
        if(reqClient.adm){
            unmute(reqClient, str.substr(commandEnd+1));
        }
        else{
            char nullName[MAX_NAME] = "#NULL";
            char warning[MAX_LEN] = "You don't have permission to execute this command";
            send(reqClient.socket,nullName,sizeof(nullName),0);
            send(reqClient.socket,warning,sizeof(warning),0);
        }
        return false;
    }
    //Comando /whois
    else if(command == "/whois"){
        if(reqClient.adm){
            whois(reqClient, str.substr(commandEnd+1));
        }
        else{
            char nullName[MAX_NAME] = "#NULL";
            char warning[MAX_LEN] = "You don't have permission to execute this command";
            send(reqClient.socket,nullName,sizeof(nullName),0);
            send(reqClient.socket,warning,sizeof(warning),0);
        }
        return false;
    }

    ////Mensagem de comando inválido
    else{
        char nullName[MAX_NAME] = "#NULL";
        char warning[MAX_LEN] = "Invalid command";
        send(reqClient.socket,nullName,sizeof(nullName),0);
        send(reqClient.socket,warning,sizeof(warning),0);
        return false;
    }
}

//Entra em uma sala
void join(client_t &reqClient, string roomName){
    if(roomName == ""){
        cout << "Nome nulo" << endl;
        char nullName[MAX_NAME] = "#NULL";
        char warning[MAX_LEN] = "Invalid room name";
        send(reqClient.socket,nullName,sizeof(nullName),0);
        send(reqClient.socket,warning,sizeof(warning),0);
    }
    else if(roomName == reqClient.room){
        char nullName[MAX_NAME] = "#NULL";
        char warning[MAX_LEN] = "You're already in this room";
        send(reqClient.socket,nullName,sizeof(nullName),0);
        send(reqClient.socket,warning,sizeof(warning),0);
    }
    else{
        leaveRoom(reqClient);
        joinRoom(reqClient, roomName);
    }
}

//Sai do servidor
void quit(client_t &reqClient){
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
    if(newName == ""){
        char nullName[MAX_NAME] = "#NULL";
        char warning[MAX_LEN] = "Invalid nickname";
        send(reqClient.socket,nullName,sizeof(nullName),0);
        send(reqClient.socket,warning,sizeof(warning),0);
    }
    else if(newName == reqClient.name){
        char nullName[MAX_NAME] = "#NULL";
        char warning[MAX_LEN] = "That already is your nickname";
        send(reqClient.socket,nullName,sizeof(nullName),0);
        send(reqClient.socket,warning,sizeof(warning),0);
    }
    else{
        setName(reqClient, newName);
    }
}

//Ping
void ping(client_t &reqClient){
    char nullName[MAX_NAME] = "#NULL\0";
    char warning[MAX_LEN] = "pong\0";
    send(reqClient.socket,nullName,sizeof(nullName),0);
    send(reqClient.socket,warning,sizeof(warning),0);
}

//Expulsa
void kick(client_t &reqClient, string targetName){
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
        joinRoom(*targetClient, string(""));

        char nullName[MAX_NAME] = "#NULL";
        char warning[MAX_LEN] = "You were kicked by the adm";
        send(targetClient->socket,nullName,sizeof(nullName),0);
        send(targetClient->socket,warning,sizeof(warning),0);
    }
    else{
        char nullName[MAX_NAME] = "#NULL";
        char warning[MAX_LEN] = "This client doesnt exists";
        send(reqClient.socket,nullName,sizeof(nullName),0);
        send(reqClient.socket,warning,sizeof(warning),0);
    }
}

//Silencia
void mute(client_t &reqClient, string targetName){
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

            char nullName[MAX_NAME] = "#NULL";
            char warning[MAX_LEN] = "You were muted by the adm";
            send(targetClient->socket,nullName,sizeof(nullName),0);
            send(targetClient->socket,warning,sizeof(warning),0);
        }
    }
    else{
        char nullName[MAX_NAME] = "#NULL";
        char warning[MAX_LEN] = "This isn't in your room";
        send(reqClient.socket,nullName,sizeof(nullName),0);
        send(reqClient.socket,warning,sizeof(warning),0);
    }
}

//Permite fala
void unmute(client_t &reqClient, string targetName){
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
        
            char nullName[MAX_NAME] = "#NULL";
            char warning[MAX_LEN] = "You were unmuted by the adm";
            send(targetClient->socket,nullName,sizeof(nullName),0);
            send(targetClient->socket,warning,sizeof(warning),0);
        }
        
    }
    else{
        char nullName[MAX_NAME] = "#NULL";
        char warning[MAX_LEN] = "This client doesn't exists";
        send(reqClient.socket,nullName,sizeof(nullName),0);
        send(reqClient.socket,warning,sizeof(warning),0);
    }
}

//Silencia
void whois(client_t &reqClient, string targetName){
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
        char nullName[MAX_NAME] = "#NULL";
        char warning[MAX_LEN];
        strcpy(warning, to_string(targetClient->socket).c_str());
        send(reqClient.socket,nullName,sizeof(nullName),0);
        send(reqClient.socket,warning,sizeof(warning),0);
    }
    else{
        char nullName[MAX_NAME] = "#NULL";
        char warning[MAX_LEN] = "This client doesn't exists";
        send(reqClient.socket,nullName,sizeof(nullName),0);
        send(reqClient.socket,warning,sizeof(warning),0);
    }
}

//Função que recebe as mensagens dos clientes
void handleClient(client_t *client){
	char name[MAX_NAME], str[MAX_LEN];

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

        //Verifica se a mensagem é um comando
		if(str[0] == '/'){
            if(handleCommands(*client, string(str))){
                //Salva o comando de saída
                sharedPrint(string(client->name) + " : " + str);
                return;
            }
		}
        else{
            //Verifica se o cliente está mutado
            if(!client->muted){
                //Envia a mensagem para todos os outros clientes
                broadcastMessage(string(client->name), client->id, client->room, MAX_NAME);
                broadcastMessage(string(str), client->id, client->room);
                
            }
            else if(client->room != ""){
                char nullName[MAX_NAME] = "#NULL";
                char warning[MAX_LEN] = "You were muted, no one saw your message\0";
                send(client->socket,nullName,sizeof(nullName),0);
                send(client->socket,warning,sizeof(warning),0);
            }
        }
        //Salva tudo que recebeu de cada cliente
        sharedPrint(string(client->name) + " : " + str);
	}
}