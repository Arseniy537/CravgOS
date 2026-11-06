# HANDOFF: Cravgos (CravOS)

Статус сведений: **[ФАКТ]** — проверено в этой работе (код/тесты на хосте и в VM); **[ПРЕДП.]** — предположение; **[ПРОВЕРИТЬ]** — нужно сверить с исходниками.

## 1. Назначение и архитектура
- 64-битная x86_64 автономная операционная система (Long Mode / IA-32e) без внешних библиотек (freestanding C). [ФАКТ]
- Поддерживает текстовую консоль 80x25, темы оформления, графический рабочий стол (`gui`), двухпанельный файловый проводник `fm`, полноэкранный редактор `edit`, веб-браузер `web` (HTTP/HTTPS, HTML, рендеринг картинок 16 цветов VGA), пакетный менеджер `stas`, сохранение состояния на ATA жесткий диск. [ФАКТ]
- Ядро и оболочка: монолитный `kernel.c` (~2670 строк) + заголовочные модули (`net.h`, `http.h`, `stas.h`, `fs.h`, `tls.h`, `crypto.h`, `html.h`, `image.h`, `cyrfont.h`). [ФАКТ]
- Текущая версия: **0.6.1 (x86_64)**. Собраны и протестированы артефакты `cravgos.iso` (гибридный BIOS + UEFI El Torito), `bootx64.efi` (UEFI GOP), `cravgos.bin` (GRUB Multiboot 1 & 2). [ФАКТ]

## 2. Загрузка и целевая среда (v0.6.1)
- **Гибридный ISO (`mkiso.py`, `boot_iso.s`, `boot_efi.c`)**:
  - **BIOS**: Real Mode 16-bit на 0x7C00 -> 32-bit Protected Mode -> 64-bit Long Mode через 4-уровневые таблицы страниц PML4 (2 МБ Huge Pages на первые 4 ГБ физической памяти).
  - **UEFI**: Загрузочный PE32+ бинарь `\EFI\BOOT\BOOTX64.EFI` упакован в FAT12 образ `efiboot.img`, подключён в El Torito Section Header (`Platform ID 0xEF`). Запрашивает протокол GOP, определяет разрешение и линейный фреймбуфер.
  - **GRUB Multiboot 1 & 2 (`boot.s`, `linker.ld`)**: бинарный образ `cravgos.bin` содержит оба заголовка Multiboot 1 (`0x1BADB002`) и Multiboot 2 (`0xE85250D6`), настраивает PML4, GDT64, шлюзы IDT ISR и переходит в ядро.
- **Раскладка памяти**:
  - `linker_iso.ld` и `linker.ld`: секция `.bigbss` на `0x200000` (`NOLOAD`) под сетевые буферы, кольца дескрипторов e1000, буферы веб-страниц и TLS. Символы `__bss_start`, `__bss_end`, `__bigbss_start`, `__bigbss_end` синхронизированы.
- **Целевая среда**: VirtualBox (тип Other 64-bit, адаптер Intel PRO/1000 MT Desktop, NAT, Primary Master ATA HDD 2048 MiB), QEMU x86_64.

## 3. Файлы репозитория
| Файл | Назначение |
|---|---|
| `kernel.c` | Ядро, драйверы (VGA/PS2/PIT/CMOS/ATA/e1000), CLI, графический десктоп `gui` (`struct fb_info`), `fm`, `web`, `edit`, `cmdtab[]` |
| `boot_iso.s` | BIOS El Torito загрузчик: 16-bit -> 32-bit -> 64-bit Long Mode + PML4 + IDT |
| `boot_efi.c` | 64-битное приложение UEFI GOP (`BOOTX64.EFI`) |
| `boot.s` | Загрузчик GRUB Multiboot 1 & 2 + 64-bit IDT ISR stubs |
| `linker_iso.ld` | Скрипт линковки ISO (точка входа 0x7C00, секция `.bigbss`) |
| `linker.ld` | Скрипт линковки GRUB Multiboot (`cravgos.bin`) |
| `mkiso.py` | Генератор гибридного ISO (BIOS El Torito + FAT12 EFI Boot Image) |
| `Makefile` | Сборка `bootx64.efi`, `cravgos.iso`, `cravgos.bin`, `test`, `clean` под Windows и Linux |
| `net.h` | Сетевой стек: ARP, IPv4, ICMP, UDP, DHCP, DNS (6 серверов fallback), TCP |
| `http.h` | HTTP/1.0, моментальный Chunked, NAT mapping `127.0.0.1` -> `10.0.2.2`, DoH fallback |
| `stas.h` | Пакетный менеджер `stas` с нормализацией CRLF, пресетами `host`/`default`/`offline` и встроенными офлайн-пакетами |
| `stas-repo/` | Репозиторий пакетов (`index.txt`, `fact.stp`, `guess.stp`, `nim.stp`) |
| `tools/stas_mkrepo.py` | Генератор `index.txt` с фиксацией LF line endings |
| `fs.h` | RAMFS: 64 узла × 2048 байт, синхронизация с диском ATA (`save`) |
| `html.h` | HTML-парсер под 80 колонок, декодирование кодировок UTF-8/CP1251/KOI8-R в CP866 |
| `image.h` | Декодеры PNG, JPEG, GIF, 16-цветный дизеринг по Байеру, псевдографика 80x50 |
| `cyrfont.h` | Растровый шрифт 8x16 (CP866, 72 глифа) |
| `tls.h`, `crypto.h` | TLS 1.2 (ECDHE X25519/P-256, AES-128-GCM, SHA-256) |
| `tests/run_all.py` | Кроссплатформенный запуск всех модульных тестов (`test_fs`, `test_net`, `test_html`, `test_http`, `test_crypto`, `test_stas`) |

## 4. Результаты тестирования (v0.6.1)

### 4.1. Модульные тесты хоста (`make test`) — 100% пройдены [ФАКТ]
- `test_fs`: OK (файловая система, каталоги, запись, удаление).
- `test_net`: OK (DHCP, ARP, ICMP ping, DNS, таймауты).
- `test_html`: OK (разбор тегов, ссылок, форматирование).
- `test_http`: OK (парсинг URL, TCP loss recovery, chunked stream, редиректы, DoH).
- `test_crypto`: OK (SHA-256, HMAC, AES, GCM, X25519).
- `test_stas`: OK (индекс, SHA-256, запуск скриптов, `repo host`, `repo offline`, нормализация Windows CRLF).

### 4.2. Прямое тестирование в VirtualBox VM (>95% команд) [ФАКТ]
В виртуальной машине `cravgos060` (VirtualBox 7.x, Intel e1000 NAT, ATA HDD 2 ГБ) в режиме реального времени выполнены и зафиксированы на скриншотах 39 команд:
1. `ver` — вывод `Cravgos 0.6.1 (x86_64) - 64-bit OS (long mode, UEFI/BIOS/GRUB, mouse, themes, GUI, http+https)`.
2. `uptime` — отсчёт аптайма в минутах и секундах.
3. `calc 40+2` — корректный расчёт `42`.
4. `cowsay CravgOS 0.6.1` — псевдографическая корова с текстом.
5. `fastfetch` — цветной логотип, процессор (AMD Ryzen), RAM (2% used), ATA Disk.
6. `free` — детализация Total RAM 112 MiB, kernel 0.1 MiB, buffers 2.7 MiB, ATA 2048 MiB.
7. `time` — текущее время UTC по CMOS RTC.
8. `date` — текущая дата (06.11.2026).
9. `lang ru` — переключение на русскую раскладку ЙЦУКЕН.
10. `lang en` / F12 — переключение раскладки обратно на English.
11. `theme retro` — переключение цветовой палитры.
12. `theme amber` — возврат к янтарной теме по умолчанию.
13. `clear` — очистка текстового буфера экрана.
14. `pwd` — печать текущей директории `/`.
15. `mkdir /demo` — создание каталога.
16. `cd /demo` — переход в каталог.
17. `write a.txt Hello-0.6.1` — создание файла с записью строки.
18. `append a.txt SecondLine` — добавление строки в конец файла.
19. `cat a.txt` — корректное чтение созданных строк.
20. `cp a.txt c.txt` — копирование файла.
21. `mv c.txt m.txt` — переименование файла.
22. `tree /demo` — дерево файлов.
23. `df` — отчёт о свободных узлах и занятых байтах в RAMFS.
24. `rm m.txt` — удаление файла.
25. `save` — запись изменений RAMFS, темы и раскладки на диск ATA (`260 sectors`).
26. `lspci` — листинг устройств шины PCI, сетевая карта Intel 82540EM (8086:100e).
27. `ifconfig` — параметры сетевого интерфейса e1000 (MAC, IP 10.0.2.15, gateway 10.0.2.2).
28. `dns 8.8.8.8` — ручная установка DNS сервера с сохранением от перезаписи DHCP.
29. `ping 10.0.2.2 2` — отправка ICMP-пакетов шлюзу VirtualBox NAT, RTT 0 мс.
30. `arp` — просмотр ARP-таблицы (MAC адрес шлюза 10.0.2.2).
31. `curl http://httpforever.com` — реальный сетевой запрос, получение HTTP 200 OK, парсинг и вывод живого контента сайта без таймаутов.
32. `stas repo offline` — переключение пакетного менеджера на встроенный офлайн-репозиторий.
33. `stas update` — обновление индекса (`Index updated: 3 packages`).
34. `stas list` — листинг доступных пакетов (`fact`, `guess`, `nim`).
35. `stas install fact` — скачивание, проверка SHA-256 и установка пакета `fact`.
36. `fact` — запуск установленного пакета прямо из шелла, ввод `5`, расчёт факториала `5! = 120`.
37. `gui` — запуск графического рабочего окружения: таскбар, окно рабочего стола с тенью, кнопками запуска файлов, браузера, смены тем, часов и статуса сети.
38. `help curl` — справка по команде.
39. `echo hello_world > /test.txt` — перенаправление вывода в файл.

## 5. Решённые ключевые проблемы (v0.6.1)
1. **Сброс настроек DNS**:
   - *Причина*: При вводе любой сетевой команды вызывался `net_ready()` -> `net_autoconf()`, который повторно слал DHCP-запрос и перезаписывал `ndns` адресом хоста (`172.19.0.2`).
   - *Решение*: Введён флаг `ndns_manual = 1` при `cmd_dns()`, `net_autoconf()` сохраняет ручной DNS. В `cand[6]` добавлены Yandex DNS (`77.88.8.8`), Google (`8.8.8.8`), Cloudflare (`1.1.1.1`) с непрерывным обходом при SERVFAIL.
2. **Проблемы STAS пакетов на Windows хосте**:
   - *Причина*: Символы перевода строк CRLF (`\r\n`) на Windows меняли размер и SHA-256 пакетов `.stp`, вызывая отказ установки (`size differs` / `SHA-256 mismatch`).
   - *Решение*: В `stas_install()` добавлена нормализация буфера в памяти; если raw или нормализованный хеш совпадает с `index.txt`, пакет успешно устанавливается. Добавлен `stas repo offline` для работы без сети и `stas repo host` для быстрого подключения к хостовому серверу.
3. **GRUB и UEFI совместимость**:
   - *Решение*: Разработан `boot_efi.c` для загрузки по UEFI GOP и созданы заголовки Multiboot 1 & 2 в `boot.s` с корректной линковкой `.bigbss` через `linker.ld`. `mkiso.py` собирает настоящий гибридный El Torito образ (BIOS + UEFI).
4. **Графическое окружение**:
   - *Решение*: Создана абстракция `struct fb_info` и реализован десктоп-менеджер `cmd_gui` с окнами, таскбаром, кнопками запуска и поддержкой мыши.
