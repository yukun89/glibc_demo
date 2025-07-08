#include "ppp.hpp"
#include <iostream>
#include <cstdio>
#include <vector>
#include <thread>

// 全局指针记录当前分配的内存
std::vector<void*> big_ptrs;
std::vector<void*> small_ptrs;
std::vector<void*> all_ptrs;

void print_mem_info(); // 假设已实现

int main() {
    char a, fragment;
    long start,  end;
    int step;

    while (true) {
        printf(">>>>>>>>type X to start:0-print, m-malloc, M-malloc&set, f-free, q-quit...");
        scanf(" %c", &a); // 空格忽略换行符
		getchar();

        if (a == '0') {
			printf("\t\t\t\t====MEM Summary====\n");
            print_mem_info();
        } else if (a == 'm' || a == 'M') {
            printf(">>>>please input:start,end,step,frag...");
            scanf(" %d,%d,%d,%d", &start, &end, &step, &fragment);
			getchar();
            printf("start=%d||end=%d||step=%d||fragment=%d\n", start, end, step, fragment);
            for (size_t i = start; i < end; i += step) {
                void* ptr = malloc(i);
                if (!ptr) {
                    perror("malloc failed");
                    continue;
                } else {
                	big_ptrs.push_back(ptr); // 记录指针
					all_ptrs.push_back(ptr);
				}
                if (a == 'M') {
					memset(ptr, 1, i); // 初始化内存
				}

				if (fragment==1) {
					void *fptr = malloc(8);
					small_ptrs.push_back(fptr);
					all_ptrs.push_back(fptr);
					if (a == 'M') {
						memset(fptr, 1, 8); // 初始化内存
					}
				}
            	printf("allocated: || big=%d || small=%d || set=%d\n", big_ptrs.size(), small_ptrs.size(), a=='M');
            }
        } else if (a == 'f' || a == 'F') {
            // 释放所有内存
			int selection;
            printf(">>>>please input:selection(1-big,2-small,3-together)...");
            scanf(" %d", &selection);
			getchar();
			if (selection==1) {
				for (auto p : big_ptrs)
					free(p);
            	big_ptrs.clear();
			} else if (selection == 2) {
				for (auto p : small_ptrs)
					free(p);
            	small_ptrs.clear();
			} else {
				for (auto p : all_ptrs)
					free(p);
            	all_ptrs.clear();
            	big_ptrs.clear();
            	small_ptrs.clear();
			}
            printf("%d memory freed.\n", selection);
        } else if (a == 'q') {
			break;
        } else if (a == 't') {
			malloc_trim(0);
            printf("All memory trimed.\n");
        } else {
			printf("unsupported selection:%c", a);
		}
    }
    return 0;
}
