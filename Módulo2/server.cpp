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

#define MAX_MES 4096
#define MAX_NAME 50
#define MAX_LEN MAX_MES+MAX_NAME

#define MAX_CLIENTS 10
#define DEFAULT_PORT 8001
#define MAX_RT 5

#define DEFAULT_NAME ""
#define NULL_NAME "#NULL"

using namespace std;

struct client_t{
	int id;
	string name;
	int socket;
	thread th;
};

struct clientSend_t{
    client_t* client;
    char* message;
    size_t messageSize;
};

list<client_t> clients;
int clientId=0;
mutex cout_mtx, clients_mtx;

bool setName(client_t &reqClient, string name);

void sharedPrint(string str, bool endLine);
void broadcastMessage(string message, int senderId);
void clientSend(client_t &client, char* message, size_t messageSize);
void clientSendThread(clientSend_t* data);

void handleClient(client_t *client);

bool handleCommands(client_t &reqClient, string str);
void quit(client_t &reqClient);
void ping(client_t &reqClient);

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
        clients.push_back({clientId, string(DEFAULT_NAME), clientSocket});
        struct client_t &newClient = clients.back();
        
        //Cria uma thread
        thread t(handleClient, &newClient);
        newClient.th = (move(t));
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
void broadcastMessage(string message, int senderId){
    //Copia a mensagem para um buffer (a string do c++ não funciona com a função "send")
    char temp[MAX_LEN];
	strcpy(temp, message.c_str());

    list<clientSend_t> dataList;
    list<thread> threadList;

    //Envia a mensagem para todos os clientes que entraram na sala
	for(list<client_t>::iterator client = clients.begin(); client != clients.end(); ++client){
        if(client->id != senderId)
		{
            dataList.push_back(clientSend_t{&(*client), temp, sizeof(temp)});
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

    for(list<client_t>::iterator client = clients.begin(); client != clients.end(); ++client){
        if(client->name == name){
            //Informa ao cliente que o nome não é válido
            char message[MAX_NAME] = "This name already is been used";
            clientSend(reqClient, message, sizeof(message));

            //Marca que o nome não é único
            return false;
        }
    }

    //Envia a confirmação para o cliente saber que o nome foi aceito
    char message[MAX_NAME] = "OK";
    clientSend(reqClient, message, sizeof(message));
    
    //Altera o nome do cliente
    reqClient.name = name;

    //Marca que o nome é válido
    return true;
}

//Verifica qual comando foi executado
//The return value is used to know if the command used was /quit
bool handleCommands(client_t &reqClient, string str){
    size_t commandEnd = str.find(' ');
    string command = str.substr(0, commandEnd);

    ////Comandos livres
    //Comando /quit
    if(command == "/quit"){
        quit(reqClient);
        return true;
    }
    //Comando /ping
    else if(command == "/ping"){
        ping(reqClient);
        return false;
    }

    ////Mensagem de comando inválido
    else{
        char warning[MAX_LEN] = "Invalid command";
        clientSend(reqClient,warning,sizeof(warning));
        return false;
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

    //Interrompe a conexão com o cliente
    close(reqClient.socket);

    //Informas os outros clientes da saída
    string message = string(reqClient.name) + string(" has left");
    broadcastMessage(message, reqClient.id);

    //Apaga o cliente da lista de clientes do servidor
    for(list<client_t>::iterator client = clients.begin(); client != clients.end(); ++client){
        if(client->id == reqClient.id){
            clients.erase(client);
            break;
        }
    }
}

//Ping
void ping(client_t &reqClient){
    char warning[MAX_LEN] = "pong";
    clientSend(reqClient,warning,sizeof(warning));
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

    //Informa os outros clientes que um novo cliente entrou
    broadcastMessage(string(client->name)+" has joined", client->id);

	while(true){
        //Lê a mensagem
		int bytes_received = recv(client->socket, str, sizeof(str), 0);
		if(bytes_received<=0){
			return;
        }

        //Salva tudo que recebeu de cada cliente no terminal
        sharedPrint(string(client->name) + ": " + string(str));

        //Verifica se a mensagem é um comando
		if(str[0] == '/'){
            if(handleCommands(*client, string(str))){
                return;
            }
		}
        else{
            //Envia a mensagem para todos os outros clientes
            broadcastMessage(string(client->name)+": "+string(str), client->id);
        }
	}
}