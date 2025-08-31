/*
 * Parallel NIP-13 Proof of Work Miner
 * Simple parallelization using pthreads - divides nonce space across cores
 * Based on the standalone version, adding minimal threading
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <math.h>
#include <pthread.h>

// Number of threads - will be set to number of CPU cores
static int num_threads = 0;

// Global result tracking
static volatile int solution_found = 0;
static uint64_t global_found_nonce = 0;
static pthread_mutex_t solution_mutex = PTHREAD_MUTEX_INITIALIZER;

// Simplified SHA256 constants and functions (same as original)
#define SHA256_DIGEST_SIZE 32
#define SHA256_BLOCK_SIZE  64

// SHA256 constants (from hashcat)
static const uint32_t sha256_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

// Rotate right
#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

// SHA256 functions (optimized versions from hashcat)
#define CH(x, y, z)  (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define S0(x)        (ROTR(x,  2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define S1(x)        (ROTR(x,  6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define s0(x)        (ROTR(x,  7) ^ ROTR(x, 18) ^ ((x) >>  3))
#define s1(x)        (ROTR(x, 17) ^ ROTR(x, 19) ^ ((x) >> 10))

// SHA256 context
typedef struct {
    uint32_t state[8];
    uint64_t count;
    uint8_t buffer[64];
} sha256_ctx_t;

// Initialize SHA256 context
void sha256_init(sha256_ctx_t *ctx) {
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
    ctx->count = 0;
}

// Process a single 512-bit block (optimized from hashcat)
void sha256_transform(sha256_ctx_t *ctx, const uint8_t *data) {
    uint32_t w[64];
    uint32_t a, b, c, d, e, f, g, h;
    uint32_t t1, t2;

    // Prepare message schedule
    for (int i = 0; i < 16; i++) {
        w[i] = (data[i * 4] << 24) | (data[i * 4 + 1] << 16) |
               (data[i * 4 + 2] << 8) | data[i * 4 + 3];
    }

    for (int i = 16; i < 64; i++) {
        w[i] = s1(w[i - 2]) + w[i - 7] + s0(w[i - 15]) + w[i - 16];
    }

    // Initialize working variables
    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];

    // Main loop (unrolled for performance like hashcat)
    for (int i = 0; i < 64; i++) {
        t1 = h + S1(e) + CH(e, f, g) + sha256_k[i] + w[i];
        t2 = S0(a) + MAJ(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    // Add compressed chunk to current hash value
    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

// Update SHA256 with new data
void sha256_update(sha256_ctx_t *ctx, const uint8_t *data, size_t len) {
    size_t buffer_space = 64 - (ctx->count % 64);

    ctx->count += len;

    if (len >= buffer_space) {
        memcpy(ctx->buffer + (64 - buffer_space), data, buffer_space);
        sha256_transform(ctx, ctx->buffer);
        data += buffer_space;
        len -= buffer_space;

        while (len >= 64) {
            sha256_transform(ctx, data);
            data += 64;
            len -= 64;
        }
    }

    if (len > 0) {
        memcpy(ctx->buffer + (ctx->count % 64) - len, data, len);
    }
}

// Finalize SHA256 and output digest
void sha256_final(sha256_ctx_t *ctx, uint8_t *digest) {
    uint64_t bit_count = ctx->count * 8;
    size_t pad_len = (ctx->count % 64 < 56) ? (56 - ctx->count % 64) : (120 - ctx->count % 64);

    uint8_t padding[64] = {0x80};
    sha256_update(ctx, padding, pad_len);

    // Append length in bits as big-endian 64-bit integer
    uint8_t len_bytes[8];
    for (int i = 7; i >= 0; i--) {
        len_bytes[i] = bit_count & 0xff;
        bit_count >>= 8;
    }
    sha256_update(ctx, len_bytes, 8);

    // Output final hash
    for (int i = 0; i < 8; i++) {
        digest[i * 4] = (ctx->state[i] >> 24) & 0xff;
        digest[i * 4 + 1] = (ctx->state[i] >> 16) & 0xff;
        digest[i * 4 + 2] = (ctx->state[i] >> 8) & 0xff;
        digest[i * 4 + 3] = ctx->state[i] & 0xff;
    }
}

// Complete SHA256 hash in one call
void sha256_hash(const uint8_t *data, size_t len, uint8_t *digest) {
    sha256_ctx_t ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, digest);
}

// Count leading zero bits in hash
int count_leading_zeros(const uint8_t *hash) {
    int zeros = 0;
    for (int i = 0; i < SHA256_DIGEST_SIZE; i++) {
        if (hash[i] == 0) {
            zeros += 8;
        } else {
            uint8_t byte = hash[i];
            while ((byte & 0x80) == 0) {
                zeros++;
                byte <<= 1;
            }
            break;
        }
    }
    return zeros;
}

// Convert hash to hex string
void hash_to_hex(const uint8_t *hash, char *hex_str) {
    for (int i = 0; i < SHA256_DIGEST_SIZE; i++) {
        sprintf(hex_str + i * 2, "%02x", hash[i]);
    }
    hex_str[64] = '\0';
}

// Get current time in microseconds
uint64_t get_time_us() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000ULL + tv.tv_usec;
}

// Simple JSON manipulation (find and replace nonce) - same as original
char* update_nonce_in_json(const char* json, uint64_t nonce) {
    // Find existing nonce or create new one
    char* result = malloc(strlen(json) + 100); // Extra space for nonce
    char nonce_str[32];
    sprintf(nonce_str, "\"%llu\"", (unsigned long long)nonce);

    // Look for existing nonce tag
    char* nonce_pos = strstr(json, "\"nonce\"");
    if (nonce_pos) {
        // Find the value after "nonce":
        char* value_start = strchr(nonce_pos, ':');
        if (value_start) {
            value_start++;
            while (*value_start == ' ' || *value_start == '\t') value_start++;

            char* value_end = value_start;
            if (*value_start == '"') {
                value_end = strchr(value_start + 1, '"') + 1;
            } else {
                while (*value_end && *value_end != ',' && *value_end != ']' && *value_end != '}') {
                    value_end++;
                }
            }

            // Replace the nonce value
            strncpy(result, json, value_start - json);
            result[value_start - json] = '\0';
            strcat(result, nonce_str);
            strcat(result, value_end);
        }
    } else {
        // Add nonce to tags array
        char* tags_pos = strstr(json, "\"tags\"");
        if (tags_pos) {
            char* array_start = strchr(tags_pos, '[');
            if (array_start) {
                strncpy(result, json, array_start - json + 1);
                result[array_start - json + 1] = '\0';

                // Add nonce tag
                strcat(result, "[\"nonce\",");
                strcat(result, nonce_str);
                strcat(result, "]");

                // Add rest of tags
                char* rest = array_start + 1;
                while (*rest == ' ' || *rest == '\t' || *rest == '\n') rest++;
                if (*rest != ']') {
                    strcat(result, ",");
                    strcat(result, rest);
                } else {
                    strcat(result, rest);
                }
            }
        }
    }

    return result;
}

// Update timestamp in JSON to make each benchmark iteration unique
char* update_timestamp_in_json(const char* json, uint64_t timestamp) {
    char* result = malloc(strlen(json) + 50); // Extra space for timestamp
    char timestamp_str[32];
    sprintf(timestamp_str, "%llu", (unsigned long long)timestamp);

    // Look for existing created_at field
    char* timestamp_pos = strstr(json, "\"created_at\"");
    if (timestamp_pos) {
        // Find the value after "created_at":
        char* value_start = strchr(timestamp_pos, ':');
        if (value_start) {
            value_start++;
            while (*value_start == ' ' || *value_start == '\t') value_start++;

            char* value_end = value_start;
            // Skip over the number value (no quotes for numbers)
            while (*value_end && *value_end != ',' && *value_end != ']' && *value_end != '}') {
                value_end++;
            }

            // Replace the timestamp value
            strncpy(result, json, value_start - json);
            result[value_start - json] = '\0';
            strcat(result, timestamp_str);
            strcat(result, value_end);
        }
    } else {
        // If no created_at found, just return a copy
        strcpy(result, json);
    }

    return result;
}

// Thread data structure
typedef struct {
    int thread_id;
    const char* event_json;
    int difficulty;
    uint64_t start_nonce;
    uint64_t end_nonce;
    uint64_t attempts;
    uint64_t found_nonce;
    int found_solution;
} thread_data_t;

// Worker thread function
void* worker_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    uint64_t nonce = data->start_nonce;
    uint8_t hash[SHA256_DIGEST_SIZE];
    data->attempts = 0;
    data->found_solution = 0;

    while (nonce < data->end_nonce && !solution_found) {
        // Update nonce in JSON
        char* event_with_nonce = update_nonce_in_json(data->event_json, nonce);

        // Hash the event
        sha256_hash((uint8_t*)event_with_nonce, strlen(event_with_nonce), hash);
        data->attempts++;

        // Check if we found a valid proof
        int leading_zeros = count_leading_zeros(hash);
        if (leading_zeros >= data->difficulty) {
            // Found a solution! Set global flag to stop other threads
            pthread_mutex_lock(&solution_mutex);
            if (!solution_found) {
                solution_found = 1;
                global_found_nonce = nonce;
                data->found_nonce = nonce;
                data->found_solution = 1;
            }
            pthread_mutex_unlock(&solution_mutex);

            free(event_with_nonce);
            break;
        }

        free(event_with_nonce);
        nonce++;
    }

    return NULL;
}

// Get number of CPU cores
int get_cpu_cores() {
    return (int)sysconf(_SC_NPROCESSORS_ONLN);
}

// Parallel NIP-13 mining
int nip13_mine_parallel(const char* event_json, int difficulty, uint64_t max_iterations, uint64_t* found_nonce) {
    uint64_t start_time = get_time_us();
    pthread_t* threads = malloc(num_threads * sizeof(pthread_t));
    thread_data_t* thread_data = malloc(num_threads * sizeof(thread_data_t));

    // Divide the nonce space among threads
    uint64_t nonces_per_thread = max_iterations / num_threads;
    uint64_t remainder = max_iterations % num_threads;

    // Start worker threads
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].event_json = event_json;
        thread_data[i].difficulty = difficulty;
        thread_data[i].start_nonce = i * nonces_per_thread;
        thread_data[i].end_nonce = (i + 1) * nonces_per_thread;

        // Give remainder nonces to the last thread
        if (i == num_threads - 1) {
            thread_data[i].end_nonce += remainder;
        }

        pthread_create(&threads[i], NULL, worker_thread, &thread_data[i]);
    }

    // Wait for all threads to complete
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    // Calculate total attempts and results
    uint64_t total_attempts = 0;
    int winning_thread = -1;

    for (int i = 0; i < num_threads; i++) {
        total_attempts += thread_data[i].attempts;
        if (thread_data[i].found_solution) {
            winning_thread = i;
        }
    }

    uint64_t elapsed = get_time_us() - start_time;

    if (solution_found && winning_thread >= 0) {
        uint8_t hash[SHA256_DIGEST_SIZE];
        char hash_hex[65];

        // Verify the solution
        char* event_with_nonce = update_nonce_in_json(event_json, global_found_nonce);
        sha256_hash((uint8_t*)event_with_nonce, strlen(event_with_nonce), hash);
        hash_to_hex(hash, hash_hex);
        int leading_zeros = count_leading_zeros(hash);

        printf("✅ Found valid proof!\n");
        printf("🎯 Nonce: %llu (found by thread %d)\n", (unsigned long long)global_found_nonce, winning_thread);
        printf("🔒 Hash:  %s\n", hash_hex);
        printf("⚡ Leading zeros: %d\n", leading_zeros);
        printf("⏱️  Time: %.2f seconds\n", elapsed / 1000000.0);
        printf("🚀 Rate: %.2f MH/s (%.2f MH/s per thread)\n",
               (total_attempts / 1000000.0) / (elapsed / 1000000.0),
               (total_attempts / 1000000.0) / (elapsed / 1000000.0) / num_threads);
        printf("📊 Total attempts: %llu across %d threads\n", total_attempts, num_threads);

        *found_nonce = global_found_nonce;
        free(event_with_nonce);
        free(threads);
        free(thread_data);
        return 1;
    }

    printf("❌ No valid proof found after %llu attempts across %d threads\n", total_attempts, num_threads);
    printf("⏱️  Time: %.2f seconds\n", elapsed / 1000000.0);
    printf("🚀 Rate: %.2f MH/s\n", (total_attempts / 1000000.0) / (elapsed / 1000000.0));

    free(threads);
    free(thread_data);
    return 0;
}

// Parallel range mining for benchmark mode
int nip13_mine_range_parallel(const char* event_json, int difficulty, uint64_t start_nonce,
                             uint64_t end_nonce, uint64_t* found_nonce, uint64_t* attempts) {
    solution_found = 0; // Reset global flag
    pthread_t* threads = malloc(num_threads * sizeof(pthread_t));
    thread_data_t* thread_data = malloc(num_threads * sizeof(thread_data_t));

    uint64_t range_size = end_nonce - start_nonce;
    uint64_t nonces_per_thread = range_size / num_threads;
    uint64_t remainder = range_size % num_threads;

    // Start worker threads
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].event_json = event_json;
        thread_data[i].difficulty = difficulty;
        thread_data[i].start_nonce = start_nonce + (i * nonces_per_thread);
        thread_data[i].end_nonce = start_nonce + ((i + 1) * nonces_per_thread);

        // Give remainder nonces to the last thread
        if (i == num_threads - 1) {
            thread_data[i].end_nonce += remainder;
        }

        pthread_create(&threads[i], NULL, worker_thread, &thread_data[i]);
    }

    // Wait for all threads to complete
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    // Calculate total attempts
    *attempts = 0;
    for (int i = 0; i < num_threads; i++) {
        *attempts += thread_data[i].attempts;
    }

    int result = 0;
    if (solution_found) {
        *found_nonce = global_found_nonce;
        result = 1;
    }

    free(threads);
    free(thread_data);
    return result;
}

// Function to increment timestamp in JSON string
char* increment_timestamp_in_json(const char* json_str, int increment_seconds) {
    // Find the "created_at" field
    char* created_at_pos = strstr(json_str, "\"created_at\"");
    if (!created_at_pos) {
        // If no created_at field, just return a copy
        char* result = malloc(strlen(json_str) + 1);
        strcpy(result, json_str);
        return result;
    }
    
    // Find the colon after "created_at"
    char* colon_pos = strchr(created_at_pos, ':');
    if (!colon_pos) {
        char* result = malloc(strlen(json_str) + 1);
        strcpy(result, json_str);
        return result;
    }
    
    // Skip whitespace after colon
    char* value_start = colon_pos + 1;
    while (*value_start == ' ' || *value_start == '\t') value_start++;
    
    // Find the end of the timestamp value (comma or closing brace)
    char* value_end = value_start;
    while (*value_end && *value_end != ',' && *value_end != '}' && *value_end != ' ') {
        value_end++;
    }
    
    // Parse the current timestamp
    long current_timestamp = strtol(value_start, NULL, 10);
    long new_timestamp = current_timestamp + increment_seconds;
    
    // Create new JSON with updated timestamp
    size_t prefix_len = value_start - json_str;
    size_t suffix_len = strlen(value_end);
    char timestamp_str[32];
    snprintf(timestamp_str, sizeof(timestamp_str), "%ld", new_timestamp);
    
    char* result = malloc(prefix_len + strlen(timestamp_str) + suffix_len + 1);
    strncpy(result, json_str, prefix_len);
    result[prefix_len] = '\0';
    strcat(result, timestamp_str);
    strcat(result, value_end);
    
    return result;
}

// Parallel benchmark mode
int benchmark_mode_parallel(char* event_json, int difficulty, int target_solutions) {
    printf("🚀 Parallel Benchmark Mode: Finding %d solutions at difficulty %d (%d threads)\n",
           target_solutions, difficulty, num_threads);
    printf("📊 Measuring solutions per second with unique timestamps...\n\n");

    struct timeval start_time, current_time;
    gettimeofday(&start_time, NULL);

    int solutions_found = 0;
    uint64_t total_attempts = 0;
    uint64_t starting_nonce = 1;
    
    // Create a working copy of the JSON that we'll modify
    char* working_json = malloc(strlen(event_json) + 1);
    strcpy(working_json, event_json);

    while (solutions_found < target_solutions) {
        uint64_t found_nonce;
        uint64_t attempts_this_round = 0;

        // Mine with current timestamp
        if (nip13_mine_range_parallel(working_json, difficulty, starting_nonce,
                                    starting_nonce + 100000000ULL, &found_nonce, &attempts_this_round)) {
            solutions_found++;
            total_attempts += attempts_this_round;
            starting_nonce = 1; // Reset to beginning for next timestamp
            
            printf("✅ Solution %d found (nonce: %llu, attempts: %llu)\n", 
                   solutions_found, found_nonce, attempts_this_round);
            
            // Increment timestamp for next solution search
            char* new_json = increment_timestamp_in_json(working_json, 1);
            free(working_json);
            working_json = new_json;

        } else {
            printf("❌ Failed to find solution in range, extending search...\n");
            starting_nonce += 100000000ULL;
            if (starting_nonce > 1000000000000ULL) { // Prevent infinite loop
                printf("💔 Benchmark failed - difficulty may be too high\n");
                free(working_json);
                return 0;
            }
        }
    }
    
    free(working_json);

    // Final statistics
    gettimeofday(&current_time, NULL);
    double total_elapsed = (current_time.tv_sec - start_time.tv_sec) +
                          (current_time.tv_usec - start_time.tv_usec) / 1000000.0;

    double final_solutions_per_sec = solutions_found / total_elapsed;
    double final_hashrate_mhs = (total_attempts / total_elapsed) / 1000000.0;

    printf("🎉 Parallel Benchmark Complete!\n");
    printf("📊 Results for difficulty %d (%d threads):\n", difficulty, num_threads);
    printf("   Solutions found: %d\n", solutions_found);
    printf("   Total time: %.2f seconds\n", total_elapsed);
    printf("   Total attempts: %llu\n", total_attempts);
    printf("   Solutions per second: %.3f\n", final_solutions_per_sec);
    printf("   Hash rate: %.2f MH/s (%.2f MH/s per thread)\n", final_hashrate_mhs, final_hashrate_mhs / num_threads);
    printf("   Average attempts per solution: %.0f\n", (double)total_attempts / solutions_found);
    printf("\n");

    return 1;
}

// Main function
int main(int argc, char* argv[]) {
    // Initialize number of threads to CPU cores
    num_threads = get_cpu_cores();

    if (argc < 2) {
        printf("Usage: %s <event.json> [difficulty] [max_attempts|benchmark] [threads]\n", argv[0]);
        printf("  event.json   - Nostr event JSON file\n");
        printf("  difficulty   - Target difficulty in bits (default: 16)\n");
        printf("  max_attempts - Maximum attempts in millions (default: 100)\n");
        printf("  benchmark N  - Benchmark mode: find N solutions and measure solutions/sec\n");
        printf("  threads      - Number of threads (default: %d CPU cores)\n\n", num_threads);
        printf("Examples:\n");
        printf("  %s event.json 20 50              # Mine once, max 50M attempts\n", argv[0]);
        printf("  %s event.json 16 benchmark 5     # Find 5 solutions, measure solutions/sec\n", argv[0]);
        printf("  %s event.json 18 100 8           # Mine with 8 threads\n", argv[0]);
        printf("  %s event.json 16 benchmark 5 4   # Benchmark with 4 threads\n", argv[0]);
        printf("  %s event.json 20 benchmark 10 1  # Single-threaded benchmark\n", argv[0]);
        return 1;
    }

    // Parse arguments
    char* json_file = argv[1];
    int difficulty = (argc > 2) ? atoi(argv[2]) : 16;

    // Check for benchmark mode
    int is_benchmark_mode = 0;
    int target_solutions = 0;
    uint64_t max_attempts = 100000000ULL; // default 100M

    if (argc > 3) {
        if (strcmp(argv[3], "benchmark") == 0) {
            is_benchmark_mode = 1;
            target_solutions = (argc > 4) ? atoi(argv[4]) : 5;
            // Check for thread count after benchmark target
            if (argc > 5) {
                num_threads = atoi(argv[5]);
            }
        } else {
            max_attempts = atoll(argv[3]) * 1000000ULL;
            // Check for thread count
            if (argc > 4) {
                num_threads = atoi(argv[4]);
            }
        }
    }

    // Validate thread count
    if (num_threads < 1 || num_threads > 128) {
        printf("❌ Error: Thread count must be between 1 and 128\n");
        return 1;
    }

    // Validate difficulty
    if (difficulty < 1 || difficulty > 32) {
        printf("❌ Error: Difficulty must be between 1 and 32 bits\n");
        return 1;
    }

    if (is_benchmark_mode && target_solutions < 1) {
        printf("❌ Error: Benchmark mode requires at least 1 solution\n");
        return 1;
    }

    // Read event JSON
    FILE* fp = fopen(json_file, "r");
    if (!fp) {
        printf("❌ Error: Cannot open file %s\n", json_file);
        return 1;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char* event_json = malloc(file_size + 1);
    fread(event_json, 1, file_size, fp);
    event_json[file_size] = '\0';
    fclose(fp);

    // Remove any trailing whitespace
    while (file_size > 0 && (event_json[file_size-1] == '\n' || event_json[file_size-1] == ' ')) {
        event_json[--file_size] = '\0';
    }

    if (is_benchmark_mode) {
        // Run benchmark mode
        int result = benchmark_mode_parallel(event_json, difficulty, target_solutions);
        free(event_json);
        return result ? 0 : 1;
    } else {
        printf("🔢 Max attempts: %.0f million across %d threads\n", max_attempts / 1000000.0, num_threads);
        printf("\n");

        // Start parallel mining
        uint64_t found_nonce;
        if (nip13_mine_parallel(event_json, difficulty, max_attempts, &found_nonce)) {
            // Output the final event with nonce
            char* final_event = update_nonce_in_json(event_json, found_nonce);
            printf("📄 Final event:\n%s\n", final_event);

            // Save to output file
            char output_file[256];
            sprintf(output_file, "mined_parallel_%s", json_file);
            FILE* out = fopen(output_file, "w");
            if (out) {
                fprintf(out, "%s\n", final_event);
                fclose(out);
                printf("💾 Saved to: %s\n", output_file);
            }

            free(final_event);
            free(event_json);
            return 0;
        } else {
            printf("\n💔 Mining failed - try lower difficulty or more attempts\n");
            free(event_json);
            return 1;
        }
    }
}
