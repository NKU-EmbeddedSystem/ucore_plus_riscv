ARCH_INLUCDES 	:= debug driver libs mm sync trap
ARCH_CFLAGS 	:= -mcmodel=medany -std=gnu99 -Wno-unused -fno-builtin -Wall -O2 -nostdinc
ARCH_CFLAGS 	+= -fno-stack-protector -ffunction-sections -fdata-sections
ARCH_LDFLAGS 	:= -m elf64lriscv -nostdlib
