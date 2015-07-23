################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include I2C_hw/subdir.mk
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
TWIcommunication.lss \

FLASH_IMAGE += \
TWIcommunication.hex \

EEPROM_IMAGE += \
TWIcommunication.eep \

SIZEDUMMY += \
sizedummy \


# All Target
all: TWIcommunication.elf secondary-outputs

# Tool invocations
TWIcommunication.elf: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: AVR C Linker'
	avr-gcc -Wl,-Map,TWIcommunication.map -mmcu=atmega32 -o "TWIcommunication.elf" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '

TWIcommunication.lss: TWIcommunication.elf
	@echo 'Invoking: AVR Create Extended Listing'
	-avr-objdump -h -S TWIcommunication.elf  >"TWIcommunication.lss"
	@echo 'Finished building: $@'
	@echo ' '

TWIcommunication.hex: TWIcommunication.elf
	@echo 'Create Flash image (ihex format)'
	-avr-objcopy -R .eeprom -O ihex TWIcommunication.elf  "TWIcommunication.hex"
	@echo 'Finished building: $@'
	@echo ' '

TWIcommunication.eep: TWIcommunication.elf
	@echo 'Create eeprom image (ihex format)'
	-avr-objcopy -j .eeprom --no-change-warnings --change-section-lma .eeprom=0 -O ihex TWIcommunication.elf  "TWIcommunication.eep"
	@echo 'Finished building: $@'
	@echo ' '

sizedummy: TWIcommunication.elf
	@echo 'Invoking: Print Size'
	-avr-size --format=avr --mcu=atmega32 TWIcommunication.elf
	@echo 'Finished building: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(FLASH_IMAGE)$(ELFS)$(OBJS)$(ASM_DEPS)$(EEPROM_IMAGE)$(S_DEPS)$(SIZEDUMMY)$(S_UPPER_DEPS)$(LSS)$(C_DEPS) TWIcommunication.elf
	-@echo ' '

secondary-outputs: $(LSS) $(FLASH_IMAGE) $(EEPROM_IMAGE) $(SIZEDUMMY)

.PHONY: all clean dependents
.SECONDARY:

-include ../makefile.targets
