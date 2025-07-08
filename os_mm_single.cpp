#include <iostream>
#include <vector>
#include <cstdlib>
#include <unistd.h>
#include <cstring>
#include <malloc.h>
#include <cstdio>
#include <fstream>
#include <string>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
using namespace std;
// 长生命周期内存持有（模拟缓存等长期对象）


void long_lived_allocation() {
    std::cout << "Thread A: 5s后申请1B大小的空间" << std::endl;
    sleep(1);
    char* ptr = (char *)malloc(1024 * 1024); // 1MB
    std::cout << "Thread A: 分配并长期持有 1MB 内存. 120s后释放" << std::endl;
    for (int i=0; i<=1000000; i++) {
	*(ptr+i)=i;
    }
    sleep(10); // 持有120秒
    free(ptr);
    std::cout << "Thread A: 释放 1MB 内存" << std::endl;
}

int tn = 0;
int loop = 100;
int unit_block = 64 * 1024;
int interval = 1;

// 短生命周期频繁分配/释放（模拟临时请求）
void short_lived_allocation(int P) {

    std::vector<void *> ptrs;
    std::vector<int> blk_list;
	long total_size = 0;
	long free_size = 0;
    for (int i = 0; i < loop; i++) {
		int malloc_block = unit_block;
		int t = i%20;
		if (i%2==0) {
			malloc_block = 8;
		}
		total_size += malloc_block;
        void* ptr = malloc(malloc_block);
        // 模拟内存使用
        memset(ptr, 0, malloc_block);
        //usleep(2000); // 20ms间隔
        ptrs.push_back(ptr);
		blk_list.push_back(malloc_block);
    }
	int actrual_free_times = 0;
    for (int i = 0; i < loop; i++) {
	//	if (i%20 != 0) {
			free(ptrs[i]);
			actrual_free_times ++;
			free_size += blk_list[i];
	//	}
	}
    printf("Thread B%d. malloc.times=%d || unit_block=%dKB || malloc.size=%.2fMB || free.times=%d || free.size=%d MB\n", P, ptrs.size(), unit_block/1024, total_size/1024/1024.0, actrual_free_times, free_size/1024.0/1024.0);
}


/*
7f224aa24000-7f224ac23000 ---p 001c4000 08:11 2155200692                 /usr/lib64/libc-2.17.so
Size:               2044 kB
KernelPageSize:        4 kB
MMUPageSize:           4 kB
Rss:                   0 kB
...
VmFlags: rd ex
*/
void get_anon_details(int &rw_block_num, size_t &size_kb, size_t &rss_kb) {
//7fec98f70000-7fec98f71000 rw-p 00000000 00:00 0
    std::ifstream maps("/proc/self/smaps");
    std::string line;
	bool is_header_line=true;
	bool is_anon_rw = true;
    while (std::getline(maps, line)) {
        if (line.find("-") != std::string::npos) {
			is_header_line = true;
			std::vector<std::string> fields = split(line, ' ');
			int inode_no = strtol(fields[4].c_str(), nullptr, 10);
			if (inode_no == 0 && line.find("rw") != std::string::npos) {
				is_anon_rw = true;
				rw_block_num ++;
				if (debug) {
					std::cout << "anon rw line:" << line << std::endl;
				}
			} else {
				is_anon_rw = false;
			}
		} else {
			if (!is_anon_rw) {
				continue;
			}
			is_header_line = false;
			size_t comma_pos = line.find(":");
			if (comma_pos != std::string::npos) {
				std::string prefix = line.substr(0, comma_pos);
				std::string value_kb = trim(line.substr(comma_pos+1));
				if (prefix == "Size") {
					size_kb += strtol(value_kb.c_str(), NULL, 10);
					if (debug) {
						std::cout << line << std::endl;
					}
				} else if (prefix == "Rss") {
					rss_kb += strtol(value_kb.c_str(), NULL, 10);
					if (debug) {
						std::cout << line << std::endl;
					}
				} else {
					if (debug) {
						//std::cout << prefix << std::endl;
					}
					//do nothing
				}
			}
		}
		/*
		std::vector<std::string> fields = split(line, ' ');
		auto &address_start_end = fields[0];
		std::vector<std::string> start_end = split(line, '-');
		size_t start = strtol(start_end[0].c_str(), NULL, 16);
		size_t end = strtol(start_end[1].c_str(), NULL, 16);
		size_t size = end - start;
        if (line.find("rw") != std::string::npos) {
			rw_block_num ++;
			rw_size += size;
		} else {
			other_block_num ++;
			other_size += size;
		}
		*/
    }
}

void *malloc_start=nullptr;

void print_mem_size() {
    pthread_attr_t attr;
    pthread_getattr_np(pthread_self(), &attr); // 获取当前线程属性
    size_t stack_size;
    pthread_attr_getstacksize(&attr, &stack_size);

	void* malloc_end = malloc(1);
    size_t malloc_range= (size_t)malloc_end - (size_t)malloc_start;

	long rss_kb = -1;
	long rss_anon_kb = -1;
	long rss_file_kb = -1;
    std::ifstream status("/proc/self/status");
    if (status.is_open()) {
        std::string line;
        while (std::getline(status, line)) {
            size_t kB_pos = line.find(" kB");
            if (kB_pos == std::string::npos) {
				continue;
			}
            size_t comma_pos = line.find(":");
            if (comma_pos == std::string::npos) {
				continue;
			}
			std::string rss_prefix="VmRSS";
			std::string rss_anon_prefix="RssAnon";
			std::string rss_file_prefix = "RssFile";

			std::string prefix = line.substr(0, comma_pos);
			std::string value = line.substr(comma_pos + 1, kB_pos - comma_pos);
            if (prefix == rss_prefix) {
                // 提取数值并转换为字节
                rss_kb = std::stol(value);
            } else if (prefix == rss_anon_prefix) {
                // 提取数值并转换为字节
                rss_anon_kb = std::stol(value);
            } else if (prefix == rss_file_prefix) {
                // 提取数值并转换为字节
                rss_file_kb = std::stol(value);
            }
        }
        status.close();
    }
	int rw_block_num=0;
	size_t size_kb=0;
	size_t rss_size_kb=0;
	get_anon_details(rw_block_num, size_kb, rss_size_kb);
	printf("MEM Usage Summary: rss=%d KB || rss_anon=%d KB || rss_file = %d KB || other_rss = %d kb || anon_rw_block_num=%d || anon_rw_size=%d KB || anon_rw_rss_size = %d kB\n", rss_kb, rss_anon_kb, rss_file_kb,  rss_kb - rss_anon_kb - rss_file_kb, rw_block_num, size_kb, rss_size_kb);
	printf("malloc Summary: malloc_range=%d || malloc_start=%p || malloc_end=%p \n", malloc_range, malloc_start, malloc_end);
	return ;
}

void print_malloc_info_summary() {
	const auto &m= mallinfo();
	printf("mallocInfo: arena(non-mmapped space allocated from system)=%d kB|| ordblks(number of free chunks)=%d|| smblks(number of fastbin blocks)=%d || hblks(number of mmapped regions)=%d || hblkhd(space in mmapped regions)=%d kB|| usmblks(maximum total allocated space)=%d kB|| fsmblks(space available in freed fastbin blocks) = %d kB|| uordblks(total allocated space)=%d kB|| fordblks(total free space)=%d kB || keepcost(top-most, releasable (via malloc_trim) space))=%d kB\n",
	m.arena/1024, m.ordblks, m.smblks, m.hblks, m.hblkhd/1024, m.usmblks/1024, m.fsmblks/1024, m.uordblks/1024, m.fordblks/1024, m.keepcost/1024);
}


void print_heap_range() {
    FILE *fp = fopen("/proc/self/maps", "r");
    if (!fp) {
        perror("Failed to open /proc/self/maps");
        return;
    }
	char *sbrk_ptr = (char*)sbrk(0);
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "[heap]")) {
            unsigned long start, end;
            sscanf(line, "%lx-%lx", &start, &end);
            printf("Heap Start: 0x%lx, Heap End: 0x%lx, size=0x%lx, sbrk=0x%lx\n", start, end, end-start, sbrk_ptr);
            break;
        }
    }
    fclose(fp);
}

int main() {
    std::cout << "========main start:" << std::endl;
	malloc_start = malloc(1);
	print_heap_range();
	tn = 0;
	loop = 1000;
	unit_block = 64 * 1024;
	interval = 2;
	std::string label = "1234567";

    std::cout << "========Start:" << std::endl;
	print_heap_range();
	print_mem_size();
	/*
	print_mem_size();
	print_malloc_info_summary();
    malloc_info(0, stdout);
    malloc_stats();
	*/



	malloc_trim(0);
	printf("========Do malloc trim End:\n");
	print_heap_range();
	print_mem_size();
	/*
	print_mem_size();
	print_malloc_info_summary();
    malloc_info(0, stdout);
    malloc_stats();
	*/
    sleep(1000);
    return 0;
}
