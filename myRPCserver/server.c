#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <json-c/json.h>
#include "mysyslog.h"

#define CONF_PATH "/etc/myRPC/myRPC.conf"
#define USERS_FILE "/etc/myRPC/users.conf"
#define BUFFER_MAX 4096

//prover4ka polzovat
int is_user_permitted(const char *user) {
    FILE *file = fopen(USERS_FILE, "r");
    if (!file) return 0;

    char entry[128];
    while (fgets(entry, sizeof(entry), file)) {
        entry[strcspn(entry, "\n")] = 0;
        if (strcmp(entry, user) == 0) {
            fclose(file);
            return 1;
        }
    }
    fclose(file);
    return 0;
}

//udalenie simvolov
char *strip_whitespace(char *str) {
    while (*str == ' ' || *str == '\t') str++;
    if (*str == '\0') return str;

    char *end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t')) *end-- = '\0';
    return str;
}

//ekranirovanie
char *escape_for_shell(const char *input) {
    size_t len = strlen(input);
    char *out = malloc(len * 4 + 3);
    if (!out) return NULL;

    char *ptr = out;
    *ptr++ = '\'';
    for (size_t i = 0; i < len; ++i) {
        if (input[i] == '\'') {
            memcpy(ptr, "'\\''", 4);
            ptr += 4;
        } else {
            *ptr++ = input[i];
        }
    }
    *ptr++ = '\'';
    *ptr = '\0';
    return out;
}

//chetinie faila
char *slurp_file(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) return strdup("(не удалось открыть файл)");

    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    rewind(fp);

    char *data = malloc(fsize + 1);
    if (!data) {
        fclose(fp);
        return strdup("(ошибка выделения памяти)");
    }

    fread(data, 1, fsize, fp);
    data[fsize] = '\0';
    fclose(fp);
    return data;
}

//obrabotka JSON zaprosa
void process_json_request(const char *request, char *reply_buf) {
    struct json_object *req_json = json_tokener_parse(request);
    struct json_object *resp_json = json_object_new_object();

    if (!req_json) {
        json_object_object_add(resp_json, "code", json_object_new_int(1));
        json_object_object_add(resp_json, "result", json_object_new_string("Некорректный JSON"));
        goto done;
    }

    struct json_object *j_login, *j_cmd;
    if (!json_object_object_get_ex(req_json, "login", &j_login) ||
        !json_object_object_get_ex(req_json, "command", &j_cmd)) {
        json_object_object_add(resp_json, "code", json_object_new_int(1));
        json_object_object_add(resp_json, "result", json_object_new_string("Отсутствуют поля login/command"));
        goto done;
    }

    const char *login = json_object_get_string(j_login);
    const char *command = json_object_get_string(j_cmd);

    if (!is_user_permitted(login)) {
        json_object_object_add(resp_json, "code", json_object_new_int(1));
        json_object_object_add(resp_json, "result", json_object_new_string("Пользователь не авторизован"));
        goto done;
    }

    char tmpname[] = "/tmp/myrpc_exec_XXXXXX";
    int tmpfd = mkstemp(tmpname);
    if (tmpfd < 0) {
        log_error("Ошибка mkstemp: %s", strerror(errno));
        json_object_object_add(resp_json, "code", json_object_new_int(1));
        json_object_object_add(resp_json, "result", json_object_new_string("Не удалось создать временный файл"));
        goto done;
    }
    close(tmpfd);

    char stdout_file[256], stderr_file[256];
    snprintf(stdout_file, sizeof(stdout_file), "%s.out", tmpname);
    snprintf(stderr_file, sizeof(stderr_file), "%s.err", tmpname);
char *escaped = escape_for_shell(command);
    if (!escaped) {
        json_object_object_add(resp_json, "code", json_object_new_int(1));
        json_object_object_add(resp_json, "result", json_object_new_string("Ошибка экранирования команды"));
        goto done;
    }

    char full_cmd[1024];
    snprintf(full_cmd, sizeof(full_cmd), "sh -c %s > %s 2> %s", escaped, stdout_file, stderr_file);
    free(escaped);

    int rc = system(full_cmd);
    char *output = slurp_file(rc == 0 ? stdout_file : stderr_file);

    json_object_object_add(resp_json, "code", json_object_new_int(rc == 0 ? 0 : 1));
    json_object_object_add(resp_json, "result", json_object_new_string(output));
    free(output);

done:
    strncpy(reply_buf, json_object_to_json_string(resp_json), BUFFER_MAX - 1);
    reply_buf[BUFFER_MAX - 1] = '\0';
    if (req_json) json_object_put(req_json);
    json_object_put(resp_json);
}

//zagruzka configur
void load_config(int *port, int *tcp_mode) {
    FILE *conf = fopen(CONF_PATH, "r");
    if (!conf) {
        log_warning("Не удалось открыть конфиг %s: %s", CONF_PATH, strerror(errno));
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), conf)) {
        char *clean = strip_whitespace(line);
        if (*clean == '#' || *clean == '\0') continue;

        if (sscanf(clean, "port = %d", port) == 1) continue;
        if (strstr(clean, "socket_type")) {
            char type[16];
            if (sscanf(clean, "socket_type = %15s", type) == 1) {
                *tcp_mode = strcmp(type, "dgram") != 0;
            }
        }
    }
    fclose(conf);
}

//mail cikl
int main() {
    log_info("Инициализация RPC-сервера");

    int port = 1234;
    int tcp_mode = 1;

    load_config(&port, &tcp_mode);

    log_info("Порт: %d | Режим: %s", port, tcp_mode ? "TCP" : "UDP");

    int sockfd = socket(AF_INET, tcp_mode ? SOCK_STREAM : SOCK_DGRAM, 0);
    if (sockfd < 0) {
        log_error("Ошибка создания сокета: %s", strerror(errno));
        return 1;
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = INADDR_ANY
    };

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        log_error("Ошибка bind: %s", strerror(errno));
        return 1;
    }

    if (tcp_mode && listen(sockfd, 10) < 0) {
        log_error("Ошибка listen: %s", strerror(errno));
        return 1;
    }

    log_info("Сервер готов к работе...");

    while (1) {
        struct sockaddr_in client;
        socklen_t len = sizeof(client);
        char buffer[BUFFER_MAX] = {0};

        if (tcp_mode) {
            int client_fd = accept(sockfd, (struct sockaddr *)&client, &len);
            if (client_fd < 0) {
                log_error("Ошибка accept: %s", strerror(errno));
                continue;
            }

            ssize_t rcv = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
            if (rcv > 0) {
                buffer[rcv] = '\0';
                log_info("TCP-запрос: %s", buffer);

                char reply[BUFFER_MAX];
                process_json_request(buffer, reply);
                send(client_fd, reply, strlen(reply), 0);
            }

            close(client_fd);

        } else {
            ssize_t rcv = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0,
                                   (struct sockaddr *)&client, &len);
            if (rcv < 0) {
                log_error("Ошибка recvfrom: %s", strerror(errno));
                continue;
            }

            buffer[rcv] = '\0';
            log_info("UDP-запрос: %s", buffer);

            char reply[BUFFER_MAX];
            process_json_request(buffer, reply);
            sendto(sockfd, reply, strlen(reply), 0, (struct sockaddr *)&client, len);
        }
    }

    close(sockfd);
    return 0;
}
