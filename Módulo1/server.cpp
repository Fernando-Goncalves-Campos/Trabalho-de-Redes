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

#define MAX_CLIENTS 2
#define DEFAULT_PORT 8000
#define MAX_RT 5

#define DEFAULT_NAME ""
#define NULL_NAME "#NULL"

using namespace std;

struct client_t{
    bool connected;
	int socket;
	thread th;
};

client_t clients[2];

mutex cout_mtx;

bool running = true;

void sharedPrint(string str, bool endLine);
void clientSend(client_t &client, char* message, size_t messageSize);

void handleClient(int index);

void handleCommands(int index, string str);
void quit();

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
	
    struct sockaddr_in client;
	unsigned int len = sizeof(sockaddr_in);
	
    //Imrprime que está em uma sala de chat
    cout << "\n\t*********CHAT SERVER*********" << "\n";

    ////Aceita o primeiro cliente////
    //Realiza a conexão com o cliente
    if((clients[0].socket = accept(serverSocket, (struct sockaddr *)&client, &len)) == -1){
        perror("accept error: ");
        exit(-1);
    }
    
    //Cria uma thread
    thread t0(handleClient, 0);
    clients[0].th = (move(t0));

    //Marca que ele está conectado
    clients[0].connected = true;

    ////Aceita o segundo cliente////
    //Realiza a conexão com o cliente
    if((clients[1].socket = accept(serverSocket, (struct sockaddr *)&client, &len)) == -1){
        perror("accept error: ");
        exit(-1);
    }
    
    //Cria uma thread
    thread t1(handleClient, 1);
    clients[1].th = (move(t1));

    //Marca que ele está conectado
    clients[1].connected = true;

    ////Mantém o servidor aberto enquanto for necessário////
    //Espera as threads finalizarem
    if(clients[0].th.joinable()){
        clients[0].th.join();
    }

    if(clients[1].th.joinable()){
        clients[1].th.join();
    }

    //Fecha o socket do servidor
	close(serverSocket);

	return 0;
}

//Envia a mensagem para o cliente selecionado
void clientSend(int index, char* message, size_t messageSize){
    //Verifica se o outro cliente já está conectado
    if(clients[index].connected){
        //Tenta enviar a mensagem até que consiga, ou até que chegue no número limite de tentativas
        size_t bytesSent = 0;
        for(int i=0; i<MAX_RT && bytesSent < messageSize; ++i){
            bytesSent = send(clients[index].socket,message,messageSize,0);
        }

        //Caso ele não tenha conseguido enviar, o processo corta a conexão com o cliente
        if(bytesSent < messageSize){
            quit();
        }
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

//Verifica qual comando foi executado
//The return value is used to know if the command used was /quit
void handleCommands(int index, string str){
    size_t commandEnd = str.find(' ');
    string command = str.substr(0, commandEnd);

    ////Comandos livres
    //Comando /quit
    if(command == "/quit"){
        //Envia a mensagem para o outro cliente informando que o servidor fechou
        char warning[MAX_LEN] = "/quit";
        clientSend(index^1,warning,sizeof(warning));
        quit();
    }

    ////Mensagem de comando inválido
    else{
        char warning[MAX_LEN] = "Invalid command";
        clientSend(index,warning,sizeof(warning));
    }
}

//Fecha o servidor caso qualquer um dos clientes tenha saído
void quit(){
    //Garante que as threads irão para de executar
    running = false;

    //Interromper a thread do cliente
    clients[0].th.detach();
    clients[1].th.detach();

    //Interrompe a conexão com o cliente
    close(clients[0].socket);
    close(clients[1].socket);
}

//Função que recebe as mensagens dos clientes
void handleClient(int index){
	char str[MAX_MES];

	while(running){
        //Lê a mensagem
		int bytes_received = recv(clients[index].socket, str, sizeof(str), 0);
		if(bytes_received<=0){
			return;
        }

        //Salva tudo que recebeu de cada cliente no terminal
        sharedPrint(string("Client") + to_string(index+1) + ": " + string(str));

        //Verifica se a mensagem é um comando
		if(str[0] == '/'){
            handleCommands(index, string(str));
		}
        else{
            //Envia a mensagem para o outro cliente
            char message[MAX_LEN] = "Other person: \0";
            strcat(message, str);
            clientSend(index^1, message, sizeof(message));
        }
	}
}