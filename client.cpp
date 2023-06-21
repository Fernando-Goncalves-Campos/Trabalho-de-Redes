#include <iostream>

#include <bits/stdc++.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <signal.h>
#include <mutex>

#define MAX_LEN 4096
#define MAX_NAME 50
#define DEF_PORT 8000

using namespace std;

bool exitFlag=false;
thread t_send, t_recv;
int clientSocket;

void catchCtrlC(int signal);
bool checkQuit(char command[]);
void eraseText(int cnt);
void sendMessage(int clientSocket);
void recvMessage(int clientSocket);

int main()
{
    //Entra no servidor padrão
    //Cria o socket do cliente
	if((clientSocket=socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("socket: ");
		exit(-1);
	}

    //Cria o endereço do cliente
	struct sockaddr_in client;
	client.sin_family=AF_INET;
	client.sin_addr.s_addr=INADDR_ANY;
	client.sin_port=htons(DEF_PORT);
	bzero(&client.sin_zero, 0);

    //Tenta se conectar com o servidor
	if((connect(clientSocket, (struct sockaddr *)&client, sizeof(struct sockaddr_in)))==-1)
	{
		perror("connect: ");
		exit(-1);
	}

    //Lida com o sinal de saída
	signal(SIGINT, catchCtrlC);
	
    //Envia o nome para o servidor
    char name[MAX_NAME];

    //Lê o nome até que ele seja um nome válido
    int unique = false;
    while(!unique){
        cout << "Enter your name : ";
        cin.getline(name, MAX_NAME);
        send(clientSocket, name, sizeof(name), 0);

        char confirmation[MAX_NAME];
        
        //Verifica se o nome é válido
        recv(clientSocket, confirmation, sizeof(confirmation), 0);
        if(strcmp(confirmation, "OK") == 0){
            unique = true;
        }
        else{
            cout << string(confirmation) << endl;
        }
    }

    //Confirma que entrou na sala
	cout <<"\n\t*********CHAT SERVER*********"<<"\n";

    //Cria as threads de leitura e de envio
    exitFlag = false;
	thread t1(sendMessage, clientSocket);
	thread t2(recvMessage, clientSocket);

	t_send=move(t1);
	t_recv=move(t2);

	if(t_send.joinable()){
		t_send.join();
    }
	if(t_recv.joinable()){
		t_recv.join();
    }

	return 0;
}

// Lida com os comandos
void catchCtrlC(int signal) 
{
    //Envia a mensagem de saída para o servidor
	char str[MAX_LEN] = "/quit";
	send(clientSocket, str, sizeof(str), 0);
	exitFlag=true;

    //Remove as threads
	t_send.detach();
	t_recv.detach();

    //Finaliza a conexão com o servidor
	close(clientSocket);
	exit(signal);
}

// Verifica se o cliente deseja desconectar do servidor
bool checkQuit(char command[]){
    //Verifica se o comando digitado era o de saída
    if(command[1] == 'q' && command[2] == 'u' && command[3] == 'i' && command[4] == 't'){
        //Interrompe a execução do cliente
        exitFlag=true;
        t_recv.detach();
        t_send.detach();
        close(clientSocket);
        return true;
    }

    return false;
}

// Apaga texto do terminal
void eraseText(int cnt)
{
	char backSpace=8;
	for(int i=0; i<cnt; i++)
	{
        /*É preciso substituir o caractere por um vazio porque se a mensagem for menor que a quantidade apagada,
          o texto antigo permance visível*/
		cout << backSpace << ' ' << backSpace;
	}

}

// Envia uma mensagem
void sendMessage(int clientSocket)
{
	while(1)
	{
        //Adiciona o texto para indicar que a mensagem está sendo escrita pelo cliente
		cout << "You : ";

        //Lê a mensagem enviada pelo cliente
        char str[MAX_LEN];
        cin.getline(str, MAX_LEN);
        
        //Envia a mensagem para o servidor
        send(clientSocket, str, sizeof(str), 0);

        //Verifica se o cliente deseja sair
        if(str[0] == '/'){
            if(checkQuit(str)){
                break;
            }
        }
        
    }
}

// Receive message
void recvMessage(int clientSocket)
{
	while(1)
	{
        //Verifica se o cliente saiu do servidor
		if(exitFlag){
			return;
        }
		
        //Define os valores para o nome e para a mensagem
        char name[MAX_NAME], str[MAX_LEN];
		
        //Lê o nome de quem enviou a mensagem
        int bytes_received=recv(clientSocket, name, sizeof(name), 0);
		if(bytes_received<=0){
			continue;
        }
		
        //Lê a mensagem
        recv(clientSocket, str, sizeof(str), 0);
		
        //Apaga o texto "You : "
        eraseText(6);

        //Adiciona o número de usuário caso seja uma mensagem de outro cliente
		if(strcmp(name, "#NULL")!=0){
			cout << name << " : " << str << endl;
        }

        //Coloca a mensagem enviada pelo cliente na tela
        else{
			cout<< str << endl;
        }

        //Adiciona o indicador de escrita do cliente
        cout << "You : ";

		fflush(stdout);
	}	
}