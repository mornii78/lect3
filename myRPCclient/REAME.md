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
