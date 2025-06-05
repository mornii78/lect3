# myRPC


## Требования

- gcc (Компилятор языка Си)
- python (Сетевой репозиторий)
- git (Клонирование репозитория)
- make (Сборка проекта)
- fakeroot (создание .deb пакетов)

## Сборка проекта

Для сборки проекта используйте Makefile

Введите:

1. Клонируйте репозиторий:
   ```bash
   git clone <ссылка на этот проект>
   ```
2. Перейдите в директорию с проектом:
   ```bash
    cd ~/myRPC/
    make all
    make deb
    make repo
   ```
   
## Сетевой репозиторий 

Сетевой репозиторий позволяет подключаться клиенту по локальной сети и устанавливать нужные deb. пакеты.

Для того чтобы запустить репозиторий перейдите:

   ```bash
    cd repo
   ```
Для запуска сетевого репозитория используем python3. Простейшая утилита для создания HTTP-серверов.
Для запуска вводим команду:

   ```bash
    python3 -m http.server <Порт>
   ```

На клиенте создаем файл:

   ```bash
   echo "deb [trusted=yes] http://<айпи или домен сервера:порт> ./" | sudo tee /etc/apt/sources.list.d/myrpc.list
   ```

Затем: 

   ```bash
   sudo apt update
sudo apt install myRPC-client myRPC-server mysyslog

   ```

Можно скачать по ссылке: http://localhost:8000/Packages.gz

## myRPC-server

Он должен автоматически установиться при скачивании .deb пакета.

Узнаем статус демона:
   ```bash
   systemctl status myRPC-server
   ```

## myRPC-client

   ```bash
   myRPC-client <ключи>
   ```
Ключи:

-c , команда
-h , айпи
-p , порт как в /etc/myRPC/myRPC.conf
-d dgram / -s stream сокет как /etc/myRPC/myRPC.conf

Пример рабочей команды:
   ```bash
   myRPC-client -c "echo Привет сервер" -h 127.0.0.1 -p 4040 -d
   ```
