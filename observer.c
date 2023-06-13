#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>
#include <semaphore.h>
#include <string.h>
#include <errno.h>
#include <sys/select.h>
#include <poll.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <arpa/inet.h>

#define PORT 16432
#define SERVER_IP "127.0.0.1"

int clientSocket;

void handleSIGINT(int signal) {
    close(clientSocket);
    exit(0); // Завершаем программу.
}

int main()
{
    signal(SIGINT, handleSIGINT);
    
    struct sockaddr_in serverAddr;
    
    // Создание сокета
    if ((clientSocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("Ошибка создания сокета");
        exit(EXIT_FAILURE);
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);

    // Преобразование IP-адреса из текстового в бинарный формат
    if (inet_pton(AF_INET, SERVER_IP, &(serverAddr.sin_addr)) <= 0)
    {
        perror("Ошибка преобразования адреса");
        exit(EXIT_FAILURE);
    }

    // Подключение к серверу не требуется для протокола UDP

    if (sendto(clientSocket, "0", strlen("0"), 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        close(clientSocket);
        perror("Ошибка при отправке данных о созданном поле серверу!\n");
        exit(EXIT_FAILURE);
    }
    
    bool firstTimeConnected = true;
    while (1) {
        char buf[32];
        socklen_t serverAddrLen = sizeof(serverAddr);
        int receivedBytes = recvfrom(clientSocket, buf, 32, 0, (struct sockaddr*)&serverAddr, &serverAddrLen);
        if (receivedBytes == -1) { // Ошибка при получении данных.
            close(clientSocket);
            printf("Ошибка при получении данных от сервера!\n");
            exit(EXIT_FAILURE);
        }
        
        if (receivedBytes == 0) {
            printf("Соединение с сервером было разорвано(сервер был закрыт)!!!");
            close(clientSocket);
            break;
        }
        
        if (firstTimeConnected) {
            printf("Наблюдатель: успешно подключен\n");
            firstTimeConnected = false;
        }
        
        buf[receivedBytes] = '\0'; // Добавляем завершающий нулевой символ для корректного вывода.
        
        int first, second;
        int n = sscanf(buf, "%d %d", &first, &second);
        
        if (n == 1) {
            if (first == -1) {
                printf("Студентка выбрала поклонника, а он сбежал со встречи, студентка идет гулять одна!\n");
                continue;
            }
            
            printf("Студентка идет гулять с поклонником с id: %d\n", first);
            continue;
        }
        
        printf("Текущее количество поклонников %d/%d\n", first, second);
    }

    return 0;
}
