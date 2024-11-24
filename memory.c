/*=====================================================================================
	Assignment 2 - Question 2: Memory Management in OS

	Name: memory.c

	Written by: Brett Challice Feb 7, 2024

	Purpose: To simulate the Operating System paging process, to demonstrate an understanding of	
			 key components including logical address translation, using boolean operators to 
			 accomodate differences in memory sizing between logical address space and physical
			 address space, TLB buffer efficiency and usefulness, and shared memory for a single process. 
			 The program is effectively a memory management simulation that otherwise would be handled
			 solely by the operating system.
			 
	Usage: gcc -o memory memory.c
	       ./memory

	Description of Parameters:

	-> N/A

	Subroutines/Libraries required:

	-> See include statements

---------------------------------------------------------------------------------------*/

// Includes
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>

// Macros
#define LOGICAL_SIZE 65536
#define PHYSICAL_SIZE 32768
#define PAGE_SIZE 256
#define TLB_SIZE 16
#define PAGE_TABLE_SIZE (LOGICAL_SIZE / PAGE_SIZE)

// Defined types
// TLB Entry Structure
typedef struct {
    int page_number;
    int frame_number;
} TLBEntry;

// Page Structure
typedef struct {
    int page_number;
    int offset;
} page;

// Shared Memory Struct
typedef struct {
	void* backing;
	int file_size;

} backing_store;

// Prototypes
page translate(int v_address);
int search_TLB(page* p);
void TLB_Add(page* p, int frame_num);
void TLB_Update(page* p, int replaced_page, int frame_num);
int checkPageTable(page* p);
int handlePageFault(backing_store * backing, page* p);
backing_store map_backing_store();

// Globals
int page_table[PAGE_TABLE_SIZE];                 // Page Table
TLBEntry TLB[TLB_SIZE];	                        // TBL Entry Table
int page_fault_count = 0;	               // Page Fault counter
int tlb_hit_count = 0;			      // TLB Hit counter
int total_addresses = 0;    	    	     // Address counter 
signed char physical_memory[PHYSICAL_SIZE]; // Physical Memory Array - Simulation

//-------------------------------------------------------------------------------------------------------
int main(int argc, char** argv){
	
	// Map backing store to process memory space
	backing_store backing = map_backing_store();

	// fopen variables
	FILE* file;
	char filename[] = "addresses.txt";
	char line[10]; // Buffer to store each line in file

	// Initialize TLB table
   	 for (int i = 0; i < TLB_SIZE; i++) {
   	        TLB[i].page_number = -1;
      	  	TLB[i].frame_number = -1;
    	}

	// Initalize page table
	for(int i = 0; i < PAGE_TABLE_SIZE; i++){
		page_table[i] = -1;
	}

	// Open file
	file = fopen(filename, "r");

	// Check if file opened successfully
	if(file == NULL){
		printf("Error opening file : %s\n", filename);
		exit(EXIT_FAILURE);
	}

	// Read each address from file
	while (fgets(line, sizeof(line), file) != NULL) {

		// Treat string as integer
		int v_address = atoi(line);

		int p_address;		
		signed char value;

		// Compute page number and offset
		page logical = translate(v_address);

		// Check TLB for page
		int frame_number = search_TLB(&logical);

		// TLB Miss
		if(frame_number == -1){

			frame_number = checkPageTable(&logical);

			// Page Table Miss
			if(frame_number == -1){

				// Increment fault counter
				page_fault_count++;

				// Handle page fault using backing store, assign replacement frame number
				frame_number = handlePageFault(&backing, &logical);

				// Compute physical address
				p_address = frame_number << 8 | logical.offset;

				// Grab value stored at physical address
				value = physical_memory[p_address];


			}

			// Page Table Hit
			else{
				// Compute physical address
				p_address = frame_number << 8 | logical.offset;

				// Grab value stored at physical address
				value = physical_memory[p_address];

				// Add the found mapping to the TLB Table for future use
				TLB_Add(&logical, frame_number);
			}
		}

		// TLB Hit
		else{
			// Increment TLB Hit counter
			tlb_hit_count++;

			// Compute physical address
			p_address = frame_number << 8 | logical.offset;

			// Grab value stored at physical address
			value = physical_memory[p_address];
			
		}
			
		// Print the address to the console
        	printf("Virtual address => %d, Physical Address => %d, Value = %d\n", v_address, p_address, value);
		
		// Increment addresses counter
		total_addresses++;
						
        }

	printf("Total Addresses => %d\n", total_addresses);
	printf("Page_Faults => %d\n", page_fault_count);
	printf("TLB Hits => %d\n", tlb_hit_count);

	// Close addresses.txt
	fclose(file);

	// Unmap shared memory and check for errors
	if(munmap(backing.backing, backing.file_size) == -1){
		printf("Error unmapping backing store! \n");
		exit(EXIT_FAILURE);
	}

	return 0;
}
//------------------------------------------------------------------------------------------------------
// Computes logical address page number and offset and returns a struct with data
page translate(int v_address){

	page translation;

	// Compute page number and offset using bitwise operators
	translation.page_number = v_address >> 8; // Dividing by 256 (2^8) using bitwise right shift
    	translation.offset = v_address & 0xFF;   // Mask low order 8 bits to calculate offset within page

	return translation;
}
//------------------------------------------------------------------------------------------------------
// Look up page number in the translation lookaside buffer
// Returns true if TLB hit, or false if TLB miss
int search_TLB (page* p){

	// Iterate through TLB Table, check if page number exists
	for(int i = 0; i < TLB_SIZE; i++){

		// If page number exists
		if(p->page_number == TLB[i].page_number)
			return TLB[i].frame_number;
	}

	// Looked through entire array, does not exist
	return -1;
}
//------------------------------------------------------------------------------------------------------
// Add entry to TLB abiding by FIFO policy
// If TLB is full, replace oldest entry with new page
void TLB_Add(page* p, int frame_num){

	static int index = 0;

	// Add entry to TLB
	TLB[index].page_number = p->page_number;
	TLB[index].frame_number = frame_num;

	// Update index for FIFO Policy
	index = (index + 1) % TLB_SIZE;
}
//------------------------------------------------------------------------------------------------------
void TLB_Update(page * p, int replaced_page, int frame_num){

	// Loop through TLB
	// See if replaced_page number exists in the array
	// If it does, replace the page number with the replacement page number (p)
	for(int i = 0; i < TLB_SIZE; i++){
		if(TLB[i].page_number == replaced_page){
			TLB[i].page_number = p->page_number;
			return;
		}
	}
	// Looped through entire array, page number does not exist, add TLB Entry with new mapping
	TLB_Add(p, frame_num);
}
//------------------------------------------------------------------------------------------------------
// Check page table to obtain the frame number
int checkPageTable(page* p){

	// Index page table, to indicate whether the page exists not
	// If it does, return the frame number
	// If it does not, indicate a page fault
	if(page_table[p->page_number] != -1){

		return page_table[p->page_number]; // return frame number
	}

	// Page does not exist
	return -1;
}
//------------------------------------------------------------------------------------------------------
// Open BACKING_STORE.bin and map it to a memory region using mmap()
backing_store map_backing_store(){

	int fd;
	backing_store storage;
	char* store_path = "data/BACKING_STORE.bin";

	// Open backing store, checking for errors
	if((fd = open(store_path, O_RDONLY)) == -1){
		printf("Error opening backing store! \n");
		exit(EXIT_FAILURE);
	}
	// Successful open

	// Determine file size to pass to mmap()
	off_t file_size = lseek(fd, 0, SEEK_END);

	// Map the entire file into memory using mmap()
	void* mapping = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);

	// Check for errors in memory mapping
	if(mapping == MAP_FAILED){
		printf("Memory mapping failed! \n");
		exit(EXIT_FAILURE);
	}

	// Close file descriptor
	close(fd);

	// Populate backing storage struct to return to main
	storage.backing = mapping;
	storage.file_size = file_size;

	return storage;
}

// Handle page fault
// Read 256-byte page from backing store and copy it to an available frame in physical memory using memcpy()
// Update the page table accordingly
int handlePageFault(backing_store* backing, page * p){

	// Initialize static index to aid in implementing physical memory as a circular array
	static int index = 0; 
	int frame_num;

	// Following FIFO policy, replace the page the starts at the offset in memory
	// There can exist a current frame, or be empty
	int offset = index * PAGE_SIZE;
	frame_num = index;
	
	// Copy 256-byte page from backing store to memory
	memcpy(physical_memory + offset, (char*)backing->backing + (p->page_number * PAGE_SIZE), PAGE_SIZE);
	
	// Check if this was a replacement or in empty space
	// See if a page pointed to this frame in the page table
	int replaced_page_num = -1;
	for (int i = 0; i < PAGE_TABLE_SIZE; i++){
		if(page_table[i] == frame_num){
			replaced_page_num = i;
			break;
		}
	}

	// Replacement
	if(replaced_page_num != -1){

		// Update TLB and Page Table
		page_table[replaced_page_num] = -1;
		TLB_Update(p, replaced_page_num, frame_num);	
	}
	
	// Add new mapping to page table
	page_table[p->page_number] = frame_num;
	
	// Increment index for next page fault
	index = (index + 1) % (PHYSICAL_SIZE / PAGE_SIZE);

	return frame_num;
}

//------------------------------------------------------------------------------------------------------
