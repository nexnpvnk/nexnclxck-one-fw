# nexnclxck-one-fw

Инициализация платы, CMSIS-драйверы и аппаратные контракты: [BSP](bsp/README.md).
Измерения BME280: [команды bme280, watch/stop и драйвер](apps/app_bme280/README.md).

Прошивка STM32F051x8 использует CMSIS и прямую работу с регистрами, без HAL/LL.
`Core` и [cmake/platform/CMakeLists.txt](cmake/platform/CMakeLists.txt)
поддерживаются вручную. CMake target `platform` задаёт device headers,
определения MCU и платформенные исходники; функции инициализации имеют
имена `platform_*_init`.
Файл [nexnclxck-one-fw.ioc](nexnclxck-one-fw.ioc) сохранён для наглядного
просмотра распиновки и настроек периферии; рабочие исходники поддерживаются вручную.
FlashDB не подключён; linker script сохраняет 64 КиБ Flash и 8 КиБ RAM.

Переход не проверен сборкой, тестами или на плате. Размер и временные
характеристики прошивки после перехода пока не измерены.

cmake --version

ninja --version


arm-none-eabi-gcc --version

arm-none-eabi-gdb --version


openocd --version


clangd --version

clang-format --version
