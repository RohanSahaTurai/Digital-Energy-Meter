################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Sources/Calc.c \
../Sources/Events.c \
../Sources/FIFO.c \
../Sources/FTM.c \
../Sources/FixedPoint.c \
../Sources/Flash.c \
../Sources/HMI.c \
../Sources/LEDs.c \
../Sources/PIT.c \
../Sources/RTC.c \
../Sources/Switch.c \
../Sources/UART.c \
../Sources/main.c \
../Sources/packet.c 

OBJS += \
./Sources/Calc.o \
./Sources/Events.o \
./Sources/FIFO.o \
./Sources/FTM.o \
./Sources/FixedPoint.o \
./Sources/Flash.o \
./Sources/HMI.o \
./Sources/LEDs.o \
./Sources/PIT.o \
./Sources/RTC.o \
./Sources/Switch.o \
./Sources/UART.o \
./Sources/main.o \
./Sources/packet.o 

C_DEPS += \
./Sources/Calc.d \
./Sources/Events.d \
./Sources/FIFO.d \
./Sources/FTM.d \
./Sources/FixedPoint.d \
./Sources/Flash.d \
./Sources/HMI.d \
./Sources/LEDs.d \
./Sources/PIT.d \
./Sources/RTC.d \
./Sources/Switch.d \
./Sources/UART.d \
./Sources/main.d \
./Sources/packet.d 


# Each subdirectory must supply rules for building sources it contributes
Sources/%.o: ../Sources/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross ARM C Compiler'
	arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -O0 -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections  -g3 -I"C:\Users\13093012\Desktop\02\Library" -I"C:/Users/13093012/Desktop/02/Static_Code/IO_Map" -I"C:/Users/13093012/Desktop/02/Sources" -I"C:/Users/13093012/Desktop/02/Generated_Code" -I"C:/Users/13093012/Desktop/02/Static_Code/PDD" -std=c99 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


