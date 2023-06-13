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
#define MAX_OBSERVERS 100

int serverSocket;
int* fans; // внутри храним сокеты поклонников, индекс массива - индес поклонника соответственно.
int* observers;
int fansCount = 0; // Счетчик поклонников.
int N = 0;

void initFans(int N) {
    fans = malloc(N * sizeof(int)); // массив поклонников.
    
    // изначально поклонников нет.
    for (int i = 0; i < N; ++i) {
        fans[i] = -1;
    }
}

void initObservers() {
    observers = malloc(MAX_OBSERVERS * sizeof(int)); // массив поклонников.
    
    // изначально поклонников нет.
    for (int i = 0; i < MAX_OBSERVERS; ++i) {
        observers[i] = -1;
    }
}

bool isConnectionAlive(int socket) {
    // Создание набора файловых дескрипторов для проверки готовности чтения
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(socket, &readfds);

    // Установка таймаута на ноль, чтобы функция select сразу вернула результат
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    // Выполнение проверки готовности чтения
    int result = select(socket + 1, &readfds, NULL, NULL, &timeout);
    if (result == -1) {
        perror("Ошибка при вызове select");
        return false;  // Обработка ошибки...
    }

    return !FD_ISSET(socket, &readfds);
}

bool isThereAvaliableConnection(int N) {
    for (int i = 0; i < N; ++i) {
        if (fans[i] == -1) {
            continue;
        }
        
        if (isConnectionAlive(fans[i])) {
            return true;
        }
    }
    
    return false;
}

int chooseFan(int N) {
    printf("Студентка выбирает поклонника на встречу.\n");
    
    srand(time(NULL)); // для рандомного выбора поклонника.
    while (1) {
        int ind = rand() % N; // выбор поклонника для встречи.
        if (fans[ind] == -1) {
            continue;
        }
        
        if (isConnectionAlive(fans[ind])) {
            return ind;
        }
        
        // Все поклонники, отправившие валентинки отключились.
        if (!isThereAvaliableConnection(N)) {
            printf("Все поклонники, отправившие валентинки, отключились.\n");
            break;
        }
    }
    
    return -1; // Если не удалось выбрать поклонника.
}

void* observerThread(void* arg) {
    int observerSocket = *(int*)arg;
    
    // Добавление нового наблюдателя в массив наблюдателей.
    for (int i = 0; i < MAX_OBSERVERS; ++i) {
        if (observers[i] == -1) {
            observers[i] = observerSocket;
            break;
        }
    }
    
    // Прием сообщения от наблюдателя.
    char buffer[1024];
    ssize_t bytesRead;
    while ((bytesRead = recv(observerSocket, buffer, sizeof(buffer), 0)) > 0) {
        buffer[bytesRead] = '\0';
        printf("Наблюдатель получил сообщение: %s\n", buffer);
        
        // Рассылка сообщения всем поклонникам.
        for (int i = 0; i < N; ++i) {
            if (fans[i] == -1) {
                continue;
            }
            
            send(fans[i], buffer, strlen(buffer), 0);
        }
    }
    
    // Удаление наблюдателя из массива наблюдателей.
    for (int i = 0; i < MAX_OBSERVERS; ++i) {
        if (observers[i] == observerSocket) {
            observers[i] = -1;
            break;
        }
    }
    
    close(observerSocket);
    pthread_exit(NULL);
}

void* fanThread(void* arg) {
    int fanSocket = *(int*)arg;
    
    // Добавление нового поклонника в массив поклонников.
    for (int i = 0; i < N; ++i) {
        if (fans[i] == -1) {
            fans[i] = fanSocket;
            break;
        }
    }
    
    // Ожидание выбора поклонником.
    while (1) {
        if (fansCount >= 2) {
            break;
        }
    }
    
    // Выбор поклонником.
    int chosenFan = chooseFan(N);
    if (chosenFan != -1) {
        // Отправка сообщения выбранному поклоннику.
        char message[] = "Поклонник выбран!";
        send(fans[chosenFan], message, strlen(message), 0);
        
        // Получение ответа от выбранного поклонника.
        char buffer[1024];
        ssize_t bytesRead = recv(fans[chosenFan], buffer, sizeof(buffer), 0);
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';
            printf("Получен ответ от поклонника: %s\n", buffer);
        }
    }
    
    // Удаление поклонника из массива поклонников.
    for (int i = 0; i < N; ++i) {
        if (fans[i] == fanSocket) {
            fans[i] = -1;
            break;
        }
    }
    
    close(fanSocket);
    pthread_exit(NULL);
}

int main() {
    // Инициализация массива поклонников.
    printf("Введите количество поклонников: ");
    scanf("%d", &N);
    initFans(N);
    
    // Создание сокета сервера.
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        perror("Ошибка при создании сокета сервера");
        exit(EXIT_FAILURE);
    }
    
    // Установка параметров сервера.
    struct sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(PORT);
    serverAddress.sin_addr.s_addr = inet_addr(SERVER_IP);
    
    // Привязка сокета сервера к адресу и порту.
    if (bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == -1) {
        perror("Ошибка при привязке сокета сервера");
        exit(EXIT_FAILURE);
    }
    
    // Прослушивание входящих соединений.
    if (listen(serverSocket, 5) == -1) {
        perror("Ошибка при прослушивании входящих соединений");
        exit(EXIT_FAILURE);
    }
    
    // Создание потока для приема соединений от наблюдателей.
    pthread_t observerAcceptThread;
    if (pthread_create(&observerAcceptThread, NULL, observerAcceptThread, NULL) != 0) {
        perror("Ошибка при создании потока для приема соединений от наблюдателей");
        exit(EXIT_FAILURE);
    }
    
    // Создание потоков для обработки соединений поклонников.
    pthread_t fanThreads[N];
    for (int i = 0; i < N; ++i) {
        int* fanSocket = malloc(sizeof(int));
        *fanSocket = accept(serverSocket, NULL, NULL);
        if (*fanSocket == -1) {
            perror("Ошибка при принятии соединения от поклонника");
            exit(EXIT_FAILURE);
        }
        
        if (pthread_create(&fanThreads[i], NULL, fanThread, fanSocket) != 0) {
            perror("Ошибка при создании потока для обработки соединения поклонника");
            exit(EXIT_FAILURE);
        }
    }
    
    // Ожидание завершения потоков поклонников.
    for (int i = 0; i < N; ++i) {
        if (pthread_join(fanThreads[i], NULL) != 0) {
            perror("Ошибка при ожидании завершения потока поклонника");
        }
    }
    
    // Закрытие сокета сервера.
    close(serverSocket);
    
    // Освобождение памяти.
    free(fans);
    free(observers);
    
    return 0;
}
