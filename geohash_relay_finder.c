/*
 * Geohash to Lat/Lon Converter and Nearest Relay Finder
 * Converts geohash to latitude/longitude and finds nearest 5 relays from CSV list
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#define MAX_RELAYS 10000
#define MAX_LINE_LENGTH 512
#define EARTH_RADIUS_KM 6371.0

// Base32 alphabet for geohash decoding
static const char base32[] = "0123456789bcdefghjkmnpqrstuvwxyz";

// Structure to hold relay information
typedef struct {
    char url[256];
    double latitude;
    double longitude;
    double distance;
} Relay;

// Structure to hold geohash decoding result
typedef struct {
    double latitude;
    double longitude;
} GeoCoordinate;

// Function to find character position in base32 alphabet
int base32_index(char c) {
    for (int i = 0; i < 32; i++) {
        if (base32[i] == c) {
            return i;
        }
    }
    return -1;
}

// Decode geohash to latitude and longitude
GeoCoordinate decode_geohash(const char* geohash) {
    GeoCoordinate coord = {0.0, 0.0};
    double lat_min = -90.0, lat_max = 90.0;
    double lon_min = -180.0, lon_max = 180.0;

    int is_even = 1; // Start with longitude (even)

    for (int i = 0; geohash[i] != '\0'; i++) {
        char c = tolower(geohash[i]);
        int idx = base32_index(c);

        if (idx == -1) {
            fprintf(stderr, "Invalid geohash character: %c\n", c);
            return coord;
        }

        // Process each bit of the base32 character (5 bits)
        for (int bit = 4; bit >= 0; bit--) {
            int bit_value = (idx >> bit) & 1;

            if (is_even) { // Longitude
                double mid = (lon_min + lon_max) / 2.0;
                if (bit_value) {
                    lon_min = mid;
                } else {
                    lon_max = mid;
                }
            } else { // Latitude
                double mid = (lat_min + lat_max) / 2.0;
                if (bit_value) {
                    lat_min = mid;
                } else {
                    lat_max = mid;
                }
            }
            is_even = !is_even;
        }
    }

    coord.latitude = (lat_min + lat_max) / 2.0;
    coord.longitude = (lon_min + lon_max) / 2.0;

    return coord;
}

// Convert degrees to radians
double deg_to_rad(double deg) {
    return deg * M_PI / 180.0;
}

// Calculate distance between two coordinates using Haversine formula
double calculate_distance(double lat1, double lon1, double lat2, double lon2) {
    double dlat = deg_to_rad(lat2 - lat1);
    double dlon = deg_to_rad(lon2 - lon1);

    lat1 = deg_to_rad(lat1);
    lat2 = deg_to_rad(lat2);

    double a = sin(dlat/2) * sin(dlat/2) +
               cos(lat1) * cos(lat2) * sin(dlon/2) * sin(dlon/2);
    double c = 2 * atan2(sqrt(a), sqrt(1-a));

    return EARTH_RADIUS_KM * c;
}

// Parse CSV line and extract relay information
int parse_relay_line(const char* line, Relay* relay) {
    char* line_copy = strdup(line);
    char* token;
    int field = 0;

    // Remove newline if present
    char* newline = strchr(line_copy, '\n');
    if (newline) *newline = '\0';

    // Parse comma-separated values
    token = strtok(line_copy, ",");
    while (token != NULL && field < 3) {
        switch (field) {
            case 0: // URL
                strncpy(relay->url, token, sizeof(relay->url) - 1);
                relay->url[sizeof(relay->url) - 1] = '\0';
                break;
            case 1: // Latitude
                relay->latitude = atof(token);
                break;
            case 2: // Longitude
                relay->longitude = atof(token);
                break;
        }
        field++;
        token = strtok(NULL, ",");
    }

    free(line_copy);
    return (field == 3) ? 1 : 0; // Return 1 if all fields parsed successfully
}

// Load relays from CSV file
int load_relays(const char* filename, Relay* relays) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error: Cannot open relay file '%s'\n", filename);
        return 0;
    }

    char line[MAX_LINE_LENGTH];
    int count = 0;

    // Skip header line if it exists
    if (fgets(line, sizeof(line), file)) {
        // Check if first line looks like a header (contains non-numeric data in lat/lon fields)
        if (strstr(line, "Relay") || strstr(line, "URL") || strstr(line, "Latitude")) {
            // This looks like a header, skip it
        } else {
            // This is data, parse it
            if (parse_relay_line(line, &relays[count])) {
                count++;
            }
        }
    }

    while (fgets(line, sizeof(line), file) && count < MAX_RELAYS) {
        if (parse_relay_line(line, &relays[count])) {
            count++;
        }
    }

    fclose(file);
    return count;
}

// Comparison function for sorting relays by distance
int compare_relays(const void* a, const void* b) {
    const Relay* relay_a = (const Relay*)a;
    const Relay* relay_b = (const Relay*)b;

    if (relay_a->distance < relay_b->distance) return -1;
    if (relay_a->distance > relay_b->distance) return 1;
    return 0;
}

// Find and return the 5 nearest relays
void find_nearest_relays(Relay* relays, int relay_count, double target_lat, double target_lon, int quiet_mode) {
    // Calculate distances for all relays
    for (int i = 0; i < relay_count; i++) {
        relays[i].distance = calculate_distance(target_lat, target_lon,
                                               relays[i].latitude, relays[i].longitude);
    }

    // Sort relays by distance
    qsort(relays, relay_count, sizeof(Relay), compare_relays);

    int max_results = (relay_count < 5) ? relay_count : 5;

    if (quiet_mode) {
        // Just print space-delimited relay URLs
        for (int i = 0; i < max_results; i++) {
            printf("%s", relays[i].url);
            if (i < max_results - 1) {
                printf(" ");
            }
        }
        printf("\n");
    } else {
        // Print full table
        printf("Nearest 5 relays:\n");
        printf("%-50s %12s %12s %10s\n", "Relay URL", "Latitude", "Longitude", "Distance (km)");
        printf("%-50s %12s %12s %10s\n", "---------", "--------", "---------", "------------");

        for (int i = 0; i < max_results; i++) {
            printf("%-50s %12.6f %12.6f %10.2f\n",
                   relays[i].url, relays[i].latitude, relays[i].longitude, relays[i].distance);
        }
    }
}

// Print usage information
void print_usage(const char* program_name) {
    printf("Usage: %s [-q] <geohash> <relay_csv_file>\n", program_name);
    printf("\n");
    printf("Arguments:\n");
    printf("  -q              Quiet mode: output only space-delimited relay URLs\n");
    printf("  geohash         A geohash string (e.g., '9q8yy')\n");
    printf("  relay_csv_file  CSV file with format: 'Relay URL,Latitude,Longitude'\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s 9q8yy relays.csv\n", program_name);
    printf("  %s -q 9q8yy relays.csv\n", program_name);
    printf("\n");
    printf("CSV file format:\n");
    printf("  wss://relay1.example.com,37.7749,-122.4194\n");
    printf("  wss://relay2.example.com,40.7128,-74.0060\n");
}

int main(int argc, char* argv[]) {
    int quiet_mode = 0;
    const char* geohash;
    const char* csv_file;

    // Parse arguments
    if (argc == 3) {
        geohash = argv[1];
        csv_file = argv[2];
    } else if (argc == 4 && strcmp(argv[1], "-q") == 0) {
        quiet_mode = 1;
        geohash = argv[2];
        csv_file = argv[3];
    } else {
        print_usage(argv[0]);
        return 1;
    }

    // Decode geohash to coordinates
    GeoCoordinate coord = decode_geohash(geohash);
    if (!quiet_mode) {
        printf("Decoding geohash: %s\n", geohash);
        printf("Latitude: %.6f, Longitude: %.6f\n\n", coord.latitude, coord.longitude);
    }

    // Load relays from CSV
    Relay* relays = malloc(MAX_RELAYS * sizeof(Relay));
    if (!relays) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return 1;
    }

    if (!quiet_mode) {
        printf("Loading relays from: %s\n", csv_file);
    }
    int relay_count = load_relays(csv_file, relays);
    if (relay_count == 0) {
        if (!quiet_mode) {
            fprintf(stderr, "Error: No relays loaded from file\n");
        }
        free(relays);
        return 1;
    }

    if (!quiet_mode) {
        printf("Loaded %d relays\n\n", relay_count);
    }

    // Find and display nearest relays
    find_nearest_relays(relays, relay_count, coord.latitude, coord.longitude, quiet_mode);

    free(relays);
    return 0;
}