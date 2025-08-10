#include "fs.h"

#define INODE_SIZE sizeof(inode)
#define SUPERBLOCK_BLOCK 0
#define BITMAP_BLOCK 1
#define INODE_TABLE_START_BLOCK 2
#define INODE_TABLE_BLOCKS 8
#define DATA_BLOCK_START 10 // Blocks 0 to 9 are reserved (superblock, bitmap, inode table).
#define DISK_SIZE (MAX_BLOCKS * BLOCK_SIZE)  // 2560 blocks * 4096 bytes = 10MB

static int fs_fd = -1; // File descriptor of mounted disk.
superblock sb; // Cached superblock.
static unsigned char bitmap[BLOCK_SIZE];           // In-memory copy of the bitmap
inode inode_table[MAX_FILES];               // In-memory inode table

int fs_format(const char* disk_path) {
    // null check before calling open().
    if (disk_path == NULL) {
    fprintf(stderr, "Invalid disk path (NULL)\n");
    return -1;
    }

    // Open or create the disk file (overwrite if it exists as instructed).
    int fd = open(disk_path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("Failed to open disk file");
        return -1;
    }

    // Setting the file size to 10MB.
    // We use lseek to move the cursor to the last byte (index 10,485,759), then write a single null byte to force the OS to allocate all space up to that point.
    if(lseek(fd,DISK_SIZE - 1, SEEK_SET) == -1 || write(fd, "", 1) != 1) {
        perror("Failed to allocate disk space");
        close(fd);
        return -1;
    }

    // Initializing and writing the superblock to block 0.
    superblock sb;
    sb.total_blocks = MAX_BLOCKS;
    sb.block_size = BLOCK_SIZE;
    sb.free_blocks = MAX_BLOCKS - DATA_BLOCK_START;
    sb.total_inodes = MAX_FILES;
    sb.free_inodes = MAX_FILES;

    // Write the superblock to block 0 by seeking to its offset and writing the struct.
    if (lseek(fd, SUPERBLOCK_BLOCK * BLOCK_SIZE, SEEK_SET) ==  -1 || write(fd ,&sb, sizeof(superblock)) != sizeof(superblock)) {
        perror("Failed to write superblock");
        close(fd);
        return -1;
    }
    
    // Initializing and writing the bitmap to block 1.
    unsigned char bitmap[BLOCK_SIZE] = {0};

    // Blocks 0-9 (metadata) will be marked as used.
    for (int i = 0; i < DATA_BLOCK_START; i++) {
        bitmap[i / 8] |= (1 << (i % 8));
    }

    // Write the block bitmap to block 1 (tracks used/free blocks, 1 bit per block).
    if (lseek(fd, BITMAP_BLOCK * BLOCK_SIZE, SEEK_SET) ==  -1 || write(fd ,bitmap, BLOCK_SIZE) != BLOCK_SIZE) {
        perror("Failed to write bitmap");
        close(fd);
        return -1;
    }

    // Zero out 256 inodes in blocks 2-9.
    inode empty_inode = {0};
    if (lseek(fd, INODE_TABLE_START_BLOCK * BLOCK_SIZE, SEEK_SET) == -1) {
        perror("Failed to seek to inode table");
        close(fd);
        return -1;
    }

    for (int i = 0; i < MAX_FILES; i++) {
        if (write(fd, &empty_inode, INODE_SIZE) != INODE_SIZE) {
            perror("Failed to write empty inode");
            close(fd);
            return -1;
        }
    }

    // Closing the disk and returning success value.
    close(fd);
    return 0;
}

int fs_mount(const char* disk_path) {
    // null check before calling open().
    if (disk_path == NULL) {
    fprintf(stderr, "Invalid disk path (NULL)\n");
    return -1;
    }

    // Opening the disk for reading and writing.
    fs_fd = open(disk_path, O_RDWR);
    if (fs_fd < 0) {
        perror("Failed to open disk file");
        return -1;
    }

    // Opening succedded - now we read the superblock which is block 0.
    if (lseek(fs_fd, 0, SEEK_SET) == -1) {
        perror("Failed to seek to superblock");
        close(fs_fd);
        return -1;
    }

    if (read(fs_fd, &sb, sizeof(superblock)) != sizeof(superblock)) {
        perror("Failed to read superblock");
        close(fs_fd);
        fs_fd = -1;
        return -1;
    }

    // Validation for superblock fields.
    if (sb.total_blocks != MAX_BLOCKS || sb.block_size != BLOCK_SIZE || sb.total_inodes != MAX_FILES) {
        fprintf(stderr, "Invalid superblock format\n");
        close(fs_fd);
        fs_fd = -1;
        return -1;
    }

    // Bitmap loading: block 1.
    if (lseek(fs_fd, BITMAP_BLOCK * BLOCK_SIZE, SEEK_SET) == -1) {
        perror("Failed to seek to bitmap block");
        close(fs_fd);
        fs_fd = -1;
        return -1;
    }

    if (read(fs_fd, bitmap, BLOCK_SIZE) != BLOCK_SIZE) {
        perror("Failed to read bitmap");
        close(fs_fd);
        fs_fd = -1;
        return -1;
    }

    // Inode table loading: blocks 2-9.
     if (lseek(fs_fd, INODE_TABLE_START_BLOCK * BLOCK_SIZE, SEEK_SET) == -1) {
        perror("Failed to seek to inode table");
        close(fs_fd);
        fs_fd = -1;
        return -1;
    }

    for (int i = 0; i < MAX_FILES; i++) {
        if (read(fs_fd, &inode_table[i], INODE_SIZE) != INODE_SIZE) {
            perror("Failed to read inode");
            close(fs_fd);
            fs_fd = -1;
            return -1;
        }
    }

    // If we reached here, all of the above succedded -> mount is successful.
    return 0;
}

void fs_unmount() {
    if (fs_fd >= 0) { // Means it's a valid open file.
        // Flush superblock to disk (block 0).
        if (lseek(fs_fd, SUPERBLOCK_BLOCK * BLOCK_SIZE, SEEK_SET) != -1) {
            write(fs_fd, &sb, sizeof(superblock));
        }

        // Flush bitmap to disk (block 1).
        if (lseek(fs_fd, BITMAP_BLOCK * BLOCK_SIZE, SEEK_SET) != -1) {
            write(fs_fd, bitmap, BLOCK_SIZE);
        }

        // Flush the inode to disk (blocks 2-9).
        if (lseek(fs_fd, INODE_TABLE_START_BLOCK * BLOCK_SIZE, SEEK_SET) != -1) {
            for (int i = 0; i < MAX_FILES; i++){
                write(fs_fd, &inode_table[i], INODE_SIZE);
            }
        }
    }

    // Close the disk file and reset descriptor.
    close(fs_fd);
    fs_fd = -1;
}

int fs_create(const char* filename) { 
    // Ensure that the filesystem is mounted before proceeding.
    if (fs_fd < 0) {
    fprintf(stderr, "Filesystem not mounted\n");
    return -3;
    }

    // Input validation.
    if (filename == NULL || strlen(filename) == 0 || strlen(filename)>= MAX_FILENAME) {
        fprintf(stderr, "Invalid filename\n");
        return -3;
    }

    // Check for existing file with same name as the input.
    for (int i = 0; i < MAX_FILES; i++) {
        if (inode_table[i].used && strncmp(inode_table[i].name, filename, MAX_FILENAME) == 0) {
            return -1; // File already exists.
        }
    }

    // Search for a free inode.
    int free_index = -1;
    for (int i = 0; i < MAX_FILES; i++) {
        if (!inode_table[i].used) {
            free_index = i;
            break;
        }
    }

    // Case that no free inode is availbale.
    if (free_index == -1) {
        return -2; 
    }

    // If we reached here there is a free inode and there isn't an existing file with the same name -> we ititalize inode.
    inode_table[free_index].used = 1;
    strncpy(inode_table[free_index].name, filename, MAX_FILENAME);
    inode_table[free_index].name[MAX_FILENAME - 1] = '\0'; // Guarantees null-termination. 
    inode_table[free_index].size = 0; // Sets the file size to 0 when it is first created.
    // Fills the array of block indices (blocks[]) with -1, indicating that no data blocks are currently assigned to this file.
    for (int j = 0; j < MAX_DIRECT_BLOCKS; j++) {
        inode_table[free_index].blocks[j] = -1;
    }
    
    // Update the superblock.
    sb.free_inodes--;
    // Return suceess value as instructed.
    return 0;
}

int fs_delete(const char* filename) { 
    // Ensure that the filesystem is mounted before proceeding.
    if (fs_fd < 0) {
    fprintf(stderr, "Filesystem not mounted\n");
    return -2;
    }

    // Input validation.
    if (filename == NULL || strlen(filename) == 0 || strlen(filename)>= MAX_FILENAME) {
        fprintf(stderr, "Invalid filename\n");
        return -2;
    }
    
    // Search for the file index by name.
    int file_index = -1;
    for (int i = 0; i < MAX_FILES; i++) {
        if (inode_table[i].used && strncmp(inode_table[i].name, filename, MAX_FILENAME) == 0) {
            file_index = i;
            break;
        }
    }

    // Case there is no file with the given name.
    if (file_index == -1) {
        return -1;
    }

    // If we reached here we can delete the file as needed, starting with freeing used blocks.
    for (int j = 0; j < MAX_DIRECT_BLOCKS; j++) {
        int block = inode_table[file_index].blocks[j];
        if (block != -1) {
            // Mark the block as free by clearing its bit in the bitmap.
            // Only the bit for this block is affected, others remain unchanged.
            bitmap[block / 8] &= ~(1 << (block % 8));
            sb.free_blocks++;
        }
    }
    // Marking the inode as free.
    inode_table[file_index].used = 0;
    sb.free_inodes++;
    // Return suceess value as instructed.
    return 0; 
}

int fs_list(char filenames[][MAX_FILENAME], int max_files) {
    // Ensure that the filesystem is mounted before proceeding.
    if (fs_fd < 0) {
    fprintf(stderr, "Filesystem not mounted\n");
    return -1;
    }

    // Input validation.
    if (filenames == NULL || max_files <= 0) {
        fprintf(stderr, "Invalid input to fs_list\n");
        return -1;
    }

    int count = 0;
    // Scan the inode table for used inodes (existing files).
    for (int i = 0; i < MAX_FILES; i++) {
        // Checks if the current inode is used.
        if (inode_table[i].used) {
            // Copy the filename to the output array.
            strncpy(filenames[count], inode_table[i].name, MAX_FILENAME);
            filenames[count][MAX_FILENAME - 1] = '\0'; // Guarantees null-termination. 
            count++;
            // Check regarding the caller's limit.
            if (count == max_files) {
                break;
            }
        }
    }
    // Returns the number of files found.
    return count;
}

int fs_write(const char* filename, const void* data, int size) {
    // Ensure that the filesystem is mounted before proceeding.
    if (fs_fd < 0) {
    fprintf(stderr, "Filesystem not mounted\n");
    return -3;
    }

    // Input validation.
    if (filename == NULL || data == NULL || size < 0 || strlen(filename) >= MAX_FILENAME) {
        fprintf(stderr, "Invalid arguments to fs_write\n");
        return -3;
    }

    // Locate the inode for the given filename.
    int file_index = -1;
    for (int i = 0; i < MAX_FILES; i++) {
        if (inode_table[i].used && strncmp(inode_table[i].name, filename, MAX_FILENAME) == 0) {
            file_index = i;
            break;
        }
    }

    // Case that the file does not exist.
    if (file_index == -1) {
        return -1;
    }
   
    // Create a shorthand pointer to the inode of the target file for cleaner and more readable access.
    inode* node = &inode_table[file_index];

    // Calculate the amount of blocks needed.
    int required_blocks = (size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    if (required_blocks > MAX_DIRECT_BLOCKS || required_blocks > sb.free_blocks) {
        return -2; // Not enough free blocks.
    }

    // Free old blocks.
    for (int i = 0; i < MAX_DIRECT_BLOCKS; i++) {
        int block = node->blocks[i];
        if (block != -1) {
            // Mark the block as free by clearing its bit in the bitmap.
            // Only the bit for this block is affected, others remain unchanged. simillar trick in fs_delete.
            bitmap[block / 8] &= ~(1 << (block % 8));
            sb.free_blocks++;
            node->blocks[i] = -1;
        }
    }

    // From this point new blocks allocation can be done.
    int allocated_blocks = 0;
    for (int i = DATA_BLOCK_START; i < MAX_BLOCKS && allocated_blocks < required_blocks; i++) {
        if((bitmap[i / 8] & (1 << (i % 8))) == 0) {
            // We found a free block.
            bitmap[i / 8] |= (1 << (i % 8)); // Marking the block as used.
            node->blocks[allocated_blocks] = i;
            sb.free_blocks--;
            allocated_blocks++;
        }
    }

    // Writing the data into the relevant blocks.
    const char* data_pointer = (const char*)data; // ensures we can access arbitrary byte offsets of the memory buffer.
    for (int i = 0; i < required_blocks; i++) {
        int block_index = node->blocks[i];
        // Seek to the location on disk.
        if (lseek(fs_fd, block_index * BLOCK_SIZE, SEEK_SET) == -1) {
            perror("Failed seeking to data block");
            return -3;
        }
        // Compute amount of data to be written.
        int chunk_size = (i == required_blocks - 1) ? (size - i * BLOCK_SIZE) : BLOCK_SIZE; // For most blocks we write exactly BLOCK_SIZE bytes, for the last block, the data may be less than a full block.
        if (write(fs_fd, data_pointer + i * BLOCK_SIZE, chunk_size) != chunk_size) {
            perror("Failed to write to block");
            return -3;
        }
    }
    
    // Finally, we will update the inode metadata and return successfully.
    node->size = size;
    return 0;
}

int fs_read(const char* filename, void* buffer, int size) { 
    // Ensure that the filesystem is mounted before proceeding.
    if (fs_fd < 0) {
    fprintf(stderr, "Filesystem not mounted\n");
    return -3;
    }

    // Input validation.
    if (filename == NULL || buffer == NULL || size < 0 || strlen(filename) >= MAX_FILENAME) {
        fprintf(stderr, "Invalid arguments to fs_read\n");
        return -3;
    }

    // Locate the inode for the given filename.
    int file_index = -1;
    for (int i = 0; i < MAX_FILES; i++) {
        if (inode_table[i].used && strncmp(inode_table[i].name, filename, MAX_FILENAME) == 0) {
            file_index = i;
            break;
        }
    }

    // Case that the file does not exist.
    if (file_index == -1) {
        return -1;
    }
   
    // Create a shorthand pointer to the inode of the target file for cleaner and more readable access.
    inode* node = &inode_table[file_index];
    int bytes_to_read = (size < node->size) ? size : node -> size; // we take the minimum of the two: we can't read more than what was written, and we can't overflow the buffer.
    char* out_pointer = (char*)buffer; // Casts the void* buffer to a char* so we can do byte-wise pointer arithmetic.

    // We'll now read the data from the allocated blocks.
    int read_counter = 0;
    for (int i = 0; i < MAX_DIRECT_BLOCKS && read_counter < bytes_to_read; i++) {
        int block_index = node->blocks[i];
        if (block_index == -1) {
            break; // No more blocks.
        }
        // Seek to the location on disk.
        if (lseek(fs_fd, block_index * BLOCK_SIZE, SEEK_SET) == -1) {
            perror("Failed seeking during read");
            return -3;
        }
        // Compute amount of data to be written.
        int chunk_size = (bytes_to_read - read_counter > BLOCK_SIZE) ? BLOCK_SIZE : (bytes_to_read - read_counter); // For most blocks we reade exactly BLOCK_SIZE bytes, for the last block, the data may be less than a full block.
        if (read(fs_fd, out_pointer + read_counter, chunk_size) != chunk_size) { // Reads chunk bytes from the current block and stores it in the buffer at the correct offset.
            perror("Failed to read from block");
            return -3;
        }
        read_counter += chunk_size; // Increment our read count so we know how much of the buffer we’ve filled.
    }
    return read_counter;
 }
