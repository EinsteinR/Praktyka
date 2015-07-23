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
nowy.lss \

FLASH_IMAGE += \
nowy.hex \

EEPROM_IMAGE += \
nowy.eep \

SIZEDUMMY += \
sizedummy \


# All Target
all: nowy.elf secondary-outputs

# Tool invocations
nowy.elf: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: AVR C Linker'
	avr-gcc -Wl,-Map,nowy.map -mmcu=atmega32 -o "nowy.elf" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '

nowy.lss: nowy.elf
	@echo 'Invoking: AVR Create Extended Listing'
	-avr-objdump -h -S nowy.elf  >"nowy.lss"
	@echo 'Finished building: $@'
	@echo ' '

nowy.hex: nowy.elf
	@echo 'Create Flash image (ihex format)'
	-avr-objcopy -R .eeprom -O ihex nowy.elf  "nowy.hex"
	@echo 'Finished building: $@'
	@echo ' '

nowy.eep: nowy.elf
	@echo 'Create eeprom image (ihex format)'
	-avr-objcopy -j .eeprom --no-change-warnings --change-section-lma .eeprom=0 -O ihex nowy.elf  "nowy.eep"
	@echo 'Finished building: $@'
	@echo ' '

sizedummy: nowy.elf
	@echo 'Invoking: Print Size'
	-avr-size --format=avr --mcu=atmega32 nowy.elf
	@echo 'Finished building: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(FLASH_IMAGE)$(ELFS)$(OBJS)$(ASM_DEPS)$(EEPROM_IMAGE)$(S_DEPS)$(SIZEDUMMY)$(S_UPPER_DEPS)$(LSS)$(C_DEPS) nowy.elf
	-@echo ' '

secondary-outputs: $(LSS) $(FLASH_IMAGE) $(EEPROM_IMAGE) $(SIZEDUMMY)

.PHONY: all clean dependents
.SECONDARY:

-include ../makefile.targets
