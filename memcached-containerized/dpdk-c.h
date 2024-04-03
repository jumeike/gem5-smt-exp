// This file containes the necessary routines to initialize
// and work with DPDK. It's written in C to be able to link with
// any application without issues and any extra steps.
//
// The implementation enables zero-copy networking.

#ifndef _DPDK_H_
#define _DPDK_H_

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <rte_config.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_mbuf.h>
#include <rte_pdump.h>
#include <zipfian_int_distribution.h>
#include <helpers.h>

////////////////////////////////////////////
// Huy
#include <time.h>
#define PERCENTILE 99

// Define a global flag for termination
volatile sig_atomic_t terminate_flag = 0;

// Signal handler function for SIGINT (Ctrl+C)
void sigint_handler(int signum) {
    printf("Termination signal received. Exiting...\n");
    terminate_flag = 1;
}

// Comparison function for qsort
int compare(const void *a, const void *b) {
    double double_a = *((double *)a);
    double double_b = *((double *)b);
    if (double_a < double_b) return -1;
    if (double_a > double_b) return 1;
    return 0;
}
////////////////////////////////////////////

#define kRingN 1
#define kRingDescN 2048
#define kMTUStandardFrames 1500
#define kMTUJumboFrames 9000
#define kLinkTimeOut_ms 100
#define kMaxBurstSize 1

#define REINTERPRET(new_type, var) (*((new_type *)&(var)))

static const size_t kMaxPacketSize = 1500;

typedef struct {
    unsigned char* data;
    size_t size;
} DSetItem;

typedef struct {
    DSetItem* items;
    size_t count;
} DSet;

void initialize(DSet* dset, int size) {
    // Allocate memory for SIZE items
    dset->items = malloc(size * sizeof(DSetItem));
    if (dset->items == NULL) {
        // Memory allocation failed, handle the error
        fprintf(stderr, "Error: Memory allocation failed in initialize\n");
        exit(EXIT_FAILURE); // or return an error code
    }
    dset->count = 0; // Initialize count to 0
}

void push_back(DSet* dset, DSetItem item) {
    if (dset->count < 10000000) {
        // Add the new item if there's still space
        dset->items[dset->count++] = item;
    } else {
        // Handle the case when the array is already full
        fprintf(stderr, "Error: DSet is full in push_back\n");
        exit(EXIT_FAILURE); // or return an error code
    }
}

void printDSetItem(const DSetItem *item, const char *label) {
    printf("%s: ", label);
    for (size_t i = 0; i < item->size; ++i) {
        printf("%u ", item->data[i]);
    }
    printf("(Size: %zu)\n", item->size);
}

typedef struct {
    size_t size;
    unsigned char* data;
} DSetItemData;

DSetItemData* getDSetItem(const DSet* dset, size_t index) {
    if (index < dset->count) {
        DSetItemData* itemData = malloc(sizeof(DSetItemData));
        if (itemData != NULL) {
            itemData->size = dset->items[index].size;
            itemData->data = dset->items[index].data;
        }
        return itemData;
    } else {
        return NULL;
    }
}

char* randomString(int size) {
    static const char text[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    char* randomString = malloc(sizeof(char) * (size + 1)); // +1 for null terminator

    if (randomString == NULL) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < size; ++i) {
        randomString[i] = text[rand() % (sizeof(text) - 1)];
    }

    randomString[size] = '\0';

    return randomString;
}

// Function to save the dataset to a binary file
void saveDatasetToFile(const char* filename, DSet* dset) {
    FILE* file = fopen(filename, "wb");
    if (file == NULL) {
        fprintf(stderr, "Error: Failed to open file for writing\n");
        return;
    }

    // Write the number of items in the dataset
    fwrite(&dset->count, sizeof(size_t), 1, file);

    // Write each item in the dataset
    for (size_t i = 0; i < dset->count; ++i) {
        DSetItem* item = &dset->items[i];
        // Write the size of the data
        // fwrite(&item->size, sizeof(size_t), 1, file);
        // Write the data itself
        fwrite(item->data, sizeof(unsigned char), item->size, file);
        fwrite("\n", sizeof(char), 1, file);
        // fwrite("\n", sizeof(char), 1, file);
    }

    fclose(file);
}

double calculate_latency(double *times, size_t size) {
    double sum = 0;
    for (size_t i = 0; i < size; i++) {
        sum += times[i];
    }
    return sum / size;
}

static uint8_t *SelfContained(size_t dataset_size, int key_size, int value_size, float vsize_skew,
                                            int vsize_min, int vsize_max, float ksize_skew, int ksize_min, 
                                            int ksize_max, size_t populate_ds_size, size_t kMaxValSize, bool use_udp, bool warmUpFlag) {
    signal(SIGINT, sigint_handler);

    // GENERATE DATASET
    // DSet dset_keys;
    // DSet dset_vals;
    // initialize(&dset_keys, dataset_size);
    // initialize(&dset_vals, dataset_size);
    // printf("DSet initialized\n");
    // size_t i, j;
    // for (i = 0; i < dataset_size; ++i) {
    //     size_t key_len = ksize_min + rand() % (ksize_max - ksize_min + 1);
    //     size_t val_len = vsize_min + rand() % (vsize_max - vsize_min + 1);

    //     DSetItem key_item;
    //     key_item.size = key_len;
    //     // printf("key_len: %zu\n", key_item.size);
    //     key_item.data = (unsigned char*)malloc(key_len * sizeof(unsigned char));

    //     memcpy(key_item.data, randomString(key_len), key_len);

    //     // printf("key_item.data: %p\n", key_item.data);
    //     // for (j = 0; j < key_len; ++j) {
    //     //     // printf("j: %zu\n", j);
    //     //     key_item.data[j] = rand() % 256;
    //     // }
        
    //     DSetItem val_item;
    //     val_item.size = val_len;
    //     // printf("val_len: %zu\n", val_item.size);
    //     val_item.data = (unsigned char*)malloc(val_len * sizeof(unsigned char));

    //     memcpy(val_item.data, randomString(val_len), val_len);
    //     // printf("val_item.data: %p\n", val_item.data);
    //     // for (j = 0; j < val_len; ++j) {
    //     //     val_item.data[j] = rand() % 256;
    //     // }
    //     // Push back key and value items to the respective DSet
    //     push_back(&dset_keys, key_item);
    //     push_back(&dset_vals, val_item);
    //     // printf("Pushed back key and value items\n");
    // }
    // saveDatasetToFile("dataset_keys", &dset_keys);
    // saveDatasetToFile("dataset_vals", &dset_vals);
    // printf("Dataset generated.\n");

    /*
    // Printing the generated data for demonstration
    for (i = 0; i < dataset_size; ++i) {
        printf("Entry %zu:\n", i);
        DSetItemData* keyData = getDSetItem(&dset_keys, i);
        if (keyData != NULL) {
            printf("Key: ");
            for (j = 0; j < keyData->size; ++j) {
                printf("%u ", keyData->data[j]);
            }
            printf("(Size: %zu)\n", keyData->size);
            free(keyData);
        }
        DSetItemData* valData = getDSetItem(&dset_vals, i);
        if (valData != NULL) {
            printf("Value: ");
            for (j = 0; j < valData->size; ++j) {
                printf("%u ", valData->data[j]);
            }
            printf("(Size: %zu)\n", valData->size);
            free(valData);
        }
        printf("\n");
    }
    */
    if (warmUpFlag){

        FILE *file_k, *file_v;
        char line_k[31];
        char line_v[201];

        // Open the file in read mode
        file_k = fopen("dataset_keys", "r");
        file_v = fopen("dataset_vals", "r");

        if (file_v == NULL || file_k == NULL) {
            printf("Could not open the file.\n");
            return 1;
        }

        for (size_t i = 0; i < populate_ds_size; i++) {
            
            if (fgets(line_k, sizeof(line_k), file_k) != NULL &&
                fgets(line_v, sizeof(line_v), file_v) != NULL) {

                perform_set_wout_udp(line_k, sizeof(line_k), line_v, sizeof(line_v));

                fgets(line_v, sizeof(line_v), file_v);
                fgets(line_k, sizeof(line_k), file_k);

            }
        }

        fclose(file_k);
        fclose(file_v);

        printf("***********************************************************************\n");
        printf("server populated/warmedUp and now stating to send GET requests\n");

        /* If we are in simulation, take checkpoint here. */
        #ifdef _GEM5_
        fprintf(stderr, "Taking post-warmup checkpoint.\n");
        system("m5 checkpoint");
        #endif

        return;
    }    


    FILE *file_k, *file_v;
    char line[31];
    char **lines = (char **)malloc(dataset_size * sizeof(char *));
    size_t i = 0;

    // Open the file
    file_k = fopen("dataset_keys", "r");
    if (file_k == NULL) {
        printf("Unable to open file.\n");
        return 1;
    }

    // Read lines and store them in the array
    while (fgets(line, sizeof(line), file_k) && i < dataset_size) {
        // lines[i] = malloc(sizeof(line));
        lines[i] = malloc(strlen(line) + 1);
        if (lines[i] == NULL) {
            printf("Memory allocation failed.\n");
            return 1;
        }
        // memcpy(lines[i], line, sizeof(line));
        strcpy(lines[i], line);
        i++;
        fgets(line, sizeof(line), file_k);
    }
    printf("file loaded into memory\n");
    fclose(file_k);

    // char line_k[31];
    // char line_v[201];

    struct timespec start_time, end_time;
    printf("Starting the experiment\n");
    // uint32_t *key;

    
    size_t count = 0;
    struct timespec throughput_start_time, current_time;
    double elapsed_time = 0;
    size_t index;
    char *key;
    double *processing_times_exp = (double *)malloc(dataset_size * sizeof(double));
    double time_taken;
    size_t hit = 0;
    size_t miss = 0;

    while (!terminate_flag){
        clock_gettime(CLOCK_MONOTONIC, &throughput_start_time);
        index = rand() % dataset_size;

        key = malloc(strlen(lines[index])+1);
 
        strcpy(key, lines[index]);
        key = lines[index];
        uint8_t *value;
        uint32_t value_len;

        clock_gettime(CLOCK_MONOTONIC, &start_time);
        perform_get_wout_udp(key, strlen(key)+1, &value, &value_len, &hit, &miss);
        clock_gettime(CLOCK_MONOTONIC, &end_time);

        time_taken = (end_time.tv_sec - start_time.tv_sec) * 1e9 + (end_time.tv_nsec - start_time.tv_nsec); // in nanoseconds
        processing_times_exp[count] = time_taken;
        // printf("count: %d\n", count);
        // sleep(1);
        
        
        if (elapsed_time >= 1) {
            double average_latency_exp = calculate_latency(processing_times_exp, count);
            qsort(processing_times_exp, count, sizeof(double), compare);
            double latency_99th_exp = processing_times_exp[(int)((double)count * PERCENTILE / 100)];
            double throughput = count / elapsed_time; 
            // Reset for the next interval
            clock_gettime(CLOCK_MONOTONIC, &throughput_start_time);

            printf("Avg Latency  : %.6f ms\n", average_latency_exp/1000);
            printf("99th Latency : %.6f ms\n", latency_99th_exp/1000);
            printf("Throughput   : %.2f rps\n", throughput);
            printf("Hit          : %d\n", hit);
            printf("Miss         : %d\n", miss);
            printf("***********************************************************************\n");
            count = 0;
            elapsed_time = 0;
            hit = 0;
            miss = 0;
        }
        count ++;      
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        elapsed_time = elapsed_time + (current_time.tv_sec - throughput_start_time.tv_sec) +
                                (current_time.tv_nsec - throughput_start_time.tv_nsec) / 1e9; // in seconds
    }

}






static uint8_t *SelfContainedUDP(size_t dataset_size, int key_size, int value_size, float vsize_skew,
                                            int vsize_min, int vsize_max, float ksize_skew, int ksize_min, 
                                            int ksize_max, size_t populate_ds_size, size_t kMaxValSize, bool use_udp) {  

    DSet dset_keys;
    DSet dset_vals;
    initialize(&dset_keys, dataset_size);
    initialize(&dset_vals, dataset_size);
    printf("DSet initialized\n");
    size_t i, j;
    for (i = 0; i < dataset_size; ++i) {
        size_t key_len = ksize_min + rand() % (ksize_max - ksize_min + 1);
        size_t val_len = vsize_min + rand() % (vsize_max - vsize_min + 1);

        DSetItem key_item;
        key_item.size = key_len;
        // printf("key_len: %zu\n", key_item.size);
        key_item.data = (unsigned char*)malloc(key_len * sizeof(unsigned char));
        // printf("key_item.data: %p\n", key_item.data);
        for (j = 0; j < key_len; ++j) {
            // printf("j: %zu\n", j);
            key_item.data[j] = rand() % 256;
        }
        

        DSetItem val_item;
        val_item.size = val_len;
        // printf("val_len: %zu\n", val_item.size);
        val_item.data = (unsigned char*)malloc(val_len * sizeof(unsigned char));
        // printf("val_item.data: %p\n", val_item.data);
        for (j = 0; j < val_len; ++j) {
            val_item.data[j] = rand() % 256;
        }
        // Push back key and value items to the respective DSet
        push_back(&dset_keys, key_item);
        push_back(&dset_vals, val_item);
        printf("Pushed back key and value items\n");
        
    }

    printf("Dataset generated.\n");
    
    printf("Dataset generated and now starting populating the server\n");

    uint8_t *tx_buff_ptr   = malloc(kMaxPacketSize);
    uint8_t *temp_buff_ptr = malloc(kMaxPacketSize);
    uint8_t *temp          = malloc(kMaxPacketSize);

    for (size_t i = 0; i < dataset_size; ++i) {

        DSetItemData* keyData = getDSetItem(&dset_keys, i);
        DSetItemData* valData = getDSetItem(&dset_vals, i);

        printf("Populating server with key %d\n", i);
        temp_buff_ptr = tx_buff_ptr;
        temp = temp_buff_ptr;

        // Call the HelperFormUdpHeader function
        size_t h_size = HelperFormUdpHeader((struct MemcacheUdpHeader *)temp_buff_ptr, 1, 0);
        temp_buff_ptr += sizeof(struct MemcacheUdpHeader);

        // Form request header.
        size_t rh_size = HelperFormSetReqHeader((struct ReqHdr *)temp_buff_ptr, keyData, valData);
        // tx_buff_ptr += sizeof(struct ReqHdr *);
        temp_buff_ptr += 24;

        // Fill packet: extra, unlimited storage time.
        uint32_t extra[2] = {0x00, 0x00};
        memcpy(temp_buff_ptr, extra, kExtraSizeForSet);
        temp_buff_ptr += kExtraSizeForSet;

        // Fill packet: key.
        memcpy(temp_buff_ptr, keyData, keyData->size);
        temp_buff_ptr += keyData->size;


        // Fill packet: value.
        memcpy(temp_buff_ptr, valData, valData->size);
        temp_buff_ptr += valData->size;

        // Check total packet size.
        uint32_t total_length = h_size + rh_size;

        free(keyData);
        free(valData);

        return temp;

    } // end of for loop

    printf("server populated/warmedUp and now stating to send GET requests\n");

    /* If we are in simulation, take checkpoint here. */
    #ifdef _GEM5_
    fprintf(stderr, "Taking post-warmup checkpoint.\n");
    system("m5 checkpoint");
    #endif


}

#endif // _DPDK_H_