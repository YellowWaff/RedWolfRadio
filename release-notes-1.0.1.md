# Red Wolf Radio 1.0.1

Prepared November 11, 2026. **The full build, automated suites and initial in-game acceptance on WRSR 1.1.1.13 pass.**

WRSR 1.1.1.13 changed the game and engine binaries, so Radio 1.0.0 correctly declined its unverified build. This compatibility update adds an exact profile for 1.1.1.13 and retains the existing 1.1.1.9 profile. Both require the verified Republic Mod Loader 1.0.1 runtime on Windows x64.

- Select the correct addresses for the native player, Script-dialog bridge and optional HUD for each supported game build.
- Retain the inspected engine music API, UI globals and object layouts; music formats and playback behavior are unchanged.
- Log the actual executable, engine and RML runtime SHA-256 hashes. Unsupported game/engine pairs and an unverified loader runtime receive separate messages; unknown binaries still decline to activate.
- Preserve original game files. Music, cache, shortcut and display configuration keys and defaults are unchanged.

Automated coverage includes both profiles' native popup and private continuations, standalone HUD fallback, exact Script call destinations, and rejection of mixed or unknown game/engine pairs. DLL SHA-256: `8ed39562a622d7676df434f48fd2674972662857d72bd47a26c1b88f0b8d2cc2`.

In the initial isolated 1.1.1.13 test, the user confirmed audible custom music, player dragging, all player buttons and keyboard shortcuts. Live source-title updates, navigation while paused and normal shutdown were also checked. The session lasted 6 minutes 51 seconds, with thirteen hooks and no hook failures or runtime errors. Relaunch with the usual plugin selections also activated Radio, played a custom track at the main menu and exited normally; unrelated plugin errors remain. The older 1.1.1.9 profile was not rerun live.

Before updating, back up your customized `RedWolfRadio.ini` outside Steam and game folders. Check your settings after the download; with the game closed, restore the backup if Steam replaced the file.

## Русский

Обновление подготовлено 11 ноября 2026 года. **Полная сборка, все автоматические проверки и первичная проверка в игре WRSR 1.1.1.13 прошли успешно.**

В WRSR 1.1.1.13 изменились файлы игры и движка, поэтому Radio 1.0.0 отказалась активироваться на непроверенной сборке. Обновление добавляет точный профиль для 1.1.1.13 и сохраняет профиль 1.1.1.9. Для обоих требуется проверенная библиотека Republic Mod Loader 1.0.1 на Windows x64.

- Для каждой поддерживаемой сборки выбираются соответствующие адреса окна плеера, диалога сценария и необязательного уведомления.
- Сохраняются проверенные функции музыкального движка, глобальные объекты интерфейса и структура его объектов. Форматы музыки и поведение воспроизведения не меняются.
- В журнал записываются фактические хеши SHA-256 игры, движка и библиотеки RML. Сообщения отдельно указывают неподдерживаемую пару файлов игры и движка или непроверенную библиотеку загрузчика. На неизвестных файлах плагин по-прежнему не активируется.
- Оригинальные файлы игры не изменяются. Ключи и значения по умолчанию для настроек музыки, кэша, сочетаний клавиш и отображения остаются прежними.

Автоматические проверки охватывают диалог плеера и собственные продолжения перехваченных функций в обоих профилях, отдельный перехватчик уведомления, точные адреса вызовов сценария и отказ для смешанных или неизвестных пар файлов игры и движка. Хеш SHA-256 библиотеки: `8ed39562a622d7676df434f48fd2674972662857d72bd47a26c1b88f0b8d2cc2`.

При первичной изолированной проверке на 1.1.1.13 пользователь подтвердил слышимое воспроизведение своей музыки, перемещение плеера, работу всех его кнопок и сочетаний клавиш. Также проверены обновление названия трека, переключение треков во время паузы и штатный выход. Сеанс длился 6 минут 51 секунду; установлены тринадцать перехватчиков без сбоев, ошибок времени выполнения не зафиксировано. При повторном запуске с обычным набором плагинов Radio также активировалась, запустила пользовательский трек в главном меню и завершилась штатно; ошибки других плагинов сохраняются. Профиль 1.1.1.9 повторно в игре не проверялся.

Перед обновлением сохраните копию своего `RedWolfRadio.ini` за пределами папок Steam и игры. После загрузки проверьте настройки; если Steam заменил файл, закройте игру и восстановите резервную копию.
