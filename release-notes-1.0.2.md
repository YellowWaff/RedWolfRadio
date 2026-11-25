# Red Wolf Radio 1.0.2

Prepared November 25, 2026. This compatibility update adds **WRSR 1.2.0.9**, retaining the **1.1.1.13** and **1.1.1.9** profiles. Requires Windows x64 and the verified original **Republic Mod Loader 1.0.1** runtime.

The game update moved music-engine state and native UI code and fields. Radio now selects the engine path-resolver prefix, loading/source-voice locations, native player and Script/HUD addresses, UI globals and object layout for the exact executable/engine pair. Unknown pairs and unverified loader runtimes still decline to activate.

Music formats, INI keys/defaults, cache behavior and controls are unchanged. Game files and source music remain preserved.

The full build and automated suites passed for all three profiles. On 1.2.0.9, thirteen hooks and their integrity checks passed; the user confirmed normal music, volume, dragging, shortcuts and a natural track transition. GUI controls, normal exit and relaunch were verified, including roughly 37m50s and 10m17s sessions. The final runtime had no ERROR entries or audit issues; the trace had no dropped events. Personal INI and RML settings remained unchanged.

Space Program DLC was installed during testing; Radio does not require it. Older profiles have automated coverage for this update and were not rerun live. Fresh live coverage of every encoding and optional HUD/Russian notification settings is not claimed. See [acceptance details](https://github.com/YellowWaff/RedWolfRadio/blob/main/TESTING-1.0.2.md) and [validation/publication records](VALIDATION.md).

Tested DLL SHA-256: `a35636a944693f5cb30cef8fc1e93f15139ad7c0c25a4cb95610b318f05b0adb`.

Before updating, back up your customized INI and plugin-local music outside Steam/game folders. Check settings afterward and restore with the game closed if needed. Enable only one Radio copy.

## Русский

Обновление подготовлено 25 ноября 2026 года. Версия добавляет совместимость с **WRSR 1.2.0.9**, сохраняя профили **1.1.1.13** и **1.1.1.9**. Требуются Windows x64 и проверенная библиотека оригинального **Republic Mod Loader 1.0.1**.

Обновление игры изменило расположение состояния музыкального движка, кода интерфейса и полей его объектов. Для точной пары файлов игры и движка Radio выбирает префикс функции пути к музыке, состояние загрузки и источник воспроизведения, адреса плеера, вызова сценария и HUD, глобальные объекты и структуру интерфейса. На неизвестных парах файлов или непроверенной библиотеке загрузчика плагин не активируется.

Форматы музыки, ключи и стандартные значения INI, кэш и управление не изменились. Файлы игры и исходная музыка сохраняются.

Полная сборка и автоматические проверки всех трёх профилей прошли успешно. В 1.2.0.9 установлены тринадцать перехватчиков, все проверки целостности пройдены. Пользователь подтвердил нормальное звучание музыки, громкость, перемещение окна, сочетания клавиш и естественную смену трека. Проверены кнопки плеера, штатный выход и повторный запуск, включая сеансы примерно по 37 мин 50 с и 10 мин 17 с. В итоговом журнале нет записей ERROR и замечаний аудита, в трассировке — потерянных событий. Личный INI и настройки RML сохранились.

При проверке установлено дополнение Space Program; для Radio оно не требуется. Старые профили в этом обновлении проверены автоматически, но повторно в игре не проверялись. Повторная проверка в игре каждого формата, необязательного уведомления HUD и русских подписей не заявляется. См. [подробности проверки](https://github.com/YellowWaff/RedWolfRadio/blob/main/TESTING-1.0.2.md) и [записи проверки и публикации](VALIDATION.md).

Хеш SHA-256 проверенной DLL: `a35636a944693f5cb30cef8fc1e93f15139ad7c0c25a4cb95610b318f05b0adb`.

Перед обновлением сохраните копию своего INI и локальной музыки плагина за пределами папок Steam и игры. Затем проверьте настройки; при необходимости закройте игру и восстановите копию. Включайте только одну копию Radio.
