// Include the most common headers from the C standard library
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Include the main libnx system header, for Switch development
#include <switch.h>

#include <curl/curl.h>

// #define SERVER_URL "https://scaleway.testdebit.info/10G/10G.iso" //internet speed test
// #define SERVER_URL "http://192.168.0.179/big_file"

#define URL_FILE_NAME "url.txt"

#define DOWNLOAD_TIME 25
#define MAX_SPEED_SAMPLE_INTERVAL 2000 // milliseconds

enum currentState {
    MENU,
    DOWNLOAD_TEST,
    UPLOAD_TEST
};

// This example shows how to use libcurl. For more examples, see the official examples: https://curl.haxx.se/libcurl/c/example.html
u64 getTimeMs() {
    return (armGetSystemTick() * 1000) / armGetSystemTickFreq();
}

void format_bandwidth(double bytes_per_sec, char *buf, size_t max_len) {
    const char *units[] = {"bps", "Kbps", "Mbps", "Gbps"};
    int i = 0;
    // while (bytes_per_sec >= 1024.0 && i < 4) {
    while (bytes_per_sec >= 1024.0 && i < 4) {
        bytes_per_sec /= 1024.0;
        i++;
    }
    snprintf(buf, max_len, "%.2f %s", bytes_per_sec * 8, units[i]);
}

static size_t cb(char *data, size_t size, size_t nmemb, void *speed)
{
    const size_t realsize = nmemb * size;
    static size_t bytesInLastSecond = 0;
    static u64 lastTime = 0;
    const u64 currentTime = getTimeMs();

    curl_off_t* maxDownloadSpeed = (curl_off_t*)speed;

    bytesInLastSecond += realsize;

    if(currentTime - lastTime > MAX_SPEED_SAMPLE_INTERVAL) { // calculate time difference
        curl_off_t calculatedSpeed = (bytesInLastSecond * 1000) / (currentTime - lastTime);

        if(calculatedSpeed > *maxDownloadSpeed)
            *maxDownloadSpeed = calculatedSpeed;
        
        lastTime = currentTime;
        bytesInLastSecond = 0;

        char buff[512];

        format_bandwidth((double)calculatedSpeed, buff, sizeof(buff));
        printf("Current speed: %s\n", buff);
        consoleUpdate(NULL);
    }
 
    return realsize;
}

curl_off_t downloadTest(void) {
    CURL *curl;
    CURLcode res;

    curl_off_t maxDownloadSpeed = 0;
    curl_off_t averageSpeed = -1;

    char URLString[512] = "";

    FILE* fp = fopen(URL_FILE_NAME, "r");

    if(fp == NULL) {
        return -1;
    }

    // get the file size
    fseek(fp, 0L, SEEK_END);
    size_t sz = ftell(fp);
    fseek(fp, 0L, SEEK_SET);

    if(sz >= 512) {
        fclose(fp);
        return -1;
    }

    fgets(URLString, sizeof(URLString), fp);
    URLString[strcspn(URLString, "\r\n")] = '\0'; // remove newline

    fclose(fp);

    //clear the screen
    consoleClear();

    curl_global_init(CURL_GLOBAL_DEFAULT);

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, URLString);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "libnx curl speedtest/1.0");
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, DOWNLOAD_TIME);
        // Add any other options here.
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&maxDownloadSpeed);

        printf("Downloading file now, please wait %d seconds\n", DOWNLOAD_TIME);
        consoleUpdate(NULL);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK && res != CURLE_OPERATION_TIMEDOUT) {
            printf("curl_easy_perform() failed: %s, \tURL: %s", curl_easy_strerror(res), URLString);
            curl_easy_cleanup(curl);
            return -1;
        }

        res = curl_easy_getinfo(curl, CURLINFO_SPEED_DOWNLOAD_T, &averageSpeed);
        if(res != CURLE_OK) {
            printf("Failed to get average download speed\n");
            curl_easy_cleanup(curl);
            return -1;
        }
        
        curl_easy_cleanup(curl);
    }

    curl_global_cleanup();

    char buffAverage[512];
    char buffMax[512];

    format_bandwidth((double)averageSpeed, buffAverage, sizeof(buffAverage));
    format_bandwidth((double)maxDownloadSpeed, buffMax, sizeof(buffMax));

    printf("Average download speed: %s, maximum speed: %s\n\n", buffAverage, buffMax);

    return -1;
}

// Main program entrypoint
int main(int argc, char* argv[])
{
    // This example uses a text console, as a simple way to output text to the screen.
    // If you want to write a software-rendered graphics application,
    //   take a look at the graphics/simplegfx example, which uses the libnx Framebuffer API instead.
    // If on the other hand you want to write an OpenGL based application,
    //   take a look at the graphics/opengl set of examples, which uses EGL instead.
    consoleInit(NULL);

    // Configure our supported input layout: a single player with standard controller styles
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);

    // Initialize the default gamepad (which reads handheld mode inputs as well as the first connected controller)
    PadState pad;
    padInitializeDefault(&pad);

    socketInitializeDefault();

    printf("Speed test program for the Nintendo Switch!\n\n");

    int state = MENU;
    int lastMenuState = -5;

    // Main loop
    while(appletMainLoop())
    {
        // Scan the gamepad. This should be done once for each frame
        padUpdate(&pad);

        // padGetButtonsDown returns the set of buttons that have been
        // newly pressed in this frame compared to the previous one
        u64 kDown = padGetButtonsDown(&pad);

        if (kDown & HidNpadButton_Plus)
            break; // break in order to return to hbmenu

        switch (state)
        {
        case MENU:
            if(kDown & HidNpadButton_B)
                goto exitProgram;
            if(kDown & HidNpadButton_A) {
                state = DOWNLOAD_TEST;
                break;
            }

            if(lastMenuState != state) {
                printf("Press A to start the download test\nPress B to quit\n\n");
            }
            break;

        case DOWNLOAD_TEST:
            state = MENU;
            printf("Starting download test\n");
            downloadTest();
            continue; // prevents lastMenuState being updated

        default:
            break;
        }

        lastMenuState = state;

        // Update the console, sending a new frame to the display
        consoleUpdate(NULL);
    }

exitProgram:

    socketExit();
    // Deinitialize and clean up resources used by the console (important!)
    consoleExit(NULL);
    return 0;
}
