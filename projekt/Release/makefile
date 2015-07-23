################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(ASM_DEPS)),)
-include $(ASM_DEPS)
endif
ifneq ($(strip $(S_DEPS)),)
-include $(S_DEPS)
endif
ifneq ($(strip $(S_UPPER_DEPS)),)
-include $(S_UPPER_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 
LSS += \
projekt.lss \

FLASH_IMAGE += \
projekt.hex \

EEPROM_IMAGE += \
projekt.eep \

SIZEDUMMY += \
sizedummy \


# All Target
all: projekt.elf secondary-outputs

# Tool invocations
projekt.elf: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: AVR C Linker'
	avr-gcc -Wl,-Map,projekt.map -mmcu=atmega8 -o "projekt.elf" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '

projekt.lss: projekt.elf
	@echo 'Invoking: AVR Create Extended Listing'
	-avr-objdump -h -S projekt.elf  >"projekt.lss"
	@echo 'Finished building: $@'
	@echo ' '

projekt.hex: projekt.elf
	@echo 'Create Flash image (ihex format)'
	-avr-objcopy -R .eeprom -R .fuse -R .lock -R .signature -O ihex projekt.elf  "projekt.hex"
	@echo 'Finished building: $@'
	@echo ' '

projekt.eep: projekt.elf
	@echo 'Create eeprom image (ihex format)'
	-avr-objcopy -j .eeprom --no-change-warnings --change-section-lma .eeprom=0 -O ihex projekt.elf  "projekt.eep"
	@echo 'Finished building: $@'
	@echo ' '

sizedummy: projekt.elf
	@echo 'Invoking: Print Size'
	-avr-size --format=avr --mcu=atmega8 projekt.elf
	@echo 'Finished building: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(FLASH_IMAGE)$(ELFS)$(OBJS)$(ASM_DEPS)$(EEPROM_IMAGE)$(S_DEPS)$(SIZEDUMMY)$(S_UPPER_DEPS)$(LSS)$(C_DEPS) projekt.elf
	-@echo ' '

secondary-outputs: $(LSS) $(FLASH_IMAGE) $(EEPROM_IMAGE) $(SIZEDUMMY)

.PHONY: all clean dependents
.SECONDARY:

-include ../makefile.targets
