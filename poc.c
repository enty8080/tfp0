#include <stdio.h>
#include <mach/mach.h>

mach_port_t tfp0 = MACH_PORT_NULL;

void get_tfp0() {
    task_dyld_info_data_t task_dyld_info;
    mach_msg_type_number_t count = TASK_DYLD_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_DYLD_INFO, (task_info_t)&task_dyld_info, &count) != KERN_SUCCESS) {
        printf("Failed to get task info\n");
        return;
    }
    uint64_t all_image_info_addr = task_dyld_info.all_image_info_addr;
    while (all_image_info_addr != 0) {
        struct mach_header_64 header;
        mach_vm_size_t size = 0;
        if (mach_vm_read_overwrite(mach_task_self(), all_image_info_addr, sizeof(header), (mach_vm_address_t)&header, &size) != KERN_SUCCESS || size != sizeof(header)) {
            printf("Failed to read mach header\n");
            return;
        }
        if (header.magic != MH_MAGIC_64) {
            printf("Invalid mach header magic number\n");
            return;
        }
        if (header.filetype != MH_EXECUTE) {
            all_image_info_addr = header.nextcmd;
            continue;
        }
        if (header.ncmds == 0 || header.cmdsize == 0) {
            printf("Invalid load command count or size\n");
            return;
        }
        uint64_t cmd_addr = all_image_info_addr + sizeof(header);
        for (uint32_t i = 0; i < header.ncmds; i++) {
            struct load_command cmd;
            if (mach_vm_read_overwrite(mach_task_self(), cmd_addr, sizeof(cmd), (mach_vm_address_t)&cmd, &size) != KERN_SUCCESS || size != sizeof(cmd)) {
                printf("Failed to read load command\n");
                return;
            }
            if (cmd.cmd == LC_SEGMENT_64) {
                struct segment_command_64 seg_cmd;
                if (mach_vm_read_overwrite(mach_task_self(), cmd_addr, sizeof(seg_cmd), (mach_vm_address_t)&seg_cmd, &size) != KERN_SUCCESS || size != sizeof(seg_cmd)) {
                    printf("Failed to read segment command\n");
                    return;
                }
                if (strcmp(seg_cmd.segname, "__TEXT_EXEC") == 0) {
                    tfp0 = seg_cmd.vmaddr;
                    printf("Got tfp0: 0x%llx\n", tfp0);
                    return;
                }
            }
            cmd_addr += cmd.cmdsize;
        }
        all_image_info_addr = header.nextcmd;
    }
    printf("Failed to get tfp0\n");
}

int main(int argc, const char * argv[]) {
    get_tfp0();
    return 0;
}
