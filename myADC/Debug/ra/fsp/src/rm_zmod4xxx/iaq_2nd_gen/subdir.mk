################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../ra/fsp/src/rm_zmod4xxx/iaq_2nd_gen/rm_zmod4410_iaq_2nd_gen.c 

C_DEPS += \
./ra/fsp/src/rm_zmod4xxx/iaq_2nd_gen/rm_zmod4410_iaq_2nd_gen.d 

CREF += \
myADC.cref 

OBJS += \
./ra/fsp/src/rm_zmod4xxx/iaq_2nd_gen/rm_zmod4410_iaq_2nd_gen.o 

MAP += \
myADC.map 


# Each subdirectory must supply rules for building sources it contributes
ra/fsp/src/rm_zmod4xxx/iaq_2nd_gen/%.o: ../ra/fsp/src/rm_zmod4xxx/iaq_2nd_gen/%.c
	@echo 'Building file: $<'
	$(file > $@.in,-mcpu=cortex-m33 -mthumb -mlittle-endian -mfloat-abi=hard -mfpu=fpv5-sp-d16 -Os -ffunction-sections -fdata-sections -fno-strict-aliasing -fmessage-length=0 -funsigned-char -Wunused -Wuninitialized -Wall -Wextra -Wmissing-declarations -Wconversion -Wpointer-arith -Wshadow -Waggregate-return -Wno-parentheses-equality -Wfloat-equal -g3 -std=c99 -fshort-enums -fno-unroll-loops -I"C:\\Users\\PC Hai Yen\\Downloads\\New folder\\myADC\\ra_gen" -I"." -I"C:\\Users\\PC Hai Yen\\Downloads\\New folder\\myADC\\ra_cfg\\fsp_cfg\\bsp" -I"C:\\Users\\PC Hai Yen\\Downloads\\New folder\\myADC\\ra_cfg\\fsp_cfg" -I"C:\\Users\\PC Hai Yen\\Downloads\\New folder\\myADC\\src" -I"C:\\Users\\PC Hai Yen\\Downloads\\New folder\\myADC\\ra\\fsp\\inc" -I"C:\\Users\\PC Hai Yen\\Downloads\\New folder\\myADC\\ra\\fsp\\inc\\api" -I"C:\\Users\\PC Hai Yen\\Downloads\\New folder\\myADC\\ra\\fsp\\inc\\instances" -I"C:\\Users\\PC Hai Yen\\Downloads\\New folder\\myADC\\ra\\arm\\CMSIS_6\\CMSIS\\Core\\Include" -D_RENESAS_RA_ -D_RA_CORE=CM33 -D_RA_ORDINAL=1 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -x c "$<" -c -o "$@")
	@clang --target=arm-none-eabi @"$@.in"

