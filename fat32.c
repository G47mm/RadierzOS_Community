#include "fat32.h"

struct __attribute__((packed)) fat32_bpb {
    uint8_t  jmp[3];
    uint8_t  oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sector_count;
    uint8_t  table_count;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t table_size_16;
    uint16_t sectors_per_track;
    uint16_t head_count;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint32_t table_size_32;
    uint16_t extended_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info_sector;
    uint16_t backup_boot_sector;
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_signature;
    uint32_t volume_id;
    uint8_t  volume_label[11];
    uint8_t  fs_type[8];
};

struct __attribute__((packed)) fat32_directory_entry {
    uint8_t  name[11];
    uint8_t  attributes;
    uint8_t  reserved;
    uint8_t  creation_time_tenth;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t first_cluster_high;
    uint16_t last_write_time;
    uint16_t last_write_date;
    uint16_t first_cluster_low;
    uint32_t file_size;
};

static uint32_t g_fat_start_lba = 0;
static uint32_t g_cluster_begin_lba = 0;
static uint32_t g_sectors_per_cluster = 0;
static uint32_t g_root_cluster = 0;
static uint32_t g_total_sectors_per_fat = 0;

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void insw(uint16_t port, void* addr, uint32_t count) {
    __asm__ volatile ("cld; rep insw" : "+D"(addr), "+c"(count) : "d"(port) : "memory");
}

static bool ata_read_sectors(uint32_t lba, uint32_t count, uint8_t* buf) {
    for (uint32_t i = 0; i < count; i++) {
        uint32_t current_lba = lba + i;
        outb(0x1F6, 0xE0 | ((current_lba >> 24) & 0x0F));
        outb(0x1F2, 1);
        outb(0x1F3, (uint8_t) current_lba);
        outb(0x1F4, (uint8_t)(current_lba >> 8));
        outb(0x1F5, (uint8_t)(current_lba >> 16));
        outb(0x1F7, 0x20);

        while (1) {
            uint8_t status = inb(0x1F7);
            if (!(status & 0x80) && (status & 0x08)) break;
            if (status & 0x01) return false;
        }

        insw(0x1F0, buf + (i * 512), 256);
    }
    return true;
}

static uint32_t get_fat_entry(uint32_t cluster) {
    uint8_t sector_buf[512];
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = g_fat_start_lba + (fat_offset / 512);
    uint32_t ent_offset = fat_offset % 512;

    if (!ata_read_sectors(fat_sector, 1, sector_buf)) {
        return 0x0FFFFFFF;
    }

    uint32_t table_value = *(uint32_t*)&sector_buf[ent_offset];
    return table_value & 0x0FFFFFFF;
}

static uint32_t cluster_to_lba(uint32_t cluster) {
    return g_cluster_begin_lba + ((cluster - 2) * g_sectors_per_cluster);
}

int fat32_init(void) {
    uint8_t boot_sector[512];
    if (!ata_read_sectors(0, 1, boot_sector)) return -1;

    struct fat32_bpb* bpb = (struct fat32_bpb*)boot_sector;
    if (bpb->bytes_per_sector != 512 || bpb->table_count == 0) return -2;

    g_sectors_per_cluster = bpb->sectors_per_cluster;
    g_fat_start_lba = bpb->reserved_sector_count;
    g_total_sectors_per_fat = bpb->table_size_32;
    g_cluster_begin_lba = g_fat_start_lba + (bpb->table_count * g_total_sectors_per_fat);
    g_root_cluster = bpb->root_cluster;

    return 0;
}

void fat32_list_directory(void (*print_fn)(const char*)) {
    uint8_t cluster_buf[4096];
    uint32_t sectors_to_read = g_sectors_per_cluster > 8 ? 8 : g_sectors_per_cluster;

    if (!ata_read_sectors(cluster_to_lba(g_root_cluster), sectors_to_read, cluster_buf)) {
        print_fn("FAT32: Failed to read root directory\n");
        return;
    }

    struct fat32_directory_entry* dir = (struct fat32_directory_entry*)cluster_buf;
    int entry_count = (g_sectors_per_cluster * 512) / sizeof(struct fat32_directory_entry);

    for (int i = 0; i < entry_count; i++) {
        if (dir[i].name[0] == 0x00) break;
        if (dir[i].name[0] == 0xE5 || dir[i].attributes == 0x0F || (dir[i].attributes & 0x08)) continue;

        char name_buf[13];
        int idx = 0;
        for (int j = 0; j < 8; j++) {
            if (dir[i].name[j] == ' ') break;
            name_buf[idx++] = dir[i].name[j];
        }
        if (dir[i].name[8] != ' ') {
            name_buf[idx++] = '.';
            for (int j = 8; j < 11; j++) {
                if (dir[i].name[j] == ' ') break;
                name_buf[idx++] = dir[i].name[j];
            }
        }
        name_buf[idx] = '\0';

        print_fn("  ");
        print_fn(name_buf);
        print_fn("\n");
    }
}

bool fat32_read_file(const char* filename, char* out_buf, uint32_t max_len, uint32_t* out_size) {
    uint8_t cluster_buf[4096];
    uint32_t sectors_to_read = g_sectors_per_cluster > 8 ? 8 : g_sectors_per_cluster;

    if (!ata_read_sectors(cluster_to_lba(g_root_cluster), sectors_to_read, cluster_buf)) return false;

    struct fat32_directory_entry* dir = (struct fat32_directory_entry*)cluster_buf;
    int entry_count = (g_sectors_per_cluster * 512) / sizeof(struct fat32_directory_entry);
    struct fat32_directory_entry* target_file = 0;

    for (int i = 0; i < entry_count; i++) {
        if (dir[i].name[0] == 0x00) break;
        if (dir[i].name[0] == 0xE5 || dir[i].attributes == 0x0F) continue;

        char name_buf[13];
        int idx = 0;
        for (int j = 0; j < 8; j++) {
            if (dir[i].name[j] == ' ') break;
            name_buf[idx++] = dir[i].name[j];
        }
        if (dir[i].name[8] != ' ') {
            name_buf[idx++] = '.';
            for (int j = 8; j < 11; j++) {
                if (dir[i].name[j] == ' ') break;
                name_buf[idx++] = dir[i].name[j];
            }
        }
        name_buf[idx] = '\0';

        bool match = true;
        for (int k = 0; ; k++) {
            char c1 = filename[k];
            char c2 = name_buf[k];
            if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
            if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
            if (c1 != c2) { match = false; break; }
            if (c1 == '\0') break;
        }

        if (match) {
            target_file = &dir[i];
            break;
        }
    }

    if (!target_file) return false;

    uint32_t file_cluster = ((uint32_t)target_file->first_cluster_high << 16) | target_file->first_cluster_low;
    uint32_t file_size = target_file->file_size;
    if (out_size) *out_size = file_size;

    uint32_t bytes_read = 0;
    uint8_t sector_buffer[512];

    while (file_cluster < 0x0FFFFFF8 && file_cluster > 0 && bytes_read < max_len) {
        uint32_t lba = cluster_to_lba(file_cluster);
        for (uint32_t s = 0; s < g_sectors_per_cluster && bytes_read < file_size && bytes_read < max_len; s++) {
            if (!ata_read_sectors(lba + s, 1, sector_buffer)) break;
            uint32_t chunk = 512;
            if (bytes_read + chunk > file_size) chunk = file_size - bytes_read;
            if (bytes_read + chunk > max_len) chunk = max_len - bytes_read;

            for (uint32_t b = 0; b < chunk; b++) {
                out_buf[bytes_read++] = sector_buffer[b];
            }
        }
        file_cluster = get_fat_entry(file_cluster);
    }

    if (bytes_read < max_len) out_buf[bytes_read] = '\0';
    else out_buf[max_len - 1] = '\0';

    return true;
}
