#include "stdio.h"
#include "stdlib.h"
#include "pci.h"
#include "sys/io.h"

#define PRIVILEGE_LVL 3

#define NUMBER_OF_BUSES 256
#define NUMBER_OF_DEVICES 32
#define NUMBER_OF_FUNCS 8

#define CONFIG_ADDRESS 0xCF8 // outl
#define CONFIG_DATA 0xCFC    // inl

#define SIGNIFICANT_BIT_SHIFT 31
#define BUS_SHIFT 16
#define DEVICE_SHIFT 11
#define FUNC_SHIFT 8
#define REG_NUM_SHIFT 2

#define REG_ID_OFFSET 0
#define REG_HEADER_OFFSET 3 
#define HEADER_OFFSET 16

#define DEVICE_ID_SHIFT 16

#define REG_BUS_NUMBER_OFFSET 6 
#define PRIMARY_NUMBER_OFFSET 0
#define SECONDARY_NUMBER_OFFSET 8
#define SUBORDINATE_NUMBER_OFFSET 16

#define REG_MEMBL_OFFSET 8 
#define MEMORY_BASE_OFFSET 0
#define MEMORY_LIMIT_OFFSET 16

#define MASK_BYTE 0xFF
#define MASK_WORD 0xFFFF

#define PCI_VEN_TABLE_LEN (sizeof(PciVenTable) / sizeof(PCI_VENTABLE))
#define PCI_DEV_TABLE_LEN (sizeof(PciDevTable) / sizeof(PCI_DEVTABLE))

#define BANNED_DEVICE_ID -1

#define UNUSED "unused register"

char privilegeLVLIsOK();
int formConfigRegAddress(int bus, int device, int func, int reg);
int extractRegisterData(int configRegAddress);
int extractVendorID(int registerData);
char *extractVendorName(int vendorID);
int extractDeviceID(int registerData);
char *extractDeviceName(int deviceID, int vendorID);
char funcIsOK(int confIDReg);
void iterateTThrough();
void printGeneral(int bus, int device, int func, int confIDReg);
char funcIsBridge(int bus, int device, int func);
void printBaseIOReg(int bus, int device, int func);
void printBusNumber(int bus, int device, int func);
void printMemBaseLim(int bus, int device, int func);


int main() {
    if (!privilegeLVLIsOK()) {
        printf("Run under sudo!\n");
        return 1;
    } 

    iterateTThrough();

    return 0;
}

char privilegeLVLIsOK() {
    return !iopl(PRIVILEGE_LVL);
}

int formConfigRegAddress(int bus, int device, int func, int reg) {
    int configRegAddress = (1 << SIGNIFICANT_BIT_SHIFT) | (bus << BUS_SHIFT) | (device << DEVICE_SHIFT) | (func << FUNC_SHIFT) | (reg << REG_NUM_SHIFT);
    return configRegAddress;
}

int extractRegisterData(int configRegAddress) {
    outl(configRegAddress, CONFIG_ADDRESS);
    return inl(CONFIG_DATA);
}

int extractVendorID(int registerData) {
    return registerData & MASK_WORD;
}

char *extractVendorName(int vendorID) {
    for (register int i = 0; i < PCI_VEN_TABLE_LEN; ++i) {
        if (PciVenTable[i].VendorId == vendorID) {
            return PciVenTable[i].VendorName;
        }
    }
    return NULL;
}

int extractDeviceID(int registerData) {
    return (registerData >> DEVICE_ID_SHIFT) & MASK_WORD;
}

char *extractDeviceName(int deviceID, int vendorID) {
    for (register int i = 0; i < PCI_DEV_TABLE_LEN; ++i) {
        if (PciDevTable[i].DeviceId == deviceID && PciDevTable[i].VendorId == vendorID) {
            return PciDevTable[i].DeviceName;
        }
    }
    return NULL;
}

char funcIsOK(int confIDReg) {
    return confIDReg != -1; 
}

void iterateTThrough() {
    for (int bus = 0; bus < NUMBER_OF_BUSES; ++bus) {
        for (int device = 0; device < NUMBER_OF_DEVICES; ++device) {
            for (int func = 0; func < NUMBER_OF_FUNCS; ++func) {
                int configIDRegAddress = formConfigRegAddress(bus, device, func, REG_ID_OFFSET);
                int configIDReg = extractRegisterData(configIDRegAddress);

                if (funcIsOK(configIDReg)) {
                    printGeneral(bus, device, func, configIDReg);
                    if (funcIsBridge(bus, device, func)) {
                        printBusNumber(bus, device, func);
                        printMemBaseLim(bus, device, func);
                    } else {
                        printBaseIOReg(bus, device, func);
                    }
                    putc('\n', stdout);
                }
            }
        }
    }
}

void printGeneral(int bus, int device, int func, int confIDReg) {
    int vendorID = extractVendorID(confIDReg);
    int deviceID = extractDeviceID(confIDReg);

    char *vendorName = extractVendorName(vendorID);
    char *deviceName = extractDeviceName(deviceID, vendorID);

    printf("ADDRESS<bus, device, func>: %d %d %d\nVendor ID: %X\nDevice ID: %x\nVendor name: %s\nDevice name: %s\n", bus, device, func, vendorID, deviceID, vendorName, deviceName);
}

char funcIsBridge(int bus, int device, int func) {
    int headerRegAddress = formConfigRegAddress(bus, device, func, REG_HEADER_OFFSET);
    int reg = extractRegisterData(headerRegAddress);
    int header = (reg >> 16) & MASK_BYTE;
    return header & 1;
}

void printBaseIOReg(int bus, int device, int func) {
    printf("BASE IO REGISTER :: ");
    for (int offset = 5; offset < 11; ++offset) {
        int ioRegAddress = formConfigRegAddress(bus, device, func, offset);
        int ioReg = extractRegisterData(ioRegAddress);
        char *sep = (offset == 10 ? "\n" : ", ");
        if (ioReg) {
            printf("%u%s", ioReg, sep);
        } else {
            printf("%s%s", UNUSED, sep);
        }
    }
}

void printBusNumber(int bus, int device, int func) {
    int busNumRegAddress = formConfigRegAddress(bus, device, func, REG_BUS_NUMBER_OFFSET);
    int busNumReg = extractRegisterData(busNumRegAddress);

    printf("BUS NUMBERS :: ");
    if (busNumReg) {
        int primary = (busNumReg >> PRIMARY_NUMBER_OFFSET) & MASK_BYTE;
        int secondry = (busNumReg >> SECONDARY_NUMBER_OFFSET) & MASK_BYTE;
        int subordinate = (busNumReg >> SUBORDINATE_NUMBER_OFFSET) & MASK_BYTE;
        printf("Primary: %d; Secondary: %d; Subordinate: %d\n", primary, secondry, subordinate);
    } else {
        printf(UNUSED);
    }
}

void printMemBaseLim(int bus, int device, int func) {
    int memBLRegAddress = formConfigRegAddress(bus, device, func, REG_MEMBL_OFFSET);
    int memBLReg = extractRegisterData(memBLRegAddress);

    printf("MEMORY BASE && LIMIT :: ");
    if (memBLReg) {
        int base = (memBLReg >> MEMORY_BASE_OFFSET) & MASK_WORD;
        int limit = (memBLReg >> MEMORY_LIMIT_OFFSET) & MASK_WORD;
        printf("Base: %d; Limit: %d\n", base, limit);
    } else {
        printf(UNUSED);
    }
}