/*
 * Standalone NIP-13 Proof of Work Miner
 * Using simplified SHA256 implementation based on hashcat primitives
 * Removes all external dependencies for maximum portability
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <math.h>

// Simplified SHA256 constants and functions based on hashcat
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

// Simple JSON manipulation (find and replace nonce)
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

// NIP-13 Proof of Work miner with range support
int nip13_mine_range(const char* event_json, int difficulty, uint64_t start_nonce,
                    uint64_t end_nonce, uint64_t* found_nonce, uint64_t* attempts) {
    uint64_t nonce = start_nonce;
    uint8_t hash[SHA256_DIGEST_SIZE];
    *attempts = 0;

    while (nonce < end_nonce) {
        // Update nonce in JSON
        char* event_with_nonce = update_nonce_in_json(event_json, nonce);

        // Hash the event
        sha256_hash((uint8_t*)event_with_nonce, strlen(event_with_nonce), hash);
        (*attempts)++;

        // Check if we found a valid proof
        int leading_zeros = count_leading_zeros(hash);
        if (leading_zeros >= difficulty) {
            *found_nonce = nonce;
            free(event_with_nonce);
            return 1;
        }

        free(event_with_nonce);
        nonce++;
    }

    return 0; // Not found in range
}

// NIP-13 Proof of Work miner
int nip13_mine(const char* event_json, int difficulty, uint64_t max_iterations, uint64_t* found_nonce) {
    printf("🔨 Starting NIP-13 mining (difficulty: %d bits)\n", difficulty);
    printf("📝 Event: %.60s%s\n", event_json, strlen(event_json) > 60 ? "..." : "");

    uint64_t nonce = 0;
    uint64_t start_time = get_time_us();
    uint64_t last_report = start_time;
    uint8_t hash[SHA256_DIGEST_SIZE];
    char hash_hex[65];

    while (nonce < max_iterations) {
        // Update nonce in JSON
        char* event_with_nonce = update_nonce_in_json(event_json, nonce);

        // Hash the event
        sha256_hash((uint8_t*)event_with_nonce, strlen(event_with_nonce), hash);

        // Check if we found a valid proof
        int leading_zeros = count_leading_zeros(hash);
        if (leading_zeros >= difficulty) {
            hash_to_hex(hash, hash_hex);
            printf("✅ Found valid proof!\n");
            printf("🎯 Nonce: %llu\n", (unsigned long long)nonce);
            printf("🔒 Hash:  %s\n", hash_hex);
            printf("⚡ Leading zeros: %d\n", leading_zeros);

            uint64_t elapsed = get_time_us() - start_time;
            printf("⏱️  Time: %.2f seconds\n", elapsed / 1000000.0);
            printf("🚀 Rate: %.2f MH/s\n", (nonce / 1000000.0) / (elapsed / 1000000.0));

            *found_nonce = nonce;
            free(event_with_nonce);
            return 1;
        }

        free(event_with_nonce);
        nonce++;

        // Progress report every 1M iterations
        if (nonce % 1000000 == 0) {
            uint64_t now = get_time_us();
            double rate = 1000000.0 / ((now - last_report) / 1000000.0);
            printf("⚡ %llu M attempts, %.2f MH/s, best: %d zeros\n",
                   (unsigned long long)(nonce / 1000000), rate / 1000000.0, leading_zeros);
            last_report = now;
        }
    }

    printf("❌ No valid proof found after %llu attempts\n", (unsigned long long)max_iterations);
    return 0;
}


// Benchmark mode - find N solutions and calculate solutions/second
int benchmark_mode(char* event_json, int difficulty, int target_solutions) {
    printf("🚀 Benchmark Mode: Finding %d solutions at difficulty %d\n", target_solutions, difficulty);
    printf("📊 Measuring solutions per second...\n\n");

    struct timeval start_time, current_time;
    gettimeofday(&start_time, NULL);

    int solutions_found = 0;
    uint64_t total_attempts = 0;
    uint64_t starting_nonce = 1;

    while (solutions_found < target_solutions) {
        uint64_t found_nonce;
        uint64_t attempts_this_round = 0;

        // Mine starting from the last found nonce + 1000 to avoid duplicates
        if (nip13_mine_range(event_json, difficulty, starting_nonce,
                            starting_nonce + 100000000ULL, &found_nonce, &attempts_this_round)) {
            solutions_found++;
            total_attempts += attempts_this_round;
            starting_nonce = found_nonce + 1000; // Skip ahead to avoid immediate re-finding

        } else {
            printf("❌ Failed to find solution in range, extending search...\n");
            starting_nonce += 100000000ULL;
            if (starting_nonce > 1000000000000ULL) { // Prevent infinite loop
                printf("💔 Benchmark failed - difficulty may be too high\n");
                return 0;
            }
        }
    }

    // Final statistics
    gettimeofday(&current_time, NULL);
    double total_elapsed = (current_time.tv_sec - start_time.tv_sec) +
                          (current_time.tv_usec - start_time.tv_usec) / 1000000.0;

    double final_solutions_per_sec = solutions_found / total_elapsed;
    double final_hashrate_mhs = (total_attempts / total_elapsed) / 1000000.0;

    printf("🎉 Benchmark Complete!\n");
    printf("📊 Results for difficulty %d:\n", difficulty);
    printf("   Solutions found: %d\n", solutions_found);
    printf("   Total time: %.2f seconds\n", total_elapsed);
    printf("   Total attempts: %llu\n", total_attempts);
    printf("   Solutions per second: %.3f\n", final_solutions_per_sec);
    printf("   Hash rate: %.2f MH/s\n", final_hashrate_mhs);
    printf("   Average attempts per solution: %.0f\n", (double)total_attempts / solutions_found);

    return 1;
}

// Main function
int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s <event.json> [difficulty] [max_attempts|benchmark]\n", argv[0]);
        printf("  event.json   - Nostr event JSON file\n");
        printf("  difficulty   - Target difficulty in bits (default: 16)\n");
        printf("  max_attempts - Maximum attempts in millions (default: 100)\n");
        printf("  benchmark N  - Benchmark mode: find N solutions and measure solutions/sec\n\n");
        printf("Examples:\n");
        printf("  %s event.json 20 50          # Mine once, max 50M attempts\n", argv[0]);
        printf("  %s event.json 16 benchmark 5 # Find 5 solutions, measure solutions/sec\n", argv[0]);
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
        } else {
            max_attempts = atoll(argv[3]) * 1000000ULL;
        }
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
        int solutions_found = benchmark_mode(event_json, difficulty, target_solutions);
        free(event_json);
        return (solutions_found == target_solutions) ? 0 : 1;
    } else {
        printf("🔢 Max attempts: %.0f million\n", max_attempts / 1000000.0);
        printf("\n");

        // Start regular mining
        uint64_t found_nonce;
        if (nip13_mine(event_json, difficulty, max_attempts, &found_nonce)) {
            // Output the final event with nonce
            char* final_event = update_nonce_in_json(event_json, found_nonce);
            printf("📄 Final event:\n%s\n", final_event);

            // Save to output file
            char output_file[256];
            sprintf(output_file, "mined_%s", json_file);
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
