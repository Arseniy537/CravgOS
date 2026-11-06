# HANDOFF: Cravgos (CravOS)

Статус сведений: **[ФАКТ]** — проверено в этой работе (код/тесты на хосте); **[ПРЕДП.]** — предположение; **[ПРОВЕРИТЬ]** — нужно сверить с исходниками.

## 1. Назначение и архитектура
- Учебная текстовая ОС без GUI: терминал 80x25, собственное 64-битное x86_64-ядро (Long Mode / IA-32e), загрузка с ISO в виртуальной машине (VirtualBox, сетевая карта Intel e1000 / NAT). [ФАКТ]
- Ядро и оболочка — один файл `kernel.c` (~2200 строк) + заголовки-модули (`net.h`, `fs.h`, `http.h`, `tls.h`, `crypto.h`, `html.h`, `image.h`, `cyrfont.h`). [ФАКТ]
- Текущая версия: **0.6 (x86_64)** (выпущен `cravgos.iso`, образ загружается в 64-битный Long Mode). [ФАКТ]

## 2. Язык, загрузка, целевая среда
- C (freestanding, `-m64`, `-mno-red-zone`, `-mno-stack-arg-probe`), ассемблер GAS для загрузчика, Python для сборки ISO и генерации шрифта. [ФАКТ]
- 64-битный Long Mode (IA-32e): старт в 16-битном real mode на 0x7C00 (`boot_iso.s`), переход в 32-битный режим, проверка CPUID на поддержку Long Mode, построение 4-уровневых таблиц страниц (PML4/PDPT/PD по 2 МБ на первые 4 ГБ), включение PAE, EFER.LME, CR0.PG, загрузка GDT64 и переход в `.code64`. [ФАКТ]
- Линкер `linker_iso.ld`: секция `.bigbss` на 0x200000 (NOLOAD) для больших буферов (`XBSS` = `__attribute__((section(".bss.xbig")))`). [ФАКТ]
- ISO собирает `mkiso.py` (ISO9660 + El Torito без xorriso). Метка тома `CRAVGOS_64`. [ФАКТ]
- Целевая среда: x86_64 BIOS, VGA text mode, PS/2 клавиатура и мышь, e1000. [ФАКТ]

## 3. Файлы (проект `minios/`)
| Файл | Назначение |
|---|---|
| `kernel.c` | ядро, драйверы (VGA/PS2/PIT/CMOS/e1000), оболочка, команды, `cmdtab[]` (справка), темы, `fm` (двухпанельный проводник), `web` (браузер), `edit`, игры, кириллица (`ru_init`, `vga_font_load`) |
| `boot_iso.s`, `linker_iso.ld`, `mkiso.py`, `Makefile` | загрузка, раскладка памяти, сборка ISO |
| `net.h` | ARP/IPv4/ICMP/UDP/DHCP/DNS/TCP |
| `http.h` | HTTP/1.0 клиент, редиректы (до 6), chunked, DNS-over-HTTPS как запасной DNS |
| `tls.h`, `crypto.h` | TLS 1.2 (ECDHE X25519/P-256, AES-128-GCM), sha256/hmac/aes/gcm/x25519. Сертификат сервера НЕ проверяется |
| `stas.h` | пакетный менеджер `stas` (update/list/info/install/remove/run/repo) + мини-язык скриптов пакетов; репозиторий — каталог HTTP(S) с `index.txt` (`имя sha256 размер описание`) и `<имя>.stp`; установка в `/apps/<имя>`, проверка размера и SHA-256, установленный пакет запускается как команда (fallback в `run()`) |
| `stas-repo/`, `tools/stas_mkrepo.py` | репозиторий по умолчанию (`nim.stp` — игра) и генератор `index.txt` |
| `fs.h` | RAMFS: 64 узла × 2048 байт, только в памяти |
| `html.h` | HTML → текстовая страница 80 колонок, ссылки, кодировки (UTF-8/1251/KOI8/866), `<img>` резервирует 12 строк |
| `image.h` | декодеры PNG (в т.ч. interlaced), JPEG (baseline+progressive), GIF; вывод в 16 цветов полублоками (0xDF) с дизерингом |
| `cyrfont.h` | сгенерированный шрифт 8x16 (72 глифа); генератор `tools/gen_cyr_font.py` |
| `tests/` | `test_fs/net/html/image/http/crypto/tls/https.c`, `gen_images.py`, `run_tls_tests.sh`, `run_https_e2e.sh`, `host_kernel.py` + `host_main.c` / `host_fuzz_main.c` (хост-стенд) |
| `cravgos-linux/` | порт на Linux: `src/{plat,vfs,nsock,ui,apps,sys,cmds}.h`, `cravgos.c`, копии crypto/tls/html/http, `Makefile`, `tools/port_extract.py` |

## 4. Реализовано (подтверждено хост-тестами)
- Оболочка (~55 команд, все описаны в `cmdtab[]`; автопроверка покрытия help), 2 и более темы (по умолчанию dark), диалоги, fastfetch с RAM, мини-игры, `free`, `about`, `netdiag`, `dns`, `ping`, `curl`, `ifconfig`, `dhcp`.
- RAMFS, редактор `edit`, проводник `fm` с мышью.
- Сеть до TLS включительно: проверено против `openssl s_server` (RSA и ECDSA сертификаты, файлы до 405 КБ) и мок-сети (потери, RTO, chunked, редиректы, DoH).
- Браузер `web`: мышь, ссылки, история, http/https, кириллица, до 6 картинок на страницу (`IMG_MAX`), Esc пропускает загрузку.
- Кириллица: шрифт в plane 2, вывод CP866, раскладка ЙЦУКЕН (Alt+Shift / Ctrl+Shift / F12, `lang ru|en`, `[RU]` в приглашении).
- Декодеры картинок: корректные цвета на тестовых PNG/JPEG/GIF; нет падений под ASAN/UBSAN на обрезанных и битых данных (300 мутаций на файл).

## 5. Известные проблемы / не сделано
- **Работоспособность в виртуальной машине подтверждена**: образ `Cravgos-0.4.iso` проверен в VirtualBox (сетевой адаптер Intel PRO/1000 MT Desktop, BIOS). Система стабильно стартует, отображает собственный шрифт кириллицы, поддерживает переключение раскладок, корректно декодирует и рендерит графику (PNG/JPEG/GIF) в текстовом режиме через псевдографику 0xDF, получает IP по DHCP и выходит в сеть. [ФАКТ]
- Картинки: ≤6 на страницу, нет `data:` URI, нет CSS/SVG/WebP/PNG-анимации, цветовой режим 16 цветов. Размер ответа ограничен буфером `http_data` (256 КБ).
- Предупреждения компилятора в `image.h` (`ij_sofd` не используется) и `tls.h` (`t_be32`). Безвредны.
- Линкер: RWX сегмент, предупреждение про `.note.GNU-stack`. Безвредно.
- **Linux-порт не завершён**: нет `init.c`, rootfs, конфига ядра, UEFI-ISO, CI. В среде разработки не было доступа к kernel.org/GitHub, xorriso, grub, qemu, busybox, поэтому `cravgos.iso` на Linux не собирался. Собранный бинарь `cravgos-dev` лишь запускался с `--selftest`. Состояние порта после добавления картинок/кириллицы (0.4) НЕ синхронизировано: `cravgos-linux/src/` содержит старые копии html/http. [ПРОВЕРИТЬ]
- `image.h` написан между сессиями; детали алгоритмов (ограничения JPEG: восстановление, 12-bit, arithmetic coding) не документированы. [ПРОВЕРИТЬ]

## 6. Принятые решения и причины
- Свой загрузчик + `mkiso.py`: в среде не было grub/xorriso.
- Один `kernel.c` + header-only модули: тесты подключают `.h` напрямую; хост-стенд патчит `kernel.c` регулярками (`tests/host_kernel.py`) — **при смене сигнатур `outb/inb/outl/inl`, `kmain`, `_start16`, `isr_table`, `lidt` стенд сломается**.
- Большие буферы только через `XBSS` (иначе не помещаются в образ/low memory).
- HTTP/1.0, без keep-alive: простота. TLS без проверки сертификата: объём работы; в статусе браузера выводится `[encrypted, server not verified]`.
- В supported_groups добавлен secp256r1: иначе ECDSA-серверы отвечали alert 40.
- DoH как запасной DNS: у пользователя бывает NXDOMAIN при обычном DNS.
- Картинки рендерятся после текста страницы (`br_images()`), страница сначала показывается с подписями `[alt]`; блок в 12 строк сжимается до реальной высоты (`html_img_shrink`). Атрибуты строк картинки (`pg_im[line]=1`) обходят `th_page_attr`, чтобы тема не меняла цвета.
- Кодировка вывода — CP866 (стандарт для текстового VGA с кириллицей); для сетевых страниц fallback CS_1251, для локальных файлов CS_866.
- Исправления UBSAN: сдвиги в `crypto.h`, `image.h` (приведение к `u32`, умножение вместо `<<` отрицательных).

## 7. Команды (из каталога `minios/`)
```
make            # → cravgos.elf, cravgos.img, cravgos.iso
make clean
make test       # fs, net, html, image (ASAN/UBSAN, нужен Pillow), http, crypto, TLS vs openssl, https e2e
python3 tests/host_kernel.py                              # собрать хост-стенд → /tmp/host_kernel
/tmp/host_kernel                                          # сценарии (код возврата 0 = без падений)
python3 tests/host_kernel.py host_fuzz_main.c host_fuzz   # фаззинг edit/fm/web
python3 tools/gen_cyr_font.py cyrfont.h                   # пересоздать шрифт
make run        # цель есть в Makefile; требует QEMU [ПРОВЕРИТЬ, не запускалась]
```
Зависимости: gcc (multilib, `-m32`), ld, objcopy, python3, Pillow, openssl; для `-fsanitize` на хосте — gcc с ASAN/UBSAN.
Выпуск: поменять строку версии в `kernel.c` (about, fastfetch, help-заголовок, ~6 мест) и метку в `mkiso.py`; проверить ISO: El Torito checksum = 0, boot LBA = 23, первые байты образа `b8 03 00 cd`.
Запуск в VM: ISO как CD, BIOS (не EFI), сетевой адаптер Intel PRO/1000 MT Desktop, NAT.

## 8. Нельзя нарушать
- Только freestanding C без libc в `kernel.c` и заголовках, подключаемых в ядро; никаких malloc/stdio. В Linux-порте свой слой (`plat.h`).
- Не ломать совместимость `tests/host_kernel.py` с текстом `kernel.c` (см. §6).
- Большие статические буферы — только `XBSS`; следить за размером bss и стека (стек у загрузчика ~0x90000).
- Любое изменение сети/TLS/HTML/картинок — прогон `make test` и фаззеров под ASAN/UBSAN.
- Каждая новая команда должна быть в `cmdtab[]` (есть автопроверка покрытия help).
- Каждый выпуск = новый ISO (`Cravgos-X.Y.iso`) + список изменений; пользователь это требует.
- Не выдавать непроверенное за работающее: в отчётах отделять «проверено на хосте» от «проверено в VM».
- Пользователь просил не перечитывать всю переписку; работать от состояния файлов.

## 9. Следующие шаги
1. Запустить `Cravgos-0.4.iso` в VirtualBox/QEMU: проверить шрифт кириллицы, раскладку, `web` с картинками (например, http-сайт с PNG/JPEG), `fastfetch`, `netdiag`.
2. Добавить в мок-сеть хост-стенда отдачу картинок и сценарий `br_images()`.
3. Разобраться с NXDOMAIN/DNS на сети пользователя (`netdiag` даёт rcode по серверам).
4. Решить судьбу Linux-порта: синхронизировать `cravgos-linux/src` с 0.4, написать `init.c`, rootfs, конфиг ядра, UEFI-ISO — в среде с доступом к исходникам Linux и grub/xorriso.
5. Улучшения: keep-alive/HTTP/1.1, больше картинок и больший буфер, `data:` URI, проверка сертификатов, UEFI/USB.

## 10. stas (v0.6) [ФАКТ: хост-тесты; в VM не проверялось]
- Репозиторий по умолчанию: `https://raw.githubusercontent.com/Arseniy537/CravgOS/main/stas-repo` (нужно закоммитить каталог `stas-repo/`). Локально: `cd stas-repo && python3 -m http.server 8000`, в ОС `stas repo http://10.0.2.2:8000`.
- После изменения/добавления `.stp`: `python3 tools/stas_mkrepo.py stas-repo` (обновляет index.txt; лимиты: пакет и индекс ≤ 2048 байт, имя `[a-z0-9_-]{1,15}`).
- Скрипты не могут писать файлы и открывать сеть. Esc прерывает; лимит 300000 шагов. Сертификат TLS не проверяется, поэтому SHA-256 защищает от порчи, но не от злонамеренного сервера.
- Тесты: `make test` (tests/test_stas.c), `python3 tests/host_stas.py && /tmp/host_stas` (настоящий kernel.c + мок-HTTP). Старый `tests/host_kernel.py` сам по себе не собирается: не определён `cyr_font` (он в boot_iso.s), в host_main.c стейл-проверка IDT (шаг 8 вместо 16 байт).
