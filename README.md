# OnlyFiles: Simple Inode-Based Filesystem in C

This project implements a block-based inode filesystem called **OnlyFiles** entirely contained within a 
single 10MB virtual disk file. It is a simplified filesystem supporting basic file operations without 
directories, inspired conceptually by ext4.

---

## Features

- **Virtual Disk:** 10MB file split into 2560 blocks of 4KB each.  
- **Disk Layout:**  
  - Block 0: Superblock (filesystem metadata)  
  - Block 1: Block bitmap (tracks free/used blocks)  
  - Blocks 2-9: Inode table (256 inodes, each 128 bytes)  
  - Blocks 10-2559: Data blocks for file content  
- **Files:**  
  - Maximum 256 files (inodes)  
  - File names up to 28 characters  
  - Max file size 48KB (12 direct blocks x 4KB each)  
- **File Operations:**  
  - Format virtual disk (`fs_format`)  
  - Mount and unmount filesystem (`fs_mount`, `fs_unmount`)  
  - Create and delete files (`fs_create`, `fs_delete`)  
  - Read and write file data (`fs_read`, `fs_write`)  
  - List files in the filesystem (`fs_list`)  
- **Low-Level I/O:** Uses `open`, `lseek`, `read`, `write`, and `close` syscalls directly (no stdio or mmap).  
- **No Dynamic Allocation:** All metadata stored statically in memory buffers.  
- **Error Handling:** Returns clear error codes and prints to `stderr` on failure.

---

## Project Structure

- `fs.h` — Filesystem API and constants (do not modify structs)  
- `fs.c` — Core implementation of OnlyFiles filesystem  
- `build.sh` — Reference build script to compile project  
- `run.sh` — Reference run script to execute compiled program  
- `main.c` — Example usage flow (can be replaced by tester’s main)  
- `testfilesystem.c` — Provided tests (do not submit)  

---

## Compilation & Execution

1. Build the project using:  
   ```bash
   ./build.sh

This compiles all sources with gcc13 on Ubuntu 24.04 using C17 standard.

2. Run the program using:

```bash
   ./run.sh
```
---
