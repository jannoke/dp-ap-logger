################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/includes/ConfigFile/ConfigFile.cpp 

OBJS += \
./src/includes/ConfigFile/ConfigFile.o 

CPP_DEPS += \
./src/includes/ConfigFile/ConfigFile.d 


# Each subdirectory must supply rules for building sources it contributes
src/includes/ConfigFile/%.o: ../src/includes/ConfigFile/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -I/usr/local/include/mysql++ -I/usr/include/mysql -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


