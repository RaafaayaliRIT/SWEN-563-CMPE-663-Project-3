################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/LED.c \
../Core/Src/SysClock.c \
../Core/Src/gpio.c \
../Core/Src/keyboard.c \
../Core/Src/main.c \
../Core/Src/phase_tests.c \
../Core/Src/race.c \
../Core/Src/race_display.c \
../Core/Src/servo.c \
../Core/Src/stm32l4xx_hal_gpio.c \
../Core/Src/stm32l4xx_it.c \
../Core/Src/syscalls.c \
../Core/Src/sysmem.c \
../Core/Src/system_stm32l4xx.c \
../Core/Src/timebase.c \
../Core/Src/timer.c \
../Core/Src/uart.c \
../Core/Src/ultrasonic.c 

OBJS += \
./Core/Src/LED.o \
./Core/Src/SysClock.o \
./Core/Src/gpio.o \
./Core/Src/keyboard.o \
./Core/Src/main.o \
./Core/Src/phase_tests.o \
./Core/Src/race.o \
./Core/Src/race_display.o \
./Core/Src/servo.o \
./Core/Src/stm32l4xx_hal_gpio.o \
./Core/Src/stm32l4xx_it.o \
./Core/Src/syscalls.o \
./Core/Src/sysmem.o \
./Core/Src/system_stm32l4xx.o \
./Core/Src/timebase.o \
./Core/Src/timer.o \
./Core/Src/uart.o \
./Core/Src/ultrasonic.o 

C_DEPS += \
./Core/Src/LED.d \
./Core/Src/SysClock.d \
./Core/Src/gpio.d \
./Core/Src/keyboard.d \
./Core/Src/main.d \
./Core/Src/phase_tests.d \
./Core/Src/race.d \
./Core/Src/race_display.d \
./Core/Src/servo.d \
./Core/Src/stm32l4xx_hal_gpio.d \
./Core/Src/stm32l4xx_it.d \
./Core/Src/syscalls.d \
./Core/Src/sysmem.d \
./Core/Src/system_stm32l4xx.d \
./Core/Src/timebase.d \
./Core/Src/timer.d \
./Core/Src/uart.d \
./Core/Src/ultrasonic.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_FULL_LL_DRIVER -DHSE_VALUE=8000000 -DHSE_STARTUP_TIMEOUT=100 -DLSE_STARTUP_TIMEOUT=5000 -DLSE_VALUE=32768 -DMSI_VALUE=4000000 -DEXTERNALSAI1_CLOCK_VALUE=2097000 -DEXTERNALSAI2_CLOCK_VALUE=2097000 -DHSI_VALUE=16000000 -DLSI_VALUE=32000 -DVDD_VALUE=3300 -DPREFETCH_ENABLE=1 -DINSTRUCTION_CACHE_ENABLE=1 -DDATA_CACHE_ENABLE=1 -DSTM32L476xx -c -I../Core/Inc -I../Drivers/STM32L4xx_HAL_Driver/Inc -I../Drivers/CMSIS/Device/ST/STM32L4xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src

clean-Core-2f-Src:
	-$(RM) ./Core/Src/LED.cyclo ./Core/Src/LED.d ./Core/Src/LED.o ./Core/Src/LED.su ./Core/Src/SysClock.cyclo ./Core/Src/SysClock.d ./Core/Src/SysClock.o ./Core/Src/SysClock.su ./Core/Src/gpio.cyclo ./Core/Src/gpio.d ./Core/Src/gpio.o ./Core/Src/gpio.su ./Core/Src/keyboard.cyclo ./Core/Src/keyboard.d ./Core/Src/keyboard.o ./Core/Src/keyboard.su ./Core/Src/main.cyclo ./Core/Src/main.d ./Core/Src/main.o ./Core/Src/main.su ./Core/Src/phase_tests.cyclo ./Core/Src/phase_tests.d ./Core/Src/phase_tests.o ./Core/Src/phase_tests.su ./Core/Src/race.cyclo ./Core/Src/race.d ./Core/Src/race.o ./Core/Src/race.su ./Core/Src/race_display.cyclo ./Core/Src/race_display.d ./Core/Src/race_display.o ./Core/Src/race_display.su ./Core/Src/servo.cyclo ./Core/Src/servo.d ./Core/Src/servo.o ./Core/Src/servo.su ./Core/Src/stm32l4xx_hal_gpio.cyclo ./Core/Src/stm32l4xx_hal_gpio.d ./Core/Src/stm32l4xx_hal_gpio.o ./Core/Src/stm32l4xx_hal_gpio.su ./Core/Src/stm32l4xx_it.cyclo ./Core/Src/stm32l4xx_it.d ./Core/Src/stm32l4xx_it.o ./Core/Src/stm32l4xx_it.su ./Core/Src/syscalls.cyclo ./Core/Src/syscalls.d ./Core/Src/syscalls.o ./Core/Src/syscalls.su ./Core/Src/sysmem.cyclo ./Core/Src/sysmem.d ./Core/Src/sysmem.o ./Core/Src/sysmem.su ./Core/Src/system_stm32l4xx.cyclo ./Core/Src/system_stm32l4xx.d ./Core/Src/system_stm32l4xx.o ./Core/Src/system_stm32l4xx.su ./Core/Src/timebase.cyclo ./Core/Src/timebase.d ./Core/Src/timebase.o ./Core/Src/timebase.su ./Core/Src/timer.cyclo ./Core/Src/timer.d ./Core/Src/timer.o ./Core/Src/timer.su ./Core/Src/uart.cyclo ./Core/Src/uart.d ./Core/Src/uart.o ./Core/Src/uart.su ./Core/Src/ultrasonic.cyclo ./Core/Src/ultrasonic.d ./Core/Src/ultrasonic.o ./Core/Src/ultrasonic.su

.PHONY: clean-Core-2f-Src

