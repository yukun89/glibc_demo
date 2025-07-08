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
bool debug = false;

std::string trim(const std::string &str);
std::vector<std::string> split(const std::string& s, char delim);
void print_anon_rw_details();
void print_rss_summary_info();
void print_malloc_info_summary();
void print_heap_range();
void print_mem_info();

std::string trim(const std::string &str) {
    const std::string whitespace = " \t\n\r\f\v";
    size_t first = str.find_first_not_of(whitespace);
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(whitespace);
    return str.substr(first, last - first + 1);
}
std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> tokens;
    size_t start = 0, end = s.find(delim);
    while (end != std::string::npos) {
        tokens.push_back(s.substr(start, end-start));
        start = end + 1;
        end = s.find(delim, start);
    }
    tokens.push_back(s.substr(start));
    return tokens;
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
void print_anon_rw_details() {
	int rw_block_num=0;
	size_t size_kb = 0;
	size_t rss_kb = 0;
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
    }
	printf("Rss anon rw Summary: anon_rw_block_num=%d || anon_rw_size=%d KB || anon_rw_rss_size = %d kB\n", rw_block_num, size_kb, rss_kb);
}

void print_rss_summary_info() {
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
	printf("MEM Usage Summary: rss=%d KB || rss_anon=%d KB || rss_file = %d KB || other_rss = %d kb\n", rss_kb, rss_anon_kb, rss_file_kb,  rss_kb - rss_anon_kb - rss_file_kb);
	return ;
}


void print_malloc_info_summary() {
    //int mmap_threshold = mallopt(M_MMAP_THRESHOLD, 0);
	//printf("Current mmap threshold: %d bytes\n", );
	const auto &m= mallinfo();
	printf("mallocTotalInfo: arena(non-mmapped space allocated from sys)=%d kB|| uordblks(total allocated space)=%d kB|| fordblks(total free space)=%d kB || keepcost=%d kB\n",m.arena/1024,m.uordblks/1024, m.fordblks/1024, m.keepcost/1024);
	printf("mmInfo: hblks(number of mmapped regions)=%d || hblkhd(space in mmapped regions)=%d kB\n",m.hblks, m.hblkhd/1024);
	printf("chunkInfo: ordblks(number of free chunks)=%d|| smblks(number of fastbin blocks)=%d ||usmblks(maximum total allocated space)=%d kB|| fsmblks(space available in free fastbin blocks) = %d kB\n", m.ordblks, m.smblks, m.usmblks/1024, m.fsmblks/1024);
	malloc_info(0, stdout);
	malloc_stats();
}


void print_heap_range() {
	size_t start=0;
	size_t end = 0;
    FILE *fp = fopen("/proc/self/maps", "r");
    if (!fp) {
        perror("Failed to open /proc/self/maps");
        return;
    }
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "[heap]")) {
            unsigned long start, end;
            sscanf(line, "%lx-%lx", &start, &end);
            printf("Heap Start: 0x%lx, Heap End: 0x%lx, size=%lx KB\n", start, end, (end-start)/1024.0);
            break;
        }
    }
    fclose(fp);
}

void print_mem_info() {
	print_rss_summary_info();
	print_anon_rw_details();
	print_heap_range();
	print_malloc_info_summary();
	return ;
}
