/*
 * @file lab5.c - Lab 5 Virtual Memory Replacement Simulator
 * @author Jiwoo Lee (c) 2019
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAXPAGES 1024
#define MAXPROC 10

// AllocEq = 0, AllocProp = 1
// effective for indexing
// define variable type as alloc_t to use it
typedef enum { AllocEq, AllocProp } alloc_t;
typedef enum { EvictFIFO, EvictSecond, EvictLRU, EvictLFU } evict_t;
typedef enum { ReplacementGlobal, ReplacementLocal } local_t;

struct access_s {
	int pid;
	int addr;
};
typedef struct access_s access_t;

// page table entry
struct PTE {
	int present, refer, frame, addts, refts, count;
};

// process control block
struct PCB {
	struct PTE PT[MAXPAGES];
	int proc_size, pid;
	// # pages, # frames, # pages, # pages mapped, # frames loaded
	int num_page, num_frame, page_mapped, frame_loaded;
	int faults, access;
};

// fifo queue entry
struct FIFOentry {
	struct PTE* pte;
	int proc_i;
};

int evictPage(struct PCB p_dir[], struct FIFOentry FIFO[], int proc_i, int FIFOsize, int naccess, evict_t e, local_t r);
int evictFIFO(struct PCB p_dir[], struct FIFOentry FIFO[], int proc_i, int FIFOsize, int naccess, local_t replace);
int evictSecond(struct PCB p_dir[], struct FIFOentry FIFO[], int proc_i, int FIFOsize, int naccess, local_t replace);
int evictLRU(struct PCB p_dir[], struct FIFOentry FIFO[], int proc_i, int FIFOsize, int naccess, local_t replace);
int evictLFU(struct PCB p_dir[], struct FIFOentry FIFO[], int proc_i, int FIFOsize, int naccess, local_t replace);

/*
 * main
 */
int main(int argc, char **argv) {
	FILE *plist, *ptrace, *outfp;
	int memsize, pagesize, period;
	int nproc, naccess;
	alloc_t alloc;
	evict_t evict;
	local_t replacement;
	access_t  *trace;
	int pid, msize, addr, count = 0;
	char ch;

	if (argc != 7) {
		// printf == fprint(stdout, "")
		fprintf(stderr, "usage: %s [memsize] [pagesize] [alloc] [eviction] [replacement] [period]\n", argv[0]);
		fprintf(stderr, "      memsize  - size of physical memory in bytes\n");
		fprintf(stderr, "      pagesize - size of pages/frames in bytes \n");
		fprintf(stderr, "      alloc:\n");
		fprintf(stderr, "          0 - equal allocation\n");
		fprintf(stderr, "          1 - proportional allocation\n");
		fprintf(stderr, "      eviction:\n");
		fprintf(stderr, "          0 - FIFO page replacement:\n");
		fprintf(stderr, "          1 - second chance replacement:\n");
		fprintf(stderr, "          2 - LRU replacement\n");
		fprintf(stderr, "          3 - LFU replacement\n");
		fprintf(stderr, "      replacement:\n");
		fprintf(stderr, "          0 - global replacement\n");
		fprintf(stderr, "          1 - local replacement\n");
		exit(1);
	}

	// process command-line args
	memsize  = atoi(argv[1]);
	pagesize = atoi(argv[2]);
	period   = atoi(argv[6]);

	// allocation algorithm
	switch(atoi(argv[3])) {
		case 0:
			alloc = AllocEq;
			break;
		case 1:
			alloc = AllocProp;
			break;
		default:
			fprintf(stderr, "allocation algorithm must be 0 (equal) or 1 (proportional)\n");
			exit(1);
			break;
	}

	// eviction algorithm
	switch(atoi(argv[4])) {
		case 0:
			evict = EvictFIFO;
			break;
		case 1:
			evict = EvictSecond;
			break;
		case 2:
			evict = EvictLRU;
			break;
		case 3:
			evict = EvictLFU;
			break;
		default:
			fprintf(stderr, "allocation algorithm must be 0 (FIFO) or 1 (second) or 2 (LRU) or 3 (LFU)\n");
			exit(1);
			break;
	}

	// global vs. local replacement
	switch(atoi(argv[5])) {
		case 0:
			replacement = ReplacementGlobal;
			break;
		case 1:
			replacement = ReplacementLocal;
			break;
		default:
			fprintf(stderr, "allocation algorithm must be 0 (global) or 1 (local)\n");
			exit(1);
			break;
	}

	// read process information
	plist = fopen("plist.txt", "r");
	if (!plist) {
		perror("plist fopen");
		exit(1);
	}

	// scanf = fscanf(stdin, "")
	fscanf(plist, "%d", &nproc);

	// pcb directory
	struct PCB p_dir[10];

	// initialize process directory
	// calculate sum of # page needed for AllocProp
	int total_page = 0;
	for (int i = 0; i < nproc; i++) {
		fscanf(plist, "%d %d", &pid, &msize);
		p_dir[i].proc_size = msize;
		p_dir[i].pid = pid;
		p_dir[i].frame_loaded = p_dir[i].page_mapped = p_dir[i].faults = p_dir[i].access = 0;

		p_dir[i].num_page = msize / pagesize;
		if ((msize % pagesize) != 0) {
			// internal fragmentation, extra page needed
			p_dir[i].num_page++;
		}

		total_page += p_dir[i].num_page;
	}
	fclose(plist);

	// allocate frames
	int nframe = memsize/pagesize;
	for (int i = 0; i < nproc; i++) {
		// AllocEq
		if (alloc == AllocEq) {
			if (i < (nproc - 1)) {
				p_dir[i].num_frame = nframe/nproc;
				if (p_dir[i].num_frame == 0) {
					printf("Allocated frame == 0\n");
					return 0;
				}
			}
			else {
				// last process
				p_dir[i].num_frame = nframe - (nframe/nproc)*(nproc-1);
				if (p_dir[i].num_frame == 0) {
					printf("Allocated frame == 0\n");
					return 0;
				}
			}
		}
		// AllocProp
		else {
			p_dir[i].num_frame = (int)(((double)p_dir[i].num_page/(double)total_page)*nframe);
			if (p_dir[i].num_frame == 0) {
				printf("Allocated frame == 0\n");
				return 0;
			}
		}
	}


	ptrace = fopen("ptrace.txt", "r");
	if (!ptrace) {
		perror("ptrace fopen");
		exit(1);
	}

	// count total # of accesses
	while ((ch = fgetc(ptrace)) != EOF) {
		if (ch == '\n') count++;
	}
	naccess = count;

	// now read them into an array
	// rewind() sets the file position to the beginning of the file
	rewind(ptrace);

	trace = calloc(count, sizeof(access_t));
	if (!trace) {
		perror("trace alloc");
		exit(1);
	}

	for (int cur = 0; cur < count; cur++) {
		fscanf(ptrace, "%d %d", &trace[cur].pid, &trace[cur].addr);
	}

	fclose(ptrace);

	if (period == 0) {
		outfp = NULL;
	}
	else {
		// w == O_WRONLY | O_TRUNC | O_CREATE
		outfp = fopen("ptable.txt", "w");
		if (!outfp) {
			perror("output file");
			exit(1);
		}
	}

	int found, inmemory, proc_i, add_here, FIFOsize = 0;
	// for Second Chance and FIFO
	struct FIFOentry FIFO[naccess];
	// process memory trace using replacement strategy
	for (int ts = 0; ts < naccess; ts++) {
		// keep track of # access per process
		proc_i = trace[ts].pid;
		(p_dir[proc_i].access)++;

		// check if the frame maps to a page & its present bit
		int inmemory = found = 0;
		int page_i = 0;
		while ((found != 1) && (page_i < p_dir[proc_i].page_mapped)) {
			if (p_dir[proc_i].PT[page_i].frame == trace[ts].addr) {
				found = 1;
				if (p_dir[proc_i].PT[page_i].present == 1) {
					inmemory = 1;
				}
			}
			else {
				page_i++;
			}
		}

		// in main memory
		if (inmemory == 1) {
			p_dir[proc_i].PT[page_i].count++;
			p_dir[proc_i].PT[page_i].refts = ts;
			p_dir[proc_i].PT[page_i].refer = 1;
		}
		// in page table but not in main memory
		else if (found == 1) {
			// evict if necessary
			if (p_dir[proc_i].frame_loaded >= p_dir[proc_i].num_frame) {
				evictPage(p_dir, FIFO, proc_i, FIFOsize, naccess, evict, replacement);
			}

			// load the frame into the memory
			(p_dir[proc_i].frame_loaded)++;
			(p_dir[proc_i].faults)++;
			p_dir[proc_i].PT[page_i].present = p_dir[proc_i].PT[page_i].refer = p_dir[proc_i].PT[page_i].count = 1;
			p_dir[proc_i].PT[page_i].refts = p_dir[proc_i].PT[page_i].addts = ts;
		}
		// not in both main memory and page table
		else {
			// no eviction, new mapping
			if (p_dir[proc_i].frame_loaded < p_dir[proc_i].num_frame) {
				add_here = p_dir[proc_i].page_mapped;
				// load frame into memory
				(p_dir[proc_i].frame_loaded)++;
				p_dir[proc_i].PT[add_here].frame = trace[ts].addr;
				p_dir[proc_i].PT[add_here].refer = p_dir[proc_i].PT[add_here].count = p_dir[proc_i].PT[add_here].present = 1;
				p_dir[proc_i].PT[add_here].addts = p_dir[proc_i].PT[add_here].refts = ts;
				(p_dir[proc_i].page_mapped)++;

				FIFO[FIFOsize].proc_i = proc_i;
				FIFO[FIFOsize].pte = &(p_dir[proc_i].PT[add_here]);
				FIFOsize++;
				(p_dir[proc_i].faults)++;
			}
			// yes eviction, new mapping
			else {
				evictPage(p_dir, FIFO, proc_i, FIFOsize, naccess, evict, replacement);

				add_here = p_dir[proc_i].page_mapped;
				// load frame into memory
				(p_dir[proc_i].frame_loaded)++;
				p_dir[proc_i].PT[add_here].frame = trace[ts].addr;
				p_dir[proc_i].PT[add_here].refer = p_dir[proc_i].PT[add_here].count = p_dir[proc_i].PT[add_here].present = 1;
				p_dir[proc_i].PT[add_here].addts = p_dir[proc_i].PT[add_here].refts = ts;
				(p_dir[proc_i].page_mapped)++;

				FIFO[FIFOsize].proc_i = proc_i;
				FIFO[FIFOsize].pte = &(p_dir[proc_i].PT[add_here]);
				FIFOsize++;
				(p_dir[proc_i].faults)++;
			}
		}

		if (period != 0) {
			// write to ptable.txt every period
			if (((ts + 1) % period) == 0) {
				fprintf(outfp, "------------------------------ Time: %d ------------------------------\n", ts);
				for(int i = 0; i < nproc; i++) {
					fprintf(outfp, "PROCESS %d: (%d pages, %d frames)\n", i, p_dir[i].num_page, p_dir[i].num_frame);
					for (int j = 0; j < p_dir[i].num_page; j++) {
						if (j < p_dir[i].page_mapped) {
							fprintf(outfp, "page:%-5d ", j);
							fprintf(outfp, "inframe:%-2d ", p_dir[i].PT[j].present);
							fprintf(outfp, "addts:%-3d", p_dir[i].PT[j].addts);
							fprintf(outfp, "refts:%-3d", p_dir[i].PT[j].refts);
							fprintf(outfp, "refbit:%-2d", p_dir[i].PT[j].refer);
							fprintf(outfp, "refcount:%-3d", p_dir[i].PT[j].count);
							fprintf(outfp, "frame address:%-5d", p_dir[i].PT[j].frame);
							fprintf(outfp, "\n");
						}
						else {
							fprintf(outfp, "page:%-5d ", j);
							fprintf(outfp, "inframe:%-2d ", 0);
							fprintf(outfp, "addts:%-3d", 0);
							fprintf(outfp, "refts:%-3d", 0);
							fprintf(outfp, "refbit:%-2d", 0);
							fprintf(outfp, "refcount:%-3d", 0);
							fprintf(outfp, "frame address:%-5d", 0);
							fprintf(outfp, "\n");
						}
					}
				}
			}
		}
		// reset refer every 100 memory access
		if (((ts + 1) % 100) == 0) {
			for (int i = 0; i < nproc; i++) {
				for (int j = 0; j < p_dir[i].page_mapped; j++) {
					p_dir[i].PT[j].refer = 0;
				}
			}
		}


	}

	int total_faults = 0;
	for (int i = 0; i < nproc; i++) {
		total_faults += p_dir[i].faults;
	}

	if (period != 0)
		fclose(outfp);
	free(trace);
	printf("*****************************************************\n");
	printf("memsize   : %13d   pagesize: %12d   period     : %8d  nframes: %d\n",
			memsize, pagesize, period, memsize/pagesize);
	printf("allocation: %13s   eviction: %12s   replacement: %8s\n",
			alloc == AllocEq ? "equal" : "proportional",
			evict == EvictFIFO ? "FIFO"  :
			(evict == EvictSecond ? "SecondChance" :
			 (evict == EvictLRU ? "LRU" : "LFU")),
			replacement == ReplacementGlobal ? "global" : "local");

	printf("trace contains %d memory accesses\n", count);
	printf("*****************************************************\n");
	printf("%d processes -- memory sizes:\n", nproc);
	for (int i = 0; i < nproc; i++) {
		printf("proc[%d] :%4d bytes pages: %4d frames:%4d free:%4d\n", i, p_dir[i].proc_size, p_dir[i].num_page, p_dir[i].num_frame, p_dir[i].num_frame);
	}
	printf("*****************************************************\n");
	for (int i = 0; i < nproc; i++) {
		printf("Process %d faults: %d/%d (%.3f%%)\n\n", i, p_dir[i].faults, p_dir[i].access, (100*(double)p_dir[i].faults/(double)p_dir[i].access));
	}
	printf("Total faults: %d/%d (%.3f%%)\n\n", total_faults, naccess, (100*(double)total_faults/(double)naccess));

	return 0;
}

/*
 * evict - evict the best candidate page from those resident in memory
 * @param pid the process requesting eviction
 * @returns 1 if frame stolen from another process, 0 otherwise
 *
 */
int evictPage(struct PCB p_dir[], struct FIFOentry FIFO[], int proc_i, int FIFOsize, int naccess, evict_t e, local_t r) {
	switch (e) {
		case EvictFIFO:
			// call FIFO eviction function here
			return evictFIFO(p_dir, FIFO, proc_i, FIFOsize, naccess, r);
			break;
		case EvictSecond:
			// call 2nd chance eviction function here
			return evictSecond(p_dir, FIFO, proc_i, FIFOsize, naccess, r);
			break;
		case EvictLRU:
			// call LRU eviction function here
			return evictLRU(p_dir, FIFO, proc_i, FIFOsize, naccess, r);
			break;
		case EvictLFU:
			// call LFU eviction function here
			return evictLFU(p_dir, FIFO, proc_i, FIFOsize, naccess, r);
			break;
	}
	return -1;
}

int evictFIFO(struct PCB p_dir[], struct FIFOentry FIFO[], int proc_i, int FIFOsize, int naccess, local_t replace) {
	if (replace == ReplacementGlobal) {
		int candidate = 0, found = 0;
		while ((found != 1) && (candidate < FIFOsize)) {
			if ((FIFO[candidate].pte)->present == 1) {
				found = 1;
			}
			else {
				candidate++;
			}
		}

		// evict the candidate
		FIFO[candidate].pte->refer = FIFO[candidate].pte->count = FIFO[candidate].pte->present = FIFO[candidate].pte->addts = FIFO[candidate].pte->refts = 0;
		(p_dir[FIFO[candidate].proc_i].frame_loaded)--;
		(p_dir[FIFO[candidate].proc_i].num_frame)--;
		(p_dir[proc_i].num_frame)++;
		return 0;
	}
	// ReplacementLocal
	else {
		int candidate = 0, found = 0;
		while ((found != 1) && (candidate < FIFOsize)) {
			if (FIFO[candidate].proc_i == proc_i) {
				if ((FIFO[candidate].pte)->present == 1) {
					found = 1;
				}
				else {
					candidate++;
				}
			}
			else {
				candidate++;
			}

		}
		// evict the candidate
		FIFO[candidate].pte->refer = FIFO[candidate].pte->count = FIFO[candidate].pte->present = FIFO[candidate].pte->addts = FIFO[candidate].pte->refts = 0;
		(p_dir[proc_i].frame_loaded)--;
		return 1;
	}
}

int evictSecond(struct PCB p_dir[], struct FIFOentry FIFO[], int proc_i, int FIFOsize, int naccess, local_t replace) {
	if (replace == ReplacementGlobal) {
		int candidate = 0, found = 0;
		while ((found != 1) && (candidate < FIFOsize)) {
			// refer == 0
			if (((FIFO[candidate].pte)->refer == 0) && ((FIFO[candidate].pte)->present) == 1) {
				found = 1;
			}
			// refer == 1, present == 1
			else if ((FIFO[candidate].pte)->present == 1) {
				FIFO[candidate].pte->refer = 0;
				candidate++;
			}
			// present == 0
			else {
				candidate++;
			}
		}

		if (found == 1) {
			// evict the candidate
			FIFO[candidate].pte->refer = FIFO[candidate].pte->count = FIFO[candidate].pte->present = FIFO[candidate].pte->addts = FIFO[candidate].pte->refts = 0;
			(p_dir[FIFO[candidate].proc_i].frame_loaded)--;
			(p_dir[FIFO[candidate].proc_i].num_frame)--;
			(p_dir[proc_i].num_frame)++;
		}
		else {
			// evict according to FIFO
			evictFIFO(p_dir, FIFO, proc_i, FIFOsize, naccess, replace);
		}
		return 0;
	}
	// ReplacementLocal
	else {
		int candidate = 0, found = 0;
		while ((found != 1) && (candidate < FIFOsize)) {
			// search only in my process table
			while (FIFO[candidate].proc_i != proc_i) {
				candidate++;
			}
			// refer == 0
			if ((FIFO[candidate].pte->refer == 0) && (FIFO[candidate].pte->present == 1)){
				found = 1;
			}
			// refer == 1, present == 1
			else if (FIFO[candidate].pte->present == 1) {
				FIFO[candidate].pte->refer = 0;
				candidate++;
			}
			// present == 0
			else {
				candidate++;
			}
		}

		if (found == 1) {
			// evict the candidate
			FIFO[candidate].pte->refer = FIFO[candidate].pte->count = FIFO[candidate].pte->present = 0;
			FIFO[candidate].pte->addts = FIFO[candidate].pte->refts = 0;
			(p_dir[FIFO[candidate].proc_i].frame_loaded)--;
		}
		else {
			// evict according to FIFO
			evictFIFO(p_dir, FIFO, proc_i, FIFOsize, naccess, replace);
		}

		return 1;
	}
}
int evictLRU(struct PCB p_dir[], struct FIFOentry FIFO[], int proc_i, int FIFOsize, int naccess, local_t replace) {
	if (replace == ReplacementGlobal) {
		// find minimum refts in ALL page tables
		// use FIFO to trace
		int candidate, min = naccess;
		for (int i = 0; i < FIFOsize; i++) {
			if ((FIFO[i].pte->present == 1) && (FIFO[i].pte->refts <= min)) {
				min = FIFO[i].pte->refts;
				candidate = i;
			}
		}

		// evict candidate
		FIFO[candidate].pte->refer = FIFO[candidate].pte->count = FIFO[candidate].pte->present = FIFO[candidate].pte->addts = FIFO[candidate].pte->refts = 0;
		(p_dir[FIFO[candidate].proc_i].frame_loaded)--;
		(p_dir[FIFO[candidate].proc_i].num_frame)--;
		(p_dir[proc_i].num_frame)++;
		return 0;
	}
	// ReplacementLocal
	else {
		int min = naccess, candidate = 0;
		for (int i = 0; i < p_dir[proc_i].page_mapped; i++) {
			if ((p_dir[proc_i].PT[i].present == 1) && (p_dir[proc_i].PT[i].refts < min)) {
				min = p_dir[proc_i].PT[i].refts;
				candidate = i;
			}
		}

		// evict candidate
		p_dir[proc_i].PT[candidate].refer = p_dir[proc_i].PT[candidate].count = p_dir[proc_i].PT[candidate].present = p_dir[proc_i].PT[candidate].addts = p_dir[proc_i].PT[candidate].refts = 0;
		(p_dir[proc_i].frame_loaded)--;
		return 1;
	}
}

int evictLFU(struct PCB p_dir[], struct FIFOentry FIFO[], int proc_i, int FIFOsize, int naccess, local_t replace) {
	if (replace == ReplacementGlobal) {
		// find minimum count in ALL page tables
		// use FIFO to trace
		int candidate, min = naccess;
		for (int i = 0; i < FIFOsize; i++) {
			if ((FIFO[i].pte->present == 1) && (FIFO[i].pte->count <= min)) {
				min = FIFO[i].pte->count;
				candidate = i;
			}
		}

		// evict candidate
		FIFO[candidate].pte->refer = FIFO[candidate].pte->count = FIFO[candidate].pte->present = FIFO[candidate].pte->addts = FIFO[candidate].pte->refts = 0;
		(p_dir[FIFO[candidate].proc_i].frame_loaded)--;
		(p_dir[FIFO[candidate].proc_i].num_frame)--;
		(p_dir[proc_i].num_frame)++;

		return 0;
	}
	// ReplacementLocal
	else {
		int min = naccess, candidate = 0;
		for (int i = 0; i < p_dir[proc_i].page_mapped; i++) {
			if ((p_dir[proc_i].PT[i].present == 1) && (p_dir[proc_i].PT[i].count < min)) {
				min = p_dir[proc_i].PT[i].count;
				candidate = i;
			}
		}

		// evict candidate
		p_dir[proc_i].PT[candidate].refer = p_dir[proc_i].PT[candidate].count = p_dir[proc_i].PT[candidate].present = p_dir[proc_i].PT[candidate].addts = p_dir[proc_i].PT[candidate].refts = 0;
		(p_dir[proc_i].frame_loaded)--;
		return 1;
	}
}
