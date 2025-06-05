#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <json-c/json.h>
#include <getopt.h>
#include <pwd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>

#define BUFFER_SIZE 4096
#define LOGFILE "client.log"

//loggirovanie
void log_error(const char *message) {
    FILE *log = fopen(LOGFILE, "a");
    if (log) {
        time_t now = time(NULL);
        fprintf(log, "[%s] ERROR: %s\n", strtok(ctime(&now), "\n"), message);
        fclose(log);
    }
}

//spravka
void show_help(const char *program) {
    printf("Применение: %s [ОПЦИИ]\n", program);
    printf("Опции:\n");
    printf("  -c, --command \"cmd\"      Команда bash для отправки\n");
    printf("  -h, --host \"IP\"           IP адрес сервера\n");
    printf("  -p, --port PORT           Порт сервера\n");
    printf("  -s, --stream              Использовать TCP\n");
    printf("  -d, --dgram               Использовать UDP\n");
    printf("      --help                Показать справку\n");
}

int main(int argc, char *argv[]) {
    char *command = NULL, *host = NULL;
    int port = 0, tcp = 0, udp = 0;

    static struct option options[] = {
        {"command", required_argument, 0, 'c'},
        {"host", required_argument, 0, 'h'},
        {"port", required_argument, 0, 'p'},
        {"stream", no_argument, 0, 's'},
        {"dgram", no_argument, 0, 'd'},
        {"help", no_argument, 0, 0},
        {0, 0, 0, 0}
    };

    int opt, idx = 0;
    while ((opt = getopt_long(argc, argv, "c:h:p:sd", options, &idx)) != -1) {
        switch (opt) {
            case 'c': command = strdup(optarg); break;
            case 'h': host = strdup(optarg); break;
            case 'p': port = atoi(optarg); break;
            case 's': tcp = 1; break;
            case 'd': udp = 1; break;
            case 0:
                if (strcmp(options[idx].name, "help") == 0) {
                    show_help(argv[0]);
                    exit(EXIT_SUCCESS);
                }
                break;
            default:
                show_help(argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (!command || !host || port <= 0 || (!tcp && !udp)) {
        fprintf(stderr, "Ошибка: Недостаточно аргументов\n");
        show_help(argv[0]);
        exit(EXIT_FAILURE);
    }

    struct passwd *pw = getpwuid(getuid());
    if (!pw) {
        log_error("Не удалось получить имя пользователя");
        perror("getpwuid");
        exit(EXIT_FAILURE);
    }

    const char *user = pw->pw_name;

    //formi JSON
    struct json_object *req = json_object_new_object();
    json_object_object_add(req, "login", json_object_new_string(user));
    json_object_object_add(req, "command", json_object_new_string(command));
    const char *json_msg = json_object_to_json_string(req);

    int sock = socket(AF_INET, tcp ? SOCK_STREAM : SOCK_DGRAM, 0);
    if (sock < 0) {
        log_error("Не удалось создать сокет");
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in serv;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &serv.sin_addr) <= 0) {
        log_error("Неверный IP адрес");
        perror("inet_pton");
        close(sock);
        exit(EXIT_FAILURE);
    }

    char buffer[BUFFER_SIZE] = {0};
    ssize_t received;

    if (tcp) {
        if (connect(sock, (struct sockaddr *)&serv, sizeof(serv)) < 0) {
            log_error("Ошибка подключения по TCP");
            perror("connect");
            close(sock);
            exit(EXIT_FAILURE);
        }

        send(sock, json_msg, strlen(json_msg), 0);
        received = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (received > 0) {
            buffer[received] = '\0';
            printf("Ответ от TCP-сервера: %s\n", buffer);
        } else {
            log_error("Ошибка при получении ответа от TCP-сервера");
            perror("recv");
        }
    } else {
        struct timeval timeout = {5, 0};
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        sendto(sock, json_msg, strlen(json_msg), 0, (struct sockaddr *)&serv, sizeof(serv));

        socklen_t serv_len = sizeof(serv);
        received = recvfrom(sock, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&serv, &serv_len);
        if (received > 0) {
            buffer[received] = '\0';
            printf("Ответ от UDP-сервера: %s\n", buffer);
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            fprintf(stderr, "UDP: таймаут ожидания ответа\n");
        } else {
            log_error("Ошибка при получении UDP-ответа");
            perror("recvfrom");
        }
    }

    //chistka
    close(sock);
    free(command);
    free(host);
    json_object_put(req);

    return 0;
}
